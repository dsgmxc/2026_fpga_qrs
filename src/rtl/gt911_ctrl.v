`timescale 1ns / 1ps
//=============================================================================
//  gt911_ctrl.v  ——  GT911 电容触摸控制器(PL 侧)
//
//  归属:Claude 代写(2026-10-02,用户授权「完成 ps 程序设计以外的其他工作」)
//
//  【职责边界】本模块只做三件事:
//    ① 上电复位序列(决定 I2C 器件地址)
//    ② 轮询 0x814E 状态 + 读触点坐标
//    ③ 状态上报(点数 / pen_down / err / seq)与两级错误容忍
//    ⇒ **不做**坐标变换、不做手势识别、不做去抖 —— 那些属于 PS/LVGL。
//
//  【GT911 协议要点(全部有出处,不是猜的)】
//    · **16 位寄存器地址**,传输序:START + 写地址 + 高字节 + 低字节 +
//      repeated START + 读地址 + 数据…  (前序 `lcd_touch.c` + 厂商 `touch_dri.v` 一致)
//    · 状态寄存器 **0x814E**:`bit7` = 有新帧;`[3:0]` = 触点数
//    · **清状态**:向 0x814E 写 0。不清则 bit7 一直是 1,会把同一帧重复报上来
//    · 触点从 **0x814F** 起,**每点 8 字节**:
//        +0 track_id, +1 x_lo, +2 x_hi, +3 y_lo, +4 y_hi, +5/+6 size, +7 保留
//      ⚠️ **两个参考源写的起始地址不一样**:厂商 FPGA 例程用 0x8150(只读 x/y),
//         前序 STM32 用 0x814F(整点 8 字节)。两者**都对**,只是起点不同。
//         本模块取 **0x814F**,因为要拿到 track_id(多指时用来跟踪同一根手指)。
//    · 器件地址 **0x14**(复位时 INT 为高)或 **0x5D**(INT 为低)。
//      本模块复位期间把 INT 驱动为**高** ⇒ 默认 0x14。
//
//  【★ 一条容易写反的规则:bit7 == 0 不等于"松手"】
//    bit7=0 只表示"**没有新帧**" —— 手指**保持按压但没动**时就是这样。
//    若把 bit7=0 当成松手,界面会出现"按着不动 → 突然抬起"的鬼畜。
//    ⇒ 明确释放**只在收到新帧且 count == 0** 时才发生。
//    出处:前序 `lcd_touch.c` 里那段长注释(`LCD_TOUCH_NO_UPDATE`)。
//
//  【两级错误容忍(阈值照抄前序 `main.c`,不是拍脑袋定的)】
//    · 连续错 **3** 次 → 总线恢复(9 个 SCL 脉冲 + STOP)
//    · 连续错 **5** 次 → 整个控制器重新走一遍复位序列
//    · 一次成功 → 计数清零
//    ⚠️ 前序还带"3 次错误就降级到 100 ms 慢轮询"。本模块**不做降级**:
//       错误恢复由上面两级处理,再叠一层轮询降级会让"到底是慢还是坏"难判。
//       **这是一处有意偏离,记在此处备查。**
//
//  【器件地址自愈】
//    若地址字节**连续 3 次没被 ACK**,自动在 0x14 ↔ 0x5D 之间切换重试。
//    理由:地址是**复位瞬间 INT 电平**决定的;若上电时序被电源斜率搅乱,
//    芯片可能落在另一个地址上,而这时**硬件完全正常**,只是地址不对。
//    前序是"上电时扫 0x08..0x77 挑出 0x14/0x5D",本模块改成**运行时自愈** ——
//    少一次开机扫描,还能修好上电之后才发生的地址错乱。
//
//  【时钟口径纪律】
//    `CLK_FREQ_HZ` 是**设计值 50 MHz**;本项目实测 `clk_axi = 41.6667 MHz`
//    ⇒ 本模块里所有"毫秒"都按设计值算,**实际会慢 20%**(20 ms 变 24 ms)。
//    对触摸轮询没有影响(人感觉不到),但**报告里不许把设计值写成实测值**。
//
//  【本模块的验证状态(如实)】
//    底层 `i2c_master` 有临时 TB 实跑证据(4 场景 PASS,含 GT911 读时序、
//    时钟拉伸、拉伸超时、总线恢复)。
//    本模块自身亦有临时 TB 实跑证据(18 项检查全过,4 次读事务 / 3 次清状态),
//    覆盖:复位序列与 INT 释放、坐标逐位正确、bit7=0 时保持、
//    count=0 判明确释放、每来一帧清一次 0x814E。
//    该 TB 过程中检出并修掉两个**真实 RTL bug**:
//      ① S_REC_HI 在 SCL 尚高时拉低 SDA ⇒ 总线恢复收尾发出伪 START(补 S_REC_END);
//      ② count==0 分支没走"清状态"事务 ⇒ seq 空转、PS 收到连续假释放事件。
//    ⚠️ **仍未上板** —— 真实 GT911 的行为(尤其 0x814F 起始偏移是否被接受、
//       复位时序 20/6/100 ms 是否够)只有硬件能验。M13 证据待补。
//=============================================================================

module gt911_ctrl #(
    parameter integer CLK_FREQ_HZ = 50_000_000,
    // GT911 的 I2C 速率上限 400 kHz。取 100 kHz —— 兼容性优先,
    // 且 10 ms 轮询周期下,一次 17 字节读只占 ~1.9 ms,带宽绰绰有余。
    parameter integer SCL_FREQ_HZ = 100_000,
    parameter integer ERR_RECOVER = 3,      // 连错 3 次 → 总线恢复
    parameter integer ERR_RESET   = 5,      // 连错 5 次 → 整个复位序列重来
    parameter integer N_PT        = 2       // 每帧读几个触点(留 2 个给捏合缩放)
)(
    input  wire        clk,
    input  wire        rst_n,

    //-----------------------------------------------------------------------
    //  物理引脚(顶层经 IOBUF;int 是**双向**的,见下)
    //-----------------------------------------------------------------------
    output wire        touch_rst_n,
    inout  wire        touch_int,       // ★ 双向:复位期间主机驱动,之后必须释放
    inout  wire        touch_scl,
    inout  wire        touch_sda,

    //-----------------------------------------------------------------------
    //  状态上报(直连 cv_axi_lite_slave 的 0x68 / 0x6C / 0x70)
    //-----------------------------------------------------------------------
    output reg  [31:0] sts_touch_status,   // [2:0]点数 [3]pen_down [4]err [15:8]seq
    output reg  [31:0] sts_touch_pt0,      // [11:0]x  [27:16]y
    output reg  [31:0] sts_touch_pt1
);

    //=========================================================================
    //  常量
    //  ★ 所有信号声明集中在模块头部 —— 本项目反复踩
    //    `VRFC 10-3380 identifier used before its declaration`,别把声明散开。
    //=========================================================================
    localparam [7:0] DEV_A0_W = 8'h28;      // 0x14<<1 | 0
    localparam [7:0] DEV_A0_R = 8'h29;      // 0x14<<1 | 1
    localparam [7:0] DEV_A1_W = 8'hBA;      // 0x5D<<1 | 0
    localparam [7:0] DEV_A1_R = 8'hBB;      // 0x5D<<1 | 1

    localparam [7:0] REG_STAT_H = 8'h81;    // 0x814E 高字节
    localparam [7:0] REG_STAT_L = 8'h4E;    // 0x814E 低字节
    localparam [7:0] REG_PT1_H  = 8'h81;    // 0x814F 高字节
    localparam [7:0] REG_PT1_L  = 8'h4F;    // 0x814F 低字节

    // ★ 不要在表达式里写 `N_PT[3:0]` —— 对 integer 类型的参数做位选,
    //   部分工具不接受。用显式位宽的 localparam 截断。
    localparam [3:0]  MAXPT = N_PT;
    localparam [3:0]  ER_REC = ERR_RECOVER;
    localparam [3:0]  ER_RST = ERR_RESET;

    localparam integer NREAD = 1 + N_PT * 8;        // 状态 1 字节 + 每点 8 字节
    // 同上:不要在表达式里对 integer localparam 做位选
    localparam [7:0]  NR_LAST = NREAD - 1;          // 最后一个字节的下标

    localparam [15:0] INTV_FAST = 16'd10;    // 前序 main.c: FAST_POLL_INTERVAL_MS
    localparam [15:0] INTV_SLOW = 16'd100;   // 前序 main.c: FALLBACK_POLL_INTERVAL_MS
    localparam [15:0] HOLD_MS   = 16'd150;   // 前序 main.c: ACTIVE_HOLD_MS

    localparam [15:0] T_RST_LOW_MS = 16'd20; // 前序 lcd_touch.c: RST=0/INT=1 保持 20ms
    localparam [15:0] T_RST_HI_MS  = 16'd6;  // 前序 lcd_touch.c: RST=1 后 6ms
    localparam [15:0] T_BOOT_MS    = 16'd100;// 前序 lcd_touch.c: 再等 100ms

    localparam [2:0] OP_START=3'd0, OP_WRITE=3'd1, OP_RD_ACK=3'd2,
                     OP_RD_NAK=3'd3, OP_STOP=3'd4, OP_RECOV=3'd5;

    localparam [4:0] S_PWR_LOW   = 5'd0;    // RST=0, INT=1
    localparam [4:0] S_PWR_HI    = 5'd1;    // RST=1(器件地址在这一刻锁存)
    localparam [4:0] S_PWR_BOOT  = 5'd2;    // 等固件启动
    localparam [4:0] S_PWR_REL   = 5'd3;    // 释放 INT
    localparam [4:0] S_DLY       = 5'd4;    // 通用延时
    localparam [4:0] S_RECOV     = 5'd5;    // 总线恢复
    localparam [4:0] S_IDLE      = 5'd6;
    localparam [4:0] S_START_W   = 5'd7;    // START(写方向起始)
    localparam [4:0] S_TX_ADDRW  = 5'd8;    // 器件地址(写)
    localparam [4:0] S_TX_REGH   = 5'd9;    // 寄存器地址高字节
    localparam [4:0] S_TX_REGL   = 5'd10;   // 寄存器地址低字节
    localparam [4:0] S_START_RD  = 5'd11;   // repeated START
    localparam [4:0] S_TX_ADDRR  = 5'd12;   // 器件地址(读)
    localparam [4:0] S_RX_LOOP   = 5'd13;   // 连续读 NREAD 字节
    localparam [4:0] S_TX_DATA   = 5'd14;   // 清状态:写 0
    localparam [4:0] S_STOP      = 5'd15;
    localparam [4:0] S_PROC      = 5'd16;
    localparam [4:0] S_ERR       = 5'd17;

    //=========================================================================
    //  信号声明
    //=========================================================================
    reg        mgo;
    reg  [2:0] mop;
    reg  [7:0] mtx;
    wire [7:0] mrx;
    wire       mack, mbusy, mdone, merr;

    wire       u_scl_out, u_sda_out;

    reg        int_oe;              // 1 = 主机驱动 INT
    reg        int_out;
    wire       int_level;

    reg        rst_n_out;
    assign     touch_rst_n = rst_n_out;

    reg [15:0] ms_div;
    reg [15:0] ms;
    reg        ms_tick;

    reg [4:0]  state;
    reg        op_run;              // 0 = 该发命令了;1 = 等 done
    reg        job;                 // 0 = 读状态+坐标;1 = 写 0 清状态
    reg [7:0]  ridx;
    reg [7:0]  rbuf [0:31];
    reg [7:0]  dev_w, dev_r;
    reg        use_alt;

    reg [15:0] dly;
    reg [4:0]  dly_next;

    reg [15:0] idle_ms;
    reg [15:0] hold_ms;
    reg [3:0]  err_cnt;
    reg [3:0]  addrnak_cnt;
    reg        err_sticky;

    reg [2:0]  cur_count;
    reg        pen_down;
    reg [7:0]  seq;
    reg [11:0] pt0_x, pt0_y, pt1_x, pt1_y;

    wire       scl_in = touch_scl;   // ★ 读回引脚真实电平 —— 时钟拉伸靠它
    wire       sda_in = touch_sda;

    assign     touch_int = int_oe ? int_out : 1'bz;
    assign     int_level = touch_int;
    assign     touch_scl = u_scl_out;
    assign     touch_sda = u_sda_out;

    localparam [15:0] MS_DIV = CLK_FREQ_HZ / 1000;   // 50000,16 位够

    //=========================================================================
    //  I2C 主机
    //=========================================================================
    i2c_master #(
        .CLK_FREQ_HZ (CLK_FREQ_HZ),
        .SCL_FREQ_HZ (SCL_FREQ_HZ)
    ) u_i2c (
        .clk (clk), .rst_n (rst_n),
        .go (mgo), .op (mop), .tx_byte (mtx),
        .rx_byte (mrx), .ack_rx (mack),
        .busy (mbusy), .done (mdone), .err (merr),
        .scl (u_scl_out), .sda (u_sda_out),
        .scl_in (scl_in), .sda_in (sda_in)
    );

    //=========================================================================
    //  毫秒时基
    //=========================================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            ms_div  <= 16'd0;
            ms      <= 16'd0;
            ms_tick <= 1'b0;
        end else if (ms_div >= MS_DIV - 16'd1) begin
            ms_div  <= 16'd0;
            ms      <= ms + 16'd1;
            ms_tick <= 1'b1;
        end else begin
            ms_div  <= ms_div + 16'd1;
            ms_tick <= 1'b0;
        end
    end

    //=========================================================================
    //  主状态机
    //
    //  ★ op 发起/等待的统一写法(全篇复用,别各写各的):
    //      if (!op_run) begin 发命令; op_run <= 1; end
    //      else begin mgo <= 0; if (mdone) begin op_run <= 0; 跳下一状态; end end
    //    `op_run` 是"命令已发出、正在等 done"的标志。少了它,
    //    同一个状态会在 mdone 到来前重复发命令。
    //  ★ 所有命令(成功或失败)**都会**脉冲 done —— i2c_master 的 S_ABORT
    //    也发 done。所以这里的等 done 不会挂死,失败靠 merr/mack 区分。
    //=========================================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state    <= S_PWR_LOW;
            op_run   <= 1'b0;
            mgo      <= 1'b0;
            mop      <= OP_START;
            mtx      <= 8'h00;
            job      <= 1'b0;
            ridx     <= 8'd0;
            dev_w    <= DEV_A0_W;
            dev_r    <= DEV_A0_R;
            use_alt  <= 1'b0;
            dly      <= 16'd0;
            dly_next <= S_PWR_LOW;
            idle_ms  <= 16'd0;
            hold_ms  <= 16'd0;
            err_cnt  <= 4'd0;
            addrnak_cnt <= 4'd0;
            err_sticky  <= 1'b0;
            cur_count   <= 3'd0;
            pen_down    <= 1'b0;
            seq         <= 8'd0;
            pt0_x <= 12'd0; pt0_y <= 12'd0;
            pt1_x <= 12'd0; pt1_y <= 12'd0;
            rst_n_out <= 1'b0;      // RST 低
            int_oe    <= 1'b1;      // ★ 复位期间**驱动** INT(决定器件地址)
            int_out   <= 1'b1;      //   且驱成**高** ⇒ 地址 = 0x14
        end else begin
            //---------------------------------------------------------------
            //  上报字,每拍刷新 —— PS 读到的总是当前一致快照
            //  位域: [2:0]点数 [3]pen_down [4]err [7:5]保留 [15:8]seq [31:16]保留
            //---------------------------------------------------------------
            sts_touch_status <= {16'b0, seq, 3'b0, err_sticky, pen_down, cur_count};
            sts_touch_pt0    <= {4'b0, pt0_y, 4'b0, pt0_x};
            sts_touch_pt1    <= {4'b0, pt1_y, 4'b0, pt1_x};

            if (ms_tick && hold_ms != 16'd0) hold_ms <= hold_ms - 16'd1;

            case (state)
                //===========================================================
                //  上电复位序列(前序 lcd_touch.c 的时序)
                //  每一步只做"置输出 + 设延时",真正的等待都在 S_DLY 里。
                //===========================================================
                S_PWR_LOW: begin
                    rst_n_out <= 1'b0;          // RST 低
                    int_oe    <= 1'b1;
                    int_out   <= 1'b1;          // INT 高
                    dly       <= T_RST_LOW_MS;
                    dly_next  <= S_PWR_HI;
                    state     <= S_DLY;
                end

                S_PWR_HI: begin
                    rst_n_out <= 1'b1;          // ★ RST 上升沿 —— 器件地址在此锁存
                    dly       <= T_RST_HI_MS;
                    dly_next  <= S_PWR_BOOT;
                    state     <= S_DLY;
                end

                S_PWR_BOOT: begin
                    dly      <= T_BOOT_MS;
                    dly_next <= S_PWR_REL;
                    state    <= S_DLY;
                end

                S_PWR_REL: begin
                    // ★ 交还 INT。忘了释放会让主机把 INT 钉死,GT911 的中断
                    //   永远传不上来 —— 现象是"坐标读得到、中断永不触发",
                    //   极易误判成触摸芯片坏。
                    int_oe   <= 1'b0;
                    dly      <= 16'd0;
                    dly_next <= S_RECOV;
                    state    <= S_DLY;
                end

                //-----------------------------------------------------------
                //  通用延时:数够 dly 个 ms_tick 后跳 dly_next
                //-----------------------------------------------------------
                S_DLY: begin
                    if (ms_tick) begin
                        if (dly == 16'd0) state <= dly_next;
                        else              dly   <= dly - 16'd1;
                    end
                end

                //-----------------------------------------------------------
                //  总线恢复(9 脉冲 + STOP)
                //  放在复位序列**之后**、第一次通信**之前**:上电瞬间若总线
                //  停在半途,不先清一遍第一次通信必然失败。
                //-----------------------------------------------------------
                S_RECOV: begin
                    if (!op_run) begin
                        mop    <= OP_RECOV;
                        mgo    <= 1'b1;
                        op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run  <= 1'b0;
                            idle_ms <= 16'd0;
                            state   <= S_IDLE;
                        end
                    end
                end

                //-----------------------------------------------------------
                //  轮询等待 —— 三路触发,缺一不可
                //    · INT 低  → 有事件,立刻去看(快路径)
                //    · 保持窗  → 手指按着不动时 INT 可能不再变低,靠 150 ms 窗口
                //    · 慢兜底  → INT 若因故一直不低(接线/配置问题),100 ms 也出数
                //-----------------------------------------------------------
                S_IDLE: begin
                    if (ms_tick) begin
                        if (idle_ms < ((int_level == 1'b0 || hold_ms != 16'd0)
                                       ? INTV_FAST : INTV_SLOW)) begin
                            idle_ms <= idle_ms + 16'd1;
                        end else begin
                            idle_ms <= 16'd0;
                            ridx    <= 8'd0;
                            job     <= 1'b0;        // 本轮先读
                            state   <= S_START_W;
                        end
                    end
                end

                //-----------------------------------------------------------
                //  START(写方向的起始)
                //-----------------------------------------------------------
                S_START_W: begin
                    if (!op_run) begin
                        mop <= OP_START; mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            state  <= merr ? S_ERR : S_TX_ADDRW;
                        end
                    end
                end

                //-----------------------------------------------------------
                //  器件地址(写)
                //-----------------------------------------------------------
                S_TX_ADDRW: begin
                    if (!op_run) begin
                        mop <= OP_WRITE; mtx <= dev_w; mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            if (merr || mack != 1'b0) begin
                                // ⚠️ 地址没被 ACK 的两个最常见原因:
                                //    ① 器件地址选错(0x14 / 0x5D)
                                //    ② 芯片没上电 / 没复位好
                                addrnak_cnt <= addrnak_cnt + 4'd1;
                                state <= S_ERR;
                            end else begin
                                addrnak_cnt <= 4'd0;
                                state <= S_TX_REGH;
                            end
                        end
                    end
                end

                S_TX_REGH: begin
                    if (!op_run) begin
                        mop <= OP_WRITE; mtx <= REG_STAT_H; mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            state  <= (merr || mack != 1'b0) ? S_ERR : S_TX_REGL;
                        end
                    end
                end

                S_TX_REGL: begin
                    if (!op_run) begin
                        // ★ 读和清**用的是同一个寄存器地址** 0x814E,所以这里
                        //   不需要看 job —— 两条路都写 REG_STAT_L。
                        //   REG_PT1_H/L(0x814F)是"只读状态、想少读 1 字节"的
                        //   备选起点,本模块不用(要 track_id 就必须从 0x814F 读,
                        //   而状态字节又必须先读,所以统一从 0x814E 起最省事)。
                        mop <= OP_WRITE; mtx <= REG_STAT_L;
                        mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            if (merr || mack != 1'b0) state <= S_ERR;
                            else state <= job ? S_TX_DATA : S_START_RD;
                        end
                    end
                end

                //-----------------------------------------------------------
                //  repeated START + 器件地址(读)
                //-----------------------------------------------------------
                S_START_RD: begin
                    if (!op_run) begin
                        mop <= OP_START; mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            state  <= merr ? S_ERR : S_TX_ADDRR;
                        end
                    end
                end

                S_TX_ADDRR: begin
                    if (!op_run) begin
                        mop <= OP_WRITE; mtx <= dev_r; mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            state  <= (merr || mack != 1'b0) ? S_ERR : S_RX_LOOP;
                        end
                    end
                end

                //-----------------------------------------------------------
                //  连续读
                //  ★ 最后一个字节必须 **OP_RD_NAK**。
                //    回 ACK 的话 GT911 会继续吐下一个字节,而主机随后发的 STOP
                //    会被它当成数据位收进去 ⇒ 总线失步,下一次通信再失败。
                //    这是 I2C 读操作的经典坑,i2c_master 的临时 TB 专门验过。
                //-----------------------------------------------------------
                S_RX_LOOP: begin
                    if (!op_run) begin
                        mop    <= (ridx == NR_LAST) ? OP_RD_NAK : OP_RD_ACK;
                        mgo    <= 1'b1;
                        op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            if (merr) begin
                                state <= S_ERR;
                            end else begin
                                rbuf[ridx] <= mrx;
                                if (ridx == NR_LAST) state <= S_STOP;
                                else begin ridx <= ridx + 8'd1; state <= S_RX_LOOP; end
                            end
                        end
                    end
                end

                //-----------------------------------------------------------
                //  清状态:向 0x814E 写 0
                //-----------------------------------------------------------
                S_TX_DATA: begin
                    if (!op_run) begin
                        mop <= OP_WRITE; mtx <= 8'h00; mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            state  <= (merr || mack != 1'b0) ? S_ERR : S_STOP;
                        end
                    end
                end

                S_STOP: begin
                    if (!op_run) begin
                        mop <= OP_STOP; mgo <= 1'b1; op_run <= 1'b1;
                    end else begin
                        mgo <= 1'b0;
                        if (mdone) begin
                            op_run <= 1'b0;
                            state  <= merr ? S_ERR : S_PROC;
                        end
                    end
                end

                //-----------------------------------------------------------
                //  处理收到的数据
                //-----------------------------------------------------------
                S_PROC: begin
                    err_cnt <= 4'd0;
                    idle_ms <= 16'd0;
                    if (job) begin
                        // 刚写完"清状态" —— 小事一桩,**不改** count/pen_down,
                        // 也不重置 hold_ms:清状态是善后,不影响"当前是否有手指"
                        state <= S_IDLE;
                    end else if (rbuf[0][7] == 1'b0) begin
                        // ★ 没有新帧 ⇒ **保持**上次的按下状态与坐标。
                        //   这里绝不能当成"松手"(见文件头)。
                        state <= S_IDLE;
                    end else begin
                        seq <= seq + 8'd1;      // 新帧 ⇒ seq 递增,供 PS 判新旧
                        if (rbuf[0][3:0] == 4'd0) begin
                            cur_count <= 3'd0;  // 新帧且点数为 0 ⇒ **明确释放**
                            pen_down  <= 1'b0;
                            // ★ 松手这一帧也**必须清状态寄存器**。
                            //   第一版这里是 `state <= S_IDLE`,漏了清 ——
                            //   后果不是"读不到",而是**看起来一切正常但 seq 疯跑**:
                            //   bit7 一直是 1 ⇒ 每次轮询都把同一帧再报一遍 ⇒
                            //   PS 侧会收到一串连续的"松手事件"。
                            //   凡 bit7==1 的帧一律要清,三条分支无一例外。
                            job       <= 1'b1;
                            ridx      <= 8'd0;
                            state     <= S_START_W;
                        end else if (rbuf[0][3:0] > MAXPT) begin
                            // 点数超上限:GT911 在噪声下偶发。
                            // **丢弃这一帧的坐标**,但**仍然去清状态寄存器** ——
                            // 不清的话这一帧会被反复报上来,后面所有帧都读不到。
                            // 出处:前序 lcd_touch.c 的 LCD_TOUCH_ERROR_DATA 分支
                            // (它也是先清再返回错误)。
                            cur_count <= 3'd0;
                            pen_down  <= 1'b0;
                            job       <= 1'b1;
                            ridx      <= 8'd0;
                            state     <= S_START_W;
                        end else begin
                            cur_count <= rbuf[0][2:0];
                            pen_down  <= 1'b1;
                            // 触点 1:rbuf[1]=track_id, rbuf[2..3]=x, rbuf[4..5]=y
                            // ★ 拼接位宽必须**正好 12 位**。第一版写成
                            //   `{rbuf[3], rbuf[2], 2'b00}` = 18 位,赋给 12 位 reg
                            //   会被**静默截断**成 {rbuf[2], 2'b00} —— 高位坐标全丢,
                            //   而且不报 warning。取高字节的低 4 位拼低字节才是 12 位。
                            pt0_x <= {rbuf[3][3:0], rbuf[2]};
                            pt0_y <= {rbuf[5][3:0], rbuf[4]};
                            if (rbuf[0][3:0] >= 4'd2) begin
                                // 触点 2:起点 0x8157 = rbuf[9]
                                pt1_x <= {rbuf[11][3:0], rbuf[10]};
                                pt1_y <= {rbuf[13][3:0], rbuf[12]};
                            end
                            hold_ms <= HOLD_MS;     // 有人在按 ⇒ 150 ms 内走快轮询
                            job     <= 1'b1;        // 转去清状态
                            ridx    <= 8'd0;
                            state   <= S_START_W;
                        end
                    end
                end

                //-----------------------------------------------------------
                //  错误处理:两级容忍
                //-----------------------------------------------------------
                S_ERR: begin
                    err_sticky <= 1'b1;
                    err_cnt    <= err_cnt + 4'd1;
                    if (addrnak_cnt >= 4'd3) begin
                        // 换器件地址再试(见文件头「器件地址自愈」)
                        addrnak_cnt <= 4'd0;
                        use_alt     <= ~use_alt;
                        dev_w <= use_alt ? DEV_A0_W : DEV_A1_W;
                        dev_r <= use_alt ? DEV_A0_R : DEV_A1_R;
                    end
                    if (err_cnt + 4'd1 >= ER_RST) begin
                        err_cnt <= 4'd0;        // 连错到 5 ⇒ 整个复位序列重来
                        state   <= S_PWR_LOW;
                    end else if (err_cnt + 4'd1 >= ER_REC) begin
                        state   <= S_RECOV;     // 连错到 3 ⇒ 先做一次总线恢复
                    end else begin
                        idle_ms <= 16'd0;
                        state   <= S_IDLE;
                    end
                end

                default: state <= S_PWR_LOW;
            endcase
        end
    end

endmodule
