#!/usr/bin/env python3
"""判决实验:src=2 为什么黑屏 —— 用新加的 0x74/0x78/0x7C 把两个假设分开。

背景:
    上板实测 src=2(DDR 帧缓存)全黑,src=0(厂商彩条)/ src=1(整屏填充)都正常。
    而 PS 侧能读到的 FB_STATUS 对两种**完全不同**的故障给出**相同读数**
    (黑屏 + underrun=0),分不开:
        假设 A:AXI 读事务根本没拿到数据(HP 口 / SmartConnect 层面)
        假设 B:数据拿到了,但乒乓选错缓冲,显示侧读到没写过的全零 BRAM

    第 33 次位流加了三个只读寄存器专治这个:
        0x74 DBG_BEATS   本帧 m_axi_rvalid 拍数(满帧 = 256×600 = 153600)
        0x78 DBG_ARADDR  最近一次 AR 握手成功时的地址
        0x7C DBG_STATE   状态快照(位布局见 cv_regs.DBG2_BIT)

⚠️ 0x74 在**每个 frame_start 清零**、帧内累加 ⇒ 单次随机采样只能读到"帧内进度"。
   所以本脚本**连采 300 次取最大值** —— 最大值才是判决依据:
       max ≈ 153600  ⇒ 数据确实进了 PL ⇒ 假设 B(问题在读侧)
       max == 0      ⇒ 一拍都没进来   ⇒ 假设 A(问题在 AXI 读通路)

只读,不写 DDR(除了画自己的帧缓存)。
"""
import sys
import time

sys.path.insert(0, "/home/xilinx/cv")
sys.path.insert(0, "/home/xilinx/cv/tools")

from drivers import board
from drivers.lcd import LcdPanel

N_SAMPLE = 300          # 采样次数
SAMPLE_DT = 0.003       # 每次间隔(秒)⇒ 约 0.9 s,跨 ~30 帧


def sample(mon):
    """连采 N 次,返回 (beats_max, beats_min, araddr_最后非零, state_最后非零)。"""
    bmax = bmin = None
    araddr = state = 0
    for _ in range(N_SAMPLE):
        b, a, s = mon.lcd_dbg_raw()
        if bmax is None or b > bmax:
            bmax = b
        if bmin is None or b < bmin:
            bmin = b
        if a:
            araddr = a
        if s and s != state:
            state = s
        time.sleep(SAMPLE_DT)
    return bmax, bmin, araddr, state


def report(mon, tag):
    bmax, bmin, araddr, state = sample(mon)
    d = mon.lcd_dbg()
    full = d["full_frame"]
    nmax = bmax & 0x3FFFF                      # ★ 低 18 位才是拍数
    pct = 100.0 * nmax / full if full else 0.0

    print("-" * 74)
    print("▶ %s" % tag)
    print("   0x74 DBG_BEATS   max = %-8d (%.1f%% of %d)   min = %d"
          % (nmax, pct, full, bmin & 0x3FFFF))
    print("        ★ 内容证据: rresp_lat=%d(%s)  axi_or_nz=%d  px_or_nz=%d"
          % (d["rresp_lat"], d["rresp_name"], d["axi_or_nz"], d["px_or_nz"]))
    print("        (axi_or_nz=0 ⇒ **DDR 读回来的数据全是 0**;"
          " px_or_nz=1 ⇒ 行缓冲确实读出了非零像素)")
    print("   0x78 DBG_ARADDR  0x%08X   ← 硬件算出来的实际读地址" % araddr)
    print("   0x7C DBG_STATE   0x%08X" % state)
    print("        state=%d rd_sel=%d wr_sel=%d buf_ready=%d fetch_en=%d"
          % (d["state"], d["rd_sel"], d["wr_sel"], d["buf_ready"], d["fetch_en"]))
    print("        arvalid_frm=%d arready_frm=%d rvalid_frm=%d rresp_bad_frm=%d"
          % (d["arvalid_frm"], d["arready_frm"], d["rvalid_frm"], d["rresp_bad_frm"]))
    print("        line_start_frm=%d de_req_frm=%d both_ready_frm=%d wdt_expired_frm=%d(×%d)"
          % (d["line_start_frm"], d["de_req_frm"], d["both_ready_frm"],
             d["wdt_expired_frm"], d["wdt_expire_cnt"]))
    print("   ★ 判决:%s" % d["verdict"])
    return d


overlay, mon = board.open_monitor()

print("=" * 74)
print(" 0. 时钟(真值,按 IO PLL 反算)")
print("=" * 74)
print("   开机默认 %.4f MHz" % board.fclk0_real_mhz())
print("   apply_fclk0() → %.4f MHz" % board.apply_fclk0())

print("=" * 74)
print(" 1. 先切回彩条并清粘滞 —— 让 axi_err 从干净状态开始")
print("=" * 74)
lcd = LcdPanel(mon).allocate()
fb0, fb1 = lcd.buffers
p0, p1 = lcd.phys_addrs
lcd.show_bars()
time.sleep(0.3)
lcd.clear_sticky()          # 返回的是原始 lcd_status(),没有 source 键
s = lcd.status()
print("   src=%d  清后 underrun=%d axi_err=%d" % (s["source"], s["underrun"], s["axi_err"]))

# 画一张一眼能认的图:整屏绿 + 中间红横带
fb0[:, :] = 0x07E0
fb0[290:310, :] = 0xF800
fb1[:, :] = 0x001F
lcd.flush()
print("   fb0 = 0x%08X(整屏绿 + 中间红横带)   fb1 = 0x%08X(整屏蓝)" % (p0, p1))

print("=" * 74)
print(" 2. 三个工况各采一次")
print("=" * 74)
mon.lcd_set_source(0)
time.sleep(0.4)
report(mon, "src=0 厂商彩条(基线:不该取数)")

mon.lcd_set_source(2)
time.sleep(0.4)
report(mon, "src=2 帧缓存 fb0 = 0x%08X(主判决点)" % p0)
print("   👀 屏上现在是:整屏绿 + 中间一条红横带 ?  还是全黑 ?")

mon.lcd_set_fb_base(0, 0x00100000)
time.sleep(0.4)
report(mon, "src=2 帧缓存 fb0 = 0x00100000(内核镜像,内容必然非零)")
print("   👀 屏上现在是花屏/噪声 ?  还是仍然全黑 ?")

mon.lcd_set_fb_base(0, p1)
time.sleep(0.4)
report(mon, "src=2 帧缓存 fb0 = 0x%08X(自己的 fb1:整屏蓝)" % p1)
print("   👀 屏上现在是整屏蓝 ?  还是仍然全黑 ?")

mon.lcd_set_fb_base(0, p0)
mon.lcd_set_source(2)
time.sleep(0.3)
print("=" * 74)
print(" 结束 —— 已还原 fb_base0 = 0x%08X,停在 src=2" % p0)
print(" 屏上应当是:整屏绿 + 中间红横带。若不是,把上面 0x74/0x7C 的数发我。")
print("=" * 74)
