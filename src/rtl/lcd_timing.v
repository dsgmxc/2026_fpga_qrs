`timescale 1ns / 1ps
//=============================================================================
//  lcd_timing.v  ——  RGB LCD 行/场时序发生器(自研,取代厂商 lcd_driver 的时序部分)
//
//  归属:Claude 代写(2026-10-02,用户明确要求「按照你推荐的方式对 pl 端修改」,
//        CLAUDE.md §6.0 显式例外)
//
//  【为什么要有这个文件】
//    厂商 lcd_driver.v 把【时序生成】和【输出级】揉在一个模块里,想加帧缓存通路就
//    绕不开它。本模块只干一件事:产生行/场计数与坐标,并把「取数」「送屏」两个
//    节拍显式引出来。输出级(lcd_rgb / lcd_de / lcd_clk / lcd_bl / lcd_rst)
//    搬到 cv_sys 里三行 assign 搞定。
//
//  【★ 相位约定 —— 改这个文件前必须先看懂这段】
//    厂商原链是三级,本模块逐级复刻,不许动:
//
//        hier  |  req_c(t)  →  de_req(t+1)  →  de_act(t+2)
//        含义  |  计数命中  |  取数请求      |  送屏 DE
//        px_x  |    —      |  ✔ 此刻有效    |    —
//        源    |    —      |    —          |  ✔ 必须已输出(源自带 1 拍寄存)
//
//    ⇒ **像素源必须是寄存输出**(lcd_display 是,framebuffer 行缓冲也是)。
//      若源是组合逻辑,画面会整体左移 2 像素。
//    ⇒ `de_act` 不是 `de_req` 的同拍,是【再延一拍】。写成 `de_act <= de_req`
//      是对的(总共 2 级);早早对齐反而错。
//
//  【相对厂商件的三处【有意】差异】
//    ① 🔴 修掉 y 坐标 off-by-one。
//       厂商:`pixel_ypos <= v_cnt + 11'b1 - (v_sync + v_back);`
//       本模块:`px_y       <= v_cnt          - V_ACT_BEG;`
//       厂商那行让 y ∈ **1..600** 而不是 0..599(最后一行整行错位、第 0 行永不出现)。
//       画竖彩条看不出来(彩条只依赖 x),但**帧缓存按行取数会整体错一行**,
//       且第 599 行取到的是行缓冲里的陈旧数据 —— 属于"能出图但图不对"的静默错误。
//    ② 新增 `line_start` / `frame_start` / `vblank` 三个节拍输出,供
//       lcd_fb_reader(上一行扫描期内预取下一行)与 lcd_vblank_ctrl(仅消隐期换址)使用。
//       ⚠️ 这是**新增端口,不改既有相位** —— 上面那三级链一字未动。
//    ③ HS/VS 由恒高改为**参数可切**,默认仍恒高(见下)。
//
//  【★ HS/VS 为什么默认还是恒高】
//    厂商原文:`//RGB LCD 采用DE模式时,行场同步信号需要拉高` —— 即 `assign lcd_hs = 1'b1;`。
//    **这块屏现在就是这么跑通的**,说明 DE 模式下屏不吃 HS/VS。
//    本模块给出真实脉冲的生成能力(HS_VS_MODE=1),但**极性未经查证**:
//    数据手册只给了脉宽(H_SYNC=20 / V_SYNC=3),没给有效电平。
//    ⇒ 默认 HS_VS_MODE=0 保持恒高 = 保持已验证的配置。
//    ⇒ 要开脉冲,先翻屏的手册确认 HS_POL/VS_POL,别猜。
//      (B04 取舍:脉冲不带来任何功能收益,却引入一个未验证变量 —— 不换。)
//
//  【本模块【不】做 / 【不】含】
//    · 不含 lcd_rgb / lcd_de / lcd_clk / lcd_bl / lcd_rst —— 输出级在 cv_sys
//    · 不含任何 AXI / 寄存器 —— 纯时序,零配置面
//    · 不含字模 / 描点 / 曲线缓存 —— 那是 D22 旧方案,已降级为 P5 后可选加速
//=============================================================================

module lcd_timing #(
    //-------------------------------------------------------------------------
    // 行时序(单位:clk_lcd 拍)—— ATK-MD0700R-1024600-IPS,即厂商 ID 0x7016
    //   计数布局:[HSYNC][H_BACK][有效 1024][H_FRONT] = 1344
    //-------------------------------------------------------------------------
    parameter integer H_SYNC  = 20,
    parameter integer H_BACK  = 140,
    parameter integer H_DISP  = 1024,
    parameter integer H_FRONT = 160,
    parameter integer H_TOTAL = 1344,

    //-------------------------------------------------------------------------
    // 场时序(单位:行)
    //   计数布局:[VSYNC][V_BACK][有效 600][V_FRONT] = 635
    //-------------------------------------------------------------------------
    parameter integer V_SYNC  = 3,
    parameter integer V_BACK  = 20,
    parameter integer V_DISP  = 600,
    parameter integer V_FRONT = 12,
    parameter integer V_TOTAL = 635,

    //-------------------------------------------------------------------------
    // HS/VS 输出方式
    //   0 = 恒高(厂商行为,**默认**,当前屏实测可用)
    //   1 = 输出真实同步脉冲(⚠️ 极性问题见文件头,需查手册后再开)
    //-------------------------------------------------------------------------
    parameter integer HS_VS_MODE = 0,
    parameter integer HS_POL     = 1,   // HS_VS_MODE=1 时有效:1=高有效
    parameter integer VS_POL     = 1    // HS_VS_MODE=1 时有效:1=高有效
)(
    input  wire        clk_lcd,     // = clk_axi = 设计 50 MHz / 实测 41.6667 MHz
    input  wire        rst_n,       // 低有效

    //-------------------------------------------------------------------------
    // 取数侧(源在此刻给下一拍的像素)
    //-------------------------------------------------------------------------
    output reg         de_req,      // 取数请求 1 拍脉冲区:此刻 px_x/px_y 有效
    output reg  [10:0] px_x,        // 0 .. H_DISP-1
    output reg  [10:0] px_y,        // 0 .. V_DISP-1(★ 已修掉厂商的 off-by-one)

    //-------------------------------------------------------------------------
    // 送屏侧
    //-------------------------------------------------------------------------
    output reg         de_act,      // 与像素源输出同拍 ⇒ cv_sys 用它选通 lcd_rgb
    output reg         vblank,      // 场消隐区(含 VSYNC 段)高
    output reg         line_start,  // 每个有效行的【取数起点】1 拍脉冲 → fb_reader 预取下一行
    output reg         frame_start, // 进入场消隐首拍 1 拍脉冲 → vblank_ctrl 换址安全点

    //-------------------------------------------------------------------------
    // 面板参数回读(给 lcd_display 用,商品件原样保留)
    //-------------------------------------------------------------------------
    output wire [10:0] h_disp,
    output wire [10:0] v_disp,

    //-------------------------------------------------------------------------
    // HS/VS
    //-------------------------------------------------------------------------
    output wire        lcd_hs,
    output wire        lcd_vs
);

    //=========================================================================
    //  0. 常量整理
    //     ⚠️ 比较/相减一律用【显式定位宽】的 localparam,不直接拿 integer 参数
    //        参与运算 —— 否则表达式位宽按 32 位扩展再截断,位宽警告里分不清真伪。
    //=========================================================================
    localparam integer H_ACT_BEG = H_SYNC + H_BACK;         // 有效行起点(计数口径)  = 160
    localparam integer V_ACT_BEG = V_SYNC + V_BACK;         // 有效区起点(计数口径)  =  23
    localparam integer H_ACT_END = H_ACT_BEG + H_DISP;      // 有效行终点(不含)      = 1184
    localparam integer V_ACT_END = V_ACT_BEG + V_DISP;      // 有效区终点(不含)      = 623

    // ★ 取数比送屏提前 2 拍 —— 沿袭厂商相位,见文件头【相位约定】
    localparam integer H_REQ_BEG = H_ACT_BEG - 2;           // = 158
    localparam integer H_REQ_END = H_ACT_END - 2;           // = 1182

    localparam [10:0] H_REQ_BEG_V = H_REQ_BEG[10:0];        // 11 位形式,便于与 h_cnt 相减
    localparam [10:0] H_REQ_END_V = H_REQ_END[10:0];        // ⚠️ 上界必须用【取数】终点 1182,
    localparam [10:0] V_ACT_BEG_V = V_ACT_BEG[10:0];        //    不是有效区终点 1184 —— 见 x_win
    localparam [10:0] V_ACT_END_V = V_ACT_END[10:0];
    localparam [10:0] H_DISP_V    = H_DISP[10:0];
    localparam [10:0] V_DISP_V    = V_DISP[10:0];
    localparam [10:0] H_TOTAL_M1  = (H_TOTAL - 1);          // 1343
    localparam [ 9:0] V_TOTAL_M1  = (V_TOTAL - 1);          //  634

    assign h_disp = H_DISP_V;
    assign v_disp = V_DISP_V;

    //=========================================================================
    //  1. 行 / 场 计数器
    //     h_cnt:0 .. H_TOTAL-1 = 0 .. 1343  → 11 位够(1344 < 2048)
    //     v_cnt:0 .. V_TOTAL-1 = 0 ..  634  → 10 位够
    //=========================================================================
    reg [10:0] h_cnt;
    reg [ 9:0] v_cnt;

    always @(posedge clk_lcd or negedge rst_n) begin
        if (!rst_n)                          h_cnt <= 11'd0;
        else if (h_cnt == H_TOTAL_M1)        h_cnt <= 11'd0;
        else                                 h_cnt <= h_cnt + 11'd1;
    end

    always @(posedge clk_lcd or negedge rst_n) begin
        if (!rst_n)                          v_cnt <= 10'd0;
        else if (h_cnt == H_TOTAL_M1) begin
            if (v_cnt == V_TOTAL_M1)         v_cnt <= 10'd0;
            else                             v_cnt <= v_cnt + 10'd1;
        end
    end

    //=========================================================================
    //  2. 有效区判据(组合,基于【当前】计数)
    //=========================================================================
    // 🔴 上界是 H_REQ_END_V(1182),【不是】H_ACT_END_V(1184)。
    //    写成后者 ⇒ 取数窗 158..1183 共 1026 拍,px_x 会跑到 1025 —— 每行多发 2 个像素。
    //    这是小规模仿真抓出来的(xsim 实测 1231200 = 1026×1200),别手滑改回去。
    wire x_win = (h_cnt >= H_REQ_BEG_V) && (h_cnt < H_REQ_END_V);
    wire y_win = (v_cnt >= V_ACT_BEG_V) && (v_cnt < V_ACT_END_V);
    wire req_c = x_win & y_win;

    wire active_v = (v_cnt >= V_ACT_BEG_V) && (v_cnt < V_ACT_END_V);
    wire vblank_c = ~active_v;

    //=========================================================================
    //  3. 三级相位链(见文件头【相位约定】,顺序不可调)
    //     t   : req_c = 1
    //     t+1 : de_req = 1,px_x/px_y 有效  ← 源在此采样
    //     t+2 : de_act = 1,源已输出像素  ← cv_sys 在此选通 lcd_rgb
    //=========================================================================
    always @(posedge clk_lcd or negedge rst_n) begin
        if (!rst_n) begin
            de_req     <= 1'b0;
            de_act     <= 1'b0;
            px_x       <= 11'd0;
            px_y       <= 11'd0;
            line_start <= 1'b0;
        end else begin
            de_req <= req_c;
            de_act <= de_req;                       // ★ 再延一拍,勿改成与 de_req 同拍

            // x:仅在取数窗内更新(窗外清 0,与厂商一致)
            px_x <= x_win ? (h_cnt - H_REQ_BEG_V) : 11'd0;

            // y:★ 相对厂商删掉了 `+ 11'b1` —— 那一项让 y ∈ 1..600
            //    判据用 y_win(不含 x 条件),与厂商逐字一致,水平消隐期 px_y 保持行号
            px_y <= y_win ? (v_cnt - V_ACT_BEG_V) : 11'd0;

            // 每有效行取数首拍 → 1 拍脉冲。fb_reader 据此预取【下一行】
            line_start <= (h_cnt == H_REQ_BEG_V) && y_win;
        end
    end

    //=========================================================================
    //  4. 场消隐节拍
    //     ⚠️ 换帧缓存地址只能在 vblank 内做。frame_start 取 vblank 的【上升沿】,
    //        即最后一个有效行扫完后的首拍 —— 此时 de_act 早已拉低,行缓冲空闲。
    //=========================================================================
    always @(posedge clk_lcd or negedge rst_n) begin
        if (!rst_n) begin
            vblank      <= 1'b0;
            frame_start <= 1'b0;
        end else begin
            vblank      <= vblank_c;
            frame_start <= vblank_c & ~vblank;
        end
    end

    //=========================================================================
    //  5. HS / VS
    //     计数布局 [sync][back][active][front] 与标准一致 ⇒ 脉冲落在 h_cnt/v_cnt 低段。
    //=========================================================================
    generate
        if (HS_VS_MODE == 0) begin : g_sync_tied_high
            // 默认:DE 模式,恒高(厂商行为,当前屏实测可用)
            assign lcd_hs = 1'b1;
            assign lcd_vs = 1'b1;
        end else begin : g_sync_pulse
            // ⚠️ 极性问题未查证,开启前请核对屏手册
            assign lcd_hs = (HS_POL != 0) ? (h_cnt < H_SYNC) : ~(h_cnt < H_SYNC);
            assign lcd_vs = (VS_POL != 0) ? (v_cnt < V_SYNC) : ~(v_cnt < V_SYNC);
        end
    endgenerate

endmodule
