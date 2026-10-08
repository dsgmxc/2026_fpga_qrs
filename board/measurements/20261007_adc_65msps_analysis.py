#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把板上 adc.out 里的真实采集拿来跑新解调(与 adc_sample.c 的 gcd 建表逐式对应)。"""
import re, math
from math import gcd, cos, sin, pi, sqrt
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

LOG = '/tmp/adc_board.out'
FS_BASE = 130_000_000
DDS_MCLK = 25_000_000

def dds_hz(code):
    return code * DDS_MCLK / 2**28

# 写入 0x80 的码 → **固件拿到的标称频率**(整数 uint32_t)。
# ⚠️ 和 dds_hz() 的真值不等 —— 25 MHz/2^28 的量化步长是 0.0931 Hz,
#    0x0A3D70A 真值 999999.9776 Hz。差 0.022 Hz,窗内 25 个周期只差 5.6e-7 周期,
#    远小于 1 码,故按标称算没有问题;但**不能拿真值去判"是否整数比"**。
NOMINAL = {0x0A3D70A: 1000000, 0x10624E: 100000, 0x147AE14: 2000000}

# ---- 解析 -----------------------------------------------------------------
runs = []          # (div, ph, fe_hz, vals)
fe_now = NOMINAL[0x0A3D70A]  # 日志开头 DDS 已是 0x80 的上电初始化值 = 1 MHz
with open(LOG) as f:
    lines = f.read().splitlines()
i = 0
while i < len(lines):
    ln = lines[i]
    m = re.match(r'>>> wr 0x80 (\S+)', ln)
    if m:
        code = int(m.group(1), 0)
        fe_now = NOMINAL.get(code, dds_hz(code))
        i += 1; continue
    m = re.match(r'>>> dump (\d+) (-?\d+) (\d+)', ln)
    if m:
        div, ph, n = int(m.group(1)), int(m.group(2)), int(m.group(3))
        vals = []
        j = i + 1
        while j < len(lines) and '===DONE===' not in lines[j]:
            if re.match(r'^\s+\d+(\s+\d+)*\s*$', lines[j]) and '统计' not in lines[j]:
                vals += [int(v) for v in lines[j].split()]
            j += 1
        if len(vals) >= n:
            runs.append((div, ph, fe_now, vals[:n]))
        i = j; continue
    i += 1

# ---- 新解调(等价 adc_sample.c::AdcSample_Demodulate) ---------------------
def demod(code, sample_hz, fe_hz):
    fs, fe = int(sample_hz), int(round(fe_hz))
    g = gcd(fs, fe)
    q = fs // g
    L = (len(code) // q) * q
    if L == 0:
        return None
    M = (L // q) * (fe // g)
    npp = L / M
    if npp < 4:
        return None
    si = sq = 0.0
    for k in range(L):
        th = 2 * pi * M * k / L
        si += code[k] * cos(th)
        sq -= code[k] * sin(th)
    si = 2 * si / L
    sq = 2 * sq / L
    return dict(L=L, M=M, npp=npp, i=si, q=sq, mag=sqrt(si * si + sq * sq))

# ---- 旧实现的门槛(表长 = 一个激励周期的点数) ---------------------------
def old_ok(sample_hz, fe_hz):
    n = round(sample_hz / fe_hz)          # 旧的 nn = f_s/f_e
    exact = abs(sample_hz / fe_hz - n) < 1e-9
    return (exact and 4 <= n <= 64), n, exact

print("=" * 78)
print("板上真实采集 · 新解调 vs 旧门槛")
print("=" * 78)
print(f"{'DIV':>3} {'f_s':>9} {'PH':>4} {'f_e':>9} {'N=f_s/f_e':>10} "
      f"{'旧':>4} {'L':>4} {'M':>4} {'点/周期':>8} {'mag(code)':>10} {'mag(V)':>9}")
print("-" * 78)
rows = []
for (div, ph, fe, vals) in runs:
    fs = FS_BASE / div
    ok_old, N, exact = old_ok(fs, fe)
    d = demod(vals, fs, fe)
    mv = d['mag'] * 2.0 / 4096 if d else float('nan')
    old_s = ("OK" if ok_old else ("静默错" if exact else "报错")) if N >= 4 else "报错"
    if d is None:
        rows.append((div, ph, fe, fs, vals, None))
        print(f"{div:>3} {fs/1e6:>6.3f}M {ph:>4} {fe/1e6:>6.3f}M {fs/fe:>10.2f} "
              f"{old_s:>4} {'--':>4} {'--':>4} {'--':>8} {'拒绝':>10}")
        continue
    rows.append((div, ph, fe, fs, vals, d))
    print(f"{div:>3} {fs/1e6:>6.3f}M {ph:>4} {fe/1e6:>6.3f}M {fs/fe:>10.2f} "
          f"{old_s:>4} {d['L']:>4} {d['M']:>4} {d['npp']:>8.2f} "
          f"{d['mag']:>10.3f} {mv:>9.5f}")

# ---- 图 -------------------------------------------------------------------
n = len(rows)
fig, axes = plt.subplots(n, 1, figsize=(13, 2.35 * n))
if n == 1:
    axes = [axes]
for ax, (div, ph, fe, fs, vals, d) in zip(axes, rows):
    x = np.array(vals, dtype=float)
    ax.plot(x, lw=0.9, color='#1f77b4', marker='o', ms=2.2, mfc='none')
    ax.set_ylabel('code', fontsize=8)
    lo, hi = min(vals), max(vals)
    pad = max(1.2, (hi - lo) * 0.35)
    ax.set_ylim(lo - pad, hi + pad)
    ttl = (f"DIV={div}  fs={fs/1e6:.3f} MSPS   PH_OFF={ph:+d}   "
           f"fe={fe/1e3:.1f} kHz   p-p={hi-lo}  uniq={len(set(vals))}")
    if d:
        ttl += (f"   |  DEMOD L={d['L']} M={d['M']} {d['npp']:.2f} pts/period"
                f"  mag={d['mag']:.2f} code = {d['mag']*2.0/4096*1000:.2f} mV")
    ax.set_title(ttl, fontsize=8.5, loc='left')
    ax.grid(alpha=0.25)
    if d:
        ax.axvline(d['L'] - 0.5, color='crimson', ls='--', lw=1.0)
axes[-1].set_xlabel('sample index   (right of red line = tail not used by the demod)',
                    fontsize=8)
fig.tight_layout()
fig.savefig('/tmp/board_wave.png', dpi=115)
print("\n图 → /tmp/board_wave.png")
