"""
drivers/board.py —— 板上引导(加载 Overlay / 配 FCLK0 / 拿 monitor)

`main.py` 与 `tools/*.py` 都从这里起步,**不要各写一遍**。

⚠️ 本模块 `import cv_monitor` 是**有意的顺序依赖**:
   用了 `bindto` 的类必须在 `Overlay()` **之前**被 import,否则绑定不生效
   (顶层 IP 会退化成一个不认识的自定义 IP —— `bindto` 静默不生效)。
   本文件在顶层就 import 了它,所以只要用本文件引导,这个坑就踩不到。

⚠️ 上板必须 `sudo -E python3` —— bare `sudo` 会清掉环境变量,XRT 报
   `No Devices Found`。
"""

import cv_regs

# ★ 顺序依赖,别挪到函数里(见文件头)
import cv_monitor                                     # noqa: F401


class BoardError(RuntimeError):
    """板级引导失败(没权限 / 找不到位流 / 时钟没配上去)。"""


def load_overlay(bit=None):
    """加载位流,返回 `pynq.Overlay`。

    ⚠️ `Overlay()` 只加载 `.bit`,**不跑 `ps7_init`** ⇒ 它**不会**改 FCLK0。
       板上 FCLK0 的实际值由启动镜像的 `BOOT.BIN` 决定 —— 所以必须显式
       `apply_fclk0()`,不能指望位流带对。
    """
    from pynq import Overlay

    path = bit or cv_regs.OVERLAY_BIT
    try:
        return Overlay(path)
    except Exception as e:
        raise BoardError(
            f"加载位流失败:{path}\n"
            f"  检查:① 文件在不在、`.hwh` 是否与 `.bit` **同名同目录**"
            f"(PYNQ 靠这个解析 ip_dict);② 是不是 sudo -E 跑的;"
            f"③ 板子是不是被别人占着(Jupyter 里还开着旧 Overlay)。\n"
            f"  原始错误:{e}"
        ) from e


def open_monitor(bit=None):
    """加载位流并取出 `CVMonitor`。返回 `(overlay, monitor)`。

    保留 overlay 的引用是**必须的** —— 它被 GC 掉会把 mmap 一起带走。
    """
    overlay = load_overlay(bit)
    ips = overlay.ip_dict
    name = cv_regs.TOP_MODULE
    if name not in ips:
        raise BoardError(
            f"位流里没有 `{name}`。现有 IP:{sorted(ips.keys())}\n"
            f"  ⇒ 两种可能:① `.bit` 与 `.hwh` 不是同一次综合出来的两个文件"
            f"(PYNQ 靠 `.hwh` 建 ip_dict,两者必须同名同目录);\n"
            f"     ② BD 里顶层 module_ref 换过名字 —— 那就改 `cv_regs` 里的"
            f"`TOP_MODULE`(实例名)和 `TOP_MODULE_VLNV`(模块名)两个常量。"
        )
    ip = getattr(overlay, name)
    if not isinstance(ip, cv_monitor.CVMonitor):
        raise BoardError(
            f"`{name}` 绑到的不是 CVMonitor,而是 {type(ip).__name__}。\n"
            f"  ⇒ `cv_regs.TOP_MODULE_VLNV` 与 `.hwh` 里的 VLNV 对不上。\n"
            f"     这种失配 **不报错**,只是没有 link_check() 等方法 —— 所以在这里拦。"
        )
    return overlay, ip


# ---------------------------------------------------------------------------
# SLCR 直读 / 直写(0xF8000000)
# ---------------------------------------------------------------------------
# ⚠️ 为什么绕开 `pynq.Clocks` 自己动寄存器:
#   `Clocks.fclk0_mhz` 是**算出来**的,它内部假定本板 **IO PLL = 1500 MHz**。
#   本板 IO PLL 实为 **1000 MHz**(2026-10-03 帧率三点反推,见 cv_regs.IO_PLL_MHZ)
#   ⇒ 它给的 DIV0 比需要的大 1.5 倍,真值只有请求值的 2/3。
#   所以先用它配好 CLKACT/SRCSEL/DIV1,再**自己覆盖 DIV0**。
FPGA0_CLK_CTRL  = 0xF8000170
_DIV0_SHIFT, _DIV0_MASK = 8, 0x3F      # DIV0:bits[13:8]
_DIV1_SHIFT, _DIV1_MASK = 20, 0x3F     # DIV1:bits[25:20]

# ⛔ 不要碰 0xF8000240(FPGA_RST_CTRL)—— PL 活着时拉 PL 复位会锁死 HP 口连带 CPU。
#    2026-10-02 把板子写挂死过一次,只能断电。要复位 PL 就重载位流。
_SLCR_FORBIDDEN = (0xF8000240,)


def _slcr(addr, value=None):
    """读写 SLCR 一个 32 位寄存器。`value=None` ⇒ 只读。返回读到的值。"""
    import mmap
    import os
    import struct

    if addr in _SLCR_FORBIDDEN:
        raise BoardError(
            f"拒绝写 0x{addr:08X}(FPGA_RST_CTRL)。\n"
            f"  PL 侧有活跃 AXI 主机时拉 PL 复位会锁死 HP 口、连带 CPU 死机 —— "
            f"2026-10-02 踩过一次,只能断电恢复。要复位 PL 请重载位流。"
        )

    page = addr & ~0xFFF
    off = addr & 0xFFF
    wr = value is not None
    fd = os.open("/dev/mem", (os.O_RDWR if wr else os.O_RDONLY) | os.O_SYNC)
    try:
        m = mmap.mmap(fd, 4096, flags=mmap.MAP_SHARED,
                      prot=mmap.PROT_READ | (mmap.PROT_WRITE if wr else 0),
                      offset=page)
        try:
            if not wr:
                return struct.unpack("<I", m[off:off + 4])[0]
            m[off:off + 4] = struct.pack("<I", value & 0xFFFFFFFF)
            return value & 0xFFFFFFFF
        finally:
            m.close()
    finally:
        os.close(fd)


def fclk0_real_mhz():
    """FCLK0 的**真值**(MHz)—— 按 IO PLL 反算,不依赖 PYNQ 的 1500 MHz 假定。

    DIV 字段为 0 编码的是 bypass(÷1),不是除零。
    """
    v = _slcr(FPGA0_CLK_CTRL)
    d0 = ((v >> _DIV0_SHIFT) & _DIV0_MASK) or 1
    d1 = ((v >> _DIV1_SHIFT) & _DIV1_MASK) or 1
    return cv_regs.IO_PLL_MHZ / d0 / d1


def fclk0_divs():
    """返回 `(DIV0, DIV1, 原始寄存器值)`,排障用。"""
    v = _slcr(FPGA0_CLK_CTRL)
    return (v >> _DIV0_SHIFT) & _DIV0_MASK, (v >> _DIV1_SHIFT) & _DIV1_MASK, v


def apply_fclk0(mhz=cv_regs.FCLK0_DESIGN_MHZ, verify=True, quiet=False):
    """把 PL 的 FCLK0 设成 `mhz`(**真值**),返回真值(MHz)。

    🔴 **FCLK0 就是像素时钟**(`cv_sys.v` `assign lcd_clk = s_axi_aclk;`)。
       屏允许窗口 **40.8 ~ 67.2 MHz**:
         · 板上 BOOT.BIN 留下的默认值是 **27.78 MHz** —— **低于下限**,不受支持;
         · `Clocks.fclk0_mhz = 50` 得到的真值只有 **33.33 MHz** —— **仍低于下限**;
         · 真 50 MHz 要 **DIV0 = 20**(= round(1000 / 50)),本函数负责这一手。

    ⚠️ **必须在 `Overlay()` 之后调用** —— 实测 `Overlay()` 会把 FPGA0_CLK_CTRL
       复位回 BOOT.BIN 的默认值 `0x00600600`(DIV0=6/DIV1=6),先前设的会被冲掉。

    ⚠️ 必须在 `set_source(SRC_DDR)` **之前**调用 —— 改 PCLK 时扫描时序会变,
       正在显示的图会闪一下。放前面就没人看得见。

    ⚠️ 分频是整数,请求值不一定精确达到(比如 45 MHz → DIV0=22 → 45.45)。
       `verify` 检查的是"写进去的 DIV0 有没有生效",不是"请求值被精确达到"。
    """
    from pynq import Clocks

    try:
        Clocks.fclk0_mhz = mhz        # ① 让 PYNQ 配好 CLKACT / SRCSEL / DIV1
    except Exception as e:
        raise BoardError(
            f"写 Clocks.fclk0_mhz = {mhz} 失败。\n"
            f"  最常见原因:**没有 root 权限**(寄存器在 SLCR 区 0xF8000170)。\n"
            f"  原始错误:{e}"
        ) from e

    # ② 覆盖 DIV0 —— 这是本函数存在的唯一理由
    div0 = max(1, int(round(cv_regs.IO_PLL_MHZ / mhz)))
    try:
        v = _slcr(FPGA0_CLK_CTRL)
        _slcr(FPGA0_CLK_CTRL,
              (v & ~(_DIV0_MASK << _DIV0_SHIFT)) | (div0 << _DIV0_SHIFT))
    except BoardError:
        raise
    except Exception as e:
        raise BoardError(
            f"写 SLCR FPGA0_CLK_CTRL 的 DIV0={div0} 失败。\n"
            f"  同上:八成是没 root 权限。原始错误:{e}"
        ) from e

    actual = fclk0_real_mhz()
    wanted = cv_regs.IO_PLL_MHZ / div0

    if verify and abs(actual - wanted) > 0.01:
        d0, d1, raw = fclk0_divs()
        raise BoardError(
            f"FCLK0 没设上:请求 {mhz} MHz(要 DIV0={div0}),但读回 {actual:.4f} MHz。\n"
            f"  当前寄存器 0x{raw:08X} ⇒ DIV0={d0} / DIV1={d1}\n"
            f"  ⇒ DIV0 被别的东西改回去了,或者时钟源不是 IO_PLL。\n"
            f"  ⛔ 不要把请求值当成实际值写进文档(见 CLAUDE.md §八 口径纪律)。"
        )

    lo, hi = cv_regs.PCLK_WINDOW_MHZ
    if not quiet:
        flag = "✓ 在窗口内" if lo <= actual <= hi else "✗ **出窗口**,屏可能无图或异常"
        print(f"[fclk0] {actual:.4f} MHz (IO_PLL {cv_regs.IO_PLL_MHZ:.0f} "
              f"/ DIV0 {div0} / DIV1 {fclk0_divs()[1]})  {flag} [{lo}~{hi}]")
    return actual


def release_dac(mon, quiet=False):
    """放行 `dac_1220` 的初始化(写 0x84 CV_CTRL[0] = 1)。返回 FCLK0 真值(MHz)。

    🔴 **必须在 `apply_fclk0()` 之后调用** —— 这是本函数的全部意义。
       `dac_1220.v` 靠 DAC1220 手册 Figure 12 / Table 5 的**串行复位图案**让芯片
       复位(该芯片没有复位引脚),而那个图案的三段高电平是**双向窗口**:

           t16 = 208.3~325.5 µs   t18 = 416.7~732.4 µs   t19 = 833.3~976.6 µs

       代码里的常数按 **50 MHz** 折(13000 / 28500 / 45000 拍),但本模块跑的是
       PS FCLK0 —— 板上 `BOOT.BIN` 把它留在 **27.778 MHz**,
       于是三段变成 **468 / 1026 / 1620 µs,全部超上界** ⇒ **复位根本不触发**
       ⇒ CMR 配置与自校准被忽略 ⇒ DIR 写入无效
       ⇒ 输出停在芯片上电默认 DIR=0 = **−10 V**(2026-10-05 实测到的症状)。

       ⇒ 所以 `cv_sys.v` 里 `dac_1220.rst_n` **不再直连 `s_axi_aresetn`**,
         而与 `0x84[0]` 相与(复位值 0 = 按在复位)。本函数是唯一的放行点。

    ⚠️ 反解窗口:`t19 = 45000 拍` 是最紧的一项 ⇒ **FCLK0 ∈ [46.1, 54.0] MHz**
       才是合法的(见 `cv_regs.FCLK0_DAC_RST_WINDOW_MHZ` 的三项推导)。
       **本函数主动查这一个条件并拒绝放行**,不做静默放行 ——
       静默放行的代价是"寄存器显示 5.0 V、DAC 实际出 −10 V",最难查的一种坏法。

    ⚠️ **改常数是修不了的**:把窗口折成拍数,27.78 MHz 与 50 MHz 两组合法区间
       在三段上**全都不重叠** ⇒ 没有一组常数能同时覆盖两个频率。
       ⇒ 唯一的解就是"保证初始化在设计频率下跑",也就是本函数的存在理由。

    ✅ 连带好处:`bias_code` 的复位值与上电脉冲本来就等 `dac_init_done`
       (见 `cv_axi_lite_slave.v` §set 脉冲)⇒ 放行后约 800 ms(50 MHz 下
       200 ms 等待 + 复位图案 + 600 ms 自校准)**自动**把 PS 最后写过的偏压码
       送到 DAC,不需要再点一次界面。放行前写进 `0x14` 的值不会丢。
    """
    actual = fclk0_real_mhz()
    lo, hi = cv_regs.FCLK0_DAC_RST_WINDOW_MHZ
    if not (lo <= actual <= hi):
        raise BoardError(
            f"FCLK0 = {actual:.4f} MHz **不在 DAC 复位图案的合法窗口内** "
            f"[{lo}~{hi} MHz],拒绝放行。\n"
            f"  ⇒ 此时放行,三段高电平会超出手册双向窗口上界(见本函数文档),\n"
            f"     复位不触发 ⇒ 偏压恒停在 −10 V,而且**寄存器看起来是正常的**。\n"
            f"  ⇒ 先调 `apply_fclk0()` 把 FCLK0 补到 "
            f"{cv_regs.FCLK0_DESIGN_MHZ:.0f} MHz(开机默认 27.78 在窗口外,\n"
            f"     是 BOOT.BIN 留下的,属预期现象不是故障)。"
        )

    held = mon.dac_release()
    if not quiet:
        if held:
            print(f"[dac] 已放行初始化 @ FCLK0 {actual:.4f} MHz —— "
                  f"约 {(cv_regs.DAC_INIT_MS / 1000.0):.1f} s 后偏压生效")
        else:
            print(f"[dac] 已经是放行态(FCLK0 {actual:.4f} MHz)—— 无需重复放行")
    return actual


def board_info(mon):
    """开机自检信息汇总。打印用。"""
    from pynq import Clocks
    return {
        "VERSION":     f"0x{mon.version():08X}",
        "link_check":  mon.link_check(verbose=False),
        # ★ 报**真值**,不是 Clocks.fclk0_mhz(PYNQ 按 IO PLL 1500 算,偏大 1.5×)
        "fclk0_real":  f"{fclk0_real_mhz():.4f} MHz",
        "fclk0_pynq":  f"{float(Clocks.fclk0_mhz):.4f} MHz (PYNQ 算的,不是真值)",
        "fclk0_expect": cv_regs.FCLK0_DESIGN_MHZ,
        "pclk_window": cv_regs.PCLK_WINDOW_MHZ,
    }
