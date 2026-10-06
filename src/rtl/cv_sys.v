`timescale 1ns / 1ps
//=============================================================================
//  cv_sys.v  ——  PL 侧顶层容器(设计清单 R1)
//
//  【职责】只做「例化 + 引线」。不含任何逻辑,不含任何寄存器。
//
//  【为什么要这一层】
//    PS 和 LCD 必须在【同一个顶层】里才能连起来。现状是两个世界:
//      · cv_axi_lite_slave 活在 BD 生成的 system_wrapper 里
//      · lcd_driver/lcd_display 活在手写顶层 qrs.v 里
//    一个工程只有一个顶层,二者互斥。
//    把两边一起包进 cv_sys,BD 里只挂 cv_sys 这一个 module_ref 即可。
//
//  【v1 —— 纯搬运,零新逻辑】(2026-10-01 上午)
//    · cv_axi_lite_slave        原样例化,一行未改
//    · lcd_driver / lcd_display 原样例化,一行未改(搬自 qrs.v)
//    · 两者之间【刻意没有任何信号连接】—— 只证明「进了同一个顶层,谁也没坏」
//    ✅ 已达成:上板彩条照旧 且 SCRATCH 回环通过
//
//  【v2 —— 接上 PS 控制】(2026-10-01 下午,当前版本)
//    · slave 新增 0x50 LCD_CTRL / 0x54 LCD_FILL,并引出 cfg_lcd_ctrl / cfg_lcd_fill
//    · cv_sys 内加一级像素源 mux:厂商彩条 ↔ 整屏填充
//    · lcd_display / lcd_driver 【仍然一行未改】
//    ✅ 验收判据:PS 写 0x54 换色、写 0x50 切源 → 整屏变色;
//                复位默认 cfg_lcd_ctrl = 0 → 上电仍是彩条,v1 那次回归自动保留
//
//  【时钟域】
//    clk_axi = PS FCLK_CLK0 = 50 MHz,同时直接用作 LCD 的 PCLK。
//    两域同频 ⇒ 不需要 cdc_sync、不需要额外 MMCM、不需要双口 BRAM 的独立写时钟。
//    (D22 附带说明:clk_axi 定为 50 MHz 后,clk_lcd 就是 clk_axi —— 这是净简化)
//
//  【复位】
//    qrs.v 里用的是 rst_n & locked;BD 里 s_axi_aresetn 来自 rst_ps7_0_50M,
//    该 IP 已经把 locked 与同步释放处理掉了,所以这里直接用 s_axi_aresetn。
//
//  【v3 —— 换掉厂商时序件,补显示寄存器端口】(2026-10-02,当前版本)
//    · 新增 lcd_timing.v 取代厂商 lcd_driver 的【时序】角色
//        — 输出级(lcd_rgb/de/clk/bl/rst)搬到本文件,五行 assign
//        — ★ 修掉厂商 pixel_ypos 的 off-by-one(y ∈ 1..600 → 0..599),
//          画彩条看不出来,但帧缓存按行取数会整体错一行(详见 lcd_timing.v 头部)
//        — HS/VS 默认仍恒高,保持已验证配置(要开脉冲见 lcd_timing.v 参数说明)
//        — 新增 line_start / frame_start / vblank 三个节拍输出,本版【还没人用】,
//          是给 Chunk B 的 lcd_fb_reader / lcd_vblank_ctrl 预留的
//    · lcd_display(厂商彩条)【保留且一行未改】—— 它是 src_sel=0 的测试图源,
//      也是"换掉时序件后屏没坏"的回归证据
//    · slave 新增 7 个寄存器(0x58~0x70)并引出 cfg_/sts_ 端口;
//      本版 sts_* 【全部接 0】—— 因为产生它们的模块(fb_reader / vblank_ctrl /
//      gt911_ctrl)属于 Chunk B,尚不存在。读 0x64~0x70 读到 0 是预期行为。
//    · src_sel 由 1 位扩到 2 位:0=彩条 / 1=整屏填充 / 2=DDR 帧缓存(本版恒黑)
//
//  ⚠️ 【本版验收判据】与 v2 逐条对照,老功能一项都不能少:
//      ① 上电仍是厂商彩条            (src_sel 复位 = 0)
//      ② PS 写 0x54 换色 + 0x50 切源 → 整屏变色
//      ③ PS 写 0x58/0x5C 基址、读回  → 读写通(回环,不依赖任何数据通路)
//      ④ PS 读 0x60 → 读到 0         (无 vblank_ctrl,预期值)
//      ★ 判据 ③ 是本次新增的**独立可测点**:不碰 DDR、不碰 BD,就能证明
//        新寄存器译码对 —— 这是整条显示通路里第一个能被单独验证的环节。
//
//  【再下一版(Chunk B,需要你在 Vivado 里动 BD)】
//    新增 lcd_fb_reader(AXI4 读主机)+ lcd_vblank_ctrl,并把 sts_* 接上;
//    BD 侧:axi_smc 加一个从口(NUM_SI 1→2),并决定走 GP0 还是开 HP0 口。
//=============================================================================

module cv_sys #(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 8,
    parameter [31:0]  VERSION_CONST      = 32'h0001_0000,   // v1.0.0
    // ⚠️ v3 起 LCD_ID 【本层已不再使用】—— 时序参数改由 lcd_timing 自己的
    //    parameter 承载(那里才是唯一副本)。保留此参数只为不动已刷过的端口边界;
    //    1024×600 这个事实现在写在 lcd_timing.v 里。
    parameter [15:0]  LCD_ID             = 16'h7016
)(
    //-------------------------------------------------------------------------
    // AXI4-Lite 从机 —— 接口标注与 cv_axi_lite_slave.v 保持一致,
    // 否则 BD 里认不出这是一条 AXI 接口,连不上 axi_smc。
    //-------------------------------------------------------------------------
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_ACLK,ASSOCIATED_BUSIF S_AXI:M_AXI,ASSOCIATED_RESET s_axi_aresetn,FREQ_HZ 50000000,PHASE 0.000" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 S_AXI_ACLK CLK" *)
    input  wire                                 s_axi_aclk,
    // ⚠️ ASSOCIATED_BUSIF 已扩成 "S_AXI:M_AXI" —— 两条 AXI 接口共用同一个时钟。
    //    这样 BD 里不需要第二个时钟端口,也就**不会凭空造出一个跨时钟域**。
    //    M_AXI 同样由 s_axi_aresetn 复位(同域同复位,不另设 M_AXI_ARESETN)。

    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI_ARESETN,POLARITY ACTIVE_LOW" *)
    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 S_AXI_ARESETN RST" *)
    input  wire                                 s_axi_aresetn,

    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI,PROTOCOL AXI4LITE,ADDR_WIDTH 8,DATA_WIDTH 32,READ_WRITE_MODE READ_WRITE,HAS_BURST 0,HAS_LOCK 0,HAS_CACHE 0,HAS_PROT 0,HAS_QOS 0,HAS_REGION 0,HAS_WSTRB 1,HAS_BRESP 1,HAS_RRESP 1,SUPPORTS_NARROW_BURST 0,MAX_BURST_LENGTH 1,NUM_READ_OUTSTANDING 1,NUM_WRITE_OUTSTANDING 1,ID_WIDTH 0,AWUSER_WIDTH 0,ARUSER_WIDTH 0,WUSER_WIDTH 0,RUSER_WIDTH 0,BUSER_WIDTH 0,FREQ_HZ 50000000" *)
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWADDR"      *) input  wire [C_S_AXI_ADDR_WIDTH-1:0]    s_axi_awaddr,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWPROT"      *) input  wire [2:0]                       s_axi_awprot,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWVALID"     *) input  wire                              s_axi_awvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI AWREADY"     *) output wire                              s_axi_awready,

    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WDATA"       *) input  wire [C_S_AXI_DATA_WIDTH-1:0]    s_axi_wdata,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WSTRB"       *) input  wire [(C_S_AXI_DATA_WIDTH/8)-1:0] s_axi_wstrb,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WVALID"      *) input  wire                              s_axi_wvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI WREADY"      *) output wire                              s_axi_wready,

    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI BRESP"       *) output wire [1:0]                       s_axi_bresp,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI BVALID"      *) output wire                              s_axi_bvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI BREADY"      *) input  wire                              s_axi_bready,

    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARADDR"      *) input  wire [C_S_AXI_ADDR_WIDTH-1:0]    s_axi_araddr,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARPROT"      *) input  wire [2:0]                       s_axi_arprot,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARVALID"     *) input  wire                              s_axi_arvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI ARREADY"     *) output wire                              s_axi_arready,

    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RDATA"       *) output wire [C_S_AXI_DATA_WIDTH-1:0]    s_axi_rdata,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RRESP"       *) output wire [1:0]                       s_axi_rresp,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RVALID"      *) output wire                              s_axi_rvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 S_AXI RREADY"      *) input  wire                              s_axi_rready,

    //-------------------------------------------------------------------------
    // AXI3 主机 —— 帧缓存行读取(lcd_fb_reader → DDR)
    //   64-bit 数据,READ_ONLY,INCR 突发,**每次 16 拍 = 128 B,一行 16 次**
    //
    //   🔴 **必须写 AXI3,不能写 AXI4**(2026-10-02 实测踩坑,改前必读):
    //      Zynq-7000 的 HP/GP 口就是 AXI3。写成 AXI4 后 BD **直接拒绝连线** ——
    //      ERROR: [BD 41-1285] The protocols of the interfaces
    //             '/cv_sys_0/M_AXI'(AXI4) and '/processing_system7_0/S_AXI_HP0'(AXI3)
    //             are incompatible. They cannot be connected.
    //      且 AXI3 单次突发**上限 16 拍**(AXI4 才放宽到 256),所以 MAX_BURST_LENGTH
    //      只能是 16、ARLEN 只有 4 位。改这里时,**lcd_fb_reader.v 的拆突发逻辑
    //      必须一起对**(那边 BURSTS_LINE / BEATS_BURST 就是照这个定的)。
    //
    //   ⚠️ 这组 X_INTERFACE_INFO 是 IP Integrator 认出"这是一条 AXI 主接口"的
    //      **必要条件**。少一个端口、或名字写错一个字母,BD 里就是一堆散线,
    //      连不上 HP 口。改动后务必再 validate_bd_design 一次。
    //   ⚠️ 常量字段(arprot/arcache)刻意【不】做端口 —— 它们对本设计毫无意义,
    //      在这里接死即可,省掉 7 根没用的引脚。
    //-------------------------------------------------------------------------
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME M_AXI,PROTOCOL AXI3,DATA_WIDTH 64,ADDR_WIDTH 32,ID_WIDTH 0,HAS_BURST 1,HAS_LOCK 0,HAS_PROT 1,HAS_CACHE 1,HAS_QOS 0,HAS_REGION 0,HAS_WSTRB 0,AWUSER_WIDTH 0,ARUSER_WIDTH 0,WUSER_WIDTH 0,RUSER_WIDTH 0,BUSER_WIDTH 0,SUPPORTS_NARROW_BURST 0,MAX_BURST_LENGTH 16,NUM_READ_OUTSTANDING 1,NUM_WRITE_OUTSTANDING 0,READ_WRITE_MODE READ_ONLY,FREQ_HZ 50000000,PHASE 0.000" *)
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARADDR"  *) output wire [31:0] m_axi_araddr,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARBURST" *) output wire [1:0]  m_axi_arburst,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARCACHE" *) output wire [3:0]  m_axi_arcache,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARLEN"   *) output wire [3:0]  m_axi_arlen,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARPROT"  *) output wire [2:0]  m_axi_arprot,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARSIZE"  *) output wire [2:0]  m_axi_arsize,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARVALID" *) output wire        m_axi_arvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI ARREADY" *) input  wire        m_axi_arready,

    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI RDATA"   *) input  wire [63:0] m_axi_rdata,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI RLAST"   *) input  wire        m_axi_rlast,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI RRESP"   *) input  wire [1:0]  m_axi_rresp,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI RVALID"  *) input  wire        m_axi_rvalid,
    (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 M_AXI RREADY"  *) output wire        m_axi_rready,

    //-------------------------------------------------------------------------
    // RGB LCD 接口(ATK-MD0700R-1024600-IPS,24bit 并行,DE 模式)
    //   ⚠️ 端口名与 io.xdc 里现有的一致(lcd_rgb / lcd_hs / ...),
    //      Make External 后把外部端口命名成 lcd,期望 wrapper 端口仍为 lcd_xxx,
    //      这样 io.xdc 那 30 行不用改。导出后请核对 system_wrapper.v 的端口表。
    //-------------------------------------------------------------------------
    output wire        lcd_de,
    output wire        lcd_hs,
    output wire        lcd_vs,
    output wire        lcd_bl,
    // ⚠️ `lcd_clk` 这个**端口名以 clk 结尾**,IP Integrator 会自动把它推断成一条
    //    **输出时钟接口**;若不给出 FREQ_HZ,每次打开 BD 都会报:
    //      CRITICAL WARNING: [IP_Flow 19-4751] Bus Interface 'lcd_clk':
    //          FREQ_HZ bus parameter is missing for output clock interface.
    //    这里只补 X_INTERFACE_PARAMETER(给上频率),**故意不加 X_INTERFACE_INFO** ——
    //    加了会把普通引脚变成接口引脚,下面 connect_bd_net 的连线方式就得跟着改。
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME lcd_clk,FREQ_HZ 50000000,PHASE 0.000" *)
    output wire        lcd_clk,
    output wire        lcd_rst,
    output wire [23:0] lcd_rgb,

    //-------------------------------------------------------------------------
    //  电容触摸(GT911)—— 4 根线,物理接在 PL BANK34/35
    //     ⚠️ 三个三态口**必须**保持 inout,不能拆成 o/oe/i 三根 wire 引出去:
    //        拆了在 BD 里会变成 3 个外部端口,而它们物理上各只有 1 个引脚,
    //        io.xdc 无法对同一引脚写三条约束。IOBUF 由综合器在顶层推断。
    //     ⚠️ `touch_int` 是**双向**的:GT911 用"复位释放瞬间 INT 的电平"
    //        锁存 I2C 器件地址(高→0x14 / 低→0x5D),所以主机在复位期间
    //        必须驱动它,之后必须释放。见 gt911_ctrl.v 文件头。
    //-------------------------------------------------------------------------
    output wire        touch_rst_n,
    inout  wire        touch_int,
    inout  wire        touch_scl,
    inout  wire        touch_sda,

    //-------------------------------------------------------------------------
    //  模拟前端(2026-10-05 加)—— DAC1220 偏压 + AD9833 激励,各 3 根线
    //
    //  ⚠️ 这是【6 根独立输出脚】,不是 3 根共享总线 + 2 个片选 ——
    //     dac_1220.v 与 adc_9833.v 内部【各自】例化了一个 u_spi_master,
    //     两个从设备之间没有任何共用信号。
    //
    //  ⚠️ DDS 三根线的内部信号名沿用 adc_9833.v 的端口名
    //     (adc_sclk / adc_sync / adc_mosi)—— 那是该模块当初按"复用 spi_master"
    //     写下的**历史名**,实际用途就是 AD9833 的 SCLK / FSYNC / SDATA。
    //     本层对外改用 dds_* 命名,例化处显式对接,不让这个歧义再往上传。
    //
    //  ⚠️ 球号:六个都属 J4 扩展口,电平全是 LVCMOS33。
    //     DAC 三根全在 BANK34;DDS 的 T11 在 BANK34、T5/U7 在 BANK13
    //     (J4 第 2 脚夹在一排 BANK13 脚中间)。约束见 src/constrs/led.xdc。
    //-------------------------------------------------------------------------
    //  ⚠️ 叫 dds_sclk 而不是 dds_clk —— 名字不是随便起的:
    //     端口名若以 `clk` 结尾,IP Integrator 会【自动把它推断成一条时钟接口】
    //     (xilinx.com:signal:clock:1.0),然后因为拿不到 FREQ_HZ 报
    //     CRITICAL WARNING: [IP_Flow 19-4751] Bus Interface 'dds_clk':
    //     FREQ_HZ bus parameter is missing for output clock interface.
    //     实测已复现(2026-10-05,刷新 module reference 时)。
    //     而 dds_sclk 本质是 SPI 移位时钟、【不是时钟域】—— 声明成时钟接口是
    //     语义错误(像 lcd_clk 那样补 FREQ_HZ 才是错的修法,那是给真时钟域用的)。
    //     改叫 _sclk 后与 dac_sclk 对称,推断不再触发。
    output wire        dac_sclk,    // → DAC1220 SCLK  (J4-36 / W15 / BANK34)
    output wire        dac_cs,      // → DAC1220 CS    (J4-32 / W13 / BANK34)
    output wire        dac_mosi,    // → DAC1220 SDIO  (J4-34 / Y14 / BANK34)
    output wire        dds_sclk,    // → AD9833  SCLK  (J4-4  / T5  / BANK13)
    output wire        dds_sync,    // → AD9833  FSYNC (J4-6  / U7  / BANK13)
    output wire        dds_data,    // → AD9833  SDATA (J4-2  / T11 / BANK34)

    //-------------------------------------------------------------------------
    //  AD9238 高速 ADC(2026-10-06 加)—— 16 根
    //
    //  ⚠️ 球号【由用户在板上实际接线后报出】,而且**报了两遍**:
    //     第一版有 y11 用两次(d1/d3)、w6 用两次(d11/otr)的重复,
    //     与"模块排针总共 16 个信号"对不上;用户核对后给了下面这版无重复的。
    //     ⇒ 这是本项目继 DAC/DDS 之后**第二处"接线先于约束"**的信号组。
    //     引脚约束写在 qrs.srcs/constrs_1/new/led.xdc(工程里唯一启用的 xdc)。
    //
    //  ⚠️ 15 根在 BANK13,只有 clk(V13/J4-27)在 BANK34 ——
    //     与 DDS 那三根同样的跨 bank 情形,功能无影响(两 bank 同为 3.3 V)。
    //
    //  ⚠️ 本模块是**单通道**接法(12 根数据线 + 1 个时钟)。
    //     AD9238 芯片本身有 A/B 两个通道、各自 12 位独立输出总线,
    //     这里只用其中一组。将来要双通道(DLIA 的 Z 反演要 MAG_A/MAG_B)
    //     就得再引 12~15 根线 —— 那是硬件侧动作,PL 侧本版不做。
    //
    //  ⚠️ `adc_clk` 端口名以 `clk` 结尾 ⇒ IP Integrator 会把它推断成一条
    //     **输出时钟接口**,不给 FREQ_HZ 会在每次打开 BD 时报
    //     CRITICAL WARNING: [IP_Flow 19-4751]。这里照 lcd_clk 的既有做法
    //     只补 X_INTERFACE_PARAMETER、**故意不加 X_INTERFACE_INFO**。
    //     ⚠️ FREQ_HZ 填的是**上限 65 MHz**,不是复位默认的 10 MHz ——
    //        它是 BD 侧的静态提示量,取最坏值;实际频率由 0x88 的 DIV 决定。
    //        (与 lcd_clk 填 50000000 同属一类"设计值口径"。)
    //     ⛔ 不要因为这里有点别扭就把它改名成 adc_sclk 之类 ——
    //        它是**真时钟域**,不是 SPI 移位时钟,补 FREQ_HZ 才是对的修法。
    //-------------------------------------------------------------------------
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME adc_clk,FREQ_HZ 65000000,PHASE 0.000" *)
    output wire        adc_clk,     // → AD9238 CLK   (V13 / J4-27)
    output wire        adc_oe_n,    // → AD9238 OEB   (U13 / J4-28),低有效
    output wire        adc_pdn,     // → AD9238 PDWN  (V6  / J4-21),高有效
    input  wire [11:0] adc_d,       // ← AD9238 D0..D11 (W11 Y11 Y12 Y13 V10 V11 W9 W10 Y8 Y9 Y6 Y7)
    input  wire        adc_otr      // ← AD9238 OTR   (W6  / J4-18)
);

    //=========================================================================
    //  0. 从机配置输出(cfg_*)—— 必须在从机例化【之前】声明
    //     ⚠️ 未声明的标识符会被 Verilog 隐式声明成 1 位 wire。若声明写在
    //        例化之后,位宽就对不上(xvlog 实测:VRFC 10-2938)。
    //=========================================================================
    wire [31:0] cfg_lcd_ctrl;       // 0x50 [1:0]src_sel
    wire [31:0] cfg_lcd_fill;       // 0x54 [23:0]RGB

    // 0x58~0x70(2026-10-02 加)—— 同样必须在例化之前声明
    wire [31:0] cfg_lcd_fb_base0;
    wire [31:0] cfg_lcd_fb_base1;
    wire        cfg_lcd_swap_tgt;
    wire        cfg_lcd_swap_req;

    // 0x64 写 = 清 lcd_fb_reader 的粘滞错误;0x74/0x78/0x7C = 调试观测(2026-10-03 加)
    wire        cfg_lcd_clr_sticky;
    wire [31:0] sts_dbg_beats;
    wire [31:0] sts_dbg_araddr;
    wire [31:0] sts_dbg_state;

    // 显示状态回读 —— v4 起由 lcd_vblank_ctrl 驱动(不再是接 0 的常量)
    wire [31:0] sts_lcd_status;

    // 触摸状态 —— v5 起由 gt911_ctrl 驱动(不再是接 0 的常量)
    //   ⚠️ 声明必须留在从机例化【之前】—— 见本节开头那条隐式 1 位 wire 的坑
    wire [31:0] sts_touch_status;
    wire [31:0] sts_touch_pt0;
    wire [31:0] sts_touch_pt1;

    // 模拟前端配置(2026-10-05 加)—— 同样必须在从机例化【之前】声明
    wire [31:0] cfg_bias_code;      // 0x14 [19:0] → dac_1220.code
    wire        cfg_bias_set;       // 0x14 写 → 1 拍脉冲 → dac_1220.set_in
    // 🔴 2026-10-06 由 cfg_dac_rst_n 改名 cfg_clk_release,消费者由 1 个变 2 个:
    //      · dac_1220.rst_n   (与 s_axi_aresetn 相与)
    //      · ADC 的 MMCM RST  (取反后驱动,见 §8)
    //    两者前置条件相同(FCLK0 必须已是 50.000 MHz)⇒ 共用一位。
    wire        cfg_clk_release;    // 0x84[0] → 时钟就绪放行
    wire [31:0] cfg_dds_freq;       // 0x80 [27:0] → adc_9833.code
    wire        cfg_dds_set;        // 0x80 写 → 1 拍脉冲 → adc_9833.set_in

    // ADC 采集(2026-10-06 加)—— 同样必须在从机例化【之前】声明
    wire [31:0] cfg_adc_cfg;        // 0x88 [7:0]DIV [15:8]PH_OFF [16]CONT [17]OEB_N [18]PDN
    wire        cfg_adc_go;         // 0x8C[0] 写 → 1 拍脉冲
    wire [31:0] cfg_adc_buf_idx;    // 0x94 [11:0] 缓冲读索引
    wire [31:0] sts_adc_sts;        // 0x90 [0]locked [1]done [2]busy [3]otr
    wire [31:0] sts_adc_buf_data;   // 0x98 [11:0]采样 [12]OTR
    // dac_1220 的 init_done —— 【回送给从机】给上电脉冲开门。
    //   ⚠️ 方向是从机收、DAC 发,所以它在从机端口表里是 input。
    //      dac_1220 的 set_in 只在 NORMAL_WAIT 状态里才锁存,不等 init_done
    //      就发脉冲 = 空放(详见 cv_axi_lite_slave.v §set 脉冲 的核对记录)。
    wire        dac_init_done;

    //=========================================================================
    //  1. AXI4-Lite 从机 —— 例化结构不变(v2 起内部扩了 2 个寄存器)
    //     R2 验收判据「SCRATCH 回环自检」已于 2026-09-29 上板达成。
    //=========================================================================
    cv_axi_lite_slave #(
        .C_S_AXI_DATA_WIDTH (C_S_AXI_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH (C_S_AXI_ADDR_WIDTH),
        .VERSION_CONST      (VERSION_CONST)
    ) u_cv_axi_lite_slave (
        .s_axi_aclk    (s_axi_aclk   ),
        .s_axi_aresetn (s_axi_aresetn),
        .s_axi_awaddr  (s_axi_awaddr ),
        .s_axi_awprot  (s_axi_awprot ),
        .s_axi_awvalid (s_axi_awvalid),
        .s_axi_awready (s_axi_awready),
        .s_axi_wdata   (s_axi_wdata  ),
        .s_axi_wstrb   (s_axi_wstrb  ),
        .s_axi_wvalid  (s_axi_wvalid ),
        .s_axi_wready  (s_axi_wready ),
        .s_axi_bresp   (s_axi_bresp  ),
        .s_axi_bvalid  (s_axi_bvalid ),
        .s_axi_bready  (s_axi_bready ),
        .s_axi_araddr  (s_axi_araddr ),
        .s_axi_arprot  (s_axi_arprot ),
        .s_axi_arvalid (s_axi_arvalid),
        .s_axi_arready (s_axi_arready),
        .s_axi_rdata   (s_axi_rdata  ),
        .s_axi_rresp   (s_axi_rresp  ),
        .s_axi_rvalid  (s_axi_rvalid ),
        .s_axi_rready  (s_axi_rready ),

        // cfg_* / sts_* —— 内部消化,不引出顶层
        .cfg_lcd_ctrl     (cfg_lcd_ctrl    ),
        .cfg_lcd_fill     (cfg_lcd_fill    ),
        .cfg_lcd_fb_base0 (cfg_lcd_fb_base0),
        .cfg_lcd_fb_base1 (cfg_lcd_fb_base1),
        .cfg_lcd_swap_tgt (cfg_lcd_swap_tgt),
        .cfg_lcd_swap_req (cfg_lcd_swap_req),
        .cfg_lcd_clr_sticky (cfg_lcd_clr_sticky),
        .sts_lcd_status   (sts_lcd_status  ),
        .sts_dbg_beats    (sts_dbg_beats   ),
        .sts_dbg_araddr   (sts_dbg_araddr  ),
        .sts_dbg_state    (sts_dbg_state   ),
        .sts_touch_status (sts_touch_status),
        .sts_touch_pt0    (sts_touch_pt0   ),
        .sts_touch_pt1    (sts_touch_pt1   ),

        // 模拟前端(2026-10-05)
        .cfg_bias_code    (cfg_bias_code   ),
        .cfg_bias_set     (cfg_bias_set    ),
        .cfg_dds_freq     (cfg_dds_freq    ),
        .cfg_dds_set      (cfg_dds_set     ),
        .dac_init_done    (dac_init_done   ),

        // ADC 采集(2026-10-06)
        .cfg_clk_release  (cfg_clk_release ),
        .cfg_adc_cfg      (cfg_adc_cfg     ),
        .cfg_adc_go       (cfg_adc_go      ),
        .cfg_adc_buf_idx  (cfg_adc_buf_idx ),
        .sts_adc_sts      (sts_adc_sts     ),
        .sts_adc_buf_data (sts_adc_buf_data)
    );

    //=========================================================================
    //  2. RGB LCD —— v3 起时序件换成自研 lcd_timing,输出级搬到本文件
    //     ✅ lcd_display(厂商彩条)保留且【一行未改】—— 它是 src_sel=0 的图源,
    //        也是"换掉时序件后屏没坏"那次回归的证据。
    //     ⚠️ 相位链一个字都没动(req_c→de_req→de_act,见 lcd_timing.v 文件头):
    //        像素源必须【寄存输出】,lcd_display 恰好是,所以直接接得上。
    //=========================================================================
    wire [10:0] pixel_xpos;     // = lcd_timing.px_x,在 de_req 当拍有效
    wire [10:0] pixel_ypos;     // = lcd_timing.px_y,★ 已修掉厂商的 off-by-one
    wire [10:0] h_disp;
    wire [10:0] v_disp;
    wire [23:0] bars_data;      // lcd_display 直出(厂商彩条)
    wire [23:0] pixel_data;     // 送进输出级的最终像素

    wire        de_req;         // 取数请求 —— 本版无消费者,Chunk B 接 fb_reader
    wire        de_act;         // 送屏 DE
    wire        vblank;         // ┐
    wire        line_start;     // ├ 本版无消费者,Chunk B 接 lcd_vblank_ctrl / fb_reader
    wire        frame_start;    // ┘

    lcd_display u_lcd_display (
        .lcd_pclk   (s_axi_aclk ),      // PCLK = clk_axi = 设计 50 MHz / 实测 41.6667 MHz
        .rst_n      (s_axi_aresetn),
        .pixel_xpos (pixel_xpos ),
        .pixel_ypos (pixel_ypos ),
        .h_disp     (h_disp     ),
        .v_disp     (v_disp     ),
        .pixel_data (bars_data  )
    );

    //-------------------------------------------------------------------------
    //  像素源 mux
    //    [1:0] = 0 → 厂商彩条(复位默认,回归路径,不依赖 DDR/PS)
    //    [1:0] = 1 → 整屏填充 cfg_lcd_fill[23:0]
    //    [1:0] = 2 → **DDR 帧缓存**(lcd_fb_reader 取行)
    //    其他值     → 落到彩条 —— 失败安全:认不出的源不给可疑画面
    //  ✅ 位 0 的含义自 v2 起从未变过 ⇒ 现有 PS 脚本一字不用改。
    //-------------------------------------------------------------------------
    wire [1:0]  lcd_src_sel  = cfg_lcd_ctrl[1:0];
    wire [23:0] lcd_fill_rgb = cfg_lcd_fill[23:0];

    // ⚠️ ddr_px565 必须【在这里】声明,不能挪到下面第 3 节去 ——
    //    它在紧接着的展开式里就被用了。放后面会直接报
    //    `VRFC 10-3380 identifier 'ddr_px565' is used before its declaration`
    //    (2026-10-02 实测踩到,和本文件开头记的是同一类坑)。
    wire [15:0] ddr_px565;      // lcd_fb_reader 输出,RGB565

    //-------------------------------------------------------------------------
    //  RGB565 → RGB888 展开
    //    ⚠️ 引脚上仍是 **24 根** RGB888,只是**颜色精度**来自 565。
    //       展开用标准的"高位补回低位"写法,不是简单左移 ——
    //       左移会让白变成 0xF8F8F8(而非纯白),渐变端点上会看出台阶。
    //    ⚠️ 纯组合,不引入延迟 ⇒ 不破坏 lcd_timing 的相位链。
    //-------------------------------------------------------------------------
    wire [4:0] ddr_r5 = ddr_px565[15:11];
    wire [5:0] ddr_g6 = ddr_px565[10: 5];
    wire [4:0] ddr_b5 = ddr_px565[ 4: 0];
    wire [23:0] ddr_pixel = {ddr_r5, ddr_r5[4:2],
                             ddr_g6, ddr_g6[5:4],
                             ddr_b5, ddr_b5[4:2]};

    assign pixel_data = (lcd_src_sel == 2'd1) ? lcd_fill_rgb :
                        (lcd_src_sel == 2'd2) ? ddr_pixel    :
                                                bars_data    ;

    //-------------------------------------------------------------------------
    //  时序发生器(自研)
    //    面板:ATK-MD0700R-1024600-IPS,1024×600
    //    行 [HSYNC 20][H_BACK 140][有效 1024][H_FRONT 160] = 1344
    //    场 [VSYNC  3][V_BACK  20][有效  600][V_FRONT  12] =  635
    //    ⇒ 这四个数字【只写在 lcd_timing.v 的默认值里】,此处【不重复覆盖】——
    //      覆盖一份就多一处要同步的副本,迟早对不上。
    //-------------------------------------------------------------------------
    lcd_timing #(
        // 唯一的显式覆盖:HS/VS 恒高 = 厂商行为 = 当前屏实测可用。
        // 改成 1 会输出真实同步脉冲,但极性未查证 ⇒ 先别动(理由见 lcd_timing.v)
        .HS_VS_MODE (0)
    ) u_lcd_timing (
        .clk_lcd     (s_axi_aclk ),
        .rst_n       (s_axi_aresetn),
        .de_req      (de_req     ),
        .px_x        (pixel_xpos ),
        .px_y        (pixel_ypos ),
        .de_act      (de_act     ),
        .vblank      (vblank     ),
        .line_start  (line_start ),
        .frame_start (frame_start),
        .h_disp      (h_disp     ),
        .v_disp      (v_disp     ),
        .lcd_hs      (lcd_hs     ),
        .lcd_vs      (lcd_vs     )
    );

    //=========================================================================
    //  3. 帧缓存通路(v4 新增,决策 D24)
    //     lcd_vblank_ctrl:决定"前台是哪一块",且**只在场消隐换址**(硬件防撕裂)
    //     lcd_fb_reader  :按行突发读 DDR → 乒乓行缓冲 → RGB565
    //     节拍全部来自 lcd_timing:frame_start 预取第 0 行,line_start 取下一行
    //=========================================================================
    wire [31:0] fb_base;
    wire        fb_underrun;
    wire        fb_axi_err;

    // M_AXI 常量字段 —— 在这里接死,不占模块引脚。
    //   arcache = 4'b0010 = Normal Non-cacheable Non-bufferable
    //   arprot  = 3'h0    = 特权 / 安全 / 数据访问
    //
    // 🔴 **arprot 的 bit1(NS,非安全)不能置 1。**置 1 后 S_AXI_HP0 对**每个**读事务
    //    都回 DECERR(2'b11)且数据恒为零,表现为 DDR 帧缓存整屏全黑。
    //
    //  2×2 单变量分离实测(2026-10-03,同一 boot 交替加载,四格全测):
    //    arcache  arprot   结果
    //    4'b0010  3'h0     ✅ 有数据     a9636e0d
    //    4'b0011  3'h0     ✅ 有数据     EXP-A
    //    4'b0010  3'b010   ❌ DECERR     EXP-B
    //    4'b0011  3'b010   ❌ DECERR     298c8f1f
    //  ⇒ **只由 arprot 决定,arcache 无关。**
    //
    //  ⚠️ 取值出处:本机 Vivado 的 Xilinx AXI master 模板与厂商已验证例程
    //     (`xilinx/2025.2/Vivado/data/emulation/hw_em/ip_repo/sim_xdma_2.0/hdl/sim_xdma_v2_0_M_AXI.v:278-279`)
    //     均取 `ARCACHE=4'b0010 / ARPROT=3'h0`,本处与之对齐。
    //     **此二者本身仍无权威手册出处**(本机无 UG585/TRM),机理未定 ——
    //     属"与已验证参照物对齐",不属"已知原理"。
    //  ⚠️ 本文上方曾于 2026-10-03 同日写下"该假设已被实测推翻"并回滚到 3'b010 ——
    //     **那次撤回是错的**:依据只有 `cdfbe0f5` 一个读数,而它**只核过源码、没核网表**。
    //     复核记录:`report/measurements/20261003_hp0_decerr_rootcause_arprot.md`
    assign m_axi_arcache = 4'b0010;
    assign m_axi_arprot  = 3'h0;

    lcd_vblank_ctrl u_lcd_vblank_ctrl (
        .clk            (s_axi_aclk       ),
        .rst_n          (s_axi_aresetn    ),
        .frame_start    (frame_start      ),
        .swap_req       (cfg_lcd_swap_req ),
        .swap_tgt       (cfg_lcd_swap_tgt ),
        .fb_base0       (cfg_lcd_fb_base0 ),
        .fb_base1       (cfg_lcd_fb_base1 ),
        .underrun       (fb_underrun      ),
        .axi_err        (fb_axi_err       ),
        .fb_base        (fb_base          ),
        .sts_lcd_status (sts_lcd_status   )
    );

    lcd_fb_reader u_lcd_fb_reader (
        .clk            (s_axi_aclk       ),
        .rst_n          (s_axi_aresetn    ),
        .fb_base        (fb_base          ),
        .frame_start    (frame_start      ),
        .line_start     (line_start       ),
        .src_sel        (lcd_src_sel      ),   // ★ 2026-10-03:非 2 时完全不取数
        .clr_sticky     (cfg_lcd_clr_sticky),
        .dbg_beats      (sts_dbg_beats    ),
        .dbg_araddr     (sts_dbg_araddr   ),
        .dbg_state      (sts_dbg_state    ),
        .de_req         (de_req           ),
        .px_x           (pixel_xpos       ),
        .px_data        (ddr_px565        ),
        .underrun       (fb_underrun      ),
        .axi_err        (fb_axi_err       ),
        .m_axi_araddr   (m_axi_araddr     ),
        .m_axi_arlen    (m_axi_arlen      ),
        .m_axi_arsize   (m_axi_arsize     ),
        .m_axi_arburst  (m_axi_arburst    ),
        .m_axi_arvalid  (m_axi_arvalid    ),
        .m_axi_arready  (m_axi_arready    ),
        .m_axi_rdata    (m_axi_rdata      ),
        .m_axi_rvalid   (m_axi_rvalid     ),
        .m_axi_rlast    (m_axi_rlast      ),
        .m_axi_rresp    (m_axi_rresp      ),
        .m_axi_rready   (m_axi_rready     )
    );

    //-------------------------------------------------------------------------
    //  输出级 —— v2 之前这五行在 lcd_driver 内部,v3 起搬到本文件
    //    逐条与厂商件对齐,一项都没改:
    //      lcd_rgb = lcd_de ? pixel_data : 24'd0
    //      lcd_de  = data_req 再延一拍(= 本层的 de_act)
    //      lcd_bl / lcd_rst 恒 1,lcd_clk = pclk
    //    ⚠️ lcd_clk 仍是组合直出 s_axi_aclk,与厂商写法一致。
    //       更规范是走 ODDR 原语,但那是【改变】一个已验证通过的东西、
    //       当前没有功能收益 ⇒ 不动(B04)。
    //    ✅ 2026-10-03 更正:上面那句「de_req / vblank / line_start / frame_start
    //       本版无消费者」是 v3 时代的注,**v4 起已经不成立** —— 四个信号都接给了
    //       lcd_vblank_ctrl / lcd_fb_reader,综合不再报 "no load"。原注释误留至今。
    //-------------------------------------------------------------------------
    assign lcd_de  = de_act;
    assign lcd_rgb = de_act ? pixel_data : 24'd0;
    assign lcd_bl  = 1'b1;
    assign lcd_rst = 1'b1;
    assign lcd_clk = s_axi_aclk;

    //=========================================================================
    //  4. 电容触摸(GT911)—— v5 新增
    //
    //  【为什么放在 clk_axi 域,而不是单开一个时钟】
    //    ① 上游 i2c_master 的位速率由 CLK_FREQ_HZ 分频而来,给多少用多少,
    //       不需要特定的时钟频率 ⇒ 没有"非它不可"的频点。
    //    ② sts_touch_* 直连从机寄存器 ⇒ **同域 = 不需要 CDC**,少一处风险。
    //    ⚠️ 代价:clk_axi 现为【设计值 50 MHz / 实测 41.6667 MHz】,
    //       而本模块内所有"毫秒"按 50 MHz 算 ⇒ 实际**慢 20%**
    //       (20 ms 复位低电平变 ~24 ms)。对触摸轮询无影响,
    //       但**报告里不许把设计值 50 MHz 写成实测值**。
    //
    //  【硬件前提】touch_scl / touch_sda / touch_int **板上必须有上拉**。
    //    没有上拉 I2C 根本起不来 —— 仿真里漏 pullup 的症状是 SCL 永久卡低、
    //    每个 op 都等满拉伸超时才退出(见 report 里 TB 记录)。
    //=========================================================================
    gt911_ctrl #(
        .CLK_FREQ_HZ (50_000_000),      // ★ 设计值(实测 41.6667 MHz)
        .SCL_FREQ_HZ (100_000),         // GT911 上限 400 kHz,取 100 kHz 保兼容
        .ERR_RECOVER (3),               // 连错 3 次 → 总线恢复
        .ERR_RESET   (5),               // 连错 5 次 → 重走整个复位序列
        .N_PT        (2)                // 读 2 个触点(给捏合缩放留余量)
    ) u_gt911_ctrl (
        .clk              (s_axi_aclk      ),
        .rst_n            (s_axi_aresetn   ),
        .touch_rst_n      (touch_rst_n     ),
        .touch_int        (touch_int       ),
        .touch_scl        (touch_scl       ),
        .touch_sda        (touch_sda       ),
        .sts_touch_status (sts_touch_status),
        .sts_touch_pt0    (sts_touch_pt0   ),
        .sts_touch_pt1    (sts_touch_pt1   )
    );

    //=========================================================================
    //  7. 模拟前端(2026-10-05 加)—— DAC1220 偏压 + AD9833 激励
    //
    //  【为什么放 clk_axi 域】两个模块的时序参数都是按 50 MHz 写死的
    //    (dac_1220.v 的 PWR_WAIT / CAL_WAIT_T 注释写着"@50MHz";
    //     adc_9833.v 的 PWR_WAIT 同理),而 clk_axi 正是【设计 50 MHz】。
    //    与 gt911_ctrl 放同一个域 ⇒ 与 cfg_* 之间无 CDC,少一处风险。
    //
    //  🔴 【2026-10-05 更正】本段此前写着「clk_axi 开机默认 27.78 MHz,
    //     "毫秒"会按比例变长,**对这里的一次性上电握手没有影响**」——
    //     **后半句是错的,而且正是 bug 藏身之处。**
    //
    //     只有【下限型】的等待(200 ms 晶振起振、600 ms 校准、30 ms 建立、
    //     SPI 半周期、字节间隔)才"变长无害"。dac_1220 的**串行复位图案
    //     是双向窗口**(手册 Figure 12 / Table 5):
    //         t16 = 208.3~325.5 µs、t18 = 416.7~732.4 µs、t19 = 833.3~976.6 µs
    //     常数按 50 MHz 折的,跑到 27.78 MHz 就变成
    //         468 / 1026 / 1620 µs —— 三段**全部超上界**
    //     ⇒ 复位不触发 ⇒ CMR 配置与自校准被忽略 ⇒ DIR 写入无效
    //     ⇒ 输出停在芯片上电默认 DIR=0 = **−10 V**(用户实测到的症状)。
    //     ⚠️ 两组窗口折成拍数**不重叠**(t16 @27.78 M = 5787~9044 拍,
    //        @50 M = 10415~16275 拍)⇒ 改常数修不了。
    //
    //     ⇒ 处置:dac_1220 的 rst_n 不再直连 s_axi_aresetn,而与
    //       `cfg_dac_rst_n`(0x84[0],复位值 **0**)相与。
    //       软件在 `apply_fclk0()` 把 FCLK0 置到 50.000 MHz **之后**
    //       写 0x84=1 放行 ⇒ 初始化必然跑在设计频率上。
    //     ✅ adc_9833 不受影响:它的时序常数**全是下限型**(PWR_WAIT 与
    //        1024 拍帧间隔),没有双向窗口,慢一点无害 —— 故照旧直连。
    //
    //  【init_done】dac_1220 的 `init_done` 已接回从机(给 bias_code 的
    //    上电脉冲开门,必须在 NORMAL_WAIT 才发);adc_9833 的仍接空 ——
    //    DDS 侧不发上电脉冲,不需要它。将来若要观测,开 ILA 比占槽位省事。
    //
    //  【上电时序】dac_1220:200 ms 等待 + 六段复位图案 + 600 ms 自校准;
    //    adc_9833:200 ms 等待 + 4 帧初始化(其中就已经把频率置成 1 MHz)。
    //    ⚠️ 时序从**软件写 0x84=1 那一刻**起算(开机时 DAC 一直按在复位态),
    //       不再是从位流加载起算。这段时间里到来的 bias_set 脉冲会被
    //       dac_1220 内部的 set_pending 锁住,等它就绪再消费 —— 不会丢;
    //       且从机侧的上电脉冲本来就等 dac_init_done,不会空放。
    //=========================================================================
    dac_1220 #(
        .PWR_WAIT    (32'd10000000),   // 200 ms @50 MHz(手册要求晶振起振 ≥25 ms)
        .CAL_WAIT_T  (32'd30000000),   // 600 ms 自校准等待(手册 300–500 ms)
        .SETTLE_WAIT (32'd1500000)     // 30 ms DIR 建立等待
    ) u_dac_1220 (
        .clk_50m   (s_axi_aclk          ),
        // 🔴 不是 s_axi_aresetn 直连 —— 初始化必须等软件把 FCLK0 补到
        //    50.000 MHz 之后才放行(写 0x84[0]=1)。理由见下面 §上电时序:
        //    三段复位图案是有【双向窗口】的,超上界就完全不复位。
        .rst_n     (s_axi_aresetn & cfg_clk_release),
        .dac_sclk  (dac_sclk            ),
        .dac_cs    (dac_cs              ),
        .dac_mosi  (dac_mosi            ),
        .code      (cfg_bias_code[19:0] ),   // 0x14,20 位
        .set_in    (cfg_bias_set        ),   // 0x14 写脉冲 + 上电那一次
        .init_done (dac_init_done       )   // → 从机,给上电脉冲开门(必须有)
    );

    adc_9833 #(
        .PWR_WAIT (32'd10000000)       // 200 ms @50 MHz
    ) u_adc_9833 (
        .clk_50m   (s_axi_aclk          ),
        .rst_n     (s_axi_aresetn       ),
        .adc_sclk  (dds_sclk            ),   // ⚠️ 模块内的历史命名,实际是:
        .adc_sync  (dds_sync            ),   //    adc_sclk → AD9833 SCLK
        .adc_mosi  (dds_data            ),   //    adc_sync → FSYNC / adc_mosi → SDATA
        .code      (cfg_dds_freq[27:0]  ),   // 0x80,f_out = code × 25 MHz / 2^28
        .set_in    (cfg_dds_set         ),   // 0x80 写脉冲(上电不发,见从机注释)
        .init_done ()                        // 未引出,见上
    );

    //=========================================================================
    //  8. ADC 采集通路(2026-10-06 新增)
    //
    //  【为什么自己例化 MMCME2_ADV,而不是再拉一个 clk_wiz IP】
    //    ① 本层的章程是"只做例化 + 引线";多一个 IP 就多一个要跟着刷的 .xci;
    //    ② 参数全是常量,用不上 IP 向导;
    //    ③ 下面这份例化模板是**从本工程里已 OOC 综合通过的那一份逐字抄的**:
    //         qrs.gen/sources_1/ip/clk_wiz_0/clk_wiz_0_clk_wiz.v
    //       ⇒ 端口名与参数名都有出处,不靠记忆拼(这是 2026-10-05 那次
    //         "刷新 module reference 时才想起端口名以 clk 结尾会被推断"的教训)。
    //
    //  【频率账】
    //     VCO     = CLKIN1 × CLKFBOUT_MULT_F / DIVCLK_DIVIDE
    //             = 50 MHz × 26.000 / 1 = 1300 MHz
    //     CLKOUT0 = VCO / CLKOUT0_DIVIDE_F = 1300 / 10.000 = **130 MHz**
    //
    //    ⚠️ VCO 1300 MHz 对 xc7z020clg400-**2** 合法(-2 档 MMCM VCO 上限 1440)。
    //       同一组参数已经在 qrs.runs/clk_wiz_0_synth_1 那次 OOC 综合里跑过本器件。
    //       ⛔ 若将来换成 **-1** 档(上限 1200),这组参数会超范围 ——
    //          把 DIVCLK_DIVIDE 改 2、CLKOUT0_DIVIDE_F 改 5(VCO 650 MHz)即可,
    //          输出频率不变。
    //
    //  【为什么 RST 挂在 cfg_clk_release 上,而不是 s_axi_aresetn】
    //    CLKIN1_PERIOD 硬编码 20 ns(即 50 MHz)。板上 BOOT.BIN 把 FCLK0
    //    留在 **27.78 MHz** ⇒ VCO 只有 361 MHz,低于 MMCM 下限 600 ⇒ **锁不住**。
    //    必须等软件 apply_fclk0() 把它改到 50.000 MHz 之后才能锁上。
    //    ⇒ 与 dac_1220 复用**同一位** cfg_clk_release(0x84[0])。
    //
    //    ✅ 失败方向是安全的:FCLK0 若是 27.78 MHz,读 0x90[0] = 0,
    //       而且 adc_capture 被按在复位态、adc_clk 不输出 —— 不会跑到一个
    //       频率错误的时钟上产生"看起来有数据但换算全错"的结果。
    //    ⚠️ 但 adc_clk 此刻是【静止低电平】而不是高阻,ADC 也就不采样。
    //       要彻底消除这个上电窗口,得重建 BOOT.BIN(原 F1 选项 B,至今未做)。
    //
    //  【为什么 CLKOUT0 要过 BUFG】
    //    MMCM 的 CLKOUTxx 是"时钟信号"但不是"时钟布线资源",直接当域时钟用
    //    会报时序问题。必须过 BUFG —— clk_wiz 生成的代码每一路都是这么做的。
    //    反馈同样要过 BUFG(clk_wiz 里那个 clkf_buf),不能直连。
    //=========================================================================
    wire clk_adc_raw;
    wire clk_adc_base;
    wire clk_fb_raw;
    wire clk_fb_buf;
    wire mmcm_locked;

    // MMCM RST 是**高有效**,cfg_clk_release 是"1 = 放行" ⇒ 取反。
    wire mmcm_rst = ~(s_axi_aresetn & cfg_clk_release);

    MMCME2_ADV #(
        .BANDWIDTH            ("OPTIMIZED"  ),
        .CLKOUT4_CASCADE      ("FALSE"      ),
        .COMPENSATION         ("ZHOLD"      ),
        .STARTUP_WAIT         ("FALSE"      ),
        .DIVCLK_DIVIDE        (1            ),
        .CLKFBOUT_MULT_F      (26.000       ),   // VCO = 50 × 26 / 1 = 1300 MHz
        .CLKFBOUT_PHASE       (0.000        ),
        .CLKFBOUT_USE_FINE_PS ("FALSE"      ),
        .CLKOUT0_DIVIDE_F     (10.000       ),   // 1300 / 10 = 130 MHz
        .CLKOUT0_PHASE        (0.000        ),
        .CLKOUT0_DUTY_CYCLE   (0.500        ),
        .CLKOUT0_USE_FINE_PS  ("FALSE"      ),
        .CLKIN1_PERIOD        (20.000       )    // ★ 20 ns = 50 MHz,不是 27.78 MHz
    ) u_adc_mmcm (
        // 输出时钟 —— 未用的一律不连(命名关联,可以省略)
        .CLKFBOUT   (clk_fb_raw    ),
        .CLKOUT0    (clk_adc_raw   ),
        // 输入时钟控制
        .CLKFBIN    (clk_fb_buf    ),
        .CLKIN1     (s_axi_aclk    ),
        .CLKIN2     (1'b0          ),
        .CLKINSEL   (1'b1          ),            // 恒选 CLKIN1
        // 动态重配置口 —— 本设计不用,全部接死
        .DADDR      (7'h0          ),
        .DCLK       (1'b0          ),
        .DEN        (1'b0          ),
        .DI         (16'h0         ),
        .DWE        (1'b0          ),
        // 动态相移口 —— 本设计不用(相位调整走 adc_capture 的 PH_OFF,不是这里)
        .PSCLK      (1'b0          ),
        .PSEN       (1'b0          ),
        .PSINCDEC   (1'b0          ),
        // 控制与状态
        .LOCKED     (mmcm_locked   ),
        .PWRDWN     (1'b0          ),
        .RST        (mmcm_rst      )
    );

    BUFG u_adc_fb_buf  (.O(clk_fb_buf  ), .I(clk_fb_raw ));
    BUFG u_adc_clk_buf (.O(clk_adc_base), .I(clk_adc_raw));

    // ⚠️ ADC 的低有效复位请求 = cfg_clk_release & s_axi_aresetn & mmcm_locked。
    //    末一项让它**锁不住就保持复位**,而不是让采集逻辑跑在一个
    //    频率错误的时钟上。adc_capture 内部对它做"异步置位 / 同步释放"。
    wire adc_rst_n_async = s_axi_aresetn & cfg_clk_release & mmcm_locked;

    adc_capture u_adc_capture (
        .clk_base        (clk_adc_base       ),
        .rst_n_async     (adc_rst_n_async    ),
        .mmcm_locked     (mmcm_locked        ),
        .clk_axi         (s_axi_aclk         ),

        // cfg_* —— 位域切分:**在这里切**,而不是把 32 位原值整个丢进去。
        //   ⇒ adc_capture 的端口边界自带契约,读代码的人不用回头查寄存器表。
        .cfg_div         (cfg_adc_cfg[ 7: 0] ),
        .cfg_ph_off      (cfg_adc_cfg[15: 8] ),   // 有符号
        .cfg_cont        (cfg_adc_cfg[16]    ),
        .cfg_oe_n        (cfg_adc_cfg[17]    ),
        .cfg_pdn         (cfg_adc_cfg[18]    ),
        .cfg_go          (cfg_adc_go         ),

        // ADC 物理引脚
        .adc_clk         (adc_clk            ),
        .adc_oe_n        (adc_oe_n           ),
        .adc_pdn         (adc_pdn            ),
        .adc_d           (adc_d              ),
        .adc_otr         (adc_otr            ),

        // 状态 → 从机 0x90
        //   [0] locked [1] done [2] busy [3] otr_sticky,其余保留读 0
        .sts_mmcm_locked (sts_adc_sts[0]     ),
        .sts_done        (sts_adc_sts[1]     ),
        .sts_busy        (sts_adc_sts[2]     ),
        .sts_otr         (sts_adc_sts[3]     ),

        // 缓冲读口 → 从机 0x94/0x98
        .rd_idx          (cfg_adc_buf_idx[11:0]),
        .rd_data         (sts_adc_buf_data[11:0]),
        .rd_otr          (sts_adc_buf_data[12]  )
    );

    // sts_adc_sts / sts_adc_buf_data 的【未用高位】显式接 0。
    //   ⚠️ 不接的话它们悬空,综合出 warning,读回值还不确定 ——
    //      而"未实现位读 0"是本文件对 0x64~0x7C 立下的既有约定,这里要一致。
    assign sts_adc_sts[31:4]        = 28'h0;
    assign sts_adc_buf_data[31:13]  = 19'h0;

endmodule
