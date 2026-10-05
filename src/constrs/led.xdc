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

#=============================================================================
#  ⚠️ 本文件名叫 led.xdc,但它**实际是工程里唯一启用的引脚约束文件**
#     (constrs_1 里 io.xdc 的 IS_ENABLED = 0)。LCD + 触摸的球号都放这里。
#     2026-10-02 实测确认:综合实现只 Parsing 了 led.xdc,没碰 io.xdc。
#     名字有误导性,但改名属于工程布局调整 —— 先记在此处备查。
#=============================================================================

# ---- GT911 电容触摸(4 根)----------------------------------------------------
# 球号出处:厂商例程 `28_top_lcd_touch` 的约束文件
#   ~/Desktop/zdyz/4_Source_Code/1_FPGA_Design/ZYNQ_7020_FPGA/28_top_lcd_touch/
#       prj/top_lcd_touch.srcs/constrs_1/new/top_lcd_touch.xdc  (#LCD TOUCH 段)
# **不是**从原理图查的 —— 但与上面已跑通的 lcd_* 那 30 行同源同文件,可信度高。
#
# ⚠️ 该厂商文件里【没有】PULLUP 属性,而它的设计能在本板上跑通
#    ⇒ 推断触摸排线上已有**外部上拉**。本工程同样不加 PULLUP:
#    加了只会把"外部上拉缺失"这个硬件问题掩盖成"时好时坏"。
#    (这是推断,不是实测 —— 上板若 I2C 完全无响应,第一件事是量 SCL/SDA 静态电平。)
#
# ⚠️ touch_int 虽是 inout,FPGA 上电/未配置期是**高阻**,
#    不违反 GT911 "复位时 INT 决定器件地址" 的前提;真正的电平由 gt911_ctrl
#    在复位窗口内主动驱动(见该模块文件头)。
set_property -dict {PACKAGE_PIN R19 IOSTANDARD LVCMOS33} [get_ports touch_scl]
set_property -dict {PACKAGE_PIN P20 IOSTANDARD LVCMOS33} [get_ports touch_sda]
set_property -dict {PACKAGE_PIN U19 IOSTANDARD LVCMOS33} [get_ports touch_int]
set_property -dict {PACKAGE_PIN M19 IOSTANDARD LVCMOS33} [get_ports touch_rst_n]

# ---- 模拟前端:DAC1220 偏压(3 根)+ AD9833 激励(3 根)------------------------
# 2026-10-05 加。球号【由用户在板上实际接线后报出】,不是照表挑的 ——
#   这是本项目里唯一一处"接线先于约束"的信号组。
#
#   DAC1220 :  SCLK = W15 (J4-36)    CS    = W13 (J4-32)    SDIO = Y14 (J4-34)
#   AD9833  :  SCLK = T5  (J4-4)     FSYNC = U7  (J4-6)     SDATA = T11 (J4-2)
#
# 电平全 LVCMOS33。两个 bank 都已是 3.3 V,依据不同:
#   · BANK34 —— 本文件上面已跑通的 LCD 那 30 根线就是 3.3 V;
#   · BANK13 —— 厂商例程 `30_digital_voltmeter/prj/digital_voltmeter.srcs/
#               constrs_1/new/pin.xdc` 把 Y12/Y13/V10/V11/W9/W10/Y8/Y9/Y11
#               全用 LVCMOS33 且能综合通过。
#
# ⚠️ DDS 三根线【跨 bank】:T11 属 BANK34,T5/U7 属 BANK13
#    (J4 第 2 脚夹在一排 BANK13 脚中间)。功能无影响 —— 两 bank 同为 3.3 V ——
#    但将来做时序/引脚分组时不要把这三根当成一组。
#
# ⚠️ 本工程里 DAC 这三行【曾经】也写在同目录的 io.xdc 里(内容一模一样),
#    但那个文件 IS_ENABLED = 0、综合时根本不 Parsing —— 2026-10-02 触摸
#    就因此报过 DRC NSTD-1/UCIO-1。**io.xdc 已于 2026-10-05 删除**,
#    引脚约束现在【只有本文件一处】,不再有重复副本。
#    ⛔ 教训:本文件名(led.xdc)与它的实际职责不符,极易误导 —— 新增引脚
#       一律加到这里,不要新建第二个 xdc。
set_property -dict {PACKAGE_PIN W15 IOSTANDARD LVCMOS33} [get_ports dac_sclk]
set_property -dict {PACKAGE_PIN W13 IOSTANDARD LVCMOS33} [get_ports dac_cs]
set_property -dict {PACKAGE_PIN Y14 IOSTANDARD LVCMOS33} [get_ports dac_mosi]
set_property -dict {PACKAGE_PIN T5  IOSTANDARD LVCMOS33} [get_ports dds_sclk]
set_property -dict {PACKAGE_PIN U7  IOSTANDARD LVCMOS33} [get_ports dds_sync]
set_property -dict {PACKAGE_PIN T11 IOSTANDARD LVCMOS33} [get_ports dds_data]
