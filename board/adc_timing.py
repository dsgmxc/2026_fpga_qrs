#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
adc_timing.py —— 用**采集时长**反向验证 ADC 分频器是真的(2026-10-06)

================================================================================
为什么这个测量有说服力
================================================================================
`adc_capture.v` 的分频器是 `adc_clk = clk_base / P`,`clk_base = 130 MHz`(MMCM)。
单次采集要**满 4096 个点**才置 done ⇒

        采集时长 = P × 4096 / 130 MHz

    P = 13  →   409.6 µs
    P = 130 →  4096.0 µs      ← 与 P=13 相差**整整 10 倍**

如果 P 只是个"写进去读了回来"的死寄存器、或者分频器实际固定(比如只有 2 分频),
这个 10 倍关系**不可能出现**。⇒ 时长本身就是一个独立的、不可伪造的读数,
比"读回寄存器值"硬得多。

⚠️ 本脚本**不加载位流**,只做 MMIO —— 可以随便重启重跑。
   `Overlay()` 只许在一个上电周期里调一次,那是硬规则。
   前提:位流已经由 adc_probe.py 的 `init` 装好了。

⚠️ 精度上限:PYNQ 的 `MMIO.read` 有 Python 开销(每次若干 µs),所以
   **先测一次"轮询空转开销"再从总时长里减掉**,否则小 P 会被开销淹没。
   P=13 与 P=130 的 10 倍关系不受开销影响(开销是加性常数,不是乘性)。

用法(root):
    echo xilinx | sudo -S -E /usr/local/share/pynq-venv/bin/python3 -u \
        /home/xilinx/adc_timing.py
"""
import time
import statistics

AXI_BASE = 0x43C00000
R_ADC_CFG = 0x88
R_ADC_CTRL = 0x8C
R_ADC_STS = 0x90
R_ADC_BUF_IDX = 0x94
R_ADC_BUF_DATA = 0x98

BASE_HZ = 130e6
BUF_N = 4096
# 全部"方便分频"的档位(130 = lcm(65,10) 的意义就在这张表上)
DIVS = [2, 4, 5, 10, 13, 26, 65, 130]


def main():
    from pynq import MMIO
    m = MMIO(AXI_BASE, 0x1000)

    # ---------------------------------------------------------------- 开销标定
    # done 已经置位的情况下反复读 STS,测出"每次读 + 循环"的纯开销
    sts = m.read(R_ADC_STS)
    n = 200
    t0 = time.perf_counter()
    for _ in range(n):
        m.read(R_ADC_STS)
    ovh = (time.perf_counter() - t0) / n
    print("轮询开销标定:%d 次读 = %.4f s ⇒ 每次 %.3f µs"
          % (n, time.perf_counter() - t0, ovh * 1e6))
    print("当前 0x90 = 0x%08X (locked=%d done=%d busy=%d)"
          % (sts, sts & 1, (sts >> 1) & 1, (sts >> 2) & 1))

    # ---------------------------------------------------------------- 按 DIV 扫
    print()
    print("  DIV   adc_clk     预期时长      实测时长    差值    首点   去重")
    print("  " + "-" * 72)
    rows = []
    for div in DIVS:
        m.write(R_ADC_CFG, div & 0xFF)          # CONT=0 / PH_OFF=0 / OE_N=0 / PDN=0
        exp = div * BUF_N / BASE_HZ

        t0 = time.perf_counter()
        m.write(R_ADC_CTRL, 1)                  # GO
        got = False
        while time.perf_counter() - t0 < 5.0:
            if (m.read(R_ADC_STS) >> 1) & 1:
                got = True
                break
        t1 = time.perf_counter()

        if not got:
            print("  %4d   %7.3f MHz   %8.1f µs   done 未置位(>5 s)" % (div, BASE_HZ / div / 1e6, exp * 1e6))
            continue

        meas = (t1 - t0) - ovh                  # 减掉最后一次读的开销
        m.write(R_ADC_BUF_IDX, 0)
        first = m.read(R_ADC_BUF_DATA) & 0xFFF
        vs = []
        for i in range(64):
            m.write(R_ADC_BUF_IDX, i)
            vs.append(m.read(R_ADC_BUF_DATA) & 0xFFF)

        print("  %4d   %7.3f MHz   %8.1f µs   %8.1f µs  %+7.2f%%  %4d   %4d"
              % (div, BASE_HZ / div / 1e6, exp * 1e6, meas * 1e6,
                 (meas - exp) / exp * 100, first, len(set(vs))))
        rows.append((div, exp, meas))

    # ---------------------------------------------------------------- 比值核对
    print()
    if len(rows) >= 2:
        a = next((r for r in rows if r[0] == 13), None)
        b = next((r for r in rows if r[0] == 130), None)
        if a and b:
            print("★ 关键比值:P=130 / P=13 = %.3f  (应为 10.000)"
                  % (b[2] / a[2]))
            print("  ⇒ 若接近 10,分频器是真的在按 P 改变采样率;")
            print("    若接近 1,说明 P 只是个死寄存器(实际采样率固定)。")
        lo = next((r for r in rows if r[0] == 2), None)
        if lo and a:
            print("  P=2 / P=13  = %.3f  (应为 %.3f)"
                  % (lo[2] / a[2], 2 / 13.0))

    # ---------------------------------------------------------------- 眼图扫描
    print()
    print("PH_OFF 扫描(DIV=13):采 256 点,看各相位的读数是否稳定")
    print("  PH_OFF   min   max  p-p  去重  首8点")
    m.write(R_ADC_CFG, 13)
    for ph in range(-6, 7):
        cfg = 13 | ((ph & 0xFF) << 8)
        m.write(R_ADC_CFG, cfg)
        t0 = time.perf_counter()
        m.write(R_ADC_CTRL, 1)
        ok = False
        while time.perf_counter() - t0 < 2.0:
            if (m.read(R_ADC_STS) >> 1) & 1:
                ok = True
                break
        if not ok:
            print("  %5d   done 未置位  ← 相位越界(有效范围 [-6, +6])" % ph)
            continue
        vs = []
        for i in range(256):
            m.write(R_ADC_BUF_IDX, i)
            vs.append(m.read(R_ADC_BUF_DATA) & 0xFFF)
        print("  %5d  %4d  %4d  %3d  %4d  %s"
              % (ph, min(vs), max(vs), max(vs) - min(vs), len(set(vs)),
                 " ".join("%4d" % v for v in vs[:8])))
    m.write(R_ADC_CFG, 13)
    print()
    print("完成(位流未重载)")


if __name__ == '__main__':
    main()
