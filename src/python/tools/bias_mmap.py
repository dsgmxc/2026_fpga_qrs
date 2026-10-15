#!/usr/bin/env python3
"""
tools/bias_mmap.py —— 偏压设定(**裸 mmap 路径,不碰位流**)

与 `tools/bias_set.py` 的分工:

| | `bias_set.py` | **本文件** |
|---|---|---|
| 位流 | **加载一次**(`Overlay()`) | ⛔ **一次都不加载** |
| 什么时候能跑 | 上电后、PL 还是空的时候(**全周期唯一一次**) | 位流**已经**配置好之后,想跑几次跑几次 |
| 干什么 | 读复位值 / 配 FCLK0 / 放行 DAC(0x84) | 改 `0x14`、回读 |

⇒ 典型用法是**一次一后**:`bias_set.py` 把链子拉起来(那时 DAC 停在复位值 0 V),
   之后用本文件**随意改偏压**,爱停多久停多久 —— 不用为了换个电压重载位流。

板上用法::

    sudo -E python3 tools/bias_mmap.py                          # 只看,不改
    sudo -E python3 tools/bias_mmap.py --volts 0 50 -50         # 偏压(含 ×10)三点
    sudo -E python3 tools/bias_mmap.py --module-volts 0 5 -5    # 按 DAC 模块输出
    sudo -E python3 tools/bias_mmap.py --code 0x3FFFF           # 直接给 20 位码

⚠️ **跑之前先确认 PL 是配置好的** —— 本文件开头自己会查 `PCAP_STATUS.PCFG_DONE`
   (`tools/pl_state.py`,PS 侧寄存器,读了不会挂总线)。PL 空着还去读 `0x43C00000`
   会把 CPU 挂在 AXI 上(2026-10-02 踩过)。

⚠️ 写通路走 `drivers/relay._DevMemRegs.write`(**切片赋值**),不是 `struct.pack_into`
   —— 后者对 Python mmap 会发两笔 AXI 事务,详见 `drivers/relay.py` §0。
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cv_regs                                       # noqa: E402
from drivers.relay import _DevMemRegs                # noqa: E402  (★ 实测过的一笔写法)
from tools.pl_state import devcfg_read, pl_loaded    # noqa: E402  (★ 位定义只此一份)

SNAP = ("VERSION", "SCRATCH", "STATUS", "BIAS_CODE", "CV_CTRL")


def _describe(module_v):
    return f"模块 {module_v:+.3f} V → 偏压 {module_v * cv_regs.BIAS_HV_RATIO:+.3f} V"


def main():
    ap = argparse.ArgumentParser(description="偏压设定(裸 mmap,不动位流)")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--volts", type=float, nargs="+", metavar="V", help="偏压(含 ×10)")
    g.add_argument("--module-volts", type=float, nargs="+", metavar="V", help="DAC 模块输出")
    g.add_argument("--code", type=lambda s: int(s, 0), nargs="+", metavar="C", help="20 位码")
    ap.add_argument("--dwell", type=float, default=20.0, help="每点停留秒数(默认 20)")
    ap.add_argument("--no-wait", action="store_true", help="不停留,一口气写完(只做回读验证)")
    args = ap.parse_args()

    if not pl_loaded():
        st, ix = devcfg_read()
        print(f"⛔ 本周期还没加载过位流(devcfg STATUS 0x{st:08X} / INT_STS 0x{ix:08X})\n"
              f"   ⇒ PL 是空的,现在读 0x43C00000 会把 CPU 挂在 AXI 上。\n"
              f"   先跑 `sudo -E python3 tools/bias_set.py`"
              f"(它会加载**本周期唯一一次**位流)。\n"
              f"   判据来历见 `tools/pl_state.py` 文件头(位 5 不能用)。")
        return 2

    regs = _DevMemRegs()
    try:
        print("--- 当前快照(裸 mmap,未动位流)---")
        for n in SNAP:
            print(f"  {n:<12} 0x{regs.read(n):08X}")

        cur = regs.read("BIAS_CODE") & cv_regs.BIAS_CODE_MAX
        print(f"\n  0x14 = 0x{cur:05X} → {_describe(cv_regs.bias_code_to_volts(cur) / cv_regs.BIAS_HV_RATIO)}")

        if args.code:
            codes = [c & cv_regs.BIAS_CODE_MAX for c in args.code]
        elif args.module_volts:
            codes = [cv_regs.bias_volts_to_code(v * cv_regs.BIAS_HV_RATIO)
                     for v in args.module_volts]
        elif args.volts:
            codes = [cv_regs.bias_volts_to_code(v) for v in args.volts]
        else:
            return 0                                  # 只读模式

        for i, code in enumerate(codes, 1):
            print(f"\n[{i}/{len(codes)}] 写 0x14 ← 0x{code:05X}  "
                  f"{_describe(cv_regs.bias_code_to_volts(code) / cv_regs.BIAS_HV_RATIO)}")
            regs.write("BIAS_CODE", code)
            rb = regs.read("BIAS_CODE") & cv_regs.BIAS_CODE_MAX
            print(f"  回读 0x14 = 0x{rb:05X} {'✅' if rb == code else '❌ 与写入不符'}")
            if args.no_wait:
                continue
            print(f"  📏 量吧({args.dwell:.0f} s):期望 DAC 模块输出 "
                  f"{cv_regs.bias_code_to_volts(code) / cv_regs.BIAS_HV_RATIO:+.3f} V")
            for left in range(int(args.dwell), 0, -1):
                print(f"     … {left:2d} s", flush=True)
                time.sleep(1.0)

        last = regs.read("BIAS_CODE") & cv_regs.BIAS_CODE_MAX
        print(f"\n停在 0x{last:05X} → "
              f"{_describe(cv_regs.bias_code_to_volts(last) / cv_regs.BIAS_HV_RATIO)}")
        return 0
    finally:
        regs.close()


if __name__ == "__main__":
    raise SystemExit(main())
