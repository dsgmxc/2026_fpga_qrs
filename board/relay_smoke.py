#!/usr/bin/env python3
"""⛔ 已作废 —— 内容并进 `tools/relay_rw.py` 第 0 节,本文件保留只为指路。

为什么撤掉:
  原脚本开头是 `Overlay("/home/xilinx/qrs.bit", download=True)`。
  `lcd_fb_reader` 是活跃 AXI 主机,在它读帧缓存的时候复位 fabric 会打断未完成的
  HP0 读事务、锁死 HP 口连带 CPU,**只能断电**(2026-10-03 踩过)。
  **一次上电只允许加载一次位流**,那一次归 `main.py`。

去哪找原来的五节:
  · ① Overlay 能加载            → 本脚本删掉这一节:位流归 `main.py` 管
  · ② VERSION / SCRATCH 回环    → `relay_rw.py` §0 只读前言
  · ③ 四个寄存器复位值 + 拆位     → `relay_rw.py` §0
  · ④ FCLK0 改频                → `relay_rw.py` §0(改成**只读直算**,不再改频)
  · ⑤ 0xF8000170 直读           → `relay_rw.py` §0 的 `fclk0_real_mhz()`

只要"读一眼、不写"的话,用::

    sudo -E python3 relay_hold.py --watch
"""

import sys

print(__doc__)
print("⛔ 本脚本已作废,不会做任何事。用 `relay_rw.py` 或 `relay_hold.py --watch`。")
sys.exit(2)
