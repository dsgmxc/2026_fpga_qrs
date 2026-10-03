#!/usr/bin/env python3
"""fb_base 地址扫描 —— 看是否存在任何一个 DDR 地址能让 HP0 通路取回非零数据。

判决:
  全部 DECERR+全零        ⇒ 端口层面拒了(与地址无关)
  有地址返回非零          ⇒ 地址窗口问题,能定位到边界
"""
import sys, time
sys.path.insert(0, "/home/xilinx/cv")
from drivers import board
from drivers.lcd import LcdPanel

ADDRS = [
    0x00000000,   # 低 1MB:OCM / BootROM 区 —— 若 HP0 窗口真的从 0 开始,这里必有非零
    0x000F0000,   # DDR 基址 0x00100000 之下
    0x00100000,   # DDR 基址(PCW_DDR_RAM_BASEADDR)
    0x01000000,
    0x10000000,
    0x20000000,
    0x30000000,
    0x3FF00000,   # DDR 顶
    0x40000000,   # 超出 1GB —— 预期 DECERR
]

overlay, mon = board.open_monitor()
board.apply_fclk0()
lcd = LcdPanel(mon).allocate()
lcd.show_bars()
lcd.clear_sticky()

def sample(n=120, dt=0.004):
    bmax = 0
    for _ in range(n):
        b, a, s = mon.lcd_dbg_raw()
        v = b & 0x3FFFF
        if v > bmax: bmax = v
        time.sleep(dt)
    return bmax, mon.lcd_dbg()

mon.lcd_set_source(2)
print("%-12s %-10s %-8s %-6s %-6s  %s" % ("fb_base", "beats", "rresp", "axi_or", "px_or", "判决"))
print("-" * 72)
for addr in ADDRS:
    mon.lcd_set_fb_base(0, addr)
    time.sleep(0.35)
    beats, d = sample()
    print("0x%08X   %-10d %-8s %-6d %-6d  %s"
          % (addr, beats, d["rresp_name"], d["axi_or_nz"], d["px_or_nz"], d["verdict"]))

mon.lcd_set_fb_base(0, lcd.phys_addrs[0])
mon.lcd_set_source(0)
print("\n已还原(src=0 彩条,屏应正常)")
