set_property PACKAGE_PIN U18 [get_ports clk_in1]
set_property PACKAGE_PIN N16 [get_ports rst_n]
set_property PACKAGE_PIN W13 [get_ports dac_cs]
set_property PACKAGE_PIN Y14 [get_ports dac_mosi]
set_property PACKAGE_PIN W15 [get_ports dac_sclk]
set_property IOSTANDARD LVCMOS33 [get_ports clk_in1]
set_property IOSTANDARD LVCMOS33 [get_ports dac_cs]
set_property IOSTANDARD LVCMOS33 [get_ports dac_mosi]
set_property IOSTANDARD LVCMOS33 [get_ports dac_sclk]
set_property IOSTANDARD LVCMOS33 [get_ports rst_n]

# ---- AD9833 三线 SPI（adc_sync = FSYNC 帧同步 / adc_sclk / adc_mosi = SDATA）----
# ⚠️ 这三个球号是照 docs/board_pins.md 的 J4 表挑的**空闲脚**（BANK34，与已跑通的 dac_* 同 bank，
#    且是相邻的奇数脚 27/29/31），**不是从实际接线查来的** —— 上板前请对 J4 丝印核对一次。
#    备选：J4 第 1/3/5 脚（V5 / U5 / V7，BANK13 纯扩展 IO，底板上无外设、无摄像头复用）。
set_property PACKAGE_PIN V12 [get_ports adc_sync]
set_property PACKAGE_PIN W14 [get_ports adc_sclk]
set_property PACKAGE_PIN V15 [get_ports adc_mosi]
set_property PACKAGE_PIN U5 [get_ports clk_65m]
set_property IOSTANDARD LVCMOS33 [get_ports adc_sync]
set_property IOSTANDARD LVCMOS33 [get_ports adc_sclk]
set_property IOSTANDARD LVCMOS33 [get_ports adc_mosi]
set_property IOSTANDARD LVCMOS33 [get_ports clk_65m]

set_property -dict {PACKAGE_PIN W18 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[0]}]
set_property -dict {PACKAGE_PIN W19 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[1]}]
set_property -dict {PACKAGE_PIN R16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[2]}]
set_property -dict {PACKAGE_PIN R17 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[3]}]
set_property -dict {PACKAGE_PIN W20 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[4]}]
set_property -dict {PACKAGE_PIN V20 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[5]}]
set_property -dict {PACKAGE_PIN P18 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[6]}]
set_property -dict {PACKAGE_PIN N17 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[7]}]
set_property -dict {PACKAGE_PIN V17 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[8]}]
set_property -dict {PACKAGE_PIN V18 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[9]}]
set_property -dict {PACKAGE_PIN T17 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[10]}]
set_property -dict {PACKAGE_PIN R18 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[11]}]
set_property -dict {PACKAGE_PIN Y18 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[12]}]
set_property -dict {PACKAGE_PIN Y19 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[13]}]
set_property -dict {PACKAGE_PIN P15 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[14]}]
set_property -dict {PACKAGE_PIN P16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[15]}]
set_property -dict {PACKAGE_PIN V16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[16]}]
set_property -dict {PACKAGE_PIN W16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[17]}]
set_property -dict {PACKAGE_PIN T14 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[18]}]
set_property -dict {PACKAGE_PIN T15 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[19]}]
set_property -dict {PACKAGE_PIN Y17 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[20]}]
set_property -dict {PACKAGE_PIN Y16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[21]}]
set_property -dict {PACKAGE_PIN T16 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[22]}]
set_property -dict {PACKAGE_PIN U17 IOSTANDARD LVCMOS33} [get_ports {lcd_rgb[23]}]

set_property -dict {PACKAGE_PIN N18 IOSTANDARD LVCMOS33} [get_ports lcd_hs]
set_property -dict {PACKAGE_PIN T20 IOSTANDARD LVCMOS33} [get_ports lcd_vs]
set_property -dict {PACKAGE_PIN U20 IOSTANDARD LVCMOS33} [get_ports lcd_de]
set_property -dict {PACKAGE_PIN M20 IOSTANDARD LVCMOS33} [get_ports lcd_bl]
set_property -dict {PACKAGE_PIN P19 IOSTANDARD LVCMOS33} [get_ports lcd_clk]
set_property -dict {PACKAGE_PIN L17 IOSTANDARD LVCMOS33} [get_ports lcd_rst]

# ⛔⛔ 本文件当前 **IS_ENABLED = 0(未启用)** —— 写在这里的约束【不会生效】。
#     2026-10-02 实测踩到:把 4 条触摸约束加在本文件里,综合实现全程
#     没有 Parsing 过 io.xdc,write_bitstream 直接报
#       ERROR: [DRC NSTD-1]/[DRC UCIO-1] Problem ports: touch_scl, touch_sda,
#                                              touch_int, touch_rst_n
#     ⇒ 生效的是同目录下的 **led.xdc**(IS_ENABLED=1),触摸约束已放那里。
#
#     另注:本文件里那 30 行 lcd_* 与 led.xdc 里的 30 行是**重复副本**;
#     上半段 clk_in1/rst_n/dac_*/adc_*/clk_65m 是**旧顶层 qrs.v 的遗留**,
#     这些端口在 system_wrapper 里根本不存在。
#     ⚠️ 若将来有人把本文件启用,重复的 lcd_* 会产生冲突约束。
#     建议(未执行,等用户决定):整份 io.xdc 删除或清空,只保留 led.xdc。
