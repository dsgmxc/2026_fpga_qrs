#!/usr/bin/env python3
"""把「回路 / 挡位」一次性设成指定值 —— **写完就退出,不回滚**。

和 `relay_hold.py` 的分工(两个都在,不是一个替一个)
--------------------------------------------------------
    `relay_hold.py`   写完**保持住**(进程不退出),退出时写回 ALL_OFF + B 挡。
                      → 给人拿万用表慢慢量、或盯着屏看顶栏变。
    本脚本            写完**立刻退出**,把状态原地留给下一个工具。
                      → 给自动化串起来用,例如紧接着跑
                        `adc_cfg_probe.py --cfg '2:-1' --only-z --z 30`。

⚠️ 因此本脚本**故意不做收尾写回**。跑完记得自己恢复:
   `sudo -E python3 relay_set.py ALL_OFF`

🔴 **不加载位流** —— `Overlay(download=True)` 在 `lcd_fb_reader` 活跃时会打断
   HP0 上未完成的读事务,锁死 HP 口连带 CPU(2026-10-03 踩过,只能断电)。
   本脚本只读写 `/dev/mem`,假定位流已经由 `main.py` / 服务放下去了。

🔴 **默认走裸 `/dev/mem`**(`backend="devmem"`),不走 PYNQ MMIO
   板上 SSH 会话**不 source `/etc/profile.d`** ⇒ `XILINX_XRT` 为空
   ⇒ `pynq.MMIO()` 抛 `RuntimeError: No Devices Found`(2026-10-14 实测,
   `sudo -E` 也救不了:`-E` 只能保住**本来就有**的变量)。
   裸后端顺带避开 `struct.pack_into` 发**两笔** AXI 事务那个坑
   —— 本脚本的后端用切片赋值,只发一笔(`report/measurements/
   20261010_relay_cfg_write_anomaly.md` §10)。

用法(板上,要 `sudo -E`)::

    sudo -E python3 relay_set.py CRSS           # 用名字
    sudo -E python3 relay_set.py 0x51           # 用位图
    sudo -E python3 relay_set.py ALL_OFF        # 复位安全态
    sudo -E python3 relay_set.py CRSS --gear B  # 同时改挡位(不给 --gear 就不动它)
    sudo -E python3 relay_set.py --watch        # 只看不写,打完一次就退
"""

import argparse
import os
import sys
import time

sys.path.insert(0, "/home/xilinx/cv")
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "python"))

import cv_regs as R                                     # noqa: E402
from drivers.relay import RelayError, RelayMatrix       # noqa: E402


def show(r):
    s = r.snapshot()
    closed = "+".join(f"K{i + 1}" for i in range(7) if (s["relay"] >> i) & 1) or "(无)"
    print(f"0x{s['relay']:02X}  {s['relay_name']:15s} 吸合: {closed:28s}"
          f" 挡位 {s['gear_name']:11s} err={s['err_name']}"
          f"  busy={int(s['busy'])} done={int(s['done'])}")


def main():
    p = argparse.ArgumentParser(description="回路/挡位一次性写入(不加载位流、不回滚)")
    p.add_argument("value", nargs="?", default=None,
                   help="组合:0xNN 位图,或 CISS/COSS/CRSS/RG/"
                        "CISS_DISCHARGE/COSS_DISCHARGE/ALL_OFF")
    p.add_argument("--gear", default=None, help="同时设挡位 A/B/C/D 或 1/2/4/8(不给则不动)")
    p.add_argument("--watch", action="store_true", help="只读打一次状态,不写任何寄存器")
    p.add_argument("--raw", action="store_true",
                   help="显式指定裸 mmap 后端(本脚本默认就是它,加不加都一样)")
    p.add_argument("--timeout-ms", type=int, default=200,
                   help="等 busy 落下的上限(默认 200;板上 40 ms 标称会被 FCLK0 拉长)")
    args = p.parse_args()

    if os.geteuid() != 0:
        print("✗ 需要 root。板上用:`sudo -E python3 relay_set.py ...`")
        return 2

    # ⚠️ 默认 devmem,不是 mmio —— 见文件头。--raw 保留只为跟 relay_hold/relay_rw 用法对齐。
    r = RelayMatrix(mon=None, backend="devmem")
    print(f"后端 = {r._regs.source}   绕法 = {r.workaround}")

    try:
        if args.watch:
            print("只读:")
            show(r)
            return 0

        if args.gear is not None:
            got = r.set_gear(args.gear)
            print(f"挡位 → 0x{got:X} ({r.gear_name(got)})")

        target = args.value if args.value is not None else R.RELAY_RESET
        got = r.apply(target, settle_ms=args.timeout_ms)
        print("已写入(**保持,本脚本不回滚**):")
        show(r)
        print(f"退出。要恢复:`sudo -E python3 {os.path.basename(__file__)} ALL_OFF`")
        return 0

    except RelayError as e:
        print(f"✗ {e}")
        show(r)
        return 1
    finally:
        r.close()


if __name__ == "__main__":
    sys.exit(main())
