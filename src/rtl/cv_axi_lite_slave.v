`timescale 1ns / 1ps
//=============================================================================
//  cv_axi_lite_slave.v  ——  手写 AXI4-Lite 从机 + 寄存器映射(P2 stub 版)
//
//  归属:Claude 代写(2026-09-29,用户明确要求,CLAUDE.md §6.0 显式例外)
//
//  【本次范围】共实现 11 个寄存器:
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
//  寄存器映射(基址 0x43C0_0000,字节偏移 → 字索引 a[6:2])
//
//    0x00 5'h00 CTRL        0x28 5'h0A I_A
//    0x04 5'h01 STATUS      0x2C 5'h0B Q_A
//    0x08 5'h02 NCO_FREQ    0x30 5'h0C I_B
//    0x0C 5'h03 NCO_PHASE   0x34 5'h0D Q_B
//    0x10 5'h04 EXC_AMP     0x38 5'h0E MAG_A
//    0x14 5'h05 BIAS_CODE   0x3C 5'h0F PHASE_A
//    0x18 5'h06 RANGE_SEL   0x40 5'h10 MAG_B
//    0x1C 5'h07 RELAY_CFG   0x44 5'h11 PHASE_B
//    0x20 5'h08 CIC_DECIM   0x48 5'h12 VERSION    <-- 已实现
//    0x24 5'h09 SETTLE_CFG  0x4C 5'h13 SCRATCH    <-- 已实现
//                           0x50 5'h14 LCD_CTRL   <-- 2026-10-01 新增
//                           0x54 5'h15 LCD_FILL   <-- 2026-10-01 新增
//                           0x58 5'h16 LCD_FB_BASE0    <-- 2026-10-02 新增
//                           0x5C 5'h17 LCD_FB_BASE1    <-- 2026-10-02 新增
//                           0x60 5'h18 LCD_SWAP        <-- 2026-10-02 新增
//                           0x64 5'h19 LCD_FB_STATUS   <-- 2026-10-02 新增
//                           0x68 5'h1A TOUCH_STATUS    <-- 2026-10-02 新增
//                           0x6C 5'h1B TOUCH_PT0       <-- 2026-10-02 新增
//                           0x70 5'h1C TOUCH_PT1       <-- 2026-10-02 新增
//                           0x74 / 0x78 / 0x7C 预留给 P4 安全链(WATCHDOG_KICK /
//                           FAULT_LATCH / SAFETY_STATE),**本次不实现**
//
//  ⚠️ 加完这 7 个后 32 个地址【全满】。再要加寄存器,必须先改
//     C_S_AXI_ADDR_WIDTH 7→8 + BD Refresh Module Reference(决策 Q27)。
//
//  ⚠️ C_S_AXI_ADDR_WIDTH 必须 >= 7。用 5 位时 0x4C & 0x1F = 0x0C,
//     会与 NCO_PHASE 混叠。
//=============================================================================

module cv_axi_lite_slave #(
    parameter integer C_S_AXI_DATA_WIDTH = 32,
    parameter integer C_S_AXI_ADDR_WIDTH = 7,
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
    (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI,PROTOCOL AXI4LITE,ADDR_WIDTH 7,DATA_WIDTH 32,READ_WRITE_MODE READ_WRITE,HAS_BURST 0,HAS_LOCK 0,HAS_CACHE 0,HAS_PROT 0,HAS_QOS 0,HAS_REGION 0,HAS_WSTRB 1,HAS_BRESP 1,HAS_RRESP 1,SUPPORTS_NARROW_BURST 0,MAX_BURST_LENGTH 1,NUM_READ_OUTSTANDING 1,NUM_WRITE_OUTSTANDING 1,ID_WIDTH 0,AWUSER_WIDTH 0,ARUSER_WIDTH 0,WUSER_WIDTH 0,RUSER_WIDTH 0,BUSER_WIDTH 0,FREQ_HZ 50000000" *)
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
    //  字索引:字节地址 [6:2] → 5 位寄存器编号(0x00 ~ 0x1F)
    //=========================================================================
    localparam [4:0] IDX_VERSION  = 5'h12;  // 0x48
    localparam [4:0] IDX_SCRATCH  = 5'h13;  // 0x4C
    localparam [4:0] IDX_LCD_CTRL = 5'h14;  // 0x50
    localparam [4:0] IDX_LCD_FILL = 5'h15;  // 0x54

    localparam [4:0] IDX_FB_BASE0  = 5'h16;  // 0x58
    localparam [4:0] IDX_FB_BASE1  = 5'h17;  // 0x5C
    localparam [4:0] IDX_LCD_SWAP  = 5'h18;  // 0x60
    localparam [4:0] IDX_FB_STATUS = 5'h19;  // 0x64
    localparam [4:0] IDX_TC_STATUS = 5'h1A;  // 0x68
    localparam [4:0] IDX_TC_PT0    = 5'h1B;  // 0x6C
    localparam [4:0] IDX_TC_PT1    = 5'h1C;  // 0x70

    // 调试观测(2026-10-03 加)—— 地址窗口本来就只有 0x00~0x7C,
    //   这三个是仅剩的空位(cv_regs.py 与 cv_sys.v 里原先标着"预留 P4 安全链",
    //   实际全工程无处使用 ⇒ 现在启用,并把那两处注释改掉)。
    localparam [4:0] IDX_DBG0      = 5'h1D;  // 0x74 本帧 rvalid 拍数
    localparam [4:0] IDX_DBG1      = 5'h1E;  // 0x78 最近 AR 地址
    localparam [4:0] IDX_DBG2      = 5'h1F;  // 0x7C 状态快照

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

    // ⚠️ 2026-10-01 由 if-else 链改为 case —— 与读通道写法一致,便于继续扩表。
    always @(posedge s_axi_aclk) begin
        if (!s_axi_aresetn) begin
            scratch  <= 32'h0000_0000;
            lcd_ctrl <= 32'h0000_0000;      // ⚠️ 复位 = 0 = 彩条,【有意为之】——
            lcd_fill <= 32'h0000_0000;      //    上电默认走厂商彩条那条路径。
            fb_base0 <= 32'h0000_0000;      //    复位 = 0 也是【有意为之】:
            fb_base1 <= 32'h0000_0000;      //    src_sel 不为 2 时基址根本不被使用,
        end else if (do_write) begin        //    PS 必须先写基址再切源,顺序错就是黑屏。
            case (awaddr_r[6:2])
                IDX_SCRATCH  : scratch  <= wdata_r;   // 未实现 WSTRB:整字写
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
            if (do_write && (awaddr_r[6:2] == IDX_LCD_SWAP)) begin
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
            if (do_write && (awaddr_r[6:2] == IDX_FB_STATUS)) begin
                clr_sticky_r <= 1'b1;                // 写入值无关:写什么都清
            end
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
                     && (s_axi_araddr[6:2] == IDX_TC_STATUS)) begin
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
            case (s_axi_araddr[6:2])
                IDX_VERSION  : rdata_r <= VERSION_CONST;
                IDX_SCRATCH  : rdata_r <= scratch;
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
    //  【扩成完整 20 寄存器时要补的两条硬性约束,见设计清单 §3】
    //   ① NCO_FREQ(0x08)/ CIC_DECIM(0x20) 的写入必须在测量间隙生效:
    //      测量中写入 → 挂起到下一次 SET_BIAS 才生效,并置 pending 标志。
    //   ② RELAY_CFG(0x1C) 写入要过互锁校验,非法组合拒绝写入并置
    //      err_code(STATUS[15:8]),而不是执行。
    //
    //  【cfg_ / sts_ 端口】R2 规格是 S_AXI ↔ cfg_* / sts_*。
    //    2026-10-01 起有 cfg_lcd_ctrl / cfg_lcd_fill;
    //    2026-10-02 起再加 cfg_lcd_fb_base0/1、cfg_lcd_swap_tgt/req、
    //    sts_lcd_status、sts_touch_status/pt0/pt1 —— 共 11 个寄存器对上号。
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
    //    ★ 这条与「RELAY_REQUEST / RELAY_APPLIED 必须分开」是同一条原则,
    //      将来写继电器时照抄。
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
