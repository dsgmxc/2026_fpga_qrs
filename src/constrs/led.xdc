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
# 2026-10-07 用户重新接线:两组固定占满 J4 的 31–36 脚
#   DAC1220 :  SCLK = W15 (J4-36)    CS    = W13 (J4-32)    SDIO = Y14 (J4-34)   ← 未动
#   AD9833  :  SCLK = W14 (J4-33)    FSYNC = V12 (J4-31)    SDATA = V15 (J4-35)  ← 换线
#   奇数列(31/33/35)= DDS、偶数列(32/34/36)= DAC。
#
# 电平全 LVCMOS33。两个 bank 都已是 3.3 V,依据不同:
#   · BANK34 —— 本文件上面已跑通的 LCD 那 30 根线就是 3.3 V;
#   · BANK13 —— 厂商例程 `30_digital_voltmeter/prj/digital_voltmeter.srcs/
#               constrs_1/new/pin.xdc` 把 Y12/Y13/V10/V11/W9/W10/Y8/Y9/Y11
#               全用 LVCMOS33 且能综合通过。
#
# ✅ 换线后 DDS 三根线【不再跨 bank】—— 31–36 脚全在 BANK34。
#    旧版是 T11(BANK34)/ T5 / U7(BANK13),此处原写"不要把这三根当成一组"的提醒作废。
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
set_property -dict {PACKAGE_PIN W14 IOSTANDARD LVCMOS33} [get_ports dds_sclk]
set_property -dict {PACKAGE_PIN V12 IOSTANDARD LVCMOS33} [get_ports dds_sync]
set_property -dict {PACKAGE_PIN V15 IOSTANDARD LVCMOS33} [get_ports dds_data]

# ---- AD9238 高速 ADC(16 根接线 / 18 脚排线,2026-10-07 换线)-------------------
# 球号【由用户在板上实际接线后报出】。2026-10-07 用户重新接线:
#   模块换成一根 2×9 排线,整段落在 **J4 第 1–18 脚**,顺序与脚号一一对应。
#
#   OEB  = V5  (J4-1)    CLK   = T11 (J4-2)
#   (NC) = U5  (J4-3)    (NC)  = T5  (J4-4)     ← 模块侧悬空脚,【不约束,见下】
#   D0..D11 = V7 U7 W8 V8 U9 U8 U10 T9 W6 V6 Y7 Y6   (J4-5 .. J4-16)
#   PDN  = Y9  (J4-17)   OTR   = Y8  (J4-18)
#
# ⚠️ U5 / T5 两根【有意不写约束】—— 它们是模块排针上的空脚(用户 2026-10-07:
#   "nc 大概就是悬空引脚,不管它")。**不约束**是有意的:若那两根实际在模块
#   内部接地,FPGA 一旦驱动就是短路 ⇒ 让它们保持未使用、综合器默认高阻。
#   (故本段只有 16 行 set_property,而排线是 18 根 —— 不是漏了。)
#
# 电平 LVCMOS33。16 根里 **15 根在 BANK13、只有 CLK(T11)在 BANK34** —— 跨 bank,
#   功能无影响(两 bank 同为 3.3 V)。BANK13 用 3.3 V 的依据不变:
#   厂商例程 30_digital_voltmeter 把 Y12/Y13/V10/V11/W9/W10/Y8/Y9/Y11
#   全用 LVCMOS33 且综合通过(那些球号本轮已全部让出,依据本身仍成立)。
#
# ⚠️ CLK(T11)【不是时钟能力脚】—— Vivado 实测 IS_CLK_CAPABLE = 0 / IS_GLOBAL_CLK = 0
#   (link_design -part xc7z020clg400-2 → get_package_pins T11 → IO_L1P_T0_34)。
#   **但这不影响**:adc_clk 是 FPGA 的【输出】(cv_sys.v:243 `output wire adc_clk`,
#   由 adc_capture 的计数器 `assign adc_clk = (cnt < half)` 产生),
#   MMCM 的输入是 PS FCLK0(50 MHz),根本不经过这个脚。
#   换线前的 V13(IO_L3N_T0_DQS_34)同样 clk_cap = 0,当年也是这么跑通的。
#
# 🔴 【D0/D11 的顺序在换线后作废,必须重验】—— 下面的 D0..D11 照用户报的名单排。
#    D0 是 LSB、D11 是 MSB(AD9238 手册 + 模块原理图 D0_A(LSB)/D11_A(MSB))。
#    ⚠️ LLM-045 那次"位序判定正确"是在【换线前】的接线下做的 ⇒ 结论不再有效。
#    判据不变:接反的症状是采样值位序整体倒置(正弦变锯齿状的怪波形),一眼能看出。
#
# 🔴 【采样窗刻度同样作废】—— PH_OFF 的窗口中心依赖 FPGA↔ADC 的传播延迟(输出时钟
#    出去、数据回来一个来回),换脚换线后窗口会平移。旧的 −2 / −5 / −31 不得沿用,
#    上板必须重扫。⚠️ 与 R08「时序常数与物理配置绑定」同族 —— 这是第二次踩。
#
# ⚠️ 【时序约束不在这里,见文件末尾的说明】—— 130 MHz 域的时钟由 MMCM 自动
#    派生,手工再 create_clock 会重复定义报错。
set_property -dict {PACKAGE_PIN T11 IOSTANDARD LVCMOS33} [get_ports adc_clk]
set_property -dict {PACKAGE_PIN V5  IOSTANDARD LVCMOS33} [get_ports adc_oe_n]
set_property -dict {PACKAGE_PIN Y9  IOSTANDARD LVCMOS33} [get_ports adc_pdn]
set_property -dict {PACKAGE_PIN Y8  IOSTANDARD LVCMOS33} [get_ports adc_otr]
set_property -dict {PACKAGE_PIN V7  IOSTANDARD LVCMOS33} [get_ports {adc_d[0]}]
set_property -dict {PACKAGE_PIN U7  IOSTANDARD LVCMOS33} [get_ports {adc_d[1]}]
set_property -dict {PACKAGE_PIN W8  IOSTANDARD LVCMOS33} [get_ports {adc_d[2]}]
set_property -dict {PACKAGE_PIN V8  IOSTANDARD LVCMOS33} [get_ports {adc_d[3]}]
set_property -dict {PACKAGE_PIN U9  IOSTANDARD LVCMOS33} [get_ports {adc_d[4]}]
set_property -dict {PACKAGE_PIN U8  IOSTANDARD LVCMOS33} [get_ports {adc_d[5]}]
set_property -dict {PACKAGE_PIN U10 IOSTANDARD LVCMOS33} [get_ports {adc_d[6]}]
set_property -dict {PACKAGE_PIN T9  IOSTANDARD LVCMOS33} [get_ports {adc_d[7]}]
set_property -dict {PACKAGE_PIN W6  IOSTANDARD LVCMOS33} [get_ports {adc_d[8]}]
set_property -dict {PACKAGE_PIN V6  IOSTANDARD LVCMOS33} [get_ports {adc_d[9]}]
set_property -dict {PACKAGE_PIN Y7  IOSTANDARD LVCMOS33} [get_ports {adc_d[10]}]
set_property -dict {PACKAGE_PIN Y6  IOSTANDARD LVCMOS33} [get_ports {adc_d[11]}]

# ---- 继电器矩阵 + 量程挡位(11 根,2026-10-10 加)------------------------------
# 球号【由用户在板上按顺序报出】,未与原理图逐脚交叉核对 ——
#   这是本项目第三处"接线先于约束"的信号组(前两处:DAC/DDS、AD9238)。
#
#   继电器 K1..K7 (relay[0..6]) = U15 U14 U13 V13 W11 Y11 Y12
#              = J4 第 30 / 29 / 28 / 27 / 26 / 25 / 24 脚(连续,逆序)
#   挡位   A B C D (gear[0..3]) = Y13 V10 V11 W10
#              = J4 第 23 / 22 / 21 / 19 脚  ← ⚠️ **不连续:第 20 脚(W9)空着**
#      A = 10 Ω / B = 100 Ω / C = 1 kΩ / D = 10 kΩ
#      (映射顺序 = 用户报出的顺序,与 acquisition_gain.c 的 GEAR_1..GEAR_4
#       逐位对齐,已确认)
#
# ⚠️ 挡位那 4 根【不连续】是用户报出的实际接线,不是抄错。
#    若上板后挡位换挡不响应,先怀疑这里 —— 但也别"顺手改成连续",
#    要以板上丝印为准(见 docs/board_pins.md 陷阱 3「相邻脚差一位」)。
#
# 电平 LVCMOS33:
#   · U15/U14/U13/V13(BANK34)—— 本文件上面 LCD/DAC/DDS 已在此 bank 跑通;
#   · Y13/V10/V11/W10(BANK13)—— 依据同 AD9238 段:厂商例程
#     30_digital_voltmeter 把这几个球号也用 LVCMOS33 且综合通过
#     (见本文件 AD9238 段 line 106 的同一句)。
#
# ⚠️ V13 = CMOS_D2 / U13 = CMOS_D1 / U14 = CMOS_D4 / U15 = CMOS_D3
#    —— 与板载摄像头接口复用(见 docs/board_pins.md 陷阱 4)。
#    本项目没插摄像头模块、也没约束 cam_*,不冲突。
#    换线前的 adc_clk 就曾是 V13(2026-10-07 换走),同一族已用过。
#
# ⚠️ 与既有的 56 根【已逐球号比对,无重复】。
set_property -dict {PACKAGE_PIN U15 IOSTANDARD LVCMOS33} [get_ports {relay[0]}]  ;# K1
set_property -dict {PACKAGE_PIN U14 IOSTANDARD LVCMOS33} [get_ports {relay[1]}]  ;# K2
set_property -dict {PACKAGE_PIN U13 IOSTANDARD LVCMOS33} [get_ports {relay[2]}]  ;# K3
set_property -dict {PACKAGE_PIN V13 IOSTANDARD LVCMOS33} [get_ports {relay[3]}]  ;# K4
set_property -dict {PACKAGE_PIN W11 IOSTANDARD LVCMOS33} [get_ports {relay[4]}]  ;# K5
set_property -dict {PACKAGE_PIN Y11 IOSTANDARD LVCMOS33} [get_ports {relay[5]}]  ;# K6
set_property -dict {PACKAGE_PIN Y12 IOSTANDARD LVCMOS33} [get_ports {relay[6]}]  ;# K7
set_property -dict {PACKAGE_PIN Y13 IOSTANDARD LVCMOS33} [get_ports {gear[0]}]   ;# A 10 Ω
set_property -dict {PACKAGE_PIN V10 IOSTANDARD LVCMOS33} [get_ports {gear[1]}]   ;# B 100 Ω
set_property -dict {PACKAGE_PIN V11 IOSTANDARD LVCMOS33} [get_ports {gear[2]}]   ;# C 1 kΩ
set_property -dict {PACKAGE_PIN W10 IOSTANDARD LVCMOS33} [get_ports {gear[3]}]   ;# D 10 kΩ

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
