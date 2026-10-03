`timescale 1ns / 1ps
//=============================================================================
//  i2c_master.v  ——  开漏 I2C 主机(字节级命令接口)
//
//  归属:Claude 代写(2026-10-02,用户授权「完成 ps 程序设计以外的其他工作」,
//        CLAUDE.md §6.0 显式例外)
//
//  【为什么自己写,不用 IP / 不用 EMIO】—— 决策 D23
//    · 4 根触摸线(touch_scl/sda/int/rst_n)**只接在 PL 上**,PS MIO 接不到;
//    · 走 EMIO 要改 PS 配置 + 改 BD,而当前是 PL-only 顶层,代价大;
//    · 与既有 `spi_master` **同构**(都是"自写一个协议主机"),不动已跑通的架构;
//    · 🔑 I2C 的 **ACK / 开漏 / 时钟拉伸** 恰好是赛题考察点「接口协议」的好载体
//      —— 这三样在 SPI 里根本不存在,是本模块的主要设计价值。
//
//  【开漏语义(★ 改前必读)】
//    本模块**从不驱动 SCL/SDA 为高**,只做两件事:
//      · 拉低(scl_oe/sda_oe = 1)
//      · 释放(高阻,靠板上上拉电阻回到高)
//    ⇒ 端口必须是 `inout`,顶层要接 IOBUF。
//    ⇒ 若谁把它改成"推挽输出",**多主机会烧管子**,且从机的时钟拉伸会被顶掉。
//
//  【时钟拉伸(stretching)—— I2C 与 SPI 的分水岭】
//    从机可以在任意时刻把 SCL 拉低不放,表示"我还没准备好"。主机**必须等**。
//    实现:`tcnt`(半周期计数)**只在 `scl_in` 真的为高时才推进**;
//    另设 `scnt` 无条件计数做**超时兜底**(否则从机死拉低会把主机永久挂死)。
//    ⚠️ 超时会脉冲 `err` 并**释放总线** —— 超时不是异常,是**必须有的出口**。
//
//  【为什么是字节级而不是位级接口】
//    GT911 的一次读是 "START + 器件地址 + 16 位字地址 + repeated START +
//    器件地址|1 + 读 N 字节 + STOP"。位级接口要上层自己数位、自己管 ACK,
//    两个状态机会纠缠在一起;字节级把"每个字节"作为一次原子命令,
//    **上层只需数字节**,两个状态机各自干净。
//
//  【命令(每次一个 1 拍脉冲;`busy` 期间不得再发)】
//    OP_START   产生 START(或 repeated START)
//    OP_WRITE   写 tx_byte,从机回的第 9 位读进 ack_rx(0 = ACK)
//    OP_RD_ACK  读一字节 → rx_byte,主机回 **ACK**  (后面还有字节)
//    OP_RD_NAK  读一字节 → rx_byte,主机回 **NACK** (最后一个字节)
//    OP_STOP    产生 STOP,释放总线
//    OP_RECOV   总线恢复:9 个 SCL 脉冲 + STOP
//
//  【为什么要 OP_RECOV】—— 前序项目的实战教训
//    主从在**字节中途**失步(例如主机被复位打断)时,从机会一直拉着 SDA 不放,
//    此后任何 START 都无效 —— 表现为"总线死了"。
//    标准恢复手法:主机发 **9 个 SCL 脉冲**,把从机剩下的位全部走完,
//    它就会释放 SDA;再补一个 STOP 让总线回到空闲。
//    出处:前序 `lcd_touch.c` 的 `GT911_I2cRecover()`
//    (9 次 `SCL 0→1` + `GT911_I2cStop()`)。
//
//  【时钟口径纪律】
//    `CLK_FREQ_HZ` 是**设计值**。本项目 `clk_axi` 实测 41.6667 MHz
//    (CLAUDE.md §八)⇒ 实际 SCL 只有设计值的 83.3%。
//    ⚠️ 引用 SCL 频率必须写「设计 x kHz / 实测 y kHz」,**不得单写一个数**。
//=============================================================================

module i2c_master #(
    // ⚠️ 设计值。实测 clk_axi = 41.6667 MHz ⇒ 实际 SCL = 设计值 × 0.833
    parameter integer CLK_FREQ_HZ     = 50_000_000,
    parameter integer SCL_FREQ_HZ     = 100_000,
    // 时钟拉伸超时(拍)。目的只是**保证能退出**,不是精确度量从机响应时间。
    // 4096 拍 @41.6667 MHz ≈ 98 µs —— 远大于 GT911 的最长拉伸,
    // 又远小于一次轮询周期(10 ms),不会拖慢正常流程。
    parameter integer STRETCH_TIMEOUT = 4096
)(
    input  wire       clk,
    input  wire       rst_n,

    //-----------------------------------------------------------------------
    // 命令接口
    //-----------------------------------------------------------------------
    input  wire       go,           // 1 拍脉冲
    input  wire [2:0] op,
    input  wire [7:0] tx_byte,
    output reg  [7:0] rx_byte,
    output reg        ack_rx,       // 写字节后从机的应答:1'b0 = ACK,1'b1 = NACK
    output reg        busy,
    output reg        done,         // 1 拍脉冲(出错时也会脉冲,见 err)
    output reg        err,          // 1 拍脉冲:拉伸超时

    //-----------------------------------------------------------------------
    // 开漏总线(顶层必须经 IOBUF 接到引脚)
    //-----------------------------------------------------------------------
    output wire       scl,          // 只拉低 / 释放
    output wire       sda,
    input  wire       scl_in,       // 引脚实际电平(拉伸检测)
    input  wire       sda_in        // 引脚实际电平(读数据 / 读 ACK)
);

    //=========================================================================
    //  参数与常量
    //=========================================================================
    // 实际 SCL 频率 = CLK/(2·HALF_T)。
    // ★ 用位宽固定的 localparam,不用 integer —— integer 参数的位选
    //   (`HALF_T[15:0]`)在部分工具上不可靠;而这里每次比较都要用到它。
    localparam [15:0] HALF_T   = (CLK_FREQ_HZ / SCL_FREQ_HZ) / 2;
    localparam [15:0] TO_P    = STRETCH_TIMEOUT[15:0];

    localparam [2:0] OP_START  = 3'd0;
    localparam [2:0] OP_WRITE  = 3'd1;
    localparam [2:0] OP_RD_ACK = 3'd2;
    localparam [2:0] OP_RD_NAK = 3'd3;
    localparam [2:0] OP_STOP   = 3'd4;
    localparam [2:0] OP_RECOV  = 3'd5;

    localparam [5:0] S_IDLE     = 6'd0;
    localparam [5:0] S_START_P  = 6'd1;    // 预备:SDA 释放、SCL 拉低
    localparam [5:0] S_START_A  = 6'd2;    // SCL 释放(高)+ SDA 已高 = 总线空闲态
    localparam [5:0] S_START_B  = 6'd3;    // SDA 下降沿 ← START
    localparam [5:0] S_START_C  = 6'd4;    // SCL 拉低
    localparam [5:0] S_WBIT_LO  = 6'd5;    // 放数据位,SCL 低
    localparam [5:0] S_WBIT_HI  = 6'd6;    // 释放 SCL,从机采样
    localparam [5:0] S_ACK_LO   = 6'd7;    // 释放 SDA,准备读应答
    localparam [5:0] S_ACK_HI   = 6'd8;    // SCL 高,采样 SDA = ACK
    localparam [5:0] S_ACK_END  = 6'd9;    // SCL 拉低
    localparam [5:0] S_RBIT_LO  = 6'd10;   // 释放 SDA,SCL 低
    localparam [5:0] S_RBIT_HI  = 6'd11;   // SCL 高,采样 SDA = 数据位
    localparam [5:0] S_MACK_LO  = 6'd12;   // 主机回 ACK/NACK,SCL 低
    localparam [5:0] S_MACK_HI  = 6'd13;   // SCL 高,从机采样
    localparam [5:0] S_MACK_END = 6'd14;   // 释放 SDA,SCL 拉低
    localparam [5:0] S_STOP_A   = 6'd15;   // SDA 拉低,SCL 释放
    localparam [5:0] S_STOP_B   = 6'd16;   // SDA 释放 ← STOP 上升沿
    localparam [5:0] S_REC_LO   = 6'd17;   // 恢复脉冲:SCL 低
    localparam [5:0] S_REC_HI   = 6'd18;   // 恢复脉冲:SCL 高
    localparam [5:0] S_REC_END  = 6'd21;   // 恢复收尾:SCL 压回低,再交 S_STOP_A
    localparam [5:0] S_END      = 6'd19;   // 统一收尾
    localparam [5:0] S_ABORT    = 6'd20;   // 超时收尾(拉低 SCL,释放 SDA)

    //=========================================================================
    //  状态
    //=========================================================================
    reg [5:0]  state;
    reg [15:0] tcnt;            // 半周期计数 —— ★ 只在 SCL 到位后推进
    reg [15:0] scnt;            // 拉伸超时计数 —— 无条件推进
    reg [3:0]  bit_cnt;         // 位序号 0..7
    reg [3:0]  rec_cnt;         // 恢复脉冲计数 0..8
    reg [7:0]  sr;              // 移位寄存器
    reg        ack_out;         // 主机要回的应答:1'b0 = ACK

    reg        scl_oe;          // 1 = 拉低
    reg        sda_oe;

    assign scl = scl_oe ? 1'b0 : 1'bz;
    assign sda = sda_oe ? 1'b0 : 1'bz;

    wire half_done  = (tcnt >= HALF_T - 16'd1);
    wire stretch_to = (scnt >= TO_P   - 16'd1);

    //=========================================================================
    //  主状态机
    //
    //  ★ 三条贯穿全篇的纪律(改任何一个状态前先读):
    //    1. **SDA 只在 SCL 为低时改变** —— SCL 高时 SDA 必须稳定,
    //       否则从机会把这次变化当成 START/STOP。
    //    2. **"SCL 高"的每个状态都要等 `scl_in` 真的到高** —— 这就是时钟拉伸;
    //       `scnt` 是无条件兜底,保证一定能退出。
    //    3. **字节命令之间 SCL 保持低** —— 见 S_END 的说明(那里曾经埋过一个 bug)。
    //=========================================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state    <= S_IDLE;
            tcnt     <= 16'd0;
            scnt     <= 16'd0;
            bit_cnt  <= 4'd0;
            rec_cnt  <= 4'd0;
            sr       <= 8'h00;
            ack_out  <= 1'b0;
            scl_oe   <= 1'b0;      // ★ 复位后**释放**总线,不拉低 ——
            sda_oe   <= 1'b0;      //   拉低会让从机以为自己在被寻址
            rx_byte  <= 8'h00;
            ack_rx   <= 1'b1;
            busy     <= 1'b0;
            done     <= 1'b0;
            err      <= 1'b0;
        end else begin
            done <= 1'b0;
            err  <= 1'b0;

            case (state)
                //-------------------------------------------------------------
                S_IDLE: begin
                    if (go) begin
                        busy <= 1'b1;
                        tcnt <= 16'd0;
                        scnt <= 16'd0;
                        case (op)
                            OP_START : state <= S_START_P;
                            OP_WRITE : begin
                                sr      <= tx_byte;
                                bit_cnt <= 4'd0;
                                state   <= S_WBIT_LO;
                            end
                            OP_RD_ACK: begin
                                ack_out <= 1'b0;   // 回 ACK:后面还有字节
                                bit_cnt <= 4'd0;
                                state   <= S_RBIT_LO;
                            end
                            OP_RD_NAK: begin
                                ack_out <= 1'b1;   // 回 NACK:这是最后一个字节
                                bit_cnt <= 4'd0;
                                state   <= S_RBIT_LO;
                            end
                            OP_STOP  : begin
                                sda_oe <= 1'b1;    // STOP 前先把 SDA 压低
                                state  <= S_STOP_A;
                            end
                            OP_RECOV : begin
                                rec_cnt <= 4'd0;
                                sda_oe  <= 1'b0;   // ★ 恢复时必须先释放 SDA
                                state   <= S_REC_LO;
                            end
                            default  : state <= S_END;
                        endcase
                    end
                end
                //-------------------------------------------------------------
                //  START 的预备相位 —— 为什么需要它
                //    若上一条命令是写字节(或中途出错),SDA 可能正被**拉低**。
                //    此时若把 SDA 和 SCL 同拍释放,SDA 上升与 SCL 上升会**赛跑**;
                //    万一 SDA 在 SCL 已经为高之后才上升,从机会把它读成 **STOP**!
                //    ⇒ 先"释放 SDA + 拉低 SCL"稳定住,再进正式 START 序列。
                //    代价只有半个 SCL 周期(5 µs),换掉一个偶发、极难复现的错帧。
                //-------------------------------------------------------------
                S_START_P: begin
                    sda_oe <= 1'b0;                 // 释放 SDA(回到高)
                    scl_oe <= 1'b1;                 // 拉低 SCL(我们驱动,不会拉伸)
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        scnt  <= 16'd0;
                        state <= S_START_A;
                    end else tcnt <= tcnt + 16'd1;
                end

                S_START_A: begin
                    scl_oe <= 1'b0;                 // 释放 SCL ⇒ 高。SDA 已是高 ⇒ 空闲态
                    if (stretch_to) begin
                        state <= S_ABORT;
                    end else if (scl_in) begin
                        if (half_done) begin
                            tcnt  <= 16'd0;
                            scnt  <= 16'd0;
                            state <= S_START_B;
                        end else tcnt <= tcnt + 16'd1;
                    end else scnt <= scnt + 16'd1;
                end

                S_START_B: begin
                    sda_oe <= 1'b1;                 // ★ SDA 下降沿,SCL 仍为高 ⇒ START
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        state <= S_START_C;
                    end else tcnt <= tcnt + 16'd1;
                end

                S_START_C: begin
                    scl_oe <= 1'b1;                 // SCL 拉低,总线被占用
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        state <= S_END;
                    end else tcnt <= tcnt + 16'd1;
                end
                //-------------------------------------------------------------
                //  写字节
                //-------------------------------------------------------------
                S_WBIT_LO: begin
                    scl_oe <= 1'b1;                 // SCL 低
                    // ★ 数据只在 SCL 低电平期间变化,且**整个低电平期间保持不变** ——
                    //   所以这里是电平赋值,**不在这里移位**。
                    sda_oe <= ~sr[3'd7];            // 位 = 0 ⇒ 拉低;位 = 1 ⇒ 释放
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        scnt  <= 16'd0;
                        state <= S_WBIT_HI;
                    end else tcnt <= tcnt + 16'd1;
                end

                S_WBIT_HI: begin
                    scl_oe <= 1'b0;                 // 释放 SCL,从机在这期间采样
                    if (stretch_to) begin
                        state <= S_ABORT;
                    end else if (scl_in) begin
                        if (half_done) begin
                            tcnt  <= 16'd0;
                            scnt  <= 16'd0;
                            // ★ 移位只在**一个位结束时发生一次**。
                            //   曾经写成"每个时钟沿都移"⇒ 一位移了 HALF_T 次,
                            //   发出去的字节全错,而且波形上看着"有时钟有数据",
                            //   非常像从机不响应。见文件尾备忘。
                            sr <= {sr[6:0], 1'b0};
                            if (bit_cnt == 4'd7) state <= S_ACK_LO;
                            else begin
                                bit_cnt <= bit_cnt + 4'd1;
                                state   <= S_WBIT_LO;
                            end
                        end else tcnt <= tcnt + 16'd1;
                    end else scnt <= scnt + 16'd1;
                end
                //-------------------------------------------------------------
                //  读应答(第 9 位)—— 这 1 位由**从机**驱动 SDA
                //-------------------------------------------------------------
                S_ACK_LO: begin
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b0;                 // ★ 必须释放,否则与从机对顶
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        scnt  <= 16'd0;
                        state <= S_ACK_HI;
                    end else tcnt <= tcnt + 16'd1;
                end

                S_ACK_HI: begin
                    scl_oe <= 1'b0;
                    if (stretch_to) begin
                        state <= S_ABORT;
                    end else if (scl_in) begin
                        // 采样一次即可(SCL 高电平期间 SDA 稳定);
                        // 用 tcnt==0 钉住"进入高电平后的第一拍",避免重复采样。
                        if (tcnt == 16'd0) ack_rx <= sda_in;   // 1'b0 = ACK
                        if (half_done) begin
                            tcnt  <= 16'd0;
                            scnt  <= 16'd0;
                            state <= S_ACK_END;
                        end else tcnt <= tcnt + 16'd1;
                    end else scnt <= scnt + 16'd1;
                end

                S_ACK_END: begin
                    scl_oe <= 1'b1;                 // 拉低,这一个字节结束
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        state <= S_END;
                    end else tcnt <= tcnt + 16'd1;
                end
                //-------------------------------------------------------------
                //  读字节
                //-------------------------------------------------------------
                S_RBIT_LO: begin
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b0;                 // 让出 SDA
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        scnt  <= 16'd0;
                        state <= S_RBIT_HI;
                    end else tcnt <= tcnt + 16'd1;
                end

                S_RBIT_HI: begin
                    scl_oe <= 1'b0;
                    if (stretch_to) begin
                        state <= S_ABORT;
                    end else if (scl_in) begin
                        if (half_done) begin
                            // ★ 同样:一个位只移一次。取**高电平末尾**采样,
                            //   此时数据最稳定(上升沿的振铃早已过去)。
                            sr <= {sr[6:0], sda_in};
                            tcnt  <= 16'd0;
                            scnt  <= 16'd0;
                            if (bit_cnt == 4'd7) state <= S_MACK_LO;
                            else begin
                                bit_cnt <= bit_cnt + 4'd1;
                                state   <= S_RBIT_LO;
                            end
                        end else tcnt <= tcnt + 16'd1;
                    end else scnt <= scnt + 16'd1;
                end
                //-------------------------------------------------------------
                //  主机回 ACK / NACK
                //  ★ 读的**最后一个字节必须回 NACK**;若回 ACK,从机会继续吐
                //    下一个字节,后面的 STOP 会被它当成一个数据位 ⇒ 错帧。
                //-------------------------------------------------------------
                S_MACK_LO: begin
                    scl_oe <= 1'b1;
                    sda_oe <= ~ack_out;             // ack_out=0(ACK) ⇒ 拉低
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        scnt  <= 16'd0;
                        state <= S_MACK_HI;
                    end else tcnt <= tcnt + 16'd1;
                end

                S_MACK_HI: begin
                    scl_oe <= 1'b0;
                    if (stretch_to) begin
                        state <= S_ABORT;
                    end else if (scl_in) begin
                        if (half_done) begin
                            tcnt    <= 16'd0;
                            scnt    <= 16'd0;
                            rx_byte <= sr;          // ★ 只在"读完一个字节"这条路上写
                            state   <= S_MACK_END;
                        end else tcnt <= tcnt + 16'd1;
                    end else scnt <= scnt + 16'd1;
                end

                S_MACK_END: begin
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b0;                 // 释放 SDA(SCL 已低)
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        state <= S_END;
                    end else tcnt <= tcnt + 16'd1;
                end
                //-------------------------------------------------------------
                //  STOP:SDA 在 SCL 高电平期间由低变高
                //-------------------------------------------------------------
                S_STOP_A: begin
                    sda_oe <= 1'b1;                 // 保证 SDA 是低的
                    scl_oe <= 1'b0;                 // 释放 SCL
                    if (stretch_to) begin
                        state <= S_ABORT;
                    end else if (scl_in) begin
                        if (half_done) begin
                            tcnt  <= 16'd0;
                            state <= S_STOP_B;
                        end else tcnt <= tcnt + 16'd1;
                    end else scnt <= scnt + 16'd1;
                end

                S_STOP_B: begin
                    sda_oe <= 1'b0;                 // ★ SDA 上升沿,SCL 为高 ⇒ STOP
                    if (half_done) begin
                        tcnt   <= 16'd0;
                        scl_oe <= 1'b0;             // 两根都释放 ⇒ 总线空闲
                        state  <= S_END;
                    end else tcnt <= tcnt + 16'd1;
                end
                //-------------------------------------------------------------
                //  总线恢复:9 个 SCL 脉冲(高电平期间 SDA 保持释放)
                //-------------------------------------------------------------
                S_REC_LO: begin
                    sda_oe <= 1'b0;
                    scl_oe <= 1'b1;
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        scnt  <= 16'd0;
                        state <= S_REC_HI;
                    end else tcnt <= tcnt + 16'd1;
                end

                S_REC_HI: begin
                    scl_oe <= 1'b0;
                    if (stretch_to) begin
                        state <= S_ABORT;
                    end else if (scl_in) begin
                        if (half_done) begin
                            tcnt <= 16'd0;
                            scnt <= 16'd0;
                            if (rec_cnt == 4'd8) begin
                                rec_cnt <= 4'd0;
                                // ★ 这里**不能**直接拉低 SDA 再进 S_STOP_A:
                                //   此刻 SCL 刚被释放、是**高**的;SCL 高时把 SDA
                                //   由高拉低 = **START**,不是 STOP 的前置。
                                //   那样总线上会凭空多出一个 START,从机被重新寻址。
                                //   ⇒ 先走 S_REC_END 把 SCL 压回低,再交给 S_STOP_A。
                                state   <= S_REC_END;
                            end else begin
                                rec_cnt <= rec_cnt + 4'd1;
                                state   <= S_REC_LO;
                            end
                        end else tcnt <= tcnt + 16'd1;
                    end else scnt <= scnt + 16'd1;
                end

                // 恢复收尾:把 SCL 压回低(SCL 低时 SDA 才能安全变化)
                S_REC_END: begin
                    scl_oe <= 1'b1;                 // SCL 低 ⇒ 不会产生 START/STOP
                    sda_oe <= 1'b1;                 // SDA 压低,给 STOP 做准备
                    if (half_done) begin
                        tcnt  <= 16'd0;
                        state <= S_STOP_A;
                    end else tcnt <= tcnt + 16'd1;
                end
                //-------------------------------------------------------------
                //  ★ 统一收尾 —— 所有命令都走这里
                //
                //  【这里曾经埋过一个 bug,别再犯】
                //    第一版在 S_END 里写了 `scl_oe <= 0; sda_oe <= 0;`(两根都释放),
                //    看着很"干净",实际是**错的**:
                //    字节命令结束时 SCL 本来是低的(我们拉着的),一释放就产生
                //    一个**上升沿**;此时 SDA 恰好也已被释放为高 ⇒ 从机会把这个
                //    边沿当作**下一个数据位**收进去,**字节边界就错位了**。
                //    正确做法:**SCL 归谁拉低就继续拉着**,由下一个命令
                //    (START_P / WBIT_LO / STOP_A …)自己决定什么时候松开。
                //    ⇒ 所以这里**一个 `scl_oe`/`sda_oe` 都不许碰**。
                //
                //  ★ 顺带:所有命令都从这里出去 ⇒ "某条路径忘了清 busy"
                //    这类 bug 在结构上就不可能发生。
                //-------------------------------------------------------------
                S_END: begin
                    busy   <= 1'b0;
                    done   <= 1'b1;
                    tcnt   <= 16'd0;
                    scnt   <= 16'd0;
                    state  <= S_IDLE;
                end
                //-------------------------------------------------------------
                //  超时收尾:拉低 SCL + 释放 SDA,避免占着总线不放
                //  ★ 同时脉冲 done 与 err —— 若只发 err 不发 done,
                //    上层的"等 done"会永远等下去(这是最典型的挂死形态)。
                //-------------------------------------------------------------
                S_ABORT: begin
                    scl_oe <= 1'b1;
                    sda_oe <= 1'b0;
                    busy   <= 1'b0;
                    done   <= 1'b1;
                    err    <= 1'b1;
                    tcnt   <= 16'd0;
                    scnt   <= 16'd0;
                    state  <= S_IDLE;
                end

                default: state <= S_IDLE;
            endcase
        end
    end

endmodule
