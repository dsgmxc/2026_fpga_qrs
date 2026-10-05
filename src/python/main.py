#!/usr/bin/env python3
"""
main.py —— 上电入口(薄装配层)

板上用法::

    sudo -E python3 main.py                 # 完整启动:时钟 → 帧缓存 → C 界面
    sudo -E python3 main.py --no-ui         # 只初始化显示,不进界面(排障用)
    sudo -E python3 main.py --ui-bin /path/to/c_ui

⚠️ **上板必须 `sudo -E python3`**。bare `sudo` 会清掉环境变量,XRT 报
   `No Devices Found` —— 这个坑踩过一次了。

────────────────────────────────────────────────────────────────────────────
本文件【只做"谁先谁后"】。任何一段逻辑如果超过 3 行,它就该去 drivers/ 里:

    drivers/board.py       Overlay 加载 / FCLK0 / monitor    ← 板级引导
    drivers/lcd.py         帧缓存分配、换址、源选择
    drivers/touch.py       触摸事件读取与新旧判定
    drivers/ui_process.py  拉起 / 收掉 C 侧 LVGL 界面
    tools/*.py             独立自检,不依赖本文件

顺序是有讲究的,**不要调换**:
    1. 先 Overlay       —— 之后才有寄存器可用
    2. 再 FCLK0         —— 它**就是像素时钟**(见 drivers/board.py),
                           改它屏会闪一下,所以要在出画面之前改
    3. 再分配帧缓存     —— 必须在 `LCD_CTRL = SRC_DDR` 之前,否则 PL 会去读
                           基址还是 0 的地方(花屏,不报错)
    4. 最后切源 + 起 UI
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import cv_regs


# ---------------------------------------------------------------------------
# 没有 C 界面时的降级画面
# ---------------------------------------------------------------------------
def _placeholder_screen(panel):
    """画一张占位图,让"上电就有画面"这件事在不依赖 gcc 的情况下也成立。

    ⚠️ 这**不是**界面初始化 —— 它只是证明"帧缓存通路可用"。真正的界面是
       `src/c_ui/`,LVGL 跑起来之后这张图会被完全盖掉。
    """
    import numpy as np
    h, w = panel.buffers[0].shape
    x = np.arange(w, dtype=np.uint16)[None, :]
    y = np.arange(h, dtype=np.uint16)[:, None]
    img = (((x * 31) // (w - 1)) << 11) | (((y * 63) // (h - 1)) << 5)
    for i in (0, 1):
        panel.buffers[i][:] = img
        panel.buffers[i][:6, :] = panel.buffers[i][-6:, :] = 0x001F   # 蓝框 = 占位图
        panel.buffers[i][:, :6] = panel.buffers[i][:, -6:] = 0x001F
    panel.flush()          # PS 写完必须 flush,PL 读的是物理内存(见 drivers/lcd.py)


# ---------------------------------------------------------------------------
def boot(args):
    from drivers import board
    from drivers.lcd import LcdPanel, LcdError
    from drivers.touch import TouchPanel
    from drivers.ui_process import UiProcess, UiProcessError

    print("=" * 74)
    print("  MOSFET C-V 在线监测平台 · PS 侧启动")
    print("=" * 74)

    # ---- 1. 位流 + 寄存器链路 --------------------------------------------
    _, mon = board.open_monitor(args.bit)
    for k, v in board.board_info(mon).items():
        print(f"  {k:<12} {v}")
    if not mon.link_check():
        raise SystemExit("✗ PS↔PL 链路不通(VERSION / SCRATCH),先解决这个再谈显示。")

    # ---- 2. FCLK0 固化 ★ 决策 F1 的落地处 --------------------------------
    # ⚠️ 报的是**真值**(按 IO PLL 反算),不是 `Clocks.fclk0_mhz` ——
    #    后者内部假定 IO PLL = 1500 MHz,本板是 1000 ⇒ 偏大 1.5 倍。
    #    那个假数字曾经被当成"实测 41.6667 MHz"写进文档,2026-10-03 才纠正。
    print("\n--- FCLK0(就是像素时钟)---")
    d0, d1, _ = board.fclk0_divs()
    if args.no_clocks:
        print(f"  --no-clocks:维持板上现况 {board.fclk0_real_mhz():.4f} MHz"
              f"(DIV0={d0} DIV1={d1})")
    else:
        print(f"  改前 {board.fclk0_real_mhz():.4f} MHz(DIV0={d0} DIV1={d1})"
              f"  ← BOOT.BIN 留下的默认值")
        actual = board.apply_fclk0()
        print(f"  改后 {actual:.4f} MHz(设计值 {cv_regs.FCLK0_DESIGN_MHZ} MHz)")
        print(f"  📌 口径:写文档一律写「设计值 {cv_regs.FCLK0_DESIGN_MHZ:.0f} MHz / "
              f"真值 {actual:.4f} MHz」")

    # ---- 2b. 放行 DAC1220 的初始化 ★ 2026-10-05 ---------------------------
    # 🔴 **必须排在 apply_fclk0() 之后**,而且**不能并进上面那个 if/else 里**
    #    —— 就算 user 用 --no-clocks,也还得按实际 FCLK0 判断一次能不能放行。
    #    原因见 drivers/board.release_dac() 的长文档:
    #    DAC 的串行复位图案是**双向窗口**,FCLK0 只有在 [46.1, 54.0] MHz
    #    才复位得起来;板上 BOOT.BIN 默认的 27.78 MHz 在窗口外 ⇒ 偏压恒 −10 V。
    print("\n--- DAC 初始化放行(0x84 CV_CTRL)---")
    board.release_dac(mon)

    # ---- 3. 帧缓存 --------------------------------------------------------
    print("\n--- 显示通路 ---")
    lcd = LcdPanel(mon).allocate()
    fb0, fb1 = lcd.phys_addrs
    print(f"  帧缓存 fb0=0x{fb0:08X}  fb1=0x{fb1:08X}"
          f"({cv_regs.FB_FRAME_SIZE} B / 块,64 KB 对齐)")
    panel_ok = False
    try:
        lcd.show_framebuffer()
        panel_ok = True
    except LcdError as e:
        print(f"  ✗ 切到帧缓存失败:{e}")

    # ---- 4. 触摸(只是看一眼,不阻塞启动)---------------------------------
    print("\n--- 触摸通路 ---")
    tp = TouchPanel(mon)
    ev = tp.poll()
    print(f"  {ev}   (没碰屏时 seq 不动、pen_down=0 属正常)")
    if ev.err:
        print("  ℹ err 粘滞位已置起 —— 表示**开机以来曾**出过总线错,不代表现在坏。")

    if not panel_ok:
        raise SystemExit("✗ 显示通路没起来,不进界面。先跑 tools/display_probe.py。")

    # ---- 5. C 界面 --------------------------------------------------------
    print("\n--- 界面 ---")
    ui = UiProcess(binary=args.ui_bin, monitor=mon, panel=lcd, poll_ms=args.poll_ms)
    avail, why = ui.check_available()

    if args.no_ui or not avail:
        if not args.no_ui:
            print(f"  ⚠️ {why}")
            print("  ⇒ 先显示占位图,证明帧缓存通路是通的。")
        _placeholder_screen(lcd)
        print("  👀 屏上应当是**蓝框 + 渐变**的占位图。")
        print("  📌 这不是界面初始化 —— 界面是 src/c_ui/,编译后重跑本脚本即可。")
        return 0

    ui.start()
    print("  👀 屏上应当出现前序那套 LVGL 界面(首页)。点一下按钮看页面切不切。")
    print("  ⚠️ 触摸的坐标轴是否要翻转,以 tools/touch_probe.py 的实测为准。")

    def on_exit(code):
        # UI 没了之后别把屏留在"最后一次 LVGL 的画面"上 —— 那看起来像卡死
        try:
            lcd.show_fill(0x101010)
            print("[main] 界面已退出,屏切到纯色(避免残留画面看着像卡死)")
        except Exception as e:                      # noqa: BLE001
            print(f"[main] 切纯色失败:{e}")

    return ui.supervise(on_exit=on_exit)


def main():
    ap = argparse.ArgumentParser(description="PS 侧启动入口")
    ap.add_argument("--bit", default=None, help=f"位流路径(默认 {cv_regs.OVERLAY_BIT})")
    ap.add_argument("--ui-bin", default=cv_regs.C_UI_BINARY, help="C 界面可执行文件")
    ap.add_argument("--poll-ms", type=int, default=10, help="界面触摸轮询周期(ms)")
    ap.add_argument("--no-clocks", action="store_true", help="不改 FCLK0")
    ap.add_argument("--no-ui", action="store_true", help="只初始化显示,不拉起界面")
    args = ap.parse_args()
    return boot(args)


if __name__ == "__main__":
    sys.exit(main())
