#!/usr/bin/env python3
"""把 LVGL 帧缓存从 DDR 里 dump 出来,存成 PPM —— **只看,不写**。

用途:在**不依赖触摸、不依赖人眼**的情况下,客观看到屏上到底画了什么。
板上 `/home/xilinx/fb_dump.py`。

为什么能这么看:PS 用 LVGL 渲染 RGB565 到 DDR 的帧缓存,PL 的 `lcd_fb_reader`
按物理地址扫描读出。帧缓存在 DDR 里 ⇒ 拿 /dev/mem 按物理地址映射就能读到
**正在显示的那一帧**(和 PL 读到的是同一份内存)。

用法:
    sudo ... fb_dump.py <物理基址(hex)> <输出.ppm> [宽] [高]
    例: sudo ... fb_dump.py 0x38100000 /home/xilinx/fb0.ppm
    两块都存: sudo ... fb_dump.py 0x38100000 /home/xilinx/fb0.ppm
              sudo ... fb_dump.py 0x38230000 /home/xilinx/fb1.ppm

⛔ 只读。帧宽高默认 1024×600(与 main.py 打印的 1228800 B/块 吻合:
   1024 × 600 × 2 = 1,228,800)。
"""
import os
import mmap
import struct
import sys

W, H = 1024, 600


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    base = int(sys.argv[1], 16)
    out = sys.argv[2]
    w = int(sys.argv[3]) if len(sys.argv) > 3 else W
    h = int(sys.argv[4]) if len(sys.argv) > 4 else H

    nbytes = w * h * 2
    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    win = mmap.mmap(fd, nbytes, mmap.MAP_SHARED,
                    mmap.PROT_READ | mmap.PROT_WRITE, offset=base)

    px = struct.unpack_from("<%dH" % (w * h), win, 0)

    # RGB565 → RGB888
    lines = []
    for y in range(h):
        row = bytearray()
        off = y * w
        for x in range(w):
            v = px[off + x]
            r = (v >> 11) & 0x1F
            g = (v >> 5) & 0x3F
            b = v & 0x1F
            row += bytes(((r << 3) | (r >> 2),
                          (g << 2) | (g >> 4),
                          (b << 3) | (b >> 2)))
        lines.append(bytes(row))

    with open(out, "wb") as f:
        f.write(("P6\n%d %d\n255\n" % (w, h)).encode())
        for r in lines:
            f.write(r)

    # 顺带打一点统计,便于判断"是不是黑屏/是不是纯色"
    uniq = len(set(px))
    from collections import Counter
    top = Counter(px).most_common(5)
    print("已写出 %s(%d×%d,%d 字节)" % (out, w, h, nbytes))
    print("  不同颜色数 = %d" % uniq)
    print("  出现最多的 5 种 RGB565 = %s" % [(hex(c), n) for c, n in top])

    win.close()
    os.close(fd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
