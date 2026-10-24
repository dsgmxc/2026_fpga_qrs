#!/usr/bin/env python3
"""比较两块帧缓存是否一致 —— 验证"矩形同步"是否把后台补齐了。

用法(板上,root):
    python3 /home/xilinx/fb_cmp.py <fb0_phys> <fb1_phys> [次数] [间隔ms]

两块的像素逐个比较,打印:不同的像素数、包围盒(面板朝向的坐标)。
为什么需要它:`sync_invalid_areas()` 只把**失效矩形**从新前台补到新后台;
  若镜像公式或矩形算错,某个角落会停在上上帧 —— 屏上表现为"局部花",
  但抽一帧来看往往又正常(那一块恰好没变)。逐块对比 + 多次采样才能坐实。

⚠️ 采样可能落在**一帧中间**(新内容已写进后台、前台还没换),所以有差异
   不一定是错;连续采几次,若**每次都有**一个固定的、与内容无关的大区域,
   才是真的坏了。

⛔ 只读,不写帧缓存。
"""
import mmap
import os
import sys
import time

import numpy as np

W, H = 1024, 600
BYTES = W * H * 2


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    base0 = int(sys.argv[1], 16)
    base1 = int(sys.argv[2], 16)
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    gap_ms = int(sys.argv[4]) if len(sys.argv) > 4 else 300

    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    a = mmap.mmap(fd, BYTES, mmap.MAP_SHARED,
                  mmap.PROT_READ | mmap.PROT_WRITE, offset=base0)
    b = mmap.mmap(fd, BYTES, mmap.MAP_SHARED,
                  mmap.PROT_READ | mmap.PROT_WRITE, offset=base1)

    for k in range(n):
        va = np.frombuffer(a, dtype=np.uint16, count=W * H)
        vb = np.frombuffer(b, dtype=np.uint16, count=W * H)
        d = np.flatnonzero(va != vb)
        if d.size == 0:
            print("[%d] 两帧完全相同" % k)
        else:
            ys, xs = np.divmod(d, W)
            print("[%d] 不同像素 %d / %d,包围盒 (%d,%d)-(%d,%d)"
                  % (k, d.size, W * H,
                     xs.min(), ys.min(), xs.max(), ys.max()))
        time.sleep(gap_ms / 1000.0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
