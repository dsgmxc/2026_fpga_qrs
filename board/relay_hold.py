#!/usr/bin/env python3
"""把继电器 / 挡位定在某个状态**并保持**,好让你拿万用表慢慢量(或盯着屏看顶栏变)。

🔴 **不加载位流** —— 所以它可以在 `c_ui` 正跑着的时候从另一个 SSH 一起用
   (`lcd_fb_reader` 在 HP 口上读帧缓存时再 `Overlay(download=True)` 会锁死
   HP 口连带 CPU,只能断电;见 `tools/adc_probe.py:11`)。

用法(板上,要 `sudo -E`)::

    sudo -E python3 relay_hold.py 0x10             # 写到 ALL_OFF 并保持(默认)
    sudo -E python3 relay_hold.py CISS             # 用名字也行
    sudo -E python3 relay_hold.py 0x00 --gear C    # 同时把挡位设成 C(1 kΩ)
    sudo -E python3 relay_hold.py --sweep          # 0x00→0x10→0x7F 每 3 s 一轮
    sudo -E python3 relay_hold.py --watch          # 只看不写,1 s 刷一次状态

⚠️ 会真的吸合/释放继电器。Ctrl-C 退出,退出时写回 `ALL_OFF(0x10)` + B 挡。

✅ `0x1C` 现在走**正常的 40 ms 硬件稳定路径**,不需要任何绕法 ——
   2026-10-10 那个"写什么都停在 `0x00`"的缺陷已结案:
   根因是 PS 侧用 `struct.pack_into` 会发**两笔** AXI 事务(先写 `0x00` 再写载荷),
   已修在 `drivers/relay.py::_DevMemRegs.write`(改切片赋值)。**PL 无缺陷。**
   证据:`report/measurements/20261010_relay_cfg_write_anomaly.md` §10。
   ⇒ 本脚本 `--watch` 看到的 `busy/done` 是**硬件真的在计时**,不是绕法下的假象。
"""

import argparse
import os
import sys
import time

sys.path.insert(0, "/home/xilinx/cv")
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "python"))

import cv_regs as R                                     # noqa: E402
from drivers.relay import RelayError, RelayMatrix       # noqa: E402

#: 扫掠序列 —— 三个都不动偏压,只是继电器组合不同
SWEEP = (0x00, 0x10, 0x7F)


def show(r, prefix=""):
    s = r.snapshot()
    closed = "+".join(f"K{i + 1}" for i in range(7) if (s["relay"] >> i) & 1) or "(无)"
    print(f"{prefix}0x{s['relay']:02X}  {s['relay_name']:15s} 吸合: {closed:28s}"
          f" 挡位 {s['gear_name']:11s} err={s['err_name']}")


def main():
    p = argparse.ArgumentParser(description="继电器/挡位定住(不加载位流)")
    p.add_argument("value", nargs="?", default=None,
                   help="组合:0xNN 位图或 CISS/COSS/CRSS/RG/CISS_DISCHARGE/COSS_DISCHARGE/ALL_OFF")
    p.add_argument("--gear", default=None, help="同时设挡位 A/B/C/D 或 1/2/4/8")
    p.add_argument("--sweep", action="store_true", help=f"循环 {[hex(v) for v in SWEEP]},每 3 s")
    p.add_argument("--watch", action="store_true", help="只读轮询,不写任何寄存器")
    p.add_argument("--raw", action="store_true", help="用裸 mmap 后端而不是 PYNQ MMIO")
    args = p.parse_args()

    if os.geteuid() != 0:
        print("✗ 需要 root。板上用:`sudo -E python3 relay_hold.py ...`")
        return 2

    r = RelayMatrix(mon=None, backend=("devmem" if args.raw else "mmio"))
    print(f"后端 = {r._regs.source}   绕法 = {r.workaround}")

    # ---- 只读监察 --------------------------------------------------------
    if args.watch:
        print("只读监察中,1 s 一次。Ctrl-C 停。")
        try:
            while True:
                show(r, f"[{time.strftime('%H:%M:%S')}] ")
                time.sleep(1.0)
        except KeyboardInterrupt:
            print("\n退出(未写过任何寄存器)。")
        finally:
            r.close()
        return 0

    # ---- 写 --------------------------------------------------------------
    def cleanup(*_):
        try:
            r.apply(R.RELAY_RESET)
            r.set_gear(R.GEAR_RESET)
            r.restore_settle()
            print("\n已写回 ALL_OFF(0x10) + B 挡,退出。")
        except RelayError as e:
            print(f"\n✗ 收尾失败:{str(e).splitlines()[0]}")
            print(f"  ⚠️ `0x24` 可能停在 0 —— 手动补:`sudo -E python3 -c \"...\"` 或重跑收尾。")
        finally:
            r.close()
        return 0

    try:
        if args.gear is not None:
            got = r.set_gear(args.gear)
            print(f"挡位 → 0x{got:X} ({r.gear_name(got)})")

        if args.sweep:
            print("扫掠中,每 3 s 一轮。Ctrl-C 停(会写回 ALL_OFF)。")
            while True:
                for v in SWEEP:
                    r.apply(v)
                    show(r, f"[{time.strftime('%H:%M:%S')}] ")
                    time.sleep(3.0)
        else:
            target = args.value if args.value is not None else R.RELAY_RESET
            got = r.apply(target)
            print("已写入并保持:")
            show(r)
            print("\n现在可以去量了 / 看屏上顶栏。按 Ctrl-C 结束(会写回 ALL_OFF)。")
            while True:
                time.sleep(1.0)
    except RelayError as e:
        print(f"✗ {e}")
        return cleanup()
    except KeyboardInterrupt:
        return cleanup()


if __name__ == "__main__":
    sys.exit(main())
