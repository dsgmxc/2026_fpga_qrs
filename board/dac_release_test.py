#!/usr/bin/env python3
"""断电重启后的 DAC 放行验证 —— 2026-10-05

前置:**本次开机还没加载过位流**(fpga_manager/state == unknown)。
跑法(板上):
    export XILINX_XRT=/usr
    /usr/local/share/pynq-venv/bin/python3 -u /tmp/dac_release_test.py

判据(第 4 步最关键 —— 窗口闸必须**拒绝**):
  [1] Overlay 成功(= 本次开机唯一一次加载)
  [2] 0x84 读回 0,DAC 被按在复位      ← 新行为,旧位流这里是"不存在"的寄存器
  [3] FCLK0 = 27.78 MHz(BOOT.BIN 默认,在窗口外)
  [4] 此刻 release_dac() **被拒绝**    ← 证明窗口闸真的在拦,不是摆设
  [5] apply_fclk0() → 50.000 MHz
  [6] release_dac() 成功,0x84 读回 1
"""
import sys, time

sys.path.insert(0, "/home/xilinx/cv")

import cv_regs
from drivers import board

print("=" * 70)
print("  DAC 放行验证 —— 2026-10-05")
print("=" * 70)

LO, HI = cv_regs.FCLK0_DAC_RST_WINDOW_MHZ
print(f"  窗口 = [{LO}, {HI}] MHz   (由复位图案三段窗口取交集反解)")

# ---- 1. 位流 ---------------------------------------------------------------
ov, mon = board.open_monitor()
print("\n[1] Overlay OK —— 本次开机唯一一次位流加载")

# ---- 2. 0x84 复位值 --------------------------------------------------------
raw = mon.mmio.read(cv_regs.REG["CV_CTRL"])
held = mon.dac_rst_held()
print(f"[2] 0x84 CV_CTRL = 0x{raw:08X}   dac_rst_held = {held}")
if not held:
    print("    ❌ 期望被按在复位(0)。可能是旧位流,或者 0x84 被人写过。")
else:
    print("    ✅ 复位值 0 —— DAC 初始化被软件闸住,等 apply_fclk0() 之后再放行")

# ---- 2b. ★ 趁**还没放行**先写一个偏压码 --------------------------------------
# 这一步是故意的:用来证明「放行前写进 0x14 的值不会被丢」。
#   dac_1220 的 set_in 只在 NORMAL_WAIT 状态里锁存,而被闸住时 FSM 停在 IDLE
#   ⇒ 这一拍脉冲**当场被丢掉**;但寄存器值留着,放行后从机侧的上电脉冲
#   (等 dac_init_done)会拿**当前** code 补发一次。
# ⇒ 万用表最后量到的电压,同时证明了两件事:复位触发了 + 这个值补发到位了。
torch = cv_regs.BIAS_CODE_MID          # 中位码 ⇒ V_module = 0.00 V
mon.set_bias(torch)
print(f"[2b] 放行前先写 0x14 = 0x{torch:05X}(V_module = 0.00 V)")
print("     ⚠️ 这一拍 set_in 会被硬件丢掉(DAC 还没放行)—— 属预期")

# ---- 3. FCLK0 -------------------------------------------------------------
f0 = board.fclk0_real_mhz()
d0, d1, s = board.fclk0_divs()
print(f"[3] FCLK0 = {f0:.4f} MHz   (DIV0={d0} DIV1={d1} raw=0x{s:08X})")
print(f"    {'❌ 竟然已在窗口内 —— 说明这台板子的 BOOT.BIN 已经改过' if LO <= f0 <= HI else '✅ 在窗口外(预期),放行必须被拒'}")

# ---- 4. 窗口闸必须拒绝 ------------------------------------------------------
print("\n[4] 此刻调用 release_dac() —— 期望**被拒绝**")
try:
    board.release_dac(mon)
    print("    ❌❌ 竟然放行了 —— 窗口闸失效!这会复现 −10 V。立即停机排查。")
    sys.exit(2)
except board.BoardError as e:
    print("    ✅ 被正确拒绝。原文:")
    for line in str(e).splitlines():
        print("       " + line)

# ---- 5. 改频 ---------------------------------------------------------------
print("\n[5] apply_fclk0()")
actual = board.apply_fclk0()

# ---- 6. 放行 ---------------------------------------------------------------
print("\n[6] release_dac()")
board.release_dac(mon)
raw = mon.mmio.read(cv_regs.REG["CV_CTRL"])
print(f"    0x84 = 0x{raw:08X}   dac_rst_held = {mon.dac_rst_held()}")

# ---- 7. 等初始化 -----------------------------------------------------------
ms = cv_regs.DAC_INIT_MS
print(f"\n[7] 等 {ms:.0f} ms 让 DAC 跑完 200 ms 上电 + 复位图案 + 600 ms 自校准 …")
t0 = time.time()
time.sleep(ms / 1000.0 + 0.4)
print(f"    等了 {time.time() - t0:.2f} s")

# ---- 8. 偏压码仍在,且应已补发到位 ------------------------------------------
back = mon.mmio.read(cv_regs.REG["BIAS_CODE"])
print(f"\n[8] 回读 0x14 = 0x{back:06X}   "
      f"{'✅ 仍是放行前写的那个值(没被冲掉)' if back == torch else '❌ 变了'}")
print(f"    ⇒ 按 RTL 推算,放行后从机侧的上电脉冲已把这个码补发到 DAC")
print(f"       ⇒ dac_1220 模块输出应为 **0.00 V**(经 10:1 高压级 ⇒ 偏压 0 V)")

print("\n" + "=" * 70)
print("  ★ 请用万用表量 dac_1220 模块输出端(不是高压级输出):")
print("      期望 ≈ 0.00 V,而不是 −10 V")
print("    这一次读数同时证明三件事:")
print("      ① 三段复位图案真的触发了(否则恒 −10 V)")
print("      ② CMR 配置与自校准跑完了")
print("      ③ 放行前写进 0x14 的值被补发到位了")
print("  之后再改 0x14(界面点偏压 / 下面这行)看是否跟着动。")
print("=" * 70)

# ---- 9. 交互:每隔几秒换一个便于万用表读的值 --------------------------------
print("\n[9] 现在试换几个点(每点留 6 s)。Ctrl-C 可提前停。")
points = [
    (0x80000,  0.0,  "中位  = 0 V"),
    (0xC0000,  5.0,  "+5 V"),          # V = -10 + 20*(0xC0000/0xFFFFF) = +5
    (0x40000, -5.0,  "−5 V"),
    (0x80000,  0.0,  "回中位 0 V"),
]
for code, v, note in points:
    mon.set_bias(code)
    got = mon.mmio.read(cv_regs.REG["BIAS_CODE"])
    print(f"    0x{code:05X} 期望 {v:+.1f} V({note})  回读 0x{got:05X} "
          f"{'✅' if got == code else '❌'}")
    time.sleep(6.0)

print("\n完成。")
