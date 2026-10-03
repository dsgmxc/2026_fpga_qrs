#!/usr/bin/env python3
"""
tools/display_probe.py —— 显示通路自检(独立可跑,不 import main)

板上用法::

    sudo -E python3 tools/display_probe.py            # 逐步走,每步停下来等人看
    sudo -E python3 tools/display_probe.py --auto      # 不停,全跑完

它验的顺序是**由外向内**的,每一步只加一个变量,坏了就能一眼定位到哪一段:

    1. PS↔PL 寄存器链路        VERSION / SCRATCH
    2. FCLK0 固化生效          (FCLK0 **就是**像素时钟,见 drivers/board.py)
    3. 厂商彩条                PL 自己的图,不碰 DDR —— 屏/时序/背光/接线的基线
    4. 整屏纯色                PS 写寄存器 → PL 用       —— 验证"PS 的写能到 PL"
    5. DDR 帧缓存渐变图        PS 写 DDR → PL 按行读     —— ★ 风险 D1 的首次实测
    6. 双缓冲换址              0x60 握手 + VBlank 生效
    7. 稳态检查                underrun / axi_err 必须保持 0

第 3 步和第 5 步的区别很关键:第 3 步全在 PL 内部,第 5 步才真正跨了 DDR。
**第 3 步过、第 5 步不过 ⇒ 问题在帧缓存地址/对齐/DDR 读写,不在屏幕时序。**
"""

import argparse
import mmap
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import numpy as np

import cv_regs
from drivers import board
from drivers.lcd import LcdPanel, LcdError


# ---------------------------------------------------------------------------
# 测试图
# ---------------------------------------------------------------------------
def pattern(fb, tag):
    """画一张一看就能判读的图。`tag` 决定中央色块的位置(0 在左 / 1 在右)。

    图里刻意放了四种元素,分别对应四种可能的画错:
      · 四角的白框   —— 行/列错位(框会缺边或跑出屏外)
      · 8 条竖色条   —— 像素错位 / 两两互换(边界会糊)
      · 对角渐变     —— 颜色位序(R 和 B 反了整张图会变"蓝调")
      · 中央色块位置 —— 确认现在**前台是哪一块**(换址真的生效了吗)
    """
    h, w = fb.shape                       # (600, 1024)

    x = np.arange(w, dtype=np.uint16)[None, :]
    y = np.arange(h, dtype=np.uint16)[:, None]

    r = (x * 31) // (w - 1)               # 5 位
    g = (y * 63) // (h - 1)               # 6 位
    b = ((w - 1 - x) * 31) // (w - 1)     # 5 位
    fb[:] = (r << 11) | (g << 5) | b      # RGB565,低字节在前(见下面"颜色不对"提示)

    # 8 条竖色条,占顶部 90 行
    palette = [(31, 0, 0), (0, 63, 0), (0, 0, 31), (31, 63, 0),
               (0, 63, 31), (31, 0, 31), (31, 63, 31), (0, 0, 0)]
    band = w // 8
    for i, (pr, pg, pb) in enumerate(palette):
        x0 = i * band
        fb[:90, x0:x0 + band] = (pr << 11) | (pg << 5) | pb

    # 4 px 白框 —— 错位一眼可见
    fb[:4, :] = fb[-4:, :] = 0xFFFF
    fb[:, :4] = fb[:, -4:] = 0xFFFF

    # 中央色块:tag=0 偏左, tag=1 偏右
    cx = w // 4 if tag == 0 else (3 * w) // 4
    px0, px1 = cx - 60, cx + 60
    py0, py1 = h // 2 - 60, h // 2 + 60
    fb[py0:py1, px0:px1] = 0xF800 if tag == 0 else 0x001F   # 红 / 蓝


# ---------------------------------------------------------------------------
# 独立读回:证明"这些字节真的躺在 DDR 里"(这才是 PL 能看到的)
# ---------------------------------------------------------------------------
def ddr_visible(phys, nbytes, expect):
    """用 /dev/mem 直接读物理内存,和刚写的图逐字节比。

    🔴 为什么值得单独做这件事:
       PS 写的是 **cacheable** 内存(`pynq.allocate`),PL 读的是**物理内存**。
       中间隔着 ARM 的 cache —— `flush()` 漏了的话,PS 自己读回去"完全正确",
       而 PL 读到的是旧数据。**这个错误自查是查不出来的。**
       走 /dev/mem(非缓存映射)读一遍,才等价于"PL 会看到什么"。

    返回 (True, None) 或 (False, 原因)。**读不了不算失败** ——
    `/dev/mem` 可能被 `CONFIG_STRICT_DEVMEM` 拦住(风险 R1),那不是显示通路的错。
    """
    page = 4096
    base = phys & ~(page - 1)
    off = phys - base
    try:
        fd = os.open("/dev/mem", os.O_RDONLY | os.O_SYNC)
    except OSError as e:
        return False, f"打不开 /dev/mem({e})—— 大概不是 root"
    try:
        # ⚠️ `offset` 必须用**关键字**传。
        #    CPython 的位置参数顺序是
        #      (fileno, length, flags, prot, access, offset)
        #    —— 第 5 个是 `access`,**不是 offset**。写成位置参数会把 base 当成 access,
        #    而 access 与 flags/prot 不能同时给 ⇒ ValueError:
        #      "mmap can't specify both access and flags, prot."
        #    ★ 2026-10-02 实测踩到:该异常**发生在 syscall 之前**,
        #      与 CONFIG_STRICT_DEVMEM 毫无关系 —— 修前它一直被误报成"风险 R1"。
        m = mmap.mmap(fd, nbytes + off, flags=mmap.MAP_SHARED,
                      prot=mmap.PROT_READ, offset=base)
        try:
            want = expect.tobytes()[:nbytes]
            raw = np.frombuffer(m, dtype=np.uint8, count=nbytes, offset=off)
            diff = int(np.count_nonzero(raw != np.frombuffer(want, dtype=np.uint8)))
            # ⚠️ 必须先把 raw 丢掉再 close —— frombuffer 视图还活着时 close 会
            #    抛 BufferError("cannot close exported pointers exist")。
            del raw
            if diff == 0:
                return True, None
            return False, f"有 {diff} / {nbytes} 字节与 PS 写的不一致"
        finally:
            m.close()
    except ValueError as e:
        # ⚠️ 这一支**必须**与 OSError 分开。以前写成 `except (OSError, ValueError)`,
        #    结果把探针自己的参数错(上面那条 syscall 都没发出的 ValueError)
        #    报成了"内核开了 CONFIG_STRICT_DEVMEM" ⇒ 风险 R1 实际上【从未被测过】。
        return False, (f"/dev/mem 映射参数写错了({e})\n"
                       f"    ⇒ **这是探针自己的 bug,与内核无关**(syscall 都没发出)。\n"
                       f"    ⇒ 修好之前 R1 是【未测】,不得记为'被内核拦住'。")
    except OSError as e:
        return False, (f"/dev/mem 映射 RAM 被拒({e})\n"
                       f"    ⇒ 这才是风险 R1:内核可能开了 CONFIG_STRICT_DEVMEM。\n"
                       f"    ⇒ 显示通路本身未必有问题,但**无法从 PS 单独证明**字节已落 DDR。")
    finally:
        os.close(fd)


# ---------------------------------------------------------------------------
def step(n, title):
    print(f"\n{'=' * 74}\n  第 {n} 步 · {title}\n{'=' * 74}")


def pause(auto, hold):
    if not auto:
        try:
            input("  看清了吗?按 Enter 继续 …")
        except EOFError:
            pass
    else:
        print(f"  (--auto:停留 {hold} s)")
        time.sleep(hold)


def show(lcd, tag):
    st = lcd.status()
    print(f"  LCD_CTRL src={st['source']}  front={st['front']} pending={st['pending']} "
          f"vblank_cnt={st['vblank_cnt']} swap_cnt={st['swap_cnt']} "
          f"underrun={st['underrun']} axi_err={st['axi_err']}")


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="显示通路自检")
    ap.add_argument("--bit", default=None, help=f"位流路径(默认 {cv_regs.OVERLAY_BIT})")
    ap.add_argument("--auto", action="store_true", help="不停下来等人看")
    ap.add_argument("--hold", type=float, default=3.0, help="--auto 时每步停留秒数")
    ap.add_argument("--no-clocks", action="store_true", help="跳过 FCLK0 固化")
    args = ap.parse_args()

    fails = []

    # ---- 1. 寄存器链路 ----------------------------------------------------
    step(1, "PS↔PL 寄存器链路")
    _, mon = board.open_monitor(args.bit)
    if not mon.link_check():
        print("\n✗ 链路就不通,后面不用看了。")
        return 1

    # ---- 2. FCLK0 ---------------------------------------------------------
    step(2, "FCLK0 固化(FCLK0 就是像素时钟)")
    if args.no_clocks:
        print("  (--no-clocks 跳过)")
    else:
        from pynq import Clocks
        before = float(Clocks.fclk0_mhz)
        after = board.apply_fclk0()
        print(f"  改前 {before:.6f} MHz  →  改后 {after:.6f} MHz "
              f"(设计值 {cv_regs.FCLK0_DESIGN_MHZ})")
        lo, hi = cv_regs.PCLK_WINDOW_MHZ
        print(f"  屏允许窗口 {lo} ~ {hi} MHz —— "
              f"{'在窗口内 ✓' if lo <= after <= hi else '★ 掉出窗口,屏会无图 ✗'}")
        if not (lo <= after <= hi):
            fails.append("FCLK0 不在面板允许窗口内")

    # ---- 3. 厂商彩条 ------------------------------------------------------
    step(3, "厂商彩条(src_sel=0,PL 内部自产,不碰 DDR)")
    lcd = LcdPanel(mon).allocate()
    print(f"  帧缓存物理地址:fb0=0x{lcd.phys_addrs[0]:08X}  fb1=0x{lcd.phys_addrs[1]:08X}")
    for i, p in enumerate(lcd.phys_addrs):
        if p % cv_regs.FB_ALIGNMENT:
            fails.append(f"fb{i} 未 64 KB 对齐")
        if p != mon.lcd_get_fb_base(i):
            fails.append(f"fb{i} 基址写进去读回不一致")
    lcd.show_bars()
    print("  👀 屏上应当出现彩色竖条(厂商自检图)。")
    print("     出不来 ⇒ 屏/背光/时序/接线的问题,**与 PS 和 DDR 都无关**。")
    show(lcd, None)
    pause(args.auto, args.hold)

    # ---- 4. 整屏纯色 ------------------------------------------------------
    step(4, "整屏纯色(src_sel=1,PS 写寄存器 → PL 用)")
    lcd.show_fill(0x0020C0)          # RGB888 蓝
    print("  👀 屏上应当整屏变为**蓝色**。")
    print("     变了 ⇒ PS 的写能到 PL。没变 ⇒ 检查 LCD_CTRL 写没写进去。")
    show(lcd, None)
    pause(args.auto, args.hold)

    # ---- 5. DDR 帧缓存 ★ --------------------------------------------------
    step(5, "DDR 帧缓存渐变图 ★ 风险 D1 的首次实测")
    fb0, fb1 = lcd.buffers
    pattern(fb0, tag=0)
    pattern(fb1, tag=1)
    lcd.flush()
    print("  已画好两块测试图并 flush。")

    ok, why = ddr_visible(lcd.phys_addrs[0], 2048, fb0.reshape(-1).view(np.uint8)[:2048])
    if ok:
        print("  ✓ /dev/mem 旁路读回:与 PS 写的一致 —— 字节确实已落 DDR(PL 看得到)")
    else:
        print(f"  ⚠ 无法独立验证(不作为失败):{why}")

    lcd.show_framebuffer()
    print("  👀 屏上现在应当是**对角渐变 + 顶部 8 色条 + 白框 + 左中红块**。")
    print("     白框四边都在 ⇒ 行/列没错位")
    print("     色条边界干净   ⇒ 像素没两两互换")
    print("     ★ 若**形状全对但颜色不对**(比如整张偏蓝、红蓝互换)⇒ 是 RGB565 的")
    print("       字节序假设反了。改 `lcd_fb_reader.v:312` 附近注释里说的那一处,或把")
    print("       LVGL 的 LV_COLOR_16_SWAP 改 1 —— **不要**在 PS 侧一个个字节去翻。")
    print("     ★ 若**整屏花屏/斜纹** ⇒ 帧缓存基址不对(必须是物理地址 + 64 KB 对齐)。")
    show(lcd, None)
    pause(args.auto, args.hold)

    # ---- 6. 双缓冲换址 ----------------------------------------------------
    step(6, "双缓冲换址(0x60 握手 → 下一个 VBlank 生效)")
    print("  👀 接下来会切到 fb1:中央色块应从**左中**跳到**右中**。")
    try:
        t = lcd.refresh(1)
        print(f"  refresh(1) 生效,耗时 {t:.2f} ms(一帧 17.07 ms,应在 1~2 帧内)")
        show(lcd, None)
        pause(args.auto, args.hold)
        t = lcd.refresh(0)
        print(f"  refresh(0) 切回,耗时 {t:.2f} ms。色块应回到左中。")
        show(lcd, None)
        pause(args.auto, args.hold)
    except LcdError as e:
        fails.append("换址失败")
        print(f"  ✗ {e}")

    # ---- 7. 稳态 ----------------------------------------------------------
    step(7, "稳态检查(持续显示,看行缓冲够不够)")
    secs = 5
    print(f"  持续显示 {secs} s 后读粘滞标志 …")
    time.sleep(secs)
    try:
        st = lcd.assert_healthy()
        print(f"  ✓ underrun=0 axi_err=0 vblank_cnt={st['vblank_cnt']} "
              f"swap_cnt={st['swap_cnt']}")
    except LcdError as e:
        fails.append("稳态不健康")
        print(f"  ✗ {e}")

    # ---- 汇总 -------------------------------------------------------------
    print(f"\n{'=' * 74}\n  display_probe 汇总\n{'=' * 74}")
    if fails:
        for f in fails:
            print(f"  ✗ {f}")
        return 1
    print("  ✓ 全部通过 —— 显示通路从 PS 到屏幕是通的")
    print("  📌 请把上面的实测值(vblank_cnt / swap_cnt / 换址耗时 / 是否看到图)")
    print("     回填到 report/measurements/ —— 模型不能替你上板,数值必须是真的。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
