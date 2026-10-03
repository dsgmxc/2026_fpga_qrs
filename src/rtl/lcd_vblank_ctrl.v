`timescale 1ns / 1ps
//=============================================================================
//  lcd_vblank_ctrl.v  ——  帧缓存换址控制器(把"防撕裂"从软件自律变成硬件保证)
//
//  归属:Claude 代写(2026-10-02,用户明确要求,CLAUDE.md §6.0 显式例外)
//
//  【它解决什么问题】
//    PS 渲染完一帧后要切前台。**如果 PS 想切就切**,扫描正好扫到该帧中间时,
//    屏幕上半是新帧、下半是旧帧 —— 这就是撕裂。
//    前序 STM32 版在 **PS 侧**用 `LCD_LTDC_SetFrameBufferVBlank(addr, 50ms)` 做,
//    靠软件等消隐 + 超时兜底,**本质上是"软件自律"**。
//    搬到 PL 后:PS 只能**提出请求**,换址时刻由硬件说了算 ——
//    **PS 无论如何都无法在扫描中途改前台**。这是硬性保证,不是纪律。
//
//  【契约(与 cv_axi_lite_slave.v 的 0x60 读口径一一对应)】
//    · 收到 `swap_req` → 置 `pending`(已受理,**未生效**)
//    · 下一个 `frame_start`(进入场消隐首拍)→ 才翻 `front`、清 `pending`
//    ⇒ 读 0x60 得到 `[0]`=front(已生效)`[1]`=pending(已受理未生效)。
//      ★ **请求与生效必须分开** —— 否则 PS 会在换址尚未发生时就宣称"切换完成"。
//      ★ 同一条原则适用于将来的 `RELAY_REQUEST` / `RELAY_APPLIED`。
//
//  【为什么 pending 存在这里而不是从机】
//    pending 的真值持有者**只能是本模块** —— 只有它知道换址到底发生过没有。
//    从机若自己锁一个 pending 并自行清除,就出现【两个事实源】。
//    因此从机只发脉冲,状态一律由本模块经 `sts_lcd_status[1:0]` 透出去。
//
//  【与 lcd_fb_reader 的时序配合(★ 改前必读)】
//    `frame_start` 同拍做两件事:
//      本模块 : front 翻转 ⇒ 组合出的 `fb_base` 立刻变成新基址
//      reader : 置预取请求(取第 0 行)
//    下一拍 reader 才锁 `m_axi_araddr <= fb_base + 行偏移`,
//    那时 `fb_base` 已是**新基址** ⇒ **新的一帧从新帧缓存取**,一帧之内不会混源。
//
//  【同一拍到达的 swap_req 不会被吃掉】
//    `frame_start` 分支在前、`swap_req` 分支在后,后者覆盖 ⇒ 恰好卡在消隐首拍的
//    请求会保留到**下一帧**才生效,而不是被静默丢弃。
//=============================================================================

module lcd_vblank_ctrl (
    input  wire        clk,
    input  wire        rst_n,

    //-------------------------------------------------------------------------
    // 节拍 / 请求
    //-------------------------------------------------------------------------
    input  wire        frame_start,     // 场消隐首拍(1 拍脉冲),来自 lcd_timing
    input  wire        swap_req,        // 换址请求(1 拍脉冲),来自 0x60 写
    input  wire        swap_tgt,        // 目标缓冲索引(电平),来自 0x60 写

    //-------------------------------------------------------------------------
    // 两块帧缓存基址(PS 分配后写 0x58 / 0x5C)
    //-------------------------------------------------------------------------
    input  wire [31:0] fb_base0,
    input  wire [31:0] fb_base1,

    //-------------------------------------------------------------------------
    // 子模块状态(粘滞,来自 lcd_fb_reader)
    //-------------------------------------------------------------------------
    input  wire        underrun,
    input  wire        axi_err,

    //-------------------------------------------------------------------------
    // 输出
    //-------------------------------------------------------------------------
    output wire [31:0] fb_base,         // 给 lcd_fb_reader
    output wire [31:0] sts_lcd_status   // 给 0x64 读,低两位同时供 0x60 读
);

    //=========================================================================
    //  状态
    //=========================================================================
    reg        front;           // 当前前台缓冲索引(0/1)—— **已生效**
    reg        pending;         // 有换址请求未生效
    reg        pending_tgt;     // 该请求的目标索引
    reg [8:0]  vblank_cnt;      // 场消隐计数(9 位)
    reg [7:0]  swap_cnt;        // 成功换址次数(8 位)

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            front       <= 1'b0;
            pending     <= 1'b0;
            pending_tgt <= 1'b0;
            vblank_cnt  <= 9'd0;
            swap_cnt    <= 8'd0;
        end else begin
            if (frame_start) begin
                vblank_cnt <= vblank_cnt + 9'd1;
                if (pending) begin
                    front      <= pending_tgt;      // ★ 只在这里翻前台
                    swap_cnt   <= swap_cnt + 8'd1;
                    pending    <= 1'b0;
                end
            end

            // ⚠️ 这一块必须写在 frame_start 之后 —— 后写覆盖。
            //    若 swap_req 恰好与 frame_start 同拍,生效顺延到下一帧,
            //    但**请求不丢**。反过来写会把这个请求静默吃掉。
            if (swap_req) begin
                pending     <= 1'b1;
                pending_tgt <= swap_tgt;
            end
        end
    end

    //=========================================================================
    //  输出
    //=========================================================================
    assign fb_base = front ? fb_base1 : fb_base0;

    // 位域(与 docs/display_subsystem.md §5.2 的 0x64 定义一致):
    //   [ 0]     front
    //   [ 1]     pending            ← 也被 0x60 读带走
    //   [10: 2]  vblank_cnt   (9 位)
    //   [18:11]  swap_cnt     (8 位)
    //   [19]     underrun(粘滞,来自 reader)
    //   [20]     axi_err (粘滞,来自 reader)
    //   [31:21]  保留 = 0
    assign sts_lcd_status = {11'b0, axi_err, underrun, swap_cnt, vblank_cnt, pending, front};

endmodule
