#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""adc_dc_probe.py —— 只读/写 AXI 寄存器,判断「ADC 读到恒定中值」是
模拟通路没信号,还是采集/配置坏了。

背景(2026-10-24):新一轮 c_ui 启动后,所有扫压点 A/B 两路**峰峰 0 码**
(原始采样恒为偏置二进制中点 2048),而几十分钟前同一块板子还能读到
~1299 码峰峰。本脚本把「偏压」当作已知激励:偏压从 0 拉到 100 V,
A 路(INA)的**直流电平**必须跟着动。不动 ⇒ 模拟通路断(继电器/前端/ADC 输入);
动 ⇒ 通路活着,问题只在 1 MHz 激励那一路。

⛔ 不加载位流、不动 DDS、不改 0x88/0x80。只在 c_ui 不跑的时候执行。
⛔ 结束必定 偏压归 0、回路回 ALL_OFF(0x10)。
"""
import mmap
import os
import struct
import sys
import time

AXI = 0x43C00000
R_RELAY_STS = 0x04
R_BIAS = 0x14
R_RANGE = 0x18
R_RELAY = 0x1C
R_CV_CTRL = 0x84
R_ADC_CFG = 0x88
R_ADC_GO = 0x8C
R_ADC_STS = 0x90
R_BUF_IDX = 0x94
R_BUF_DATA = 0x98


def v2code(v):
    raw = ((v / 10.0) + 10.0) * 1048575.0 / 20.0
    raw = min(max(raw, 0.0), 1048575.0)
    return (int(raw + 0.5) ^ 0x80000) & 0xFFFFF


def main():
    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    m = mmap.mmap(fd, 0x1000, mmap.MAP_SHARED,
                  mmap.PROT_READ | mmap.PROT_WRITE, offset=AXI)

    def rd(o):
        return struct.unpack("<I", m[o:o + 4])[0]

    def wr(o, v):
        m[o:o + 4] = struct.pack("<I", v & 0xFFFFFFFF)

    def wait_busy_zero(timeout=1.0):
        t = time.monotonic()
        while time.monotonic() - t < timeout:
            if (rd(R_RELAY_STS) & 0x1) == 0:
                return True
            time.sleep(0.0005)
        return False

    def relay(code):
        wait_busy_zero()
        wr(R_RELAY, code)
        time.sleep(0.15)
        ok = wait_busy_zero()
        st = rd(R_RELAY_STS)
        print("  0x1C=0x%02X → 读回 0x%02X busy落=%s st=0x%08X err=0x%02X"
              % (code, rd(R_RELAY) & 0x7F, ok, st & 0xFFFF, (st >> 8) & 0xFF))

    def capture(n=1024):
        t0 = time.monotonic()
        while (rd(R_ADC_STS) & 0x4) != 0:      # busy=0
            if time.monotonic() - t0 > 0.5:
                return None
            time.sleep(0.0002)
        wr(R_ADC_GO, 1)
        t0 = time.monotonic()
        while (rd(R_ADC_STS) & 0x4) == 0:      # busy=1
            if time.monotonic() - t0 > 0.05:
                return None
            time.sleep(0.0002)
        while (rd(R_ADC_STS) & 0x2) == 0:      # done=1
            if time.monotonic() - t0 > 1.0:
                return None
            time.sleep(0.0002)
        a = []
        b = []
        for i in range(n):
            wr(R_BUF_IDX, i)
            v = rd(R_BUF_DATA) & 0xFFF
            (a if (i % 2 == 0) else b).append(v)
        return a, b

    def show(tag):
        r = capture()
        if r is None:
            print("  %-22s 采集失败" % tag)
            return
        a, b = r
        print("  %-22s A: mean %7.1f p-p %4d [%4d..%4d]   "
              "B: mean %7.1f p-p %4d [%4d..%4d]   st=0x%X"
              % (tag, sum(a) / len(a), max(a) - min(a), min(a), max(a),
                 sum(b) / len(b), max(b) - min(b), min(b), max(b),
                 rd(R_ADC_STS) & 0xF))

    print("0x84 CV_CTRL    = 0x%08X (dac_rst_n=%d)"
          % (rd(R_CV_CTRL), rd(R_CV_CTRL) & 1))
    print("0x18 挡位       = 0x%X" % (rd(R_RANGE) & 0xF))
    print("0x88 ADC_CFG    = 0x%08X" % rd(R_ADC_CFG))

    try:
        for code, name in ((0x7F, "CISS"), (0x51, "CRSS")):
            print("\n[回路 %s]" % name)
            relay(code)
            for volts, settle in ((0.0, 0.6), (20.0, 0.8), (60.0, 1.0),
                                  (100.0, 1.5), (0.0, 2.0)):
                wr(R_BIAS, v2code(volts))
                time.sleep(settle)
                show("偏压 %6.1f V" % volts)

        print("\n[回路 ALL_OFF 参考]")
        relay(0x10)
        wr(R_BIAS, v2code(0.0))
        time.sleep(0.5)
        show("偏压    0.0 V")
    finally:
        wr(R_BIAS, v2code(0.0))
        wait_busy_zero()
        wr(R_RELAY, 0x10)
        time.sleep(0.2)
        print("\n[收尾] 0x14=0x%05X 0x1C=0x%02X"
              % (rd(R_BIAS) & 0xFFFFF, rd(R_RELAY) & 0x7F))
    return 0


if __name__ == "__main__":
    sys.exit(main())
