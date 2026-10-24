#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""adc_phase_probe.py —— 把 PH_OFF 在所有合法刻度上扫一遍,看 ADC 原始码
是否**任何相位**都恒定在同一个值。

判据:
  · 若某个相位能读到非零峰峰 ⇒ 数据线是活的,只是眼图/相位的问题;
  · 所有相位读数完全一样(恒 2048)⇒ **数据总线是静止的**(断开/未供电/
    输出被关),与相位无关,别再在 PL 侧找原因。

⛔ 跑完把 0x88 恢复成跑之前的值。⛔ 只在 c_ui 空闲时跑。
"""
import mmap
import os
import struct
import sys
import time

AXI = 0x43C00000
R_ADC_CFG = 0x88
R_ADC_GO = 0x8C
R_ADC_STS = 0x90
R_BUF_IDX = 0x94
R_BUF_DATA = 0x98


def main():
    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED,
                  mmap.PROT_READ | mmap.PROT_WRITE, offset=AXI)

    def rd(o):
        return struct.unpack("<I", m[o:o + 4])[0]

    def wr(o, v):
        m[o:o + 4] = struct.pack("<I", v & 0xFFFFFFFF)

    def capture(n=256):
        t0 = time.monotonic()
        while (rd(R_ADC_STS) & 0x4) != 0:
            if time.monotonic() - t0 > 0.5:
                return None
            time.sleep(0.0002)
        wr(R_ADC_GO, 1)
        t0 = time.monotonic()
        while (rd(R_ADC_STS) & 0x4) == 0:
            if time.monotonic() - t0 > 0.05:
                return None
            time.sleep(0.0002)
        while (rd(R_ADC_STS) & 0x2) == 0:
            if time.monotonic() - t0 > 1.0:
                return None
            time.sleep(0.0002)
        vals = []
        for i in range(n):
            wr(R_BUF_IDX, i)
            vals.append(rd(R_BUF_DATA) & 0xFFF)
        return vals

    orig = rd(R_ADC_CFG)
    print("原 0x88 = 0x%08X" % orig)
    try:
        for div in (2, 4, 13):
            lo, hi = -(div // 2), div - 1 - (div // 2)
            print("\n[DIV=%d] 合法 PH_OFF = [%d, %d]" % (div, lo, hi))
            for ph in range(lo, hi + 1):
                wr(R_ADC_CFG, (div & 0xFF) | ((ph & 0xFF) << 8))
                time.sleep(0.01)
                v = capture()
                if v is None:
                    print("  PH %+3d : 采集失败" % ph)
                    continue
                print("  PH %+3d : min %4d max %4d p-p %4d  (前 8: %s)"
                      % (ph, min(v), max(v), max(v) - min(v),
                         " ".join(str(x) for x in v[:8])))
    finally:
        wr(R_ADC_CFG, orig)
        print("\n[收尾] 0x88 = 0x%08X" % rd(R_ADC_CFG))
    return 0


if __name__ == "__main__":
    sys.exit(main())
