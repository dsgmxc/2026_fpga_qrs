#!/usr/bin/env python3
"""
tools/touch_probe.py —— 触屏通路自检(独立可跑,不 import main)

板上用法::

    sudo -E python3 tools/touch_probe.py                 # 跟着提示按 5 次
    sudo -E python3 tools/touch_probe.py --seconds 20    # 自己按,自己看
    sudo -E python3 tools/touch_probe.py --raw           # 每次 poll 都打一行

它验的是**四件事**,分不开来判的话坏了根本不知道从哪查:

    1. 有没有帧         seq 在动           ← 有帧 = I2C 通 + GT911 应答
    2. 坐标合不合理     0~1023 / 0~599     ← 越界 = GT911 配置表分辨率不对
    3. 判不判得准       按下/抬起边沿      ← pen_down 的 1→0 只能由"新帧且点数 0"产生
    4. 位置对不对       五个指定点          ← 屏上的物理位置与读出的坐标对得上

第 4 项靠**提示用户点指定位置**来验 —— 这样坐标轴反了、xy 换了、缩放不对
都能当场看出来,而不是"能读出数就发过"。

⚠️ 本脚本**从不替用户按下**。没有手指就没有事件,这不是失败。
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cv_regs
from drivers import board
from drivers.touch import TouchPanel


# 依次要点的地方:(名称, x, y, 容差 px)
PROMPTS = [
    ("左上角",       60,  40, 90),
    ("右上角",     1024 - 60, 40, 90),
    ("屏幕正中",    1024 // 2, 600 // 2, 90),
    ("左下角",       60, 600 - 40, 90),
    ("右下角",     1024 - 60, 600 - 40, 90),
]


class Stats:
    def __init__(self):
        self.polls = 0
        self.frames = 0
        self.presses = 0
        self.releases = 0
        self.out_of_range = 0
        self.xmin, self.xmax = 1 << 30, -1
        self.ymin, self.ymax = 1 << 30, -1
        self.seq_gaps = 0
        self.err_seen = False
        self._prev_seq = None

    def feed(self, ev):
        self.polls += 1
        if ev.err:
            self.err_seen = True
        if not ev.fresh:
            return
        self.frames += 1
        if self._prev_seq is not None:
            # seq 是 8 位,自然回绕;差 1 是正常,差得多说明 PS 漏读了若干帧
            delta = (ev.seq - self._prev_seq) & 0xFF
            if delta != 1:
                self.seq_gaps += 1
        self._prev_seq = ev.seq

        if ev.pressed:
            self.presses += 1
        if ev.released:
            self.releases += 1
        if ev.pen_down:
            self.xmin = min(self.xmin, ev.x); self.xmax = max(self.xmax, ev.x)
            self.ymin = min(self.ymin, ev.y); self.ymax = max(self.ymax, ev.y)
            if ev.x >= cv_regs.PANEL_WIDTH or ev.y >= cv_regs.PANEL_HEIGHT:
                self.out_of_range += 1

    def report(self):
        rng = lambda a, b: f"{a}~{b}" if b >= 0 else "—"   # noqa: E731
        return [
            ("poll 次数",       self.polls),
            ("新坐标帧数",       self.frames),
            ("按下次数",         self.presses),
            ("抬起次数",         self.releases),
            ("seq 跳号次数",     self.seq_gaps),
            ("x 范围",          rng(self.xmin, self.xmax)),
            ("y 范围",          rng(self.ymin, self.ymax)),
            ("越界点数",         self.out_of_range),
            ("err 粘滞曾置起",   "是" if self.err_seen else "否"),
        ]


def dump(ev):
    flag = "NEW " if ev.fresh else "    "
    print(f"  {flag}seq={ev.seq:3d} n={ev.count} pen={int(ev.pen_down)} "
          f"err={int(ev.err)}  ({ev.x:4d},{ev.y:4d})")


def main():
    ap = argparse.ArgumentParser(description="触屏通路自检")
    ap.add_argument("--bit", default=None)
    ap.add_argument("--raw", action="store_true", help="每次 poll 都打一行(很吵)")
    ap.add_argument("--seconds", type=float, default=0,
                    help=">0 时改为『自行触摸 N 秒』,不做逐点提示")
    ap.add_argument("--no-clocks", action="store_true")
    args = ap.parse_args()

    print("=" * 74)
    print("  触屏自检 —— GT911 经 PL 的 i2c_master / gt911_ctrl")
    print("=" * 74)

    _, mon = board.open_monitor(args.bit)
    if not mon.link_check():
        print("\n✗ PS↔PL 链路不通,先解决这个。")
        return 1

    if not args.no_clocks:
        from pynq import Clocks
        print(f"  FCLK0 改前 {float(Clocks.fclk0_mhz):.6f} MHz")
        print(f"  FCLK0 改后 {board.apply_fclk0():.6f} MHz")

    tp = TouchPanel(mon)
    st = Stats()

    print("\n  ⚠️ GT911 复位/配置由 PL 上电时自己跑(20/6/100 ms 序列)。")
    print("     若一直读不到帧,先怀疑**外部 I2C 上拉**(R19/P20 静态电平应为 3.3 V)")
    print("     —— 这是唯一还没被实测确认的硬件前提。\n")

    if args.seconds > 0:
        print(f"  自行触摸 {args.seconds} s …(随便点、随便划)")
        end = time.monotonic() + args.seconds
        while time.monotonic() < end:
            ev = tp.poll()
            st.feed(ev)
            if args.raw or ev.fresh:
                dump(ev)
            time.sleep(0.01)
    else:
        for name, x, y, tol in PROMPTS:
            print(f"  👉 请用手指**按住**屏幕的【{name}】(期望约 {x},{y}),然后松开 …")
            pair = tp.wait_press_release(timeout_ms=15000)
            if pair is None:
                print("     ✗ 15 s 内没等到完整的按下+抬起。")
                print("       —— 只等到了按下?可能是按住不动(seq 不再变),重试一次。")
                continue
            press, rel = pair
            st.feed(press)
            st.feed(rel)
            dx, dy = abs(press.x - x), abs(press.y - y)
            verdict = "✓" if (dx <= tol and dy <= tol) else "✗"
            print(f"     {verdict} 读出 ({press.x},{press.y}),偏差 ({dx},{dy}) px,"
                  f"容差 {tol};按下 seq={press.seq} → 抬起 seq={rel.seq}")
            time.sleep(0.3)

    print(f"\n{'=' * 74}\n  统计\n{'=' * 74}")
    for k, v in st.report():
        print(f"  {k:<16} {v}")

    fails = []
    if st.frames == 0:
        fails.append("一个坐标帧都没读到 ⇒ I2C 没通(查上拉 / 地址 0x14 or 0x5D / 复位)")
    if st.presses and not st.releases:
        fails.append("有按下没抬起 ⇒ 松手那帧没上报(PL 侧 pen_down 不会自己变 0)")
    if st.out_of_range:
        fails.append(f"{st.out_of_range} 个点越界 ⇒ GT911 配置表分辨率与面板(1024×600)不符")
    if st.seq_gaps > max(3, st.frames // 4):
        fails.append(f"seq 跳号 {st.seq_gaps} 次 ⇒ PS 轮询跟不上 PL,或者格触摸帧丢得多")

    print()
    if fails:
        for f in fails:
            print(f"  ✗ {f}")
        code = 1
    else:
        print("  ✓ 触屏通路正常")
        code = 0

    if st.err_seen:
        print("\n  ℹ err 粘滞标志置起过。它**只在重新配置 PL 时清零**,表示"
              "开机以来曾出过一次总线错,\n"
              "    不代表现在坏了 —— 上面若没有别的问题,可以忽略;"
              "但报告里要如实记一笔。")
    print("\n  📌 请把上面的实测坐标/范围/越界数回填到 report/measurements/。")
    return code


if __name__ == "__main__":
    sys.exit(main())
