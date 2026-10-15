#!/usr/bin/env python3
"""
tools/bias_set.py —— 偏压设定 + 复位值核验(上电周期的**唯一一次**位流加载)

用途:Power-on 之后把 DAC1220 从复位态放行,先读回 `0x14` 的**复位值**,
      再按给定的一组设定点逐点改偏压,让人拿万用表对。

板上用法::

    sudo -E python3 tools/bias_set.py                       # 只放行 + 读复位值,停在 0 V
    sudo -E python3 tools/bias_set.py --volts 0 50 -50      # 0 V → +50 V → −50 V,各停 dwell 秒
    sudo -E python3 tools/bias_set.py --module-volts 0 5 -5 # 同上,但按**模块输出**给数

⚠️ **一次上电只允许加载一次位流**(红线)。所以本脚本**把整串设定点一次跑完**,
   不提供"先设一个、退出、再设一个"的用法 —— 那会需要第二次 `Overlay()`。

⚠️ **本脚本跑完退出后,PL 继续按最后写进去的码驱动 DAC。** 进程退出只是回收了
   mmap,位流与寄存器值都还在。想停在某个值上让人量,就把它放在最后一步。

────────────────────────────────────────────────────────────────────────────
口径(2026-10-15,DF=0):

    V_module(c) = -10 + 20 * ((c ^ 0x80000) / 1048575)      ← DAC 模块输出
    V_bias      = V_module * 10                             ← 外部高压级(硬件接线)

    0x00000 → 模块  0.0 V → 偏压    0.0 V   ★ 复位值,也是本节目的主角
    0x7FFFF → 模块 +10.0 V → 偏压 +100.0 V
    0x80000 → 模块 -10.0 V → 偏压 -100.0 V  ⚠️ 旧复位值(2026-10-15 前),危险端
    0xFFFFF → 模块  0.0 V → 偏压    0.0 V

📌 `--volts` 给的是**偏压**(含 ×10);`--module-volts` 给的是**DAC 模块输出**。
   两者都打印出来,别猜自己量的是哪一段。
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cv_regs                                       # noqa: E402  (★ 常量)
from drivers import board                            # noqa: E402  (★ 顶层 import cv_monitor)

#: 开跑时抓一次快照的寄存器(**读**而已,不改任何东西)
SNAP = ("VERSION", "SCRATCH", "STATUS", "BIAS_CODE",
        "RANGE_SEL", "RELAY_CFG", "SETTLE_CFG", "CV_CTRL")


def _snapshot(mon, title):
    print(f"\n--- {title} ---")
    for name in SNAP:
        try:
            print(f"  {name:<12} 0x{mon._read(name):08X}")
        except Exception as e:                       # noqa: BLE001 —— 只想打印,不想因此停
            print(f"  {name:<12} ✗ {e}")


def _describe(code):
    """20 位码 → (模块电压, 偏压) 一行字。"""
    return (f"码 0x{code:05X} → 模块 {cv_regs.bias_code_to_volts(code) / cv_regs.BIAS_HV_RATIO:+.3f} V"
            f" → 偏压 {cv_regs.bias_code_to_volts(code):+.3f} V")


def main():
    ap = argparse.ArgumentParser(description="偏压设定 + 0x14 复位值核验")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--volts", type=float, nargs="+", metavar="V",
                   help="偏压设定点(含 ×10 高压级),可给多个")
    g.add_argument("--module-volts", type=float, nargs="+", metavar="V",
                   help="DAC 模块输出设定点(不含 ×10),可给多个")
    ap.add_argument("--code", type=lambda s: int(s, 0), nargs="+", metavar="C",
                    help="直接给 20 位码(hex 用 0x…),可给多个")
    ap.add_argument("--dwell", type=float, default=20.0,
                    help="每个设定点停留秒数(默认 20,够人读数)")
    ap.add_argument("--bit", default=None, help="换一个位流(默认 cv_regs.OVERLAY_BIT)")
    ap.add_argument("--no-release", action="store_true",
                    help="不放行 DAC(只读复位值,不碰 0x84)—— 排障用")
    args = ap.parse_args()

    # ---- 设定点 → 码 ------------------------------------------------------
    if args.code:
        codes = [c & cv_regs.BIAS_CODE_MAX for c in args.code]
        src = "直接给码"
    elif args.module_volts:
        codes = [cv_regs.bias_volts_to_code(v * cv_regs.BIAS_HV_RATIO)
                 for v in args.module_volts]
        src = "按模块输出"
    elif args.volts:
        codes = [cv_regs.bias_volts_to_code(v) for v in args.volts]
        src = "按偏压"
    else:
        codes, src = [], "(无设定点)"

    print("=" * 74)
    print("  偏压设定 · 本周期**唯一一次**位流加载")
    print("=" * 74)
    for c in codes:
        print(f"  设定点({src}):{_describe(c)}")
    if not codes:
        print("  (无设定点 —— 只放行并停在复位值)")

    # ---- 1. 位流(★ 本周期唯一一次)--------------------------------------
    overlay, mon = board.open_monitor(args.bit)      # noqa: F841 —— 必须留引用,GC 掉会带走 mmap
    print("\n[1] 位流已加载")

    # ---- 2. 复位值快照 ★ 今天修的就是这个 --------------------------------
    # 🔴 必须在 apply_fclk0() / release_dac() **之前**读 —— 那两步会写 SLCR / 0x84。
    #    此刻 PL 刚配置完,0x14 就是 RTL 里 `bias_code` 的复位初值,**未经任何软件写入**。
    _snapshot(mon, "PL 刚配置完的复位快照(未经任何软件写入)")
    raw = mon._read("BIAS_CODE") & cv_regs.BIAS_CODE_MAX
    expect = cv_regs.BIAS_CODE_ZERO
    print(f"\n  0x14 = 0x{raw:05X}(期望 0x{expect:05X})  "
          f"{'✅ 复位值正确' if raw == expect else '❌ 复位值不对 —— 先别往下设电压!'}")
    if raw != expect:
        print(f"  ⚠️ 若 0x14 == 0x80000,那是**旧位流**(2026-10-15 前),"
              f"DF=0 下它 = 偏压 −100 V。停下,换回 c92c7ed5… 那位流再跑。")
    print(f"  {_describe(raw)}")

    if mon.dac_rst_held():
        print("\n  ℹ️ 0x84[0] = 0 ⇒ **DAC 仍被按在复位态**,此刻模块输出不反映 0x14。")
    else:
        print("\n  ⚠️ 0x84[0] = 1 ⇒ DAC 已被放行过(本周期不是第一次?)。")

    # ---- 3. 时钟 + 放行 --------------------------------------------------
    print("\n[2] FCLK0")
    actual = board.apply_fclk0()
    print(f"  真值 {actual:.4f} MHz(设计值 {cv_regs.FCLK0_DESIGN_MHZ:.0f} MHz)")

    if args.no_release:
        print("\n[3] --no-release:不碰 0x84,到此为止。")
        return 0

    print("\n[3] DAC 初始化放行(0x84[0])")
    board.release_dac(mon)
    time.sleep(1.0)                                  # 放行后约 800 ms 才把码送进 DAC
    _snapshot(mon, "放行 1 s 后")
    print(f"  回读 0x14 = 0x{mon._read('BIAS_CODE') & cv_regs.BIAS_CODE_MAX:05X}")

    # ---- 4. 逐点设定 ------------------------------------------------------
    if not codes:
        print(f"\n[4] 无设定点。**停在复位值**:{_describe(mon._read('BIAS_CODE') & cv_regs.BIAS_CODE_MAX)}")
        print("    现在可以拿万用表量了。")
        return 0

    for i, code in enumerate(codes, 1):
        print(f"\n[4.{i}] 写 0x14 ← {_describe(code)}")
        mon.set_bias(code)
        rb = mon._read("BIAS_CODE") & cv_regs.BIAS_CODE_MAX
        ok = "✅" if rb == code else "❌"
        print(f"  回读 0x14 = 0x{rb:05X} {ok}(期望 0x{code:05X})")
        print(f"  📏 **量吧**(还有 {args.dwell:.0f} s):DAC 模块输出期望 "
              f"{cv_regs.bias_code_to_volts(code) / cv_regs.BIAS_HV_RATIO:+.3f} V")
        for left in range(int(args.dwell), 0, -1):
            print(f"     … {left:2d} s  当前 0x{rb:05X}", flush=True)
            time.sleep(1.0)

    last = codes[-1]
    print(f"\n[5] 跑完。**最后停在** {_describe(last)}")
    print(f"    0x14 回读 = 0x{mon._read('BIAS_CODE') & cv_regs.BIAS_CODE_MAX:05X}")
    print("    (进程退出后 PL 继续按这个码驱动 DAC,可以接着量;")
    print("     想改别的值要重新加载位流 —— 本周期已经用掉了那次机会。)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
