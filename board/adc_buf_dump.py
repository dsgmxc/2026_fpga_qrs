#!/usr/bin/env python3
"""把 PL 里**冻结**的 ADC 采集缓冲 dump 出来看。

用法(板上,root):

    sudo -E python3 tools/adc_buf_dump.py            # 用默认 f_e = 1 MHz
    sudo -E python3 tools/adc_buf_dump.py --fe 100k  # 改激励频率
    sudo -E python3 tools/adc_buf_dump.py --raw      # 只出原始码,不做 DFT

--------------------------------------------------------------------------
为什么是**只读**的,为什么安全
--------------------------------------------------------------------------
`adc_capture.v` 在 `done` 之后缓冲就**冻结**了 —— 读指针 `0x94` 只是把
16384 个字的缓冲当 ROM 一样翻页,**不触发任何采集、不写任何控制寄存器、
不碰位流**。所以它可以在 `c_ui` 正跑着的时候用,不需要停界面。

⛔ 本脚本**不写** `0x88`(刻度)/`0x8C`(GO)/`0x1C`(继电器)/`0x84`(DAC)。
   它读出来的是**上一次采集**留下的东西 —— 想知道它属于哪一次,
   拿 `0x90` 的 done 和 `/var/log/qrs-ui.log` 的最后一行 `[adc]` 对齐。

--------------------------------------------------------------------------
这次是为查什么而写的(2026-10-14)
--------------------------------------------------------------------------
扫压读数**幅值稳、相角抖**:`|Z|` 只在 241–253 Ω 之间(±2.4%),但
`Im Z` 在 −33 到 −86 Ω 之间摆(±44%),换算成相角是 9.8°–19.7°。

线索:同一路里「解调基波」与「原始峰峰」对不上 ——

    A(INA):  基波 594 码,峰峰/2 = 688 码   → 比值 0.86(正常)
    B(INB):  基波 500 码,峰峰/2 = 1402 码  → 比值 0.36(**不对**)

B 的基波和 A 差不多,峰峰却是 A 的两倍 ⇒ B 的波形里有**非基波的大幅度成分**。
孤立的尖刺正好解释:幅值几乎不动(尖刺是少数点)、相角随尖刺落在哪个相位
而随机抖(尖刺在 I/Q 上的投影方向随机)。

本脚本要回答的是:**尖刺到底长什么样、在哪些点上**。
    · 如果是 **固定位置** 的尖刺(比如每个周期的第 0 点)→ 刻度/总线翻转问题
    · 如果是 **随机位置** → 输入噪声或电源耦合
    · 如果 B 的偶/奇点混了两路数据 → 拆交织的奇偶错了

--------------------------------------------------------------------------
⚠️ 单位陷阱
--------------------------------------------------------------------------
缓冲里存的是**偏移二进制**(`adc_capture.v` 写入时做了 `^ 12'h800`),
不是二进制补码。所以码值 2048 = 零点,0 和 4095 是两端。
"""

import argparse
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_HERE, "..", "src", "python"))
sys.path.insert(0, "/home/xilinx/cv")

import cv_regs as R                                     # noqa: E402

#: 缓冲深度 —— 必须与 `adc_capture.v` 的 `BUF_DEPTH` 一致(2026-10-14 起 16384)
BUF_DEPTH = 16384

#: 每通道点数 = 缓冲深度 / 2(偶 = A、奇 = B)
CH_POINTS = BUF_DEPTH // 2

F_S_HZ = 65_000_000     # f_s = 130 MHz / max(DIV,2),DIV=2 ⇒ 固定 65 MSPS


class _Regs:
    """裸 mmap 的寄存器访问。

    ⚠️ 与 `src/python/drivers/relay.py::_DevMemRegs` 是同一套办法的第二处实现。
       那处的注释说「若将来第三个模块也要 /dev/mem,就该把它提出来做成
       `drivers/devmem.py`」—— 这里算是第二处,还没到第三处,先不提前重构。
       (写一律用**切片赋值**,不用 `struct.pack_into` —— 后者会发两笔 AXI 事务,
        见 `report/measurements/20261010_relay_cfg_write_anomaly.md` §10。)
    """

    def __init__(self, base=R.BASE_ADDR, span=0x1000):
        import mmap

        self._fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
        self._map = mmap.mmap(self._fd, span, flags=mmap.MAP_SHARED,
                              prot=mmap.PROT_READ | mmap.PROT_WRITE,
                              offset=base)

    def read(self, name):
        import struct
        return struct.unpack_from("<I", self._map, R.REG[name])[0]

    def write(self, name, value):
        off = R.REG[name]
        self._map[off:off + 4] = int(value).to_bytes(4, "little")

    def close(self):
        self._map.close()
        os.close(self._fd)


def read_buffer(regs):
    """把 16384 个交织字读出来(`0x94` 读指针 → `0x98` 数据)。

    ⛔ 不要按「前半段 A、后半段 B」切 —— 布局是 `2k = A_k`、`2k+1 = B_k`。
    """
    raw = []
    for i in range(BUF_DEPTH):
        regs.write("ADC_BUF_IDX", i)
        raw.append(regs.read("ADC_BUF_DATA"))
    return raw


def split(raw):
    """拆交织:偶 → A(INA),奇 → B(INB)。"""
    a = [(raw[2 * k] & 0xFFF) for k in range(CH_POINTS)]
    b = [(raw[2 * k + 1] & 0xFFF) for k in range(CH_POINTS)]
    otr = sum(((d >> 12) & 1) for d in raw)
    return a, b, otr


def m_l_for(fe_hz, n=CH_POINTS, fs_hz=F_S_HZ):
    """照 `AdcSample_Demodulate()` 的做法求 (m, l):窗内整数个激励周期。

    相干采样只要求 `l·f_e/f_s ∈ ℤ`,化到最简 `f_e/f_s = p/q` ⇒ l 必须是 q 的倍数。

    `fs_hz` 可换 —— `f_s = 130 MHz / max(DIV,2)`,换 DIV 就换采样率。
    """
    from math import gcd
    g = gcd(fs_hz, fe_hz)
    q = fs_hz // g
    l = (n // q) * q
    m = (l // q) * (fe_hz // g)
    return m, l


def fs_for_div(div):
    """与 `adc_capture.v` 的 `P = max(div, 2)`、`f_s = 130 MHz / P` 一致。"""
    return 130_000_000 // max(int(div), 2)


def dft(code, m, l, off=0):
    """参考表式 DFT —— no window,整周期 ⇒ 直流与 2f_e 被精确抵消。

    ⚠️ 必须走 numpy:`l` 是 8190,纯 Python 的 `sum(code[k]*exp(...))` 单次要
       几秒,粗扫 200 个 m 就是十几分钟 —— 2026-10-14 第一版这么写,板上直接
       超时被杀。这不是"慢一点",是**根本跑不完**。

    `off` 是**整体后移的采样点数** —— 给 `z_scan()` 用(判"两路差一个转换周期")。
    `l = 8190`、数组长 8192 ⇒ `off = 1` 时 `code[1:8191]` 刚好放得下,不用补零。
    """
    import numpy as np
    k = np.arange(l, dtype=np.float64)
    x = np.asarray(code[off:off + l], dtype=np.float64)
    if x.size < l:
        raise ValueError(f"offset {off} 越界:{x.size} < {l}")
    return 2.0 * np.sum(x * np.exp(-2j * np.pi * m * k / l)) / l


def z_from(pa, pb, fe_hz, r0):
    """逐字镜像 `adc_sample.c::AdcSample_Impedance()`(复数相量版)。

    pa = VH 相量(INA),pb = VL 相量(INB),都是 ADC 码域的复数。
    ⛔ 这条式子**只在两路相量出自同一次采集时成立**(相位原点相同)。
       换个说法:它的正确性前提正是本轮要验的东西。
    """
    import cmath
    ia, qa = pa.real, pa.imag
    ib, qb = pb.real, pb.imag
    dv_re = ia - ib
    dv_im = qa - qb
    vb2 = ib * ib + qb * qb
    if vb2 <= 0.0:
        return None
    zr = r0 * (dv_re * ib + dv_im * qb) / vb2
    zi = r0 * (dv_im * ib - dv_re * qb) / vb2
    w = 2.0 * cmath.pi * fe_hz
    mag2 = zr * zr + zi * zi
    cs = ((-1e12 / w) / zi) if zi != 0.0 else 0.0
    cp = (-zi * 1e12 / (w * mag2)) if mag2 > 0 else 0.0
    dd = abs(zr / zi) if zi != 0.0 else float("inf")
    return zr, zi, cs, cp, dd


def z_scan(a, b, fe_hz, r0, m, l):
    """🔴 本次查「读数跳跃」的主判据,而且是**零风险**的(只读).

    假设:两路之间**差了整数个 ADC 转换周期**(1/65 MHz = 15.385 ns)。
    若成立,那么:
      · 差 1 个周期 ⇒ 相量被整段平移 1 个采样点
      · DFT 的**幅值对整数样本平移完全不变**(整周期窗),只有**相位**变
        ⇒ 恰好解释「I 稳、Cs/Cp/D 跳」
      · 相位变化量 = 360°·f_e/f_s = 360·126/8190 = **5.538°** @1 MHz
        ⇒ 恰好是实测两状态之间那个 ~4–5° 的夹角

    ⚠️ 只能往**正**方向移(`code[1:1+l]` 刚好塞进 8192 点)。相对平移 −1
       等价于把**另一路** +1,所以 `(A+1, B+0)` 与 `(A+0, B+1)` 中间就夹着
       相对 0 那一档,三种情形一张表看完。
    """
    print(f"\n== Z 扫描:相对平移一个 ADC 转换周期(15.385 ns ⇒ 相位 "
          f"{360.0 * m / l:.3f}°) ==")
    print(f"   R0 = {r0:.1f} Ω,f_e = {fe_hz / 1e6:.4g} MHz")
    print(f"   {'A移':>4} {'B移':>4} {'Re Z':>10} {'Im Z':>10} "
          f"{'|Z|':>8} {'Cs(pF)':>10} {'Cp(pF)':>10} {'D':>8}")
    rows = {}
    for oa in (0, 1):
        for ob in (0, 1):
            pa = dft(a, m, l, oa)
            pb = dft(b, m, l, ob)
            z = z_from(pa, pb, fe_hz, r0)
            if z is None:
                print(f"   {oa:>4} {ob:>4}   |VL| = 0")
                continue
            zr, zi, cs, cp, dd = z
            rows[(oa, ob)] = z
            print(f"   {oa:>4} {ob:>4} {zr:>10.2f} {zi:>10.2f} "
                  f"{(zr * zr + zi * zi) ** 0.5:>8.2f} {cs:>10.1f} "
                  f"{cp:>10.1f} {dd:>8.4f}")
    return rows


def stats(x):
    import statistics
    return min(x), max(x), statistics.fmean(x), max(x) - min(x)


def ascii_plot(x, n=128, width=56, label=""):
    """把前 n 个点画成一行一点的条带,方便一眼看出尖刺。"""
    ys = x[:n]
    lo, hi = min(ys), max(ys)
    span = (hi - lo) or 1
    print(f"  {label} 前 {n} 点  [{lo} .. {hi}]  峰峰 {hi - lo} 码")
    for i, v in enumerate(ys):
        col = int((v - lo) * (width - 1) / span)
        print(f"    {i:5d} {v:5d} |{' ' * col}*")


def report(name, code, fe_hz, do_dft=True):
    lo, hi, mean, pp = stats(code)
    print(f"\n=== 通道 {name} ===")
    print(f"  min {lo}  max {hi}  均值 {mean:.1f}  峰峰 {pp} 码")
    print(f"  零点应在 2048(偏移二进制);均值偏离 = {mean - 2048:+.1f} 码")

    if not do_dft:
        return None

    m, l = m_l_for(fe_hz)
    if l == 0:
        print(f"  ⛔ 窗长 0:{CH_POINTS} 点里放不下 {fe_hz} Hz 的整数周期")
        return None

    ph = dft(code, m, l)
    fund = abs(ph)
    print(f"  窗 l = {l} 点,m = {m} 个激励周期(f_e = {fe_hz} Hz)")
    print(f"  基波幅值 {fund:.1f} 码(峰峰/2 = {pp / 2:.1f} 码,"
          f"比值 {fund / (pp / 2):.3f})← 远小于 1 说明有非基波成分")

    # 粗扫一遍 m,看能量是不是真在预期的那根线上(能抓出 f_e 报错/混叠)。
    # 用**一次矩阵乘**代替逐个 m —— 逐个跑在板上跑不完(见 dft 的注释)。
    import numpy as np
    import cmath
    ms = np.arange(1, 200)
    kk = np.arange(l, dtype=np.float64)
    x = np.asarray(code[:l], dtype=np.float64)
    mags = np.abs(2.0 * (np.exp(-2j * np.pi * np.outer(ms, kk) / l) @ x) / l)
    best_i = int(np.argmax(mags))
    best, best_mag = int(ms[best_i]), float(mags[best_i])
    print(f"  频谱峰值在 m = {best}(幅值 {best_mag:.1f} 码)"
          f"{'  ✅ 与预期一致' if best == m else '  ⚠️ **与预期不符**'}")

    # 残差 = (去掉直流的原始码) − 基波。
    # ⚠️ **必须扣直流** —— 不扣的话残差里那 2034 码的直流项会盖住一切,
    #    算出来的"残差 RMS"就等于均值(2026-10-14 第一版就是这么错的:
    #    A 路算出残差 2034 / 信号 394,比值 5.16,一看就不可能)。
    print(f"  相位 {cmath.phase(ph) * 180 / cmath.pi:+.2f}°")
    res = (x - mean) - (ph * np.exp(2j * np.pi * m * kk / l)).real
    rms_res = float(np.sqrt(np.mean(res * res)))
    rms_sig = float(np.sqrt(np.mean((x - mean) ** 2)))
    print(f"  残差 RMS {rms_res:.1f} 码 / 信号 RMS {rms_sig:.1f} 码 "
          f"= {rms_res / rms_sig:.3f}   ← 越接近 0 越干净")

    # 🔴 **残差直方图** —— 这是本次查「尖刺从哪来」的主判据。
    #    如果尖刺是**数据总线 MSB(D11)锁在翻转中途**,残差就只会出现在
    #    **±2048 码**附近(±2048 = 第 11 位翻转的量),中间那一堆是干净的。
    hist = np.histogram(res, bins=np.arange(-3000, 3000.1, 256))[0]
    cnt = np.arange(-3000, 3000, 256)
    nonzero = [(int(c), int(n)) for c, n in zip(cnt, hist) if n]
    print(f"  残差直方图(每格 256 码,只列非空的格):")
    for c, n in nonzero:
        bar = "#" * min(60, max(1, n * 60 // max(1, max(h[1] for h in nonzero))))
        mark = ""
        if abs(abs(c + 128) - 2048) < 128:
            mark = "  ← ★ **±2048 ⇒ MSB 翻转**"
        print(f"    {c:+6d}..{c + 255:+6d}  {n:5d}  {bar}{mark}")

    # 尖刺计数:残差超 5σ 的点 —— 位置固定还是随机,一眼可辨
    sd = rms_res or 1.0
    spikes = np.nonzero(np.abs(res) > 5.0 * sd)[0]
    print(f"  残差 > 5σ 的点:{len(spikes)} 个 / {l}")
    if len(spikes):
        print(f"    前 24 个位置 {list(spikes[:24])}")
        if len(spikes) > 1:
            gaps = sorted(set(int(b - a) for a, b in zip(spikes[:-1], spikes[1:])))
            print(f"    相邻间隔 {gaps[:12]}{' …' if len(gaps) > 12 else ''}"
                  f"{'   ← **等间隔 ⇒ 与采样刻度有关**' if len(gaps) <= 2 else ''}")
        big = [float(res[i]) for i in spikes[:12]]
        print(f"    前 12 个残差值 {[f'{v:+.0f}' for v in big]}"
              f"  ← 若都≈±2048 ⇒ MSB 假设成立")

    # 🔴 **残差到底能带偏相位多少** —— 这决定了「尖刺」是不是「相角跳」的原因。
    #    单点残差 g 对 l 点窗内 m 次谐波的贡献幅值是 **2|g|/l**,
    #    g = 2048、l = 8190 ⇒ 0.50 码;对 500 码的基波就是 0.06° 量级。
    #    ⇒ 若这里算出来远小于观测到的 5°,就说明**尖刺不是相角跳的成因**,
    #      两件事得分开查 —— 不要一股脑归到「线太长」上。
    ph_res = dft(res, m, l)
    dang = abs(ph_res) / (abs(ph) or 1.0) * 180.0 / 3.141592653589793
    print(f"  残差部分落在基波频点上的分量 {abs(ph_res):.2f} 码 "
          f"⇒ 最多能把相位带偏 {dang:.3f}°")
    return ph


def main():
    p = argparse.ArgumentParser(description="dump 冻结的 ADC 采集缓冲(只读)")
    p.add_argument("--fe", default="1M",
                   help="激励频率:1M / 100k / 10k(默认 1M)")
    p.add_argument("--raw", action="store_true", help="只出原始码,不做 DFT")
    p.add_argument("--plot", type=int, default=128, help="每路画多少个点(默认 128)")
    p.add_argument("--no-zscan", action="store_true",
                   help="不跑 Z 平移扫描(默认跑)")
    p.add_argument("--r0", type=float, default=100.0,
                   help="参考电阻 Ω(从 0x18 回读换算;**默认 100 = B 挡**,"
                        "本轮不写 0x18,板上实测就是 2 号挡)")
    args = p.parse_args()

    mult = {"k": 1e3, "M": 1e6, "": 1.0}
    fe_hz = int(float(args.fe[:-1]) * mult[args.fe[-1]]) \
        if args.fe[-1] in "kM" else int(float(args.fe))

    regs = _Regs()
    try:
        sts = regs.read("ADC_STS")
        print(f"== ADC_STATUS(0x90) = 0x{sts:08X}")
        print(f"   mmcm_locked={sts & 1}  done={(sts >> 1) & 1}  busy={(sts >> 2) & 1}")
        if not (sts >> 1) & 1:
            print("   ⚠️ done = 0 —— 缓冲里**没有**有效的采集结果(上一次 GO 还没完成,")
            print("      或上电后还没采过)。下面的数字是残留,不能当测量结果。")

        raw = read_buffer(regs)
        a, b, otr = split(raw)
        print(f"== 缓冲 = {BUF_DEPTH} 字 ⇒ 每通道 {CH_POINTS} 点;OTR 置位 {otr} 次")

        for name, code in (("A/INA", a), ("B/INB", b)):
            report(name, code, fe_hz, do_dft=not args.raw)
            if args.plot:
                ascii_plot(code, n=args.plot, label=name)

        if not args.raw and not args.no_zscan:
            m, l = m_l_for(fe_hz)
            if l:
                z_scan(a, b, fe_hz, args.r0, m, l)
    finally:
        regs.close()


if __name__ == "__main__":
    main()
