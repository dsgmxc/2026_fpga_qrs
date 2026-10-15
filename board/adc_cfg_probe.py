#!/usr/bin/env python3
"""**主动触发**多次 ADC 采集,换采样刻度 `0x88` 逐一比对,看哪一档的位错最少。

板上(root):

    sudo -E python3 /home/xilinx/adc_cfg_probe.py                # 默认四档矩阵
    sudo -E python3 /home/xilinx/adc_cfg_probe.py --repeat 3
    sudo -E python3 /home/xilinx/adc_cfg_probe.py --cfg "2:-1 4:1"

--------------------------------------------------------------------------
⚠️ 这是**写**寄存器的工具 —— 与只读的 `adc_buf_dump.py` 分开存放
--------------------------------------------------------------------------
`adc_buf_dump.py` 的文件头明写「⛔ 本脚本不写 0x88/0x8C」。本脚本正是要写这两个,
所以**不合并**。两个文件都靠 `_Regs` 那套 /dev/mem 访问,但用途相反。

本脚本只碰两个寄存器,别的**一个都不写**:

    0x88 ADC_CFG   DIV + PH_OFF(换采样刻度)
    0x8C ADC_CTRL  写 1 = GO(触发一次采集)

⛔ **不写** 0x1C(继电器)/ 0x18(挡位)/ 0x84(DAC)/ 0x60(帧缓存换址)。
⛔ 不加载位流、不重启任何服务、不碰 `qrs-ui.service`。

离开时**把 `0x88` 恢复成进来时读到的那个值**并在 finally 里做 —— 界面每次采集
开头都会自己写一遍 `0x88`(见 `measure_config.c::MeasureConfig_PushPhase`),
所以即使这里没恢复也不会把界面带偏;但**"靠别人兜底"不是理由**,自己收干净。

--------------------------------------------------------------------------
为什么要换 DIV(2026-10-14)
--------------------------------------------------------------------------
上一份冻结缓冲:`A` 残差 RMS 10.6/394.1 = **0.027**(干净),
`B` 残差 RMS 91.7/146.9 = **0.624**(坏),残差直方图在 **±2048 / ±1024** 各有一撮
⇒ B 的总线数据被**整个翻转了一位**(D11 / D10)。而 `Z = R0(VH−VL)/VL` 里 VL 就是 B。

`adc_capture.v` 的两个采样刻度贴死了 `adc_clk` 的两个边沿:

    P = 2 ⇒ tick ∈ {−1, 0} ⇒ 正好是 cnt==0(上升沿)与 cnt==half(下降沿)
    ⇒ 零建立裕量,杜邦线 ~4 ns 的 tPD 离散(见 LED.xdc 的注释)直接变成位错

**P=2 时没有第二个刻度可选** —— 整张刻度表就这两个点。但**换 DIV 就换了刻度表**:

    P=4 ⇒ tick ∈ {−2,−1,0,+1},而 A 的合法窗是 {0,+1}、B 是 {−2,−1}
        ph_off = 0 ⇒ A@tick0(cnt2,下降沿)  B@tick−2(cnt0,上升沿)   ← 仍在边沿
        ph_off =+1 ⇒ A@tick1(cnt3)         B@tick−1(cnt1)           ← 窗正中
    离开边沿 = 多出 1 个 130 MHz tick = **7.69 ns** 的建立裕量

⚠️ **本脚本只回答"哪个刻度位错少",不回答"能不能就这么定"** ——
换 DIV 就换 `f_s`(65 → 32.5 MSPS),相干窗、挡位语义、前序对比性都要另算。
判完再决定,别拿一次实测当结论。

--------------------------------------------------------------------------
结果怎么读
--------------------------------------------------------------------------
主判据是 **A/B 两路的残差 RMS 与"残差 RMS / 信号 RMS"** ——
它不受"某一路本来信号就小"的影响,直接量"数据里有多少不是基波的东西"。
尖刺计数(>5σ)是辅助量:它**低估**坏点(σ 本身已经被坏点抬起来了)。
`重复一致性` 一列是**自检**:同一档采两次,数据若逐字节相同 ⇒ GO 根本没重触发,
那一行整行作废(不是"位错为 0")。
"""

import argparse
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _HERE)
sys.path.insert(0, os.path.join(_HERE, "..", "src", "python"))
sys.path.insert(0, "/home/xilinx/cv")

import cv_regs as R                                              # noqa: E402
from adc_buf_dump import (_Regs, dft, m_l_for, read_buffer,       # noqa: E402
                          split, z_from)

#: `adc_clk` 基础频率 —— 与 `cv_sys.v` 的 `clk_adc_base` 一致
CLK_ADC_BASE_HZ = 130_000_000

#: 默认矩阵:(DIV, PH_OFF)。见文件头那张表。
DEFAULT_CFG = "2:-1 4:0 4:1 13:-2"

#: 采集完成等待 —— 一次采集 16384 个转换 @65 MSPS 只有 252 µs,
#: 但寄存器回读是 AXI 一笔一笔走的,给足余量。不是"等 ADC",是"等我自己别抢"。
SETTLE_S = 0.35


def fs_for_div(div):
    return CLK_ADC_BASE_HZ // max(int(div), 2)


def cfg_word(div, ph_off):
    """与 `pynq_regs.h::cv_adc_cfg_word()` 逐位等价。"""
    return (int(div) & 0xFF) | (((int(ph_off) & 0xFF)) << 8)


def decode_cfg(word):
    div = word & 0xFF
    ph = word >> 8
    ph = ph - 256 if ph >= 128 else ph          # PH_OFF 是有符号字节
    return div, ph


def tap_xy(div, ph_off):
    """纸上把 (DIV, PH_OFF) 翻成"采样落在 cnt 几、离边沿多远"。

    与 `adc_capture.v` 的三行折回逻辑同一个算法(见 `adc_sample.h` 的窗口表),
    **这里只是为了打印时人看得懂**,判据一律用实测数据,不用这张纸。
    """
    p = max(int(div), 2)
    half = p >> 1
    hi = p - 1 - half
    lo = -half

    def wrap(t):
        while t > hi:
            t -= p
        while t < lo:
            t += p
        return t

    ph_a = wrap(ph_off)
    ph_b = wrap(ph_off + half)
    # tick = cnt − half ⇒ cnt = tick + half;adc_clk = (cnt < half)
    return ph_a, ph_b, half, lo, hi


def hamming_diff(x, y):
    """两份缓冲逐字节差异的个数 —— 用来证"这一枪真的打响了"。"""
    return sum(1 for u, v in zip(x, y) if u != v)


def capture(regs, word, fe_hz):
    """写 0x88 + GO,等 done,读回来算指标。"""
    import time
    import numpy as np

    regs.write("ADC_CFG", word)
    back = regs.read("ADC_CFG")
    if back != word:
        return None, f"0x88 回读不一致:写 0x{word:08X} 读回 0x{back:08X}"

    regs.write("ADC_CTRL", 1)
    time.sleep(SETTLE_S)

    sts = regs.read("ADC_STS")
    if not (sts >> 1) & 1:
        return None, f"done 没置起来(0x90 = 0x{sts:08X})"
    if (sts >> 2) & 1:
        return None, f"仍在 busy(0x90 = 0x{sts:08X})"

    raw = read_buffer(regs)
    a, b, otr = split(raw)

    div, ph = decode_cfg(word)
    fs = fs_for_div(div)
    m, l = m_l_for(fe_hz, fs_hz=fs)

    # ⚠️ 偏压**没有可读寄存器** —— DAC1220 走 SPI,不在地址空间里。
    #    所以只能读"两个能证明别人没动过手"的:激励字与 DAC 放行位。
    out = {"word": word, "div": div, "ph": ph, "fs": fs, "m": m, "l": l,
           "otr": otr, "raw": raw,
           "dds": regs.read("DDS_FREQ"), "ctl": regs.read("CV_CTRL")}
    for name, code in (("a", a), ("b", b)):
        ph_c = dft(code, m, l) if l else 0j
        out[name + "_ph"] = ph_c
        x = np.asarray(code[:l], dtype=np.float64)
        mean = float(np.mean(x))
        sig = float(np.sqrt(np.mean((x - mean) ** 2)))
        res = (x - mean) - (ph_c * np.exp(2j * np.pi * m * np.arange(l) / l)).real
        rms = float(np.sqrt(np.mean(res * res)))
        out[name] = {
            "fund": abs(ph_c), "rms": rms, "sig": sig,
            "ratio": rms / sig if sig else float("nan"),
            "pp": max(code) - min(code),
            "mn": min(code), "mx": max(code),
            "spk": int(np.sum(np.abs(res) > 5.0 * (rms or 1.0))),
        }
    return out, None


def z_detail(rows, div, ph, fe_hz, r0):
    """逐次把**中间量**摊开 —— 判「跳的是幅值还是相角、跳在哪一路」。

    单看 Z 的 SD 分不出:
      · `|VH|`/`|VL|` 变了 ⇒ 激励幅值或偏压变了(物理量真变了)
      · `∠VH − ∠VL` 变了而幅值不动 ⇒ 相位通路的问题
    这两条要分开,否则会一路修错地方。
    """
    import cmath
    print(f"   -- {div}:{ph:+d} 逐次明细(R0 = {r0:.0f} Ω)--")
    print(f"   {'#':>3} {'|VH|':>9} {'|VL|':>9} {'∠VH':>9} {'∠VL':>9} "
          f"{'∠VH−∠VL':>10} {'Re Z':>9} {'Im Z':>9}  {'0x80':>10} {'0x84':>6}")
    for i, r in enumerate(rows):
        pa, pb = r["a_ph"], r["b_ph"]
        za = z_from(pa, pb, fe_hz, r0)
        dphi = (cmath.phase(pa) - cmath.phase(pb)) * 180.0 / cmath.pi
        dphi = (dphi + 180.0) % 360.0 - 180.0
        print(f"   {i + 1:>3} {abs(pa):>9.1f} {abs(pb):>9.1f} "
              f"{cmath.phase(pa) * 180 / cmath.pi:>9.2f} "
              f"{cmath.phase(pb) * 180 / cmath.pi:>9.2f} {dphi:>10.2f} "
              f"{za[0]:>9.2f} {za[1]:>9.2f}  0x{r['dds']:08X} 0x{r['ctl']:02X}")
    return None


def z_block(regs, cfgs, fe_hz, n, r0, detail=False):
    """🔴 **直接回答「读数跳跃」**:同一档位连采 n 次,看 Z / Cs / D 的散度。

    继电器不动(本脚本不写 0x1C)、DAC 不动 ⇒ 理论上每次的 Z **应该是同一个数**,
    散度就是**纯测量噪声**。所以这一列百分比可以直接读成"这一档读数会跳多少"。

    ⚠️ 相位原点在每次采集里是随机的(AD9833 板载晶振不与 FPGA 同源),
       但 `Z = R0(VH−VL)/VL` 是**同一份采集内两路相量的比**,
       原点相同 ⇒ 约掉了。**这正是本次修的 RTL 的意义,也是这一列能成立的前提。**
    """
    import numpy as np
    print(f"\n== 逐次采样的 Z 散度(每档连采 {n} 次;继电器/DAC 全程不动)"
          f"   R0 = {r0:.1f} Ω ==")
    print(f"   {'档位':<16} {'Re Z 均值±SD (Ω)':>24} {'Im Z 均值±SD (Ω)':>24} "
          f"{'Cs 均值±SD (pF)':>24} {'D 均值±SD':>20}")
    for div, ph in cfgs:
        word = cfg_word(div, ph)
        rows = []
        for _ in range(n):
            r, err = capture(regs, word, fe_hz)
            if err:
                print(f"   DIV={div} PH_OFF={ph:+d}  ⛔ {err}")
                break
            z = z_from(r["a_ph"], r["b_ph"], fe_hz, r0)
            if z is None:
                print(f"   DIV={div} PH_OFF={ph:+d}  ⛔ |VL| = 0")
                break
            rows.append(r)
            r["z"] = z
        if not rows:
            continue
        # 采完立刻把明细打出来 —— 汇总表是结论,明细才是能证伪它的东西。
        if detail:
            z_detail(rows, div, ph, fe_hz, r0)
        arr = np.asarray([r["z"] for r in rows], dtype=float)
        mean = arr.mean(axis=0)
        sd = arr.std(axis=0, ddof=1) if len(rows) > 1 else np.zeros(4)

        def cell(m, s, rel=True):
            tail = f" ({s / abs(m) * 100:>{4 if s / abs(m) < 1 else 5}.2f}%)" \
                if (rel and m) else ""
            return f"{m:>12.2f}±{s:<7.2f}{tail}"

        print(f"   {f'{div}:{ph:+d} ({fs_for_div(div) / 1e6:.1f}Msps)':<16}"
              f"{cell(mean[0], sd[0]):>24}{cell(mean[1], sd[1]):>24}"
              f"{cell(mean[2], sd[2]):>24}{cell(mean[4], sd[4]):>20}")
    return None


def fmt_row(tag, r, prev_raw):
    a, b = r["a"], r["b"]
    same = "-" if prev_raw is None else str(hamming_diff(r["raw"], prev_raw))
    if prev_raw is not None and same == "0":
        same = "0 ⚠️未重触发"
    return (f"  {tag:<14} {r['div']:>3} {r['ph']:>+4} {r['fs'] / 1e6:>7.1f} "
            f"{r['m']:>5} {r['l']:>6} {r['otr']:>4} "
            f"{a['ratio']:>9.4f} {a['rms']:>9.1f} "
            f"{b['ratio']:>9.4f} {b['rms']:>9.1f} "
            f"{a['spk']:>4} {b['spk']:>4}  {same}")


HEAD = ("  档位           DIV PH_OFF  f_s/MHz     m      l  OTR "
        " A:残差/信号    A:RMS  B:残差/信号    B:RMS  A尖刺 B尖刺  重复差异")


def main():
    ap = argparse.ArgumentParser(description="换 0x88 刻度逐档比对 ADC 位错")
    ap.add_argument("--cfg", default=DEFAULT_CFG,
                    help=f'档位表 "DIV:PH_OFF ..."(默认 "{DEFAULT_CFG}")')
    ap.add_argument("--fe", default="1M", help="激励频率 1M/100k/10k(默认 1M)")
    ap.add_argument("--repeat", type=int, default=2, help="每档重复次数(默认 2)")
    ap.add_argument("--z", type=int, default=0, metavar="N",
                    help="跑完比例表后,每档**再连采 N 次**出 Z/Cs/D 的散度"
                         "(回答「读数跳跃」;0 = 不跑)")
    ap.add_argument("--r0", type=float, default=100.0,
                    help="参考电阻 Ω(默认 100 = B 挡;本轮不写 0x18)")
    ap.add_argument("--only-z", action="store_true",
                    help="跳过比例表,只跑 --z")
    ap.add_argument("--detail", action="store_true",
                    help="--z 的每一次采集再摊开 |VH|/|VL|/∠VH/∠VL(默认汇总为止)")
    args = ap.parse_args()

    mult = {"k": 1e3, "M": 1e6, "": 1.0}
    fe_hz = int(float(args.fe[:-1]) * mult[args.fe[-1]]) \
        if args.fe[-1] in "kM" else int(float(args.fe))

    cfgs = []
    for tok in args.cfg.split():
        d, _, p = tok.partition(":")
        cfgs.append((int(d), int(p)))
    if not cfgs:
        print("⛔ 档位表空")
        return 2

    regs = _Regs()
    orig = regs.read("ADC_CFG")
    print(f"== 进场时 0x88 = 0x{orig:08X}  (DIV={decode_cfg(orig)[0]}, "
          f"PH_OFF={decode_cfg(orig)[1]:+d})  ← 离开时恢复成这个")
    print(f"== 激励 f_e = {fe_hz} Hz")
    print(f"== 继电器 0x1C = 0x{regs.read('RELAY_CFG'):08X} / "
          f"挡位 0x18 = 0x{regs.read('RANGE_SEL'):08X}(本脚本全程不写,"
          f"两档之间的 Z 可直接比)")
    print(HEAD)

    try:
        for div, ph in ([] if args.only_z else cfgs):
            word = cfg_word(div, ph)
            pa, pb, half, lo, hi = tap_xy(div, ph)
            prev = None
            for k in range(args.repeat):
                r, err = capture(regs, word, fe_hz)
                if err:
                    print(f"  DIV={div} PH_OFF={ph:+d}  ⛔ {err}")
                    break
                print(fmt_row(f"{div} / {k + 1}", r, prev))
                prev = r["raw"]
            print(f"  {'':<14} 刻度:cntA={pa + half} cntB={pb + half} "
                  f"(half={half}, 合法 tick [{lo},{hi}])  0x88=0x{word:08X}")

        if args.z:
            z_block(regs, cfgs, fe_hz, args.z, args.r0, detail=args.detail)
    finally:
        regs.write("ADC_CFG", orig)
        back = regs.read("ADC_CFG")
        ok = "✅" if back == orig else "⛔ **没恢复成功**"
        print(f"\n== 恢复 0x88 = 0x{back:08X} {ok}")
        regs.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
