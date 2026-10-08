#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
adc_probe.py —— AD9238 采集通路【常驻】板上探针(2026-10-06)

================================================================================
为什么是常驻进程,而不是"跑一次脚本"
================================================================================
板上硬规则:**一个上电周期只加载一次位流**。理由不是迷信 ——
新位流里 `lcd_fb_reader` 是**活跃的 AXI 主机**(自由运行读 DDR 帧缓存),
第二次 `Overlay()` 会让它在事务中途消失 ⇒ 锁死 HP 口连带 CPU(不可恢复,只能断电)。
(2026-10-05 付出 3 次断电买来的规则,见 skill/pynq-overlay-bringup。)

可是调试又必须反复改参数(P / PH_OFF / 单次 vs 连续),不能每改一次就断电。
⇒ 把**唯一那次 `Overlay()`** 放进一个不死的进程,参数通过**文件投递**:
   我 `>> adc_cmd.txt` 追加命令行 → 进程轮询到新行 → 执行 → 结果写 adc.out。
**不用管道/FIFO 做 stdin** —— 那样 SSH 一断进程就 EOF 退出,前功尽弃。

================================================================================
启动(每个上电周期一次,必须在任何 PL 访问之前)
================================================================================
    echo xilinx | sudo -S -E sh -c \
      ': > /home/xilinx/adc.out; : > /home/xilinx/adc_cmd.txt; \
       setsid /usr/local/share/pynq-venv/bin/python3 -u /home/xilinx/adc_probe.py serve \
       >> /home/xilinx/adc.out 2>&1 < /dev/null &'

投递命令:   echo "dump 13 0" >> /home/xilinx/adc_cmd.txt
看结果:     tail -60 /home/xilinx/adc.out     (每条命令后打 ===DONE===)

================================================================================
命令表
================================================================================
    init                    加载位流 + FCLK0=50M + 放行(0x84[0]=1)。**全程只许一次**
    attach                  只映射 MMIO,**不加载位流**(PL 已由 main.py 加载好时用)。
                            与 init 二选一,每个上电周期只用其一。
    fclk [mhz]              单独重设 FCLK0(默认 50)。init 里那步失败时补投
    sts                     读 0x84 / 0x88 / 0x90
    dump <div> <ph> [n]     单次采集,读 n 个点(默认 64),打统计 + 原始值
    wave <div> <ph> [n]     单次采集,只打原始值(默认 128)
    eye  <div> [n]          扫 PH_OFF 全范围,每档采 n 点(默认 256),打 p-p / 去重数
    rd <n>                  只读缓冲前 n 个点(不改配置、不重新 GO)
    wr <off> <val>          裸写寄存器(排查用)
    quit                    退出

寄存器(见 cv_axi_lite_slave.v 译码表)
    0x84 CV_CTRL       RW [0] cfg_clk_release(原 dac_rst_n,扩义:放行按 50MHz 定常数的外设)
    0x88 ADC_CFG       RW [7:0]DIV [15:8]PH_OFF(有符号) [16]CONT [17]OE_N [18]PDN
    0x8C ADC_CTRL      RW [0]GO(1 脉冲,读回 0)
    0x90 ADC_STS       RO [0]mmcm_locked [1]done [2]busy [3]otr_sticky
    0x94 ADC_BUF_IDX   RW [12:0]
    0x98 ADC_BUF_DATA  RO [11:0]sample [12]OTR

⚠️ PH_OFF 是**相对 adc_clk 下降沿的有符号偏移**,有效范围 [-P/2, P-1-P/2]
   (P = DIV)。越界不会误触发,只是**永远采不到**、done 不置位 —— 失败方向安全。
⚠️ 复位默认 ADC_CFG 随版本变,是**判位流新旧**最快的判据(旧位流没这个寄存器,读 0):
       0x0000000D ⇒ DIV=13 / 10 MSPS  ← 2026-10-08 之前的位流
       0x00000002 ⇒ DIV=2  / 65 MSPS  ← 🆕 2026-10-08 起(采样率统一 65 MSPS)
   ⛔ 只能在**刚加载完位流、还没有别人写过 0x88** 时读。`c_ui` 一上来就会写它
      ⇒ 用 `attach`(PL 已由 main.py 加载)时读到的不是复位默认值,别据此判新旧。
"""

import os
import sys
import time
import traceback

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------
AXI_BASE = 0x43C00000
SLCR_BASE = 0xF8000000
FPGA0_CLK_CTRL = 0x170
IO_PLL_MHZ = 1000.0          # 2026-10-03 帧率法三点独立实测(1500 被排除)
DIV0_SHIFT, DIV0_MASK = 8, 0x3F
DIV1_SHIFT, DIV1_MASK = 20, 0x3F

R_CV_CTRL = 0x84
R_ADC_CFG = 0x88
R_ADC_CTRL = 0x8C
R_ADC_STS = 0x90
R_ADC_BUF_IDX = 0x94
R_ADC_BUF_DATA = 0x98

BUF_N = 8192
BIT_FILE = '/home/xilinx/qrs.bit'
CMD_FILE = '/home/xilinx/adc_cmd.txt'


def b2s(b):
    """8 位无符号 → 有符号(PH_OFF 显示用)"""
    return b - 256 if b >= 128 else b


# ---------------------------------------------------------------------------
# 命令实现
# ---------------------------------------------------------------------------
def cmd_init(st):
    from pynq import Overlay, MMIO
    from pynq import Clocks

    if st.get('ol') is not None:
        print("!! 本上电周期已经加载过位流 —— 拒绝第二次(会锁死 HP 口)。要重来请断电。",
              flush=True)
        return

    print("[init] Overlay(%s) …" % BIT_FILE, flush=True)
    t0 = time.time()
    st['ol'] = Overlay(BIT_FILE)
    print("[init] 加载完成,%.2f s" % (time.time() - t0), flush=True)
    print("[init] ip_dict: %s" % sorted(st['ol'].ip_dict.keys()), flush=True)

    st['mmio'] = MMIO(AXI_BASE, 0x1000)
    st['slcr'] = MMIO(SLCR_BASE, 0x1000)

    # --- FCLK0 + 放行 -------------------------------------------------------
    # ⚠️ 位流已经加载了(FCLK0 必须排在 Overlay() 之后,见上)。所以这两步
    #    **绝不能**让异常冒出去 —— 那样进程虽在,但配置停在一半,这一整个
    #    上电周期就白费了。拆成独立命令,失败可以单独重投。
    try:
        cmd_fclk(st, 50.0)
    except Exception as e:
        print("[init] 🔴 fclk0 失败:%s —— 单独重投 `fclk 50` 补做" % e, flush=True)
        traceback.print_exc()
    try:
        st['mmio'].write(R_CV_CTRL, 1)
        time.sleep(0.05)
        print("[init] 0x84 CV_CTRL = 0x%08X (bit0=cfg_clk_release 应为 1)"
              % st['mmio'].read(R_CV_CTRL), flush=True)
    except Exception as e:
        print("[init] 🔴 放行写失败:%s" % e, flush=True)

    # --- 位流身份判据 -------------------------------------------------------
    # 🔴 2026-10-08:复位默认由 0x0D(DIV=13/10 MSPS)改成 **0x02(DIV=2/65 MSPS)**。
    #    ⛔ 只有走到这一步才看得到它 —— 下一条 `cmd_sts` 之前没人写过 0x88。
    #    从 `attach` 进来时 0x88 已被 `c_ui` 写过,**本判据不适用**,那时靠
    #    `wr 0x94 0x1FFF` 的回读(13 位掩码)判新旧,见本文件头。
    cfg = st['mmio'].read(R_ADC_CFG)
    if cfg == 0x00000002:
        print("[init] ✅ 0x88 = 0x00000002 ⇒ **这是 2026-10-08 之后的位流**"
              "(DIV=2 / 65 MSPS / 缓冲 8192)", flush=True)
    elif cfg == 0x0000000D:
        print("[init] 🔴 0x88 = 0x0000000D ⇒ **这是旧位流**(DIV=13 / 缓冲 4096)。"
              "65 MSPS 与 10k/100k 两档激励在当前位流上**不成立** —— 先换位流。", flush=True)
    else:
        print("[init] 🔴 0x88 = 0x%08X 既不是 0x00000002 也不是 0x0000000D "
              "⇒ 位流版本不明或译码不对,后面的采集结果不可信" % cfg, flush=True)
    cmd_sts(st)


def cmd_attach(st):
    """只映射 MMIO,**不加载位流** —— 给「PL 已经由别的进程加载好」的情形用。

    🔴 存在的理由(2026-10-07 补):`init` 里那句 `Overlay()` 是**本上电周期
       唯一允许的一次**,而它常已被 `main.py` 用掉(UI 正常跑着就是这个状态)。
       此时本探针仍然可以读写寄存器 —— `/dev/mem` 的 mmap **不排斥多进程**,
       位流也早就在 PL 里了。缺的只是 `st['mmio']` 这个映射,补上即可。

       ⛔ `attach` 与 `init` 每个上电周期只用其一。为防手滑,attach 成功后
          **主动占住 `st['ol']`** —— `cmd_init` 开头那句 `st.get('ol') is not None`
          就会拦住后来的 `Overlay()`。这是复用既有守卫,不是新加逻辑。

    ⚠️ attach **不碰 FCLK0**。若 PL 是 `main.py` 加载的,FCLK0 已由
       `board.py::apply_fclk0()` 设成 50 MHz;要复核请单独投 `sts` 看
       `0x90[0] locked` 是否为 1(为 0 ⇒ ADC 时钟没起来,先查 FCLK0)。
    """
    from pynq import MMIO

    if st.get('mmio') is not None:
        print("!! 已经 attach/init 过了。")
        return
    st['mmio'] = MMIO(AXI_BASE, 0x1000)
    st['slcr'] = MMIO(SLCR_BASE, 0x1000)
    st['ol'] = 'attached-externally'        # 占位:让 cmd_init 的守卫拒绝二次 Overlay

    print("[attach] MMIO 已映射(未加载位流,未改 FCLK0)。0x84 = 0x%08X"
          % st['mmio'].read(R_CV_CTRL), flush=True)
    cmd_sts(st)


def cmd_fclk(st, mhz=50.0):
    """把 FCLK0 设成真值 `mhz`。独立命令 —— init 失败后可以单独补投。

    🔑 **`FPGA0_CLK_CTRL` 是两级串联分频:FCLK0 = IO_PLL / (DIV0 × DIV1)。**
       两个已记录的值都据此对上了(DIV0 = bits[13:8],DIV1 = bits[25:20]):
         默认(BOOT.BIN) `0x00600600` → DIV0=6  DIV1=6 → 1000/36 = **27.78 MHz** ✓ 与实测一致
         改频后          `0x00101400` → DIV0=20 DIV1=1 → 1000/20 = **50.000 MHz** ✓
       (27.78 曾被误当成"另一条分频路径";其实 6×6=36 一步就解释完了。)

    ⚠️ 必须排在 Overlay() **之后** —— Overlay() 会把本寄存器整个复位回 0x00600600。

    ⚠️ **不用 `Clocks.fclk0_mhz` 的结果当真值**:它内部假定 IO PLL = 1500,
       本板实为 1000 ⇒ 它算 DIV0 = 1500/50/DIV1。先让它配好 CLKACT/SRCSEL/DIV1,
       再把 **DIV0 与 DIV1 一起钉死**(DIV0 = round(1000/50) = 20,DIV1 = 1)。
       ⛔ 只钉 DIV0 不钉 DIV1 是有风险的 —— 若它自作主张选了 DIV1=2,
          2000/... 就会落成 25 MHz 而不是 50,而且**不报错**。
    """
    from pynq import Clocks
    try:
        Clocks.fclk0_mhz = mhz          # 配 CLKACT / SRCSEL / DIV1(值不可信,只借它的副作用)
    except Exception as e:
        print("[fclk0] ⚠️ Clocks.fclk0_mhz 失败(%s)—— 继续按已知常量硬写" % e, flush=True)

    div0 = max(1, int(round(IO_PLL_MHZ / mhz)))
    div1 = max(1, int(round(IO_PLL_MHZ / mhz / div0))) or 1
    v = st['slcr'].read(FPGA0_CLK_CTRL)
    new = (v & ~((DIV0_MASK << DIV0_SHIFT) | (DIV1_MASK << DIV1_SHIFT))) \
        | (div0 << DIV0_SHIFT) | (div1 << DIV1_SHIFT)
    st['slcr'].write(FPGA0_CLK_CTRL, new)
    v2 = st['slcr'].read(FPGA0_CLK_CTRL)
    d0 = (v2 >> DIV0_SHIFT) & DIV0_MASK
    d1 = (v2 >> DIV1_SHIFT) & DIV1_MASK
    print("[fclk0] 0x%08X -> 0x%08X  DIV0=%d DIV1=%d  ⇒ %.4f MHz"
          % (v, v2, d0, d1, IO_PLL_MHZ / (d0 * d1) if d0 * d1 else 0.0), flush=True)
    if d0 == 20 and d1 == 1 and v2 == 0x00101400:
        print("[fclk0] ✅ 与已跑通的 0x00101400 逐位一致 ⇒ FCLK0 = 50.000 MHz", flush=True)
    else:
        print("[fclk0] ⚠️ 不是已知good的 0x00101400 ⇒ FCLK0 ≠ 50 MHz 的可能性存在,"
              "ADC MMCM 可能锁不住(它以 CLKIN1_PERIOD=20ns 定常数)", flush=True)


def cmd_sts(st):
    m = st['mmio']
    ctrl = m.read(R_CV_CTRL)
    cfg = m.read(R_ADC_CFG)
    s = m.read(R_ADC_STS)
    print("  0x84 CV_CTRL   = 0x%08X  release=%d" % (ctrl, ctrl & 1), flush=True)
    print("  0x88 ADC_CFG   = 0x%08X  DIV=%d PH_OFF=%d CONT=%d OE_N=%d PDN=%d"
          % (cfg, cfg & 0xFF, b2s((cfg >> 8) & 0xFF), (cfg >> 16) & 1,
             (cfg >> 17) & 1, (cfg >> 18) & 1), flush=True)
    print("  0x90 ADC_STS   = 0x%08X  mmcm_locked=%d done=%d busy=%d otr_sticky=%d"
          % (s, s & 1, (s >> 1) & 1, (s >> 2) & 1, (s >> 3) & 1), flush=True)
    print("  ⇒ adc_clk = 130/%.0f = %.4f MHz" % (cfg & 0xFF, 130.0 / max(1, cfg & 0xFF)),
          flush=True)


def _wait_not_busy(m, timeout=2.0):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if not ((m.read(R_ADC_STS) >> 2) & 1):
            return True
        time.sleep(0.005)
    return False


def cmd_go(st, div, ph, cont=False, timeout=3.0):
    """配置 + 触发一次采集。返回 True 表示 done 已置位。"""
    m = st['mmio']
    if not _wait_not_busy(m):
        print("  ⚠️ 触发前 busy 一直不落,仍继续", flush=True)

    cfg = (div & 0xFF) | ((ph & 0xFF) << 8) | ((1 if cont else 0) << 16)
    m.write(R_ADC_CFG, cfg)
    m.write(R_ADC_CTRL, 1)                      # GO,1 脉冲

    if cont:
        time.sleep(0.03)
        return True
    t0 = time.time()
    while time.time() - t0 < timeout:
        if (m.read(R_ADC_STS) >> 1) & 1:
            print("  [go] done 置位,耗时 %.4f s" % (time.time() - t0), flush=True)
            return True
        time.sleep(0.002)
    lo, hi = -(div // 2), div - 1 - div // 2
    print("  🔴 done 一直没置位(等了 %.1f s)。PH_OFF=%d,本档(DIV=%d)有效范围 [%d, %d]"
          % (timeout, ph, div, lo, hi), flush=True)
    return False


def _read_buf(m, n):
    out = []
    for i in range(n):
        m.write(R_ADC_BUF_IDX, i)
        out.append(m.read(R_ADC_BUF_DATA))
    return out


def _report(ws, n):
    vals = [w & 0xFFF for w in ws]
    otr = sum((w >> 12) & 1 for w in ws)
    mn, mx = min(vals), max(vals)
    mean = float(sum(vals)) / len(vals)
    print("  统计 n=%d  min=%d max=%d p-p=%d mean=%.1f  去重=%d  OTR=%d"
          % (n, mn, mx, mx - mn, mean, len(set(vals)), otr), flush=True)
    if len(set(vals)) <= 2:
        print("  ⇒ 去重 ≤2:数据线像**卡死/没接**或 ADC 无输出(OE_N/PDN 查一下)", flush=True)
    return vals


def cmd_dump(st, div, ph, n):
    if not cmd_go(st, div, ph):
        return
    ws = _read_buf(st['mmio'], n)
    vals = _report(ws, n)
    for r in range(0, n, 16):
        print("    " + " ".join("%4d" % v for v in vals[r:r + 16]), flush=True)
    return vals


def cmd_wave(st, div, ph, n):
    if not cmd_go(st, div, ph):
        return
    vals = [w & 0xFFF for w in _read_buf(st['mmio'], n)]
    for r in range(0, n, 16):
        print("    " + " ".join("%4d" % v for v in vals[r:r + 16]), flush=True)
    return vals


def cmd_eye(st, div):
    """扫 PH_OFF 全范围。每档采 256 点,报 p-p 与去重数。
    眼中心 = 采样值稳定、p-p 大且重复性好的那几档;过渡沿 = 采到摆动中间值。"""
    lo, hi = -(div // 2), div - 1 - div // 2
    print("  DIV=%d(adc_clk=%.4f MHz)PH_OFF 扫 %d..%d" % (div, 130.0 / div, lo, hi), flush=True)
    print("  PH_OFF  p-p   去重  mean     首8点", flush=True)
    for ph in range(lo, hi + 1):
        if not cmd_go(st, div, ph):
            print("  %5d   -- done 未置位" % ph, flush=True)
            continue
        vals = [w & 0xFFF for w in _read_buf(st['mmio'], 256)]
        mn, mx = min(vals), max(vals)
        print("  %5d  %5d  %4d  %7.1f  %s"
              % (ph, mx - mn, len(set(vals)), float(sum(vals)) / len(vals),
                 " ".join("%4d" % v for v in vals[:8])), flush=True)


def cmd_rd(st, n):
    vals = [w & 0xFFF for w in _read_buf(st['mmio'], n)]
    for r in range(0, n, 16):
        print("    " + " ".join("%4d" % v for v in vals[r:r + 16]), flush=True)
    return vals


def cmd_wr(st, off, val):
    st['mmio'].write(off, val)
    print("  write 0x%02X = 0x%08X  → 读回 0x%08X"
          % (off, val, st['mmio'].read(off)), flush=True)


# ---------------------------------------------------------------------------
# 常驻服务
# ---------------------------------------------------------------------------
DISPATCH = {}


def serve():
    print("=== adc_probe serve  start  pid=%d  %s ==="
          % (os.getpid(), time.strftime('%Y-%m-%d %H:%M:%S')), flush=True)
    print("=== 命令文件 %s / 位流 %s ===" % (CMD_FILE, BIT_FILE), flush=True)

    st = {}
    consumed = 0
    while True:
        try:
            lines = []
            if os.path.exists(CMD_FILE):
                with open(CMD_FILE) as f:
                    for ln in f:
                        ln = ln.strip()
                        if ln and not ln.startswith('#'):
                            lines.append(ln)
            while consumed < len(lines):
                c = lines[consumed]
                consumed += 1
                print(">>> %s" % c, flush=True)
                try:
                    run_cmd(c, st)
                except Exception as e:
                    print("!!! %s: %s" % (type(e).__name__, e), flush=True)
                    traceback.print_exc()
                print("===DONE===", flush=True)
        except Exception as e:
            print("!!! serve loop: %s" % e, flush=True)
            traceback.print_exc()
        time.sleep(0.2)


def run_cmd(c, st):
    p = c.split()
    op = p[0]

    if op in ('quit', 'exit'):
        print("bye", flush=True)
        sys.exit(0)
    if op == 'init':
        return cmd_init(st)
    if op == 'attach':
        return cmd_attach(st)
    if st.get('mmio') is None:
        print("!! 还没 init/attach(MMIO 没映射)。二选一先投:位流未加载投 `init`,"
              "已由别的进程(如 main.py)加载好则投 `attach`。", flush=True)
        return
    if op == 'sts':
        return cmd_sts(st)
    if op == 'fclk':
        return cmd_fclk(st, float(p[1]) if len(p) > 1 else 50.0)
    if op == 'dump':
        return cmd_dump(st, int(p[1]), int(p[2]), int(p[3]) if len(p) > 3 else 64)
    if op == 'wave':
        return cmd_wave(st, int(p[1]), int(p[2]), int(p[3]) if len(p) > 3 else 128)
    if op == 'eye':
        return cmd_eye(st, int(p[1]))
    if op == 'rd':
        return cmd_rd(st, int(p[1]) if len(p) > 1 else 64)
    if op == 'wr':
        return cmd_wr(st, int(p[1], 0), int(p[2], 0))
    print("?? 不认识的命令: %s" % c, flush=True)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'serve':
        serve()
    else:
        print(__doc__)
