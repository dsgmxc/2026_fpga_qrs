`timescale 1ns / 1ps
//=============================================================================
//  cv_axi_lite_slave.v  ——  手写 AXI4-Lite 从机 + 寄存器映射(P2 stub 版)
//
//  归属:Claude 代写(2026-09-29,用户明确要求,CLAUDE.md §6.0 显式例外)
//
//  【2026-10-10 本次范围】新落地 4 个 —— 继电器矩阵 / 量程挡位:
//      0x04  STATUS     RO   [0]busy [1]done [2]fault [15:8]err_code
//      0x18  RANGE_SEL  RW   [3:0] 量程挡位(必须独热)   ← 规格占位,终于落地
//      0x1C  RELAY_CFG  RW   [6:0] 继电器组合(白名单互锁)← 规格占位,终于落地
//      0x24  SETTLE_CFG RW   [31:16]稳定时间 **ms** [15:0]偏压建立(仍未用)
//    ★ 0x18 / 0x1C / 0x24 是 **report/source_design_checklist.md §3 表里早就写着**
//      的三个地址,此前一直"规格已定、RTL 未实现"(见本文件原有的两处 note)。
//      本次把它们实现掉,**地址一个都没挪** —— 这正是当初坚持不复用 0x18
//      的回报(见下面 IDX_CV_CTRL 那段"为什么不放 0x18")。
//    ★ 读回语义:0x18 / 0x1C 读回来的是【已生效】值,由 relay_ctrl 的
//      sts_*_applied 回送,不是本层锁存的请求值 —— 与 0x60 同一条原则。
//      ⇒ 写入被互锁拒收时,PS 读回的是**旧值**,不是它刚写的那个坏值。
//      设计正文见 relay_ctrl.v 文件头。
//
//  【P2 stub 版】实现 11 个寄存器:
//      0x48  VERSION   RO   常数 —— 读通路 + 地址译码的独立证据
//      0x4C  SCRATCH   RW   —— P2 判据「SCRATCH 读写回环自检」本体
//      0x50  LCD_CTRL  RW   [1:0]src_sel 0=彩条/1=PS填充/2=DDR帧缓存 <-- 2026-10-01 加
//                            ⚠️ 2026-10-02 由 1 位扩到 2 位(决策 D24);PS 若照旧写 1
//                               会得到【整屏纯色填充】而不是帧缓存 —— 不报错,只是画不出来。
//      0x54  LCD_FILL  RW   [23:0] RGB888 填充颜色              <-- 2026-10-01 加
//      0x58  LCD_FB_BASE0    RW  帧缓存 0 物理基址              <-- 2026-10-02 加
//      0x5C  LCD_FB_BASE1    RW  帧缓存 1 物理基址              <-- 2026-10-02 加
//      0x60  LCD_SWAP        RW  写[0]目标;读[0]前台 [1]pending <-- 2026-10-02 加
//      0x64  LCD_FB_STATUS   RO  诊断位(见下方 sts_ 端口表)     <-- 2026-10-02 加
//      0x68  TOUCH_STATUS    RO  触点数 / pen_down / err / seq   <-- 2026-10-02 加
//      0x6C  TOUCH_PT0       RO  触点 0 x/y                     <-- 2026-10-02 加
//      0x70  TOUCH_PT1       RO  触点 1 x/y                     <-- 2026-10-02 加
//    其余地址:读返回 32'h0,写静默忽略。
//
//  【2026-10-02 为什么一次把 7 个都加上】
//    决策 D24:显示改走「PS 写 DDR 双帧缓存 + PL 自研 AXI 读主机」。
//    ★ 本次只加【寄存器 + 端口边界】,不加数据通路 —— lcd_fb_reader /
//      lcd_vblank_ctrl 属下一批(Chunk B),本次在 cv_sys 里把 sts_* 全部接 0,
//      读回全 0 是【预期】的,不是故障。
//    一次加齐的理由:**BD 里 module_ref 的 .xci 是端口边界快照**,
//    每改一次端口就要 Refresh Module Reference 一次。分两批就要刷两遍,
//    而第二批(读主机)还要再动 BD(加 HP 口),把端口一次性定死可以少刷一遍。
//
//  【为什么先加 LCD 这两个】
//    PS 原来对屏幕零控制权,只能看厂商彩条。加上这两个后,PS 一写屏就变 ——
//    一次同时验完:读通道 / 写通道(AW·W·B)/ LCD 面板与 PCLK / 新寄存器逻辑。
//    LCD_CTRL 复位值 = 0 = 彩条,所以上电默认仍走彩条路径,
//    「搬进 cv_sys 后 LCD 没坏」那次回归测试等于免费保留了,不必专门停一步去看。
//
//  【为什么先写 stub 不写全表】
//    完整映射表另 18 个寄存器里有 10 个是 RO(I_A/Q_A/I_B/Q_B/MAG_A/
//    PHASE_A/MAG_B/PHASE_B/STATUS/VERSION),它们的数据源(nco / dac_1220 /
//    CIC / CORDIC / meas_fsm)目前全都不存在。现在就写全表 = 给不存在的信号
//    做读多路选择,综合出一堆悬空警告还分不清对错。
//    但 —— 位宽与 20 个地址索引【本次就定死】(见下),以后只往 case 里加行,
//    不动地址。
//
//  【已知限制,不是 bug】WSTRB 未实现,只支持整字(32-bit)写。
//    PYNQ 的 mmio.write() 就是整字写,功能上不受影响。
//=============================================================================
//
//  寄存器映射(基址 0x43C0_0000,字节偏移 → 字索引 a[7:2])
//
//    0x00 6'h00 CTRL        0x28 6'h0A I_A
//    0x04 6'h01 STATUS      0x2C 6'h0B Q_A        <-- 2026-10-10 实现
//    0x08 6'h02 NCO_FREQ    0x30 6'h0C I_B
//    0x0C 6'h03 NCO_PHASE   0x34 6'h0D Q_B
//    0x10 6'h04 EXC_AMP     0x38 6'h0E MAG_A
//    0x14 6'h05 BIAS_CODE   0x3C 6'h0F PHASE_A  <-- 2026-10-05 实现
//    0x18 6'h06 RANGE_SEL   0x40 6'h10 MAG_B    <-- 2026-10-10 实现
//    0x1C 6'h07 RELAY_CFG   0x44 6'h11 PHASE_B  <-- 2026-10-10 实现
//    0x20 6'h08 CIC_DECIM   0x48 6'h12 VERSION    <-- 已实现
//    0x24 6'h09 SETTLE_CFG  0x4C 6'h13 SCRATCH    <-- 2026-10-10 实现
//                           0x50 6'h14 LCD_CTRL   <-- 2026-10-01 新增
//                           0x54 6'h15 LCD_FILL   <-- 2026-10-01 新增
//                           0x58 6'h16 LCD_FB_BASE0    <-- 2026-10-02 新增
//                           0x5C 6'h17 LCD_FB_BASE1    <-- 2026-10-02 新增
//                           0x60 6'h18 LCD_SWAP        <-- 2026-10-02 新增
//                           0x64 6'h19 LCD_FB_STATUS   <-- 2026-10-02 新增
//                           0x68 6'h1A TOUCH_STATUS    <-- 2026-10-02 新增
//                           0x6C 6'h1B TOUCH_PT0       <-- 2026-10-02 新增
//                           0x70 6'h1C TOUCH_PT1       <-- 2026-10-02 新增
//                           0x74 6'h1D DBG0            <-- 2026-10-03 新增
//                           0x78 6'h1E DBG1            <-- 2026-10-03 新增
//                           0x7C 6'h1F DBG2            <-- 2026-10-03 新增
//                           0x80 6'h20 DDS_FREQ        <-- 2026-10-05 新增
//                           0x84 6'h21 CV_CTRL         <-- 2026-10-05 新增
//                                     ★ [0] clk_release:软件放行【所有按 50 MHz
//                                       折常数工作的外设】—— 目前是 dac_1220 的
//                                       初始化和 ADC 的 MMCM 两个消费者。
//                                       复位值 0 = 都按在复位。
//                                       ⚠️ 2026-10-06 由「dac_rst_n」改名/扩义,
//                                          见端口区 cfg_clk_release 的长注释。
//                                       为什么不放 0x18:那是 RANGE_SEL 的规格位置。
//                           0x88 6'h22 ADC_CFG         <-- 2026-10-06 新增
//                                     [ 7: 0] DIV     分频比 P(≥2),复位 2 → 65 MHz(2026-10-08 定)
//                                     [15: 8] PH_OFF  采样相位偏移(有符号,单位=clk_base 周期)
//                                     [16]    CONT    1=自由运行 / 0=单次 GO 触发
//                                     [17]    OEB_N   0=ADC 输出使能
//                                     [18]    PDN     1=ADC 掉电
//                           0x8C 6'h23 ADC_CTRL        <-- 2026-10-06 新增
//                                     [0] GO —— 写 1 启动一次单次采集(1 拍脉冲,
//                                         **读恒为 0**,与 0x60/0x64 同一种命令语义)
//                           0x90 6'h24 ADC_STS         RO  <-- 2026-10-06 新增
//                                     [0] mmcm_locked  [1] done  [2] busy  [3] otr_sticky
//                           0x94 6'h25 ADC_BUF_IDX     RW  <-- 2026-10-06 新增
//                                     [12:0] 缓冲读索引(0..8191)
//                           0x98 6'h26 ADC_BUF_DATA    RO  <-- 2026-10-06 新增
//                                     [11:0] 采样值 [12] OTR
//
//  ⚠️ PS 侧 `cv_regs.py` 在交付仓库里(本工作区没有副本),新地址**
//     需要用户手工同步过去** —— 见本次交付说明。
//
//  ⚠️ 0x00~0x7C 这 32 个槽位【已于 2026-10-03 全满】—— DBG0/1/2 占掉了原计划
//     留给 P4 安全链(WATCHDOG_KICK / FAULT_LATCH / SAFETY_STATE)的
//     0x74/0x78/0x7C。**上面那句"预留给 P4"是 2026-10-02 写的,已过时。**
//     再加寄存器就必须扩地址,本次即按原预案执行:C_S_AXI_ADDR_WIDTH 7→8
//     (决策 Q27)⇒ 可用槽位 32 → 64,DDS_FREQ 落在第 33 个(0x80)。
//     ⚠️ BD 里 cv_sys_0 是 module_ref(不是打包 IP),改完 ADDR_WIDTH 后
//        需要 Refresh Module Reference,BD 才能看到新的地址宽度。
//
//  ⚠️ C_S_AXI_ADDR_WIDTH 必须 >= 7。用 5 位时 0x4C & 0x1F = 0x0C,
//     会与 NCO_PHASE 混叠。
//=============================================================================

module cv_axi_lite_slave #(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 8,
    parameter [31:0]  VERSION_CONST      = 32'h0001_0000    // v1.0.0
)(
    //-------------------------------------------------------------------------
    // AXI4-Lite 时钟 / 复位
    //-------------------------------------------------------------------------
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_ACLK,ASSOCIATED_BUSIF S_AXI,ASSOCIATED_RESET s_axi_aresetn,FREQ_HZ 50000000,PHASE 0.000" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 S_AXI_ACLK CLK" *)
    input  wire                                 s_axi_aclk,

    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_ARESETN,POLARITY ACTIVE_LOW" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 S_AXI_ARESETN RST" *)
    input  wire                                 s_axi_aresetn,

    //-------------------------------------------------------------------------
    // 写地址通道 AW
    //-------------------------------------------------------------------------
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI,PROTOCOL AXI4LITE,ADDR_WIDTH 8,DATA_WIDTH 32,READ_WRITE_MODE READ_WRITE,HAS_BURST 0,HAS_LOCK 0,HAS_CACHE 0,HAS_PROT 0,HAS_QOS 0,HAS_REGION 0,HAS_WSTRB 1,HAS_BRESP 1,HAS_RRESP 1,SUPPORTS_NARROW_BURST 0,MAX_BURST_LENGTH 1,NUM_READ_OUTSTANDING 1,NUM_WRITE_OUTSTANDING 1,ID_WIDTH 0,AWUSER_WIDTH 0,ARUSER_WIDTH 0,WUSER_WIDTH 0,RUSER_WIDTH 0,BUSER_WIDTH 0,FREQ_HZ 50000000" *)
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWADDR"      *) input  wire [C_S_AXI_ADDR_WIDTH-1:0]  s_axi_awaddr,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWPROT"      *) input  wire [2:0]                     s_axi_awprot,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWVALID"     *) input  wire                            s_axi_awvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWREADY"     *) output wire                            s_axi_awready,

    //-------------------------------------------------------------------------
    // 写数据通道 W
    //-------------------------------------------------------------------------
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WDATA"       *) input  wire [C_S_AXI_DATA_WIDTH-1:0]  s_axi_wdata,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WSTRB"       *) input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0] s_axi_wstrb,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WVALID"      *) input  wire                            s_axi_wvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WREADY"      *) output wire                            s_axi_wready,

    //-------------------------------------------------------------------------
    // 写响应通道 B
    //-------------------------------------------------------------------------
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI BRESP"       *) output wire [1:0]                     s_axi_bresp,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI BVALID"      *) output wire                            s_axi_bvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI BREADY"      *) input  wire                            s_axi_bready,

    //-------------------------------------------------------------------------
    // 读地址通道 AR
    //-------------------------------------------------------------------------
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARADDR"      *) input  wire [C_S_AXI_ADDR_WIDTH-1:0]  s_axi_araddr,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARPROT"      *) input  wire [2:0]                     s_axi_arprot,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARVALID"     *) input  wire                            s_axi_arvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARREADY"     *) output wire                            s_axi_arready,

    //-------------------------------------------------------------------------
    // 读数据通道 R
    //-------------------------------------------------------------------------
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RDATA"       *) output wire [C_S_AXI_DATA_WIDTH-1:0]  s_axi_rdata,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RRESP"       *) output wire [1:0]                     s_axi_rresp,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RVALID"      *) output wire                            s_axi_rvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RREADY"      *) input  wire                            s_axi_rready,

    //-------------------------------------------------------------------------
    // 配置输出(R2 规格 S_AXI ↔ cfg_* / sts_*)
    //   ⚠️ 这两个【不是 AXI 端口,不要加 X_INTERFACE_INFO】—— 普通线网,
    //      在 cv_sys 内部消化,不引出顶层、不产生 BD 引脚。
    //-------------------------------------------------------------------------
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_lcd_ctrl,   // 0x50 [1:0]src_sel 0=彩条/1=填充/2=DDR帧缓存
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_lcd_fill,   // 0x54 [23:0]RGB

    //-------------------------------------------------------------------------
    // 显示通路配置/状态(2026-10-02 加,决策 D24)
    //   同样【不是 AXI 端口,不要加 X_INTERFACE_INFO】—— 普通线网,在 cv_sys 内消化。
    //-------------------------------------------------------------------------
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_lcd_fb_base0,   // 0x58
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_lcd_fb_base1,   // 0x5C
    output wire                                 cfg_lcd_swap_tgt,   // 0x60 写 目标索引(电平保持)
    output wire                                 cfg_lcd_swap_req,   // 0x60 写 1 拍脉冲

    // 0x64 写 = 清 lcd_fb_reader 的粘滞错误(1 拍脉冲;2026-10-03 加)
    //   ★ 此前文档(本文件 §位定义、cv_regs.py、drivers/lcd.py)都写了
    //     「写 0x60 清粘滞」,但 **RTL 里从未实现** —— 三处文档同时说谎。
    //     现在落地在 0x64(**不是 0x60**):0x64 本来就是 FB_STATUS 的读地址,
    //     "读状态/写清零"同址是这类寄存器的常规做法。
    output wire                                 cfg_lcd_clr_sticky,

    //-------------------------------------------------------------------------
    // 模拟前端配置(2026-10-05 加:DAC1220 偏压 + AD9833 激励频率)
    //   ⚠️ 与 cfg_lcd_* 同例 —— 【不是 AXI 端口,不要加 X_INTERFACE_INFO】:
    //      普通线网,在 cv_sys 内部消化,不引出顶层、不产生 BD 引脚。
    //      位域 mask 同样交给消费侧做(本层出寄存器原值)。
    //-------------------------------------------------------------------------
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_bias_code,  // 0x14 [19:0] → dac_1220.code
    output wire                                 cfg_bias_set,   // 0x14 写 → 1 拍脉冲 → dac_1220.set_in
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_dds_freq,   // 0x80 [27:0] → adc_9833.code
    output wire                                 cfg_dds_set,    // 0x80 写 → 1 拍脉冲 → adc_9833.set_in

    //-------------------------------------------------------------------------
    // CV_CTRL(0x84)—— 2026-10-05 加。复位值【0】,即 dac_1220 上电被按在复位态。
    //
    //   🔴 为什么必须有这个位(dac_1220 复位图案是【双向窗口】,不是下限):
    //     DAC1220 的串行复位靠三段高电平的宽度认(手册 Figure 12 / Table 5):
    //       t16 = 208.3~325.5 µs、t18 = 416.7~732.4 µs、t19 = 833.3~976.6 µs
    //     —— **上界与下界都有效**,超上界复位【不触发】。
    //     dac_1220.v 里的常数(13000/28500/45000 拍)是按 50 MHz 折的,
    //     而这块模块跑的是 s_axi_aclk = PS FCLK0,板上 BOOT.BIN 把它留在
    //     **27.7778 MHz** ⇒ 三段变成 468/1026/1620 µs,全部超上界
    //     ⇒ 复位不触发 ⇒ CMR 配置与自校准被忽略 ⇒ DIR 写入无效
    //     ⇒ 输出停在芯片上电默认 DIR=0 = **−10 V**(实测症状)。
    //     ⚠️ 两组窗口折成拍数【不重叠】(t16 @27.78 M 是 5787~9044 拍,
    //        @50 M 是 10415~16275 拍)⇒ **改常数修不了**,只能让初始化
    //        在一个【已知频率】下才放行。
    //   ⇒ 由软件在 apply_fclk0() 把 FCLK0 置到 50.000 MHz 之后写 0x84=1 放行。
    //   ⇒ 这也顺带解释了 09-19 为什么是好的:当时的顶层 qrs.v 用 clk_wiz_0
    //      固定的 50 MHz,天然满足窗口;换成 PS FCLK0 之后才引入本缺陷。
    //
    //   ⚠️ 复位值取 0(按在复位)而不是 1,是为了**不违反本文件的读回契约**:
    //      若取 1,寄存器显示"已放行"而 DAC 实际初始化失败,就是「寄存器说谎」。
    //      取 0 时读回 0 与硬件真实状态一致。下游 bias_set 上电脉冲
    //      本来就等 dac_init_done 才发,而 init_done 在未放行时恒 0 —— 自洽。
    //-------------------------------------------------------------------------
    // 🔴 2026-10-06 由 `cfg_dac_rst_n` 改名为 `cfg_clk_release`,**语义同时扩大**:
    //    它不再只管 dac_1220,而是"FCLK0 已确认为 50.000 MHz,放行所有
    //    按 50 MHz 折时序常数的外设"。目前两个消费者:
    //      · dac_1220.rst_n        —— 复位图案是双向窗口,超上界不复位(2026-10-05)
    //      · adc_capture 的 MMCM   —— CLKIN1_PERIOD 硬编码 20 ns,
    //                                 FCLK0=27.78 MHz 时 VCO 落到 361 MHz,
    //                                 低于 MMCM 下限 600 MHz ⇒ 不锁定
    //    ⇒ **复用同一位**(而不是新开 0x84[1])是刻意的:两者前置条件完全相同,
    //      拆成两位就多一个"置了 bit1 忘了 bit0"的故障模式,而症状是
    //      "DAC 静默停在复位态",极难查。
    //    ✅ 对 PS 侧**无破坏**:旧代码写 0x84=1 仍然有效,且现在会顺带把 ADC 时钟也放行。
    //    ⚠️ 交付仓库 cv_regs.py 里该位仍注释为 dac_rst_n —— 需要在那边补一句。
    output wire                                 cfg_clk_release, // 0x84[0] → dac_1220.rst_n / MMCM RST

    //-------------------------------------------------------------------------
    // ADC 采集(2026-10-06 加)—— 130 MHz 分频 + 8192 深缓冲(2026-10-08 加长)
    //   同样【不是 AXI 端口,不要加 X_INTERFACE_INFO】—— 普通线网,在 cv_sys 内消化。
    //   位域 mask 交给消费侧做(本层出寄存器原值;但见下面 0x88 的例外说明)。
    //-------------------------------------------------------------------------
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_adc_cfg,      // 0x88
    output wire                                 cfg_adc_go,       // 0x8C[0] 写 → 1 拍脉冲
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_adc_buf_idx,  // 0x94 [12:0] 读索引(2026-10-08 12→13 位)
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_adc_sts,      // 0x90
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_adc_buf_data, // 0x98
    // adc_capture 的 done/busy 之类【不单独开口】—— 已经打包进 sts_adc_sts,
    // 再多开几个端口只会让 cv_sys 的引线变噪音。将来若要单独用 done 去触发
    // 别的逻辑,再加端口。

    //-------------------------------------------------------------------------
    // 继电器矩阵 + 量程挡位(2026-10-10 加)—— 消费者是 relay_ctrl.v
    //   ★ 这一组【与其它 cfg_* 都不同】:那三个寄存器(0x18/0x1C/0x24)的
    //     **写入不是照单全收**,要过互锁校验;被拒时旧值不动并置 err_code。
    //     ⇒ 所以本层出的是「请求」+「写脉冲」,回读走 sts_*_applied(已生效值)。
    //     ★ 这就是文件尾那条 note 说的:
    //       「RELAY_REQUEST / RELAY_APPLIED 必须分开」—— 本次照抄落地。
    //
    //   ⚠️ 为什么写脉冲要**单独开口**,而不是让 relay_ctrl 自己去比较
    //      cfg_relay_cfg 的变化沿:
    //      PS 连写两次同一个组合(很常见:重试)时电平不变 ⇒ 采不到沿
    //      ⇒ 第二次请求被静默丢掉。与 0x60 LCD_SWAP 需要 cfg_lcd_swap_req
    //      是**同一条理由**(见文件尾「为什么要 tgt 与 req 两个信号」)。
    //
    //   ⚠️ 0x24 SETTLE_CFG 不需要写脉冲:它是纯参数(延时多少),不是事件。
    //      与 ADC_CFG(0x88)同例 —— 那边也没有脉冲,只有电平。
    //-------------------------------------------------------------------------
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_range_sel,    // 0x18 [3:0] 挡位请求
    output wire                                 cfg_range_set,    // 0x18 写 → 1 拍脉冲
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_relay_cfg,    // 0x1C [6:0] 组合请求
    output wire                                 cfg_relay_set,    // 0x1C 写 → 1 拍脉冲
    output wire [C_S_AXI_DATA_WIDTH-1:0]        cfg_settle_cfg,   // 0x24 [31:16]稳定ms
    input  wire [3:0]                           sts_gear_applied, // ← 0x18 读(已生效)
    input  wire [7:0]                           sts_relay_applied,// ← 0x1C 读(已生效)
    // 0x04 STATUS —— 32 位整字由 cv_sys 从 relay_ctrl 的散信号打包。
    //   ⚠️ 打包放 cv_sys 不放本层,理由与本文件对 sts_adc_sts 的处理一致:
    //      cv_sys 已经有一句 `assign sts_adc_sts[31:4] = 28'h0;` —— 同一个套路。
    //      ★ 位布局(std/done/busy/err_code)的定义在 relay_ctrl.v 里,
    //        本层只负责把它原样送到读通路,不解释。
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_status,       // 0x04

    // ⚠️ 输入(不是输出):dac_1220 的 init_done,给上面的上电脉冲【开门】用。
    //    没有它那个脉冲就是空放 —— 逐状态核对见下面 §set 脉冲 的说明。
    //    同为普通线网,不是 AXI 端口,不要加 X_INTERFACE_INFO。
    input  wire                                 dac_init_done,

    // 调试观测输入(2026-10-03 加,来源 lcd_fb_reader,只读)
    //   · 0x74 本帧 m_axi_rvalid 拍数(满帧 = 256×600 = 153600)
    //   · 0x78 最近一次 AR 握手成功时的地址
    //   · 0x7C 状态快照(位布局见 lcd_fb_reader.v §6)
    //   为什么需要:上板 src=2 全黑时,FB_STATUS 无法区分
    //   「AXI 读不回数据」与「读回了但乒乓选错缓冲」—— 这两个计数能分开。
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_dbg_beats,      // 0x74
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_dbg_araddr,     // 0x78
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_dbg_state,      // 0x7C

    // 状态输入(来源:lcd_vblank_ctrl / gt911_ctrl)——
    //   ⚠️ Chunk B 之前它们在本层【尚无驱动源】,cv_sys 里统一接 0。
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_lcd_status,     // 0x64(+0x60 读的 [1:0])
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_touch_status,   // 0x68
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_touch_pt0,      // 0x6C
    input  wire [C_S_AXI_DATA_WIDTH-1:0]        sts_touch_pt1       // 0x70
);

    //=========================================================================
    //  ★ sts_lcd_status 位定义(由 lcd_vblank_ctrl 驱动,本层只读不造)
    //=========================================================================
//    [ 0]     front          当前前台缓冲索引(0/1)
//    [ 1]     swap_pending   有 swap 请求未生效     ← 【只被 0x60 读带走】
//    [10: 2]  vblank_cnt     场消隐计数(9 位)
//    [18:11]  swap_cnt       成功换址次数(8 位)
//    [19]     underrun       行缓冲欠载(粘滞,由 PS **写 0x64** 清)
//    [20]     axi_err        AXI 读响应非 OKAY(粘滞,同样写 0x64 清)
//    ✅ 2026-10-03:上面两行的清除契约此前只存在于文档里,RTL 未实现 ⇒ axi_err 恒 1。
//       现在 `cfg_lcd_clr_sticky` 真正把它接了出来(见端口区)。**清除地址是 0x64。**
//
//  🔑 为什么 pending 放在 sts_lcd_status[1] 而不是让从机自己锁存:
//     pending 的【真值持有者是 lcd_vblank_ctrl】—— 只有它知道换址到底发生没有。
//     ★ 决策 5.3 第 1 条:LCD_SWAP 读回必须是「已生效」值,不是「已请求」值。
//       若从机自己锁一个 pending,PS 可能读到"请求已受理"而谎称"切换完成"。
//     ⇒ 单一事实源归 vblank_ctrl;从机 0x60 读只是把 [1:0] 透出来。
//     ⇒ 这也顺带解释了 bit[1] 为什么没被 vblank_cnt 占用:vblank_cnt 从 bit[2] 起。

    //=========================================================================
    //  字索引:字节地址 [7:2] → 6 位寄存器编号(0x00 ~ 0x3F)
    //    ⚠️ 2026-10-05 由 [6:2]/5 位扩到 [7:2]/6 位(决策 Q27,ADDR_WIDTH 7→8)。
    //       原 32 个槽位在 DBG0/1/2 加入后已满,DDS_FREQ 只能落在 0x80。
    //=========================================================================
    //-------------------------------------------------------------------------
    //  继电器矩阵 / 量程挡位(2026-10-10 实现)
    //    ★ 这四个索引值【原封不动】取自 report/source_design_checklist.md §3 表。
    //      0x18 尤其值得一提:2026-10-05 加 CV_CTRL 时本可以顺手占了它,
    //      但当时判断"规格是 M9 接口设计的正文,不能图方便挪用",
    //      于是 CV_CTRL 落到 0x84。**今天这次落地就是那个判断的回报** ——
    //      规格地址与 RTL 逐字对得上,PS 侧 / 报告 / 清单三处不用改。
    //-------------------------------------------------------------------------
    localparam [5:0] IDX_STATUS     = 6'h01;  // 0x04 <-- 2026-10-10 实现
    localparam [5:0] IDX_BIAS_CODE = 6'h05;  // 0x14 <-- 2026-10-05 实现
    localparam [5:0] IDX_VERSION  = 6'h12;  // 0x48
    localparam [5:0] IDX_SCRATCH  = 6'h13;  // 0x4C
    localparam [5:0] IDX_LCD_CTRL = 6'h14;  // 0x50
    localparam [5:0] IDX_LCD_FILL = 6'h15;  // 0x54

    localparam [5:0] IDX_FB_BASE0  = 6'h16;  // 0x58
    localparam [5:0] IDX_FB_BASE1  = 6'h17;  // 0x5C
    localparam [5:0] IDX_LCD_SWAP  = 6'h18;  // 0x60
    localparam [5:0] IDX_FB_STATUS = 6'h19;  // 0x64
    localparam [5:0] IDX_TC_STATUS = 6'h1A;  // 0x68
    localparam [5:0] IDX_TC_PT0    = 6'h1B;  // 0x6C
    localparam [5:0] IDX_TC_PT1    = 6'h1C;  // 0x70

    // 调试观测(2026-10-03 加)—— 当时地址窗口只有 0x00~0x7C,
    //   这三个是仅剩的空位(cv_regs.py 与 cv_sys.v 里原先标着"预留 P4 安全链",
    //   实际全工程无处使用 ⇒ 现在启用,并把那两处注释改掉)。
    localparam [5:0] IDX_DBG0      = 6'h1D;  // 0x74 本帧 rvalid 拍数
    localparam [5:0] IDX_DBG1      = 6'h1E;  // 0x78 最近 AR 地址
    localparam [5:0] IDX_DBG2      = 6'h1F;  // 0x7C 状态快照

    // DDS 激励频率(AD9833)—— 2026-10-05 加。
    //   🔑 这是扩地址空间【唯一】的动机:0x00~0x7C 已满,只能落 0x80。
    //   [27:0] = AD9833 的 FREQREG 原值,f_out = code × 25 MHz / 2^28。
    //   复位值 0x0A3D70A ⇒ 10 737 418 × 25e6 / 2^28 ≈ 1.0000 MHz ——
    //   与 adc_9833.v 上电初始化序列里本来就写死的那个值一致,
    //   此处只是让它【可被 PS 改写】,不改上电行为。
    localparam [5:0] IDX_DDS_FREQ  = 6'h20;  // 0x80

    // CV_CTRL(0x84)—— 2026-10-05 加,`dac_1220` 初始化的软件放行位。
    //   ⚠️ **为什么不放 0x18**:`0x18` 已被测量主链规格占给 `RANGE_SEL`
    //      (见 report/source_design_checklist.md §3 表 line 406)。
    //      那一位 RTL 尚未实现,但**规格是 M9「接口设计」的正文**,
    //      不能为了图方便挪用。⇒ 落到扩展区,紧邻同属模拟前端的 DDS_FREQ(0x80)。
    localparam [5:0] IDX_CV_CTRL   = 6'h21;  // 0x84

    // ADC 采集(2026-10-06 加)—— 紧接模拟前端往后排,仍属"模拟通道"这一族。
    //   ⚠️ 0x88~0x98 共 5 个槽位,地址空间还剩 0x9C~0xFC 共 25 个 ⇒ 不需要再扩宽度。
    localparam [5:0] IDX_ADC_CFG      = 6'h22;  // 0x88
    localparam [5:0] IDX_ADC_CTRL     = 6'h23;  // 0x8C
    localparam [5:0] IDX_ADC_STS      = 6'h24;  // 0x90
    localparam [5:0] IDX_ADC_BUF_IDX  = 6'h25;  // 0x94
    localparam [5:0] IDX_ADC_BUF_DATA = 6'h26;  // 0x98

    // 继电器矩阵 / 量程挡位(2026-10-10 加)—— 见上面 IDX_STATUS 那段的说明,
    //   四个索引全部取自原规格表,一个都没挪。
    localparam [5:0] IDX_RANGE_SEL  = 6'h06;  // 0x18
    localparam [5:0] IDX_RELAY_CFG  = 6'h07;  // 0x1C
    localparam [5:0] IDX_SETTLE_CFG = 6'h09;  // 0x24

    //=========================================================================
    //  写通道
    //=========================================================================
    reg        aw_hs;       // 已捕获 AW
    reg        w_hs;        // 已捕获 W
    reg        bvalid_r;

    // ⚠️ 必须把 AWADDR / WDATA 在握手当拍锁进寄存器,不能直接用输入引脚。
    //    AXI 只要求 AWADDR 在 AWVALID 为高期间稳定;AWREADY 之后主设备
    //    可以立刻改 AWADDR 去发下一笔。若译码时直接读 s_axi_awaddr,
    //    当 W 比 AW 晚到时(stub 完全允许),译出的就是【别的地址】。
    reg [C_S_AXI_ADDR_WIDTH-1:0] awaddr_r;
    reg [C_S_AXI_DATA_WIDTH-1:0] wdata_r;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            aw_hs    <= 1'b0;
            w_hs     <= 1'b0;
            awaddr_r <= {C_S_AXI_ADDR_WIDTH{1'b0}};
            wdata_r  <= {C_S_AXI_DATA_WIDTH{1'b0}};
        end else begin
            if (s_axi_awvalid && s_axi_awready) begin
                aw_hs    <= 1'b1;
                awaddr_r <= s_axi_awaddr;
            end else if (s_axi_bvalid && s_axi_bready) begin
                aw_hs    <= 1'b0;
            end

            if (s_axi_wvalid && s_axi_wready) begin
                w_hs    <= 1'b1;
                wdata_r <= s_axi_wdata;
            end else if (s_axi_bvalid && s_axi_bready) begin
                w_hs    <= 1'b0;
            end
        end
    end

    // 只要手上没有待回的 B,就同时开放 AW 与 W —— 两个通道可以任意顺序到达
    assign s_axi_awready = ~aw_hs & ~bvalid_r;
    assign s_axi_wready  = ~w_hs  & ~bvalid_r;

    // 🔑 核心不变量:两个通道【都】到齐才允许动寄存器,不是任一个
    wire do_write = aw_hs & w_hs & ~bvalid_r;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            bvalid_r <= 1'b0;
        end else if (do_write) begin
            bvalid_r <= 1'b1;
        end else if (s_axi_bvalid && s_axi_bready) begin
            bvalid_r <= 1'b0;
        end
    end

    assign s_axi_bvalid = bvalid_r;
    assign s_axi_bresp  = 2'b00;            // 恒 OKAY,stub 阶段不报错

    //=========================================================================
    //  寄存器本体
    //=========================================================================
    reg [C_S_AXI_DATA_WIDTH-1:0] scratch;
    reg [C_S_AXI_DATA_WIDTH-1:0] lcd_ctrl;
    reg [C_S_AXI_DATA_WIDTH-1:0] lcd_fill;
    reg [C_S_AXI_DATA_WIDTH-1:0] fb_base0;
    reg [C_S_AXI_DATA_WIDTH-1:0] fb_base1;
    reg [C_S_AXI_DATA_WIDTH-1:0] bias_code;   // 0x14  2026-10-05 加
    reg [C_S_AXI_DATA_WIDTH-1:0] cv_ctrl;     // 0x84  2026-10-05 加(bit0 = dac_rst_n)
    reg [C_S_AXI_DATA_WIDTH-1:0] dds_freq;    // 0x80  2026-10-05 加
    reg [C_S_AXI_DATA_WIDTH-1:0] adc_cfg;     // 0x88  2026-10-06 加
    reg [C_S_AXI_DATA_WIDTH-1:0] adc_buf_idx; // 0x94  2026-10-06 加
    reg [C_S_AXI_DATA_WIDTH-1:0] range_sel;   // 0x18  2026-10-10 加(挡位请求)
    reg [C_S_AXI_DATA_WIDTH-1:0] relay_cfg;   // 0x1C  2026-10-10 加(组合请求)
    reg [C_S_AXI_DATA_WIDTH-1:0] settle_cfg;  // 0x24  2026-10-10 加(稳定延时)
    // ⚠️ 这三个寄存器是【请求】的最后一站,不是真值。
    //    真值在 relay_ctrl 里(它可能拒收),所以 0x18/0x1C 的【读】不走这里,
    //    走 sts_gear_applied / sts_relay_applied。
    //    ⇒ 本层的 range_sel / relay_cfg 只用两个地方:① 喂给 relay_ctrl;
    //      ② 本文件的复位值。**没有第三条读路径** —— 这是刻意的,
    //      多一条读路径就多一处"两份真值"的机会。

    // ⚠️ 2026-10-01 由 if-else 链改为 case —— 与读通道写法一致,便于继续扩表。
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            scratch  <= 32'h0000_0000;
            lcd_ctrl <= 32'h0000_0000;      // ⚠️ 复位 = 0 = 彩条,【有意为之】——
            lcd_fill <= 32'h0000_0000;      //    上电默认走厂商彩条那条路径。
            fb_base0 <= 32'h0000_0000;      //    复位 = 0 也是【有意为之】:
            fb_base1 <= 32'h0000_0000;      //    src_sel 不为 2 时基址根本不被使用,
            //-------------------------------------------------------------------------
            // 🔴 偏压复位值【不是 0】—— 这是安全设计,不是笔误,改之前先读完本段。
            //
            //   DAC1220 模块层的实测传递关系(docs/dac1220_interface.md §八,
            //   板上三点实测 −5/0/+5 V)是:
            //       V_module = −10 + 20 × code / 1048575
            //   ⇒ code = 0 ⇒ **−10 V**。经外部 10:1 高压级 ⇒ 偏压 **−100 V**。
            //   ⇒ 若复位值取 0,上电后偏压会停在 −100 V,而不是人们以为的 0 V。
            //
            //   0x80000 / 0xFFFFF = 1/2 满量程 ⇒ V_module = 0 V ⇒ 偏压 0 V。
            //   上电停在 0 V 是无条件安全的中点(不依赖高压级是否支持负压)。
            //
            //   ⚠️ 配套:复位值只有配上【上电自动发一次 bias_set】才真的到达 DAC,
            //      否则寄存器显示 0 V 而 DAC 仍停在它自己的默认码上 ——
            //      那就是「寄存器说谎」,违反本项目对 0x60 立下的同一条规矩
            //      (读回必须是【已生效】值,不是【已请求】值)。见下面 §set 脉冲。
            //-------------------------------------------------------------------------
            bias_code <= 32'h0008_0000;     // 中位码 = 0 V 偏压
            // DDS 复位值 = 1.0000 MHz。⚠️ 这一个是【真值】不是意图:
            //   adc_9833.v 的上电初始化序列本来就写死 0x0A3D70A,
            //   此处只是让寄存器与硬件实际状态对得上,并给 PS 一个改写入口。
            dds_freq  <= 32'h0A3D_70A;      // 10 737 418 × 25 MHz / 2^28 ≈ 1 MHz
            // 🔴 CV_CTRL 复位 = 0 ⇒ **dac_1220 上电就被按在复位态,不跑初始化**。
            //    放行权交给软件(必须在 apply_fclk0() 之后)——
            //    理由见端口区 CV_CTRL 那段。**这不是笔误,改之前先读完那段。**
            //    ⚠️ 连带:上面 bias_code 的上电脉冲要等 dac_init_done,
            //       未放行时它恒 0 ⇒ 脉冲不发,寄存器 0 V 与 DAC 状态不会互相说谎。
            cv_ctrl   <= 32'h0000_0000;     // bit0 clk_release = 0 = 保持复位
            //-----------------------------------------------------------------
            // ADC_CFG(0x88)复位值 = 分频比 2 → 130 MHz / 2 = **65 MHz**
            //   🔴 2026-10-08 由 13(=10 MHz)改为 2。理由:采样率**统一固定
            //      65 MSPS 不再可调**,复位值必须是这一档 —— 否则"界面显示 65、
            //      硬件复位回 10"就是个不一致组合(正是要消灭的那类)。
            //      ⛔ 原文"默认给 10 MHz 因为杜邦线接不出 65 MHz"**已作废**:
            //        用户换 2×9 排线后 P=2 实测可用(见 adc_capture.v 文件头 ②)。
            //   其余位全 0:
            //     PH_OFF = 0 → 采在 adc_clk 下降沿(tick 0,见 adc_capture.v)
            //       ⚠️ P=2 的合法刻度只有 {−1, 0} 两个(半个周期一格,无细调余量),
            //          0 实测即干净正弦(p-p=42,250 点 3.85 个周期,2026-10-07)。
            //          c_ui 侧 `cv_adc_phase_center(2)` 折回到 −1,两者都在窗内。
            //     CONT   = 0 → 单次模式,等 PS 写 GO 才采
            //     OEB_N  = 0 → ADC 输出**使能**(与已删除的 adc_9833/ad9238 老代码一致)
            //     PDN    = 0 → 不掉电
            //-----------------------------------------------------------------
            adc_cfg     <= 32'h0000_0002;   // [7:0]=2,其余 0
            adc_buf_idx <= 32'h0000_0000;
            //-----------------------------------------------------------------
            // 继电器 / 挡位 / 稳定延时(2026-10-10)
            //
            //   🔴 复位值【必须与 relay_ctrl.v 里的 localparam 一致】——
            //      从机的寄存器是 relay_ctrl 的【输入】,relay_ctrl 内部的
            //      relay_reg / gear_reg 才是引脚真值。两者若不一致:
            //      上电后 PS 读 0x1C 拿到的是 sts_relay_applied(relay_ctrl 的值),
            //      而 relay_ctrl 的输入却是另一个值 —— 直到 PS 第一次写它才同步。
            //      ⇒ 症状是"上电读到的和硬件实际跑的对得上,但和本文件写的对不上",
            //        极难查。**改一处必须改两处。**
            //      ⚠️ 更好的做法是把这三个初值作为 parameter 从 relay_ctrl 传上来,
            //         但那要多两个 module_ref 参数 + 刷一次 BD 边界。
            //         当前只有 3 个数,重复风险可控 —— 但**记在这里**:
            //         若将来这些初值开始动,就去改成 parameter,别再靠自觉。
            //-----------------------------------------------------------------
            range_sel  <= 32'h0000_0002;   // = B 档 100 Ω(与前序默认一致)
            relay_cfg  <= 32'h0000_0010;   // = ALL_OFF(K5 闭),前序的"安全状态"
            // 0x24 SETTLE_CFG:[31:16] = 40 ⇒ 40 ms,逐字等于前序
            //   RELAY_CONTROLLER_SWITCH_SETTLE_MS = 40。
            //   ★ 单位是 **ms 不是周期数**,理由见 relay_ctrl.v §稳定延时
            //     (40 ms @ 50 MHz = 2e6 拍,16 位放不下)。
            //   [15:0] = 1500 是**偏压建立延时**,目前**无人消费** ——
            //     规格里这个字段是给"改偏压后等模拟建立"用的,而当前
            //     PS 侧的偏压建立是自己在 Python 里 sleep 的。
            //     ⚠️ 留一个非零的默认值而不是 0,是为了让"这个字段还没接"
            //        能被人看出来(读回 0 会让人以为"延时确实是 0",那是在说谎)。
            settle_cfg <= 32'h0028_05DC;   // [31:16]=40ms, [15:0]=1500(未消费)
        end else if (do_write) begin        //    PS 必须先写基址再切源,顺序错就是黑屏。
            case (awaddr_r[7:2])
                IDX_SCRATCH  : scratch  <= wdata_r;   // 未实现 WSTRB:整字写
                IDX_BIAS_CODE: bias_code <= wdata_r;
                // ⚠️ 这里【掩码到 bit0】,与其它寄存器"整字写"的写法【刻意不同】:
                //    0x84 是控制寄存器,只有 [0] 有效。若整字存,软件写 0xFFFFFFFF
                //    会读回 0xFFFFFFFF —— [31:1] 是谎报。读回必须是已生效值。
                IDX_CV_CTRL  : cv_ctrl   <= {31'b0, wdata_r[0]};   // 0x84,只取 bit0
                IDX_DDS_FREQ : dds_freq  <= wdata_r;
                // ⚠️ 下面两个【掩码到有效位】,理由同 0x84:读回必须是已生效值。
                //    ADC_CFG 有效到 [18](DIV/PH_OFF/CONT/OEB_N/PDN),掩到 19 位;
                //    ADC_BUF_IDX 有效到 [12](8192 深),掩到 13 位。
                //    上面那些整字存的寄存器是"留给将来的位还能读回"的历史约定,
                //    控制寄存器不适用。
                IDX_ADC_CFG     : adc_cfg     <= wdata_r & 32'h0007_FFFF;
                // ⚠️ 掩码 = 缓冲深度 −1。2026-10-08 BUF_DEPTH 4096→8192 ⇒ 掩码
                //    12 位→13 位。**这一行是本改动真正的闸**:忘记放宽的话,
                //    写 5000 会被静默截成 904,读回的是**另一个点的数据**,
                //    不报错、不复位、不复位 done —— 与"按钮守卫停在旧档位"同形。
                IDX_ADC_BUF_IDX : adc_buf_idx <= wdata_r & 32'h0000_1FFF;
                //-------------------------------------------------------------
                // 0x18 / 0x1C / 0x24(2026-10-10)
                //
                // 🔴 这三个【刻意不掩码】,与上面 ADC_CFG / ADC_BUF_IDX 的
                //    "掩到有效位"是**相反**的处置。理由是掩码在这里**有害**:
                //      PS 写 0x1C = 0x0000_0180,若掩到 [6:0] 就变成 0x00,
                //      而 0x00 是白名单里的**合法**组合(COSS_DISCHARGE)
                //      ⇒ 一个本该被拒收的越界写,被静默转成了另一个合法动作。
                //      这与 ADC_BUF_IDX 掩码(截断成另一个地址)是同一类错误,
                //      但后果更重:那边是读错数据,这边是**动了继电器**。
                //    ⇒ 原样存,让 relay_ctrl 看到 bit7 = 1 并**拒收**。
                //      (relay_ctrl.v §位宽 已写明:bit7 无对应引脚 ⇒ 必然非法。)
                //  ⚠️ 同理 0x18 也不掩:4 位全 0 与两位同时为 1 都必须能被
                //     relay_ctrl 看见并拒收,不能被掩成别的合法组合。
                //  ⚠️ 0x24 本来就没有"越界"概念(整个 32 位都有意义),
                //     原样存即可。
                //  ⚠️ 高位的垃圾值【不会泄漏到 PS】:0x18/0x1C 的读走
                //     sts_*_applied,不读这三个寄存器。0x24 读回原值 ——
                //     那正是它该有的语义(参数,不是状态)。
                //-------------------------------------------------------------
                IDX_RANGE_SEL  : range_sel  <= wdata_r;   // 0x18 挡位请求
                IDX_RELAY_CFG  : relay_cfg  <= wdata_r;   // 0x1C 组合请求
                IDX_SETTLE_CFG : settle_cfg <= wdata_r;   // 0x24 稳定延时参数
                // ⚠️ 这里同样【没有】IDX_ADC_CTRL —— 0x8C 是命令,见下面的脉冲块。
                IDX_LCD_CTRL : lcd_ctrl <= wdata_r;
                IDX_LCD_FILL : lcd_fill <= wdata_r;
                IDX_FB_BASE0 : fb_base0 <= wdata_r;
                IDX_FB_BASE1 : fb_base1 <= wdata_r;
                default      : ;                      // 未实现 → 静默忽略
                // 🔑 注意这里【没有】IDX_LCD_SWAP —— 0x60 是【命令】不是【存储】,
                //    它必须变成脉冲,不能进这个纯电平寄存器块。见下。
                //    其余 0x64/0x68/0x6C/0x70 是 RO,写进来同样落 default 静默忽略,
                //    与既有约定一致(见文件尾设计备忘)。
            endcase
        end
    end

    //=========================================================================
    //  LCD_SWAP(0x60)写 —— 命令语义,转成 1 拍脉冲 + 电平目标索引
    //    ⚠️ 单独一个 always 块:因为需要「默认清 0」才能产生脉冲,
    //       与上面那个纯寄存器块的复位/保持语义不同,不能合并。
    //=========================================================================
    reg lcd_swap_req_r;
    reg lcd_swap_tgt_r;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            lcd_swap_req_r <= 1'b0;
            lcd_swap_tgt_r <= 1'b0;
        end else begin
            lcd_swap_req_r <= 1'b0;                  // 默认清 ⇒ 只在写当拍为高
            if (do_write && (awaddr_r[7:2] == IDX_LCD_SWAP)) begin
                lcd_swap_req_r <= 1'b1;
                lcd_swap_tgt_r <= wdata_r[0];        // 目标缓冲索引
            end
        end
    end

    //=========================================================================
    //  FB_STATUS(0x64)写 —— 清粘滞错误(命令语义,1 拍脉冲)
    //    ⚠️ 与上面的寄存器块【不能合并】:那个块是"电平保持",这个要"默认清 0"。
    //    与 0x60 的写法同构 —— 同一种命令语义,同一个套路,便于对照检查。
    //=========================================================================
    reg clr_sticky_r;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            clr_sticky_r <= 1'b0;
        end else begin
            clr_sticky_r <= 1'b0;                    // 默认清 ⇒ 只在写当拍为高
            if (do_write && (awaddr_r[7:2] == IDX_FB_STATUS)) begin
                clr_sticky_r <= 1'b1;                // 写入值无关:写什么都清
            end
        end
    end

    //=========================================================================
    //  ADC_CTRL(0x8C)写 —— 命令语义,1 拍脉冲(2026-10-06 加)
    //    ⚠️ 与 0x60 / 0x64 同构:第三个"命令寄存器",照同一个套路写。
    //    ⚠️ 本寄存器的【读回是 0】,而且是【有意为之】:
    //       GO 是一次性事件,做成电平就会出现"PS 连写两次 1、第二次采不到沿";
    //       而锁存"上次写过"又会让读回值在事件早已过去后仍显示 1 —— 那是说谎。
    //       命令寄存器的正确读回语义是"没有待处理的命令",即恒 0。
    //       (与 0x60 的处理不同:0x60 读回的是**消费者持有的已生效状态**,
    //        因为 vblank_ctrl 那边确实有"换址还没发生"这个待处理态。)
    //=========================================================================
    reg adc_go_r;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            adc_go_r <= 1'b0;
        end else begin
            adc_go_r <= 1'b0;                        // 默认清 ⇒ 只在写当拍为高
            if (do_write && (awaddr_r[7:2] == IDX_ADC_CTRL)) begin
                adc_go_r <= wdata_r[0];              // 只有 bit0 是 GO,写 0 不发脉冲
            end
        end
    end

    //=========================================================================
    //  RANGE_SEL(0x18)/ RELAY_CFG(0x1C)写 —— 命令语义,1 拍脉冲
    //    (2026-10-10 加,第四个/第五个"命令寄存器",照 0x60/0x64/0x8C 同一个套路)
    //
    //  ★ 为什么【必须】是脉冲,而不是像它俩的邻居那样只出电平:
    //    「写了一次 0x1C」是一个**事件**(要触发一次稳定计时),不是一个**状态**。
    //    只给电平的话 relay_ctrl 得自己采沿,还会踩到"PS 重试写同一个组合、
    //    电平不变 ⇒ 采不到沿 ⇒ 请求丢失"这个坑 —— 与 0x60 同一条理由。
    //
    //  ⚠️ 与 0x8C ADC_CTRL 有一点【不同】:那边的脉冲要 `wdata_r[0]`,
    //     写 0 不发脉冲;这里**写什么值都发脉冲**(包括写 0x00 = 合法组合)。
    //     理由:0x00 是白名单里的合法组合(COSS_DISCHARGE),是一个真动作;
    //     而 GO 的 bit0 写 0 就是"不下命令",没有对应的物理动作。
    //     ⚠️ 因此这两个脉冲【无条件取 wdata_r 全部位】送往 relay_ctrl,
    //        由它做合法性判断 —— 本层不预先筛,否则 err_code 永远置不起来。
    //=========================================================================
    reg range_set_r;
    reg relay_set_r;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            range_set_r <= 1'b0;
            relay_set_r <= 1'b0;
        end else begin
            range_set_r <= 1'b0;                     // 默认清 ⇒ 只在写当拍为高
            relay_set_r <= 1'b0;
            if (do_write && (awaddr_r[7:2] == IDX_RANGE_SEL)) range_set_r <= 1'b1;
            if (do_write && (awaddr_r[7:2] == IDX_RELAY_CFG)) relay_set_r <= 1'b1;
        end
    end

    //=========================================================================
    //  BIAS_CODE(0x14)/ DDS_FREQ(0x80)写 —— 命令语义,1 拍脉冲
    //
    //  【为什么不能只靠上面的电平寄存器块】下游 dac_1220 / adc_9833 的
    //    set_in 是**沿触发**的。若只给电平,PS 连写两次同一个码值时电平不变
    //    ⇒ 采不到沿 ⇒ 第二次请求丢失。这与 0x60 LCD_SWAP 需要
    //    cfg_lcd_swap_req 脉冲是**同一条理由**(见文件尾设计备忘)。
    //
    //  ★ 【上电自动发一次 bias_set】—— 配套上面 bias_code 的复位值。
    //    🔴 必须等 dac_init_done 才发(2026-10-05 逐状态核对后修正,原实现是空放)。
    //       理由:dac_1220.v 把 set_in 的上升沿锁进 set_pending,但**那一句写在
    //       NORMAL_WAIT 分支【内部】**,不在 case 语句之后 ——
    //       所以只有 FSM 正处于 NORMAL_WAIT 时到来的沿才被记住,
    //       IDLE / DELAY / INIT / CAL / CAL_WAIT 期间来的沿【直接丢掉】。
    //       而该模块上电走 IDLE →(PWR_WAIT 200 ms)→ INIT → CAL →
    //       CAL_WAIT → NORMAL_WAIT,init_done 在 CAL_WAIT 出口才拉高
    //       ⇒ 复位刚放手就发脉冲 = 空放,寄存器会显示 0 V 而 DAC 停在默认码上。
    //    ⚠️ 不能按 adc_9833.v 类推:它的同一句写在 case **之后**(always 块顶层),
    //       任何状态都收 ⇒ 两个模块的 set_in 语义【不同】。
    //    ⚠️ 若将来删掉这段,必须同时把 bias_code 复位值改回 0 ——
    //       否则寄存器显示 0 V、DAC 却停在别的码上,即「寄存器说谎」。
    //    ⚠️ DDS 侧【不发】上电脉冲:adc_9833 的初始化序列已经把频率置成 1 MHz,
    //       (init 两帧 0x570A/0x428F 反推 = 0x0A3D70A,与本寄存器复位值逐位相同)
    //       再发一次只是重复写同一个值,没有信息量。
    //=========================================================================
    reg bias_set_r;
    reg dds_set_r;
    reg bias_pwr_done;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            bias_set_r    <= 1'b0;
            dds_set_r     <= 1'b0;
            bias_pwr_done <= 1'b0;
        end else begin
            bias_set_r <= 1'b0;                      // 默认清 ⇒ 只在下面两种情形为高
            dds_set_r  <= 1'b0;

            if (dac_init_done && !bias_pwr_done) begin
                bias_set_r    <= 1'b1;               // init_done 抬起后那一次
                bias_pwr_done <= 1'b1;
            end
            // ⚠️ 下面两行写在上电脉冲【之后】= 同拍优先级更高,
            //    与 adc_9833.v 里 set_pending 的写法是同一个套路。
            if (do_write && (awaddr_r[7:2] == IDX_BIAS_CODE)) bias_set_r <= 1'b1;
            if (do_write && (awaddr_r[7:2] == IDX_DDS_FREQ )) dds_set_r  <= 1'b1;
        end
    end

    // R2 规格的 cfg_* 输出(寄存器原值直接引出,位域 mask 交给消费侧做)
    assign cfg_lcd_ctrl      = lcd_ctrl;
    assign cfg_lcd_clr_sticky = clr_sticky_r;
    assign cfg_lcd_fill      = lcd_fill;
    assign cfg_lcd_fb_base0  = fb_base0;
    assign cfg_lcd_fb_base1  = fb_base1;
    assign cfg_lcd_swap_tgt  = lcd_swap_tgt_r;
    assign cfg_lcd_swap_req  = lcd_swap_req_r;

    // 模拟前端(2026-10-05)
    assign cfg_bias_code = bias_code;
    assign cfg_bias_set  = bias_set_r;
    assign cfg_dds_freq  = dds_freq;
    assign cfg_dds_set   = dds_set_r;

    // 0x84 CV_CTRL —— 只引出 bit0。⚠️ 它是**电平**不是脉冲:
    //   与上面两个 set 不同,它表示"保持放行",不是"触发一次"。
    //   2026-10-06 改名/扩义为 cfg_clk_release,消费者见端口区。
    assign cfg_clk_release = cv_ctrl[0];

    // ADC 采集(2026-10-06)
    assign cfg_adc_cfg     = adc_cfg;
    assign cfg_adc_go      = adc_go_r;
    assign cfg_adc_buf_idx = adc_buf_idx;

    // 继电器 / 挡位 / 稳定延时(2026-10-10)
    //   ⚠️ 这三个是**寄存器原值直出**(位域 mask 交给消费侧做)——
    //      与上面 cfg_adc_cfg 同一条约定。消费侧 cv_sys 在例化处切位宽:
    //         .cfg_relay (cfg_relay_cfg[7:0])   ← 保留 bit7,好让非法值能被拒
    //         .cfg_gear  (cfg_range_sel[3:0])
    //         .cfg_settle_ms (cfg_settle_cfg[31:16])
    assign cfg_range_sel  = range_sel;
    assign cfg_range_set  = range_set_r;
    assign cfg_relay_cfg  = relay_cfg;
    assign cfg_relay_set  = relay_set_r;
    assign cfg_settle_cfg = settle_cfg;

    //=========================================================================
    //  读通道
    //=========================================================================
    reg                         rvalid_r;
    reg [C_S_AXI_DATA_WIDTH-1:0] rdata_r;

    //-------------------------------------------------------------------------
    //  触摸快照锁存(2026-10-02 加,与 gt911_ctrl 上板同批)
    //
    //  【为什么需要】PS 要读满 3 个寄存器(0x68 状态 / 0x6C 点1 / 0x70 点2)
    //    才能拼出一次完整的触摸事件,而 gt911_ctrl 是**自由运行**的 ——
    //    它不管 AXI 读没读,触摸芯片一来新帧就刷新上报字。
    //    三次独立 AXI 读之间若正好跨过一帧边界,就会读到
    //    "状态说 1 个点、坐标却已经是下一帧" 这类**撕裂**。
    //    ⚠️ 撕裂**不报错**,只让界面偶尔跳一下 —— 最难查的那一类 bug。
    //
    //  【做法】把"读 0x68"当成本次事件的**提交点**:在那一拍的 AR 握手上,
    //    把 pt0/pt1 一并锁进 hold;之后读 0x6C/0x70 一律给 hold
    //    ⇒ 同一轮 3 次读看到的是**同一帧**的快照。
    //
    //  【前提 / 代价】PS **必须先读 0x68,再读坐标**;反序会拿到上一次快照
    //    (复位后为 0)。此前提已写进寄存器表 §3 与本处注释。
    //    只覆盖这 3 个寄存器 —— 其余寄存器单次即完整,不需要锁。
    //
    //  【时序】hold 与 rdata_r 同拍锁存,AXI 单笔未完成前 arready 不放开
    //    ⇒ 读 0x6C 最早也晚 2 拍,看到的必然是刚锁好的那份。
    //-------------------------------------------------------------------------
    reg [C_S_AXI_DATA_WIDTH-1:0] tch_pt0_hold;
    reg [C_S_AXI_DATA_WIDTH-1:0] tch_pt1_hold;

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            tch_pt0_hold <= 32'h0000_0000;
            tch_pt1_hold <= 32'h0000_0000;
        end else if (s_axi_arvalid && s_axi_arready
                     && (s_axi_araddr[7:2] == IDX_TC_STATUS)) begin
            tch_pt0_hold <= sts_touch_pt0;
            tch_pt1_hold <= sts_touch_pt1;
        end
    end

    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            rvalid_r <= 1'b0;
        end else if (s_axi_arvalid && s_axi_arready) begin
            rvalid_r <= 1'b1;
        end else if (s_axi_rvalid && s_axi_rready) begin
            rvalid_r <= 1'b0;
        end
    end

    // 在 AR 握手当拍采样地址(rvalid_r 与 rdata_r 同拍锁存,一拍后一起生效)
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            rdata_r <= 32'h0000_0000;
        end else if (s_axi_arvalid && s_axi_arready) begin
            case (s_axi_araddr[7:2])
                IDX_VERSION  : rdata_r <= VERSION_CONST;
                IDX_SCRATCH  : rdata_r <= scratch;
                // 0x04 STATUS(2026-10-10)—— 整字由 cv_sys 从 relay_ctrl 打包,
                //   本层不解释位含义(见端口区那句"布局定义在 relay_ctrl.v")。
                IDX_STATUS   : rdata_r <= sts_status;
                // 0x18 / 0x1C(2026-10-10)—— ★★★ 读【已生效】值,不是【已请求】值。
                //   ⚠️ 这里是整个 0x18/0x1C 实现里【最容易写错的一行】:
                //      直觉写法是 `rdata_r <= relay_cfg;`(本层锁存的请求值),
                //      那样写的话,PS 写一个被互锁拒收的坏组合之后**读回来是坏值**,
                //      而硬件其实一动不动 ⇒ PS 会以为设置成功了。
                //      ⇒ 必须回送 sts_*_applied(relay_ctrl 持有的真值)。
                //   ★ 与下面 0x60 取 sts_lcd_status[1:0] 是**同一条原则**
                //     (文件尾 §LCD_SWAP 读为什么…):单一事实源归消费者。
                //   ⚠️ 越界位由 relay_ctrl 保证不会出现在 applied 里
                //      (它只可能装着白名单值),所以这里不用再掩。
                IDX_RANGE_SEL: rdata_r <= {28'h0, sts_gear_applied};
                IDX_RELAY_CFG: rdata_r <= {24'h0, sts_relay_applied};
                // 0x24 读原值 —— 它是参数不是状态,没有"已生效/已请求"之分。
                IDX_SETTLE_CFG: rdata_r <= settle_cfg;
                IDX_BIAS_CODE: rdata_r <= bias_code;   // 0x14 2026-10-05
                IDX_CV_CTRL  : rdata_r <= cv_ctrl;     // 0x84 2026-10-05 (bit0 = dac_rst_n)
                IDX_DDS_FREQ : rdata_r <= dds_freq;    // 0x80 2026-10-05
                // ADC 采集(2026-10-06)
                IDX_ADC_CFG     : rdata_r <= adc_cfg;
                IDX_ADC_STS     : rdata_r <= sts_adc_sts;
                IDX_ADC_BUF_IDX : rdata_r <= adc_buf_idx;
                IDX_ADC_BUF_DATA: rdata_r <= sts_adc_buf_data;
                // 0x8C 落 default ⇒ 读 0。**这是有意为之**,理由见上面的脉冲块。
                IDX_LCD_CTRL : rdata_r <= lcd_ctrl;
                IDX_LCD_FILL : rdata_r <= lcd_fill;
                IDX_FB_BASE0 : rdata_r <= fb_base0;
                IDX_FB_BASE1 : rdata_r <= fb_base1;

                // 0x60 读:★【已生效】值,不是【已请求】值 —— 决策 5.3 第 1 条。
                //   [1:0] 直接取自 sts_lcd_status,不锁本层的 lcd_swap_req_r。
                //   理由见文件尾部设计备忘。
                IDX_LCD_SWAP : rdata_r <= {30'b0, sts_lcd_status[1], sts_lcd_status[0]};

                IDX_FB_STATUS: rdata_r <= sts_lcd_status;
                // 0x68 读:★ 本次事件的**提交点** —— 同时也是坐标快照的锁存拍
                IDX_TC_STATUS: rdata_r <= sts_touch_status;
                // 坐标读**给快照**而非直连上报字,防止跨帧撕裂(见上文备忘)
                IDX_TC_PT0   : rdata_r <= tch_pt0_hold;
                IDX_TC_PT1   : rdata_r <= tch_pt1_hold;

                // 0x74/0x78/0x7C 调试观测(2026-10-03 加)—— 直连,不锁存。
                //   dbg_state 的脉冲类位已由 lcd_fb_reader 在 **frame_start 清零**
                //   ⇒ 读到的永远是"本帧内是否发生过",不必抢拍。
                IDX_DBG0     : rdata_r <= sts_dbg_beats;
                IDX_DBG1     : rdata_r <= sts_dbg_araddr;
                IDX_DBG2     : rdata_r <= sts_dbg_state;

                default      : rdata_r <= 32'h0000_0000;   // 未实现 → 读 0
            endcase
        end
    end

    assign s_axi_arready = ~rvalid_r;
    assign s_axi_rvalid  = rvalid_r;
    assign s_axi_rdata   = rdata_r;
    assign s_axi_rresp   = 2'b00;           // 恒 OKAY

    //=========================================================================
    //  设计决策备忘(扩表前请先读)
    //
    //  · RO 寄存器被写 → 静默忽略(上面的 case 里根本不列它)
    //  · 未实现地址   → 读返回 32'h0,写静默忽略(不返回 SLVERR)
    //  · WSTRB 未实现 —— 已知限制,不是 bug
    //
    //  【扩成完整 20 寄存器时要补的硬性约束,见设计清单 §3】
    //   ① NCO_FREQ(0x08)/ CIC_DECIM(0x20) 的写入必须在测量间隙生效:
    //      测量中写入 → 挂起到下一次 SET_BIAS 才生效,并置 pending 标志。
    //      ⚠️ **仍待实现** —— 它们的消费者(nco / CIC)目前还不存在。
    //   ② RELAY_CFG(0x1C) 写入要过互锁校验,非法组合拒绝写入并置
    //      err_code(STATUS[15:8]),而不是执行。
    //      ✅ **2026-10-10 已实现** —— 落在 relay_ctrl.v,不在本层。
    //         本层只做「出请求 + 回送已生效值」,这是刻意的分层:
    //         互锁判据(白名单)属于被控对象的知识,不属于总线接口。
    //      ⚠️ 连带:0x04 STATUS 也因此实现了 —— err_code 要有地方放。
    //         位定义 [0]busy [1]done [2]fault [15:8]err_code,见 relay_ctrl.v。
    //
    //  【cfg_ / sts_ 端口】R2 规格是 S_AXI ↔ cfg_* / sts_*。
    //    2026-10-01 起有 cfg_lcd_ctrl / cfg_lcd_fill;
    //    2026-10-02 起再加 cfg_lcd_fb_base0/1、cfg_lcd_swap_tgt/req、
    //    sts_lcd_status、sts_touch_status/pt0/pt1 —— 共 11 个寄存器对上号。
    //    2026-10-05 起再加 cfg_bias_code/set、cfg_dds_freq/set(模拟前端),
    //    并把 C_S_AXI_ADDR_WIDTH 由 7 扩到 8(见文件头寄存器映射的说明)。
    //    2026-10-06 起再加 cfg_clk_release、cfg_adc_*、sts_adc_*(ADC 采集)。
    //    2026-10-10 起再加 cfg_range_sel/set、cfg_relay_cfg/set、cfg_settle_cfg、
    //    sts_gear_applied、sts_relay_applied、sts_status(继电器/挡位)。
    //    扩表时按寄存器逐个加即可,地址不变。
    //
    //  【为什么要 cfg_lcd_swap_tgt 与 cfg_lcd_swap_req 两个信号】
    //    消费侧(lcd_vblank_ctrl)需要的是「来了一个换址请求」这个【事件】,
    //    不是「0x60 当前值多少」这个【电平】。若只给电平,消费侧自己再采沿,
    //    就会引入一个本层已经知道答案的判断,还得处理"PS 连写两次同一个值"
    //    (电平不变 ⇒ 采不到沿 ⇒ 请求丢失)。本层直接给脉冲,这个坑不存在。
    //
    //  【LCD_SWAP 读为什么取 sts_lcd_status[1:0] 而不是本地锁存】(决策 5.3 第 1 条)
    //    pending 的真值持有者是 lcd_vblank_ctrl —— 只有它知道换址发生过没有。
    //    从机若自己锁一个 pending 并自行清除,就出现【两个事实源】:
    //    PS 读到 pending=0 只说明"从机认了",不说明"扫描真的换过地址了"。
    //    ⇒ 从机只发请求,状态一律从 sts_* 透传。
    //    ★ 这条与「RELAY_REQUEST / RELAY_APPLIED 必须分开」是同一条原则。
    //      ✅ 2026-10-10 **已照抄落地**:0x18 / 0x1C 读 sts_*_applied,
    //         本层的 range_sel / relay_cfg 只喂给 relay_ctrl、不给读通路。
    //
    //  ⚠️ 【未决】TOUCH_STATUS[15:8] 的 seq 语义在 display_subsystem.md §5.2 里
    //      写的是「读一次递增 seq」。但 seq 又是 gt911_ctrl 产出的状态字的一部分,
    //      "读一次递增"与"控制器持有"两种解释会给出不同的实现(前者是从机的
    //      读副作用,后者是控制器的帧计数)。**本层现在按【原样透传】实现**,
    //      不加读副作用 —— 因为 gt911_ctrl 一行未写,现在定死没有依据。
    //      ⇒ 待 gt911_ctrl 落地时一并定论;届时若确需"读一次递增",
    //        必须在【同一次 AR 握手内】把(status, pt0, pt1)三字打成快照,
    //        否则 PS 分三次读会读到撕裂的 x/y(旧 x 配新 y)。
    //        这才是 seq 真正要解决的问题,记在这里免得忘。
    //=========================================================================

endmodule
