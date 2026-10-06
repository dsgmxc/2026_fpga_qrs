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

# ---- AD9238 高速 ADC(16 根,2026-10-06 加)------------------------------------
# 球号【由用户在板上实际接线后报出】。第一版报的名单有重复
#   (y11 同时给了 d1/d3,w6 同时给了 d11/otr),与"模块排针共 16 个信号"矛盾;
#   下面这版是用户核对后的无重复名单,并与 J4 脚号单调对应(J4-27/28 = clk/oeb)。
#
#   OEB_N = U13 (J4-28)   CLK  = V13 (J4-27)
#   D0..D11 = W11 Y11 Y12 Y13 V10 V11 W9 W10 Y8 Y9 Y6 Y7   (J4-26 .. J4-15)
#   PDN   = V6  (J4-21)   OTR  = W6  (J4-18)
#
# 电平 LVCMOS33。15 根在 BANK13、只有 CLK(V13)在 BANK34 —— 跨 bank,
#   功能无影响(两 bank 同为 3.3 V)。BANK13 用 3.3 V 的依据与 DDS 那三根相同:
#   厂商例程 30_digital_voltmeter 把 Y12/Y13/V10/V11/W9/W10/Y8/Y9/Y11
#   全用 LVCMOS33 且综合通过。
#
# 🔴 【D0/D11 的顺序没有第二条证据】—— 上面的 D0..D11 完全照用户报的名单排。
#    D0 是 LSB、D11 是 MSB(AD9238 手册 + 模块原理图 D0_A(LSB)/D11_A(MSB))。
#    若实际接反,症状是**采样值位序整体倒置**(正弦会变成锯齿状的怪波形),
#    一眼能看出来 —— 上板第一件事就是看这个。
#
# ⚠️ 【时序约束不在这里,见文件末尾的说明】—— 130 MHz 域的时钟由 MMCM 自动
#    派生,手工再 create_clock 会重复定义报错。
set_property -dict {PACKAGE_PIN V13 IOSTANDARD LVCMOS33} [get_ports adc_clk]
set_property -dict {PACKAGE_PIN U13 IOSTANDARD LVCMOS33} [get_ports adc_oe_n]
set_property -dict {PACKAGE_PIN V6  IOSTANDARD LVCMOS33} [get_ports adc_pdn]
set_property -dict {PACKAGE_PIN W6  IOSTANDARD LVCMOS33} [get_ports adc_otr]
set_property -dict {PACKAGE_PIN W11 IOSTANDARD LVCMOS33} [get_ports {adc_d[0]}]
set_property -dict {PACKAGE_PIN Y11 IOSTANDARD LVCMOS33} [get_ports {adc_d[1]}]
set_property -dict {PACKAGE_PIN Y12 IOSTANDARD LVCMOS33} [get_ports {adc_d[2]}]
set_property -dict {PACKAGE_PIN Y13 IOSTANDARD LVCMOS33} [get_ports {adc_d[3]}]
set_property -dict {PACKAGE_PIN V10 IOSTANDARD LVCMOS33} [get_ports {adc_d[4]}]
set_property -dict {PACKAGE_PIN V11 IOSTANDARD LVCMOS33} [get_ports {adc_d[5]}]
set_property -dict {PACKAGE_PIN W9  IOSTANDARD LVCMOS33} [get_ports {adc_d[6]}]
set_property -dict {PACKAGE_PIN W10 IOSTANDARD LVCMOS33} [get_ports {adc_d[7]}]
set_property -dict {PACKAGE_PIN Y8  IOSTANDARD LVCMOS33} [get_ports {adc_d[8]}]
set_property -dict {PACKAGE_PIN Y9  IOSTANDARD LVCMOS33} [get_ports {adc_d[9]}]
set_property -dict {PACKAGE_PIN Y6  IOSTANDARD LVCMOS33} [get_ports {adc_d[10]}]
set_property -dict {PACKAGE_PIN Y7  IOSTANDARD LVCMOS33} [get_ports {adc_d[11]}]

# ---- 跨时钟域约束(2026-10-06 加)---------------------------------------------
# 时钟名出处:**不是猜的**,是读 `report_clocks` 得到的
#   (vivado -mode batch → open_run synth_1 → get_clocks *)。当时的输出:
#     clk_fpga_0    period = 20.000 ns   ← PS FCLK0,= 50 MHz
#     clk_fb_raw    period = 20.000 ns   ← MMCM CLKFBOUT,= fVCO/M = 1300/26
#     clk_adc_raw   period =  7.692 ns   ← MMCM CLKOUT0 = 1300/10 = **130 MHz** ✅
#   ⛔ 不要用通配符(get_clocks -filter {NAME =~ ...})去"兜" —— 通配符一条都
#      匹配不上时是**静默失效**,不报错,等于没约束。名字照抄实测值。
#
# 为什么可以声明为异步(这是要紧处,不是图省事):
#   两个域同源(FCLK0 → MMCM),ZHOLD 补偿下相位对齐,物理上是同步的。
#   但本设计里两域之间的**每一条**通路都落在下面两类里:
#     · 同步器 —— clk_axi → clk_base:cfg_div / cfg_ph_off / cfg_cont / cfg_go
#                 clk_base → clk_axi:done / busy / otr_sticky / mmcm_locked
#     · 双口 BRAM —— 写口 clk_base / 读口 clk_axi(存储器内部路径不做用户时序)
#   同步器的**第一级按设计就是要踩亚稳态**,拿普通建立/保持去卡它必然报违规,
#   而且不是"可能报"、是"一定报"。声明异步 = 明确告诉工具这两组之间的路径
#   不按周期对齐来卡;MTBF 交给 adc_capture.v 里的 ASYNC_REG + 布局保证。
#   ⇒ 这和 adc_capture.v 文件头的【时钟域清单】是同一条契约的两半,改一边要动另一边。
#
# ⚠️ clk_fb_raw 与 clk_fpga_0 放**同一组**:它是 PLL 反馈时钟,锁定时与参考
#    同频同相,归到一组才符合物理。
# ⚠️ FCLK0 若停在 27.78 MHz(板上 BOOT.BIN 的默认),MMCM 锁不住 ⇒ clk_adc_raw
#    这个时钟对象**根本不会生成** ⇒ 下面这条会报 "clock not found"。
#    那是**预期行为**,说明 0x84[0] 还没放行,不是约束写错了。
set_clock_groups -asynchronous \
    -group {clk_fpga_0 clk_fb_raw} \
    -group {clk_adc_raw}

# ⚠️ ADC 并行数据 adc_d[*] / adc_otr 的【输入时序本版有意不约束】。
#    原因如实记录,不糊过去:
#      · 采样时钟 adc_clk 的周期由运行时寄存器 P 决定(P=13 → 100 ns,
#        P=2 → 15.4 ns),**一个静态 XDC 写不下所有档位**;
#      · 当前数据线是杜邦线,tPD 离散度 ~4 ns 本身就盖过 65 MHz 的眼宽,
#        此时给一个精确的 set_input_delay 只是伪造精度。
#    代价:实现报告里这几个端口会以 no_input_delay 出现在未约束清单中 ——
#    这是**已知且有意**的,不是漏了。要严格签核得先换排线 + 上 IDELAYE2。
