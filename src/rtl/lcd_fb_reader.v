`timescale 1ns / 1ps
//=============================================================================
//  lcd_fb_reader.v  ——  帧缓存行读取器(AXI4 只读主机 + 乒乓行缓冲)
//
//  归属:Claude 代写(2026-10-02,用户明确要求「自己用 vivado 跑出上板需要的文件,
//        完成 ps 程序设计以外的其他工作」,CLAUDE.md §6.0 显式例外)
//
//  【为什么自写而不用 AXI VDMA】(B04 取舍论证的正文素材)
//    ① PS 画帧缓存是 **CPU 直写 DDR** ⇒ VDMA 的**写通道完全用不上**;
//       而 VDMA 的读通道要配 `v_tc`(视频时序控制器)才能跑,等于为了取数再引一个 IP。
//    ② 本设计需要与 `lcd_timing` **紧耦合**:取第 N+1 行必须在第 N 行扫描期内发起
//       (行消隐只有 6.14 µs,不够取一行)。VDMA 的 FIFO 深度不可控,拿不到这个节拍保证。
//    ③ **AXI 突发读的握手(rvalid/rready/rlast)正是赛题「接口协议」考察点** ——
//       自己写才讲得清时序,外挂 IP 讲不清。
//    代价:多花 ~1.2K LUT(约 XC7Z020 的 3%),换一条时序可控、可解释的读通路。
//
//  【为什么是 AXI3 而不是 AXI4】(2026-10-02 实测踩坑,改前必读)
//    Zynq-7000 的 **HP/GP 口是 AXI3**。把 M_AXI 声明成 AXI4 后 BD 直接拒绝连线:
//      ERROR: [BD 41-1285] The protocols of the interfaces
//             '/cv_sys_0/M_AXI'(AXI4) and '/processing_system7_0/S_AXI_HP0'(AXI3)
//             are incompatible. They cannot be connected.
//    AXI3 的单次突发**上限 16 拍**(AXI4 才放宽到 256),于是"一行 256 拍"必须拆成
//    **16 次突发 × 16 拍**。
//
//    ★ 为什么不在 BD 里插一个 axi_protocol_converter 把 AXI4 转成 AXI3?
//      HP 是本设计的**实时数据路径**,而它有**每行硬截止**(行消隐只有 6.14 µs,
//      取一行要在上一行扫描期内完成)。转换器会在这条路径上再引入一级不确定延迟,
//      且多一个不可控的 IP。**延迟可预测 > 少写几行代码。**
//      代价是状态机多一个突发计数(4 bit 寄存器 + 一个比较器),约十几 LUT。
//
//  【数据通路】
//    DDR 帧缓存(RGB565,1024×600,每行 2048 B)
//      → AXI3 突发读,**16 次 × 16 拍**,64 bit ⇒ 128 B/突发 × 16 = 2048 B/行
//      → 乒乓行缓冲(2 块 × 256 word × 64 bit,**合起来 512×64 占 1 个 RAMB36**)
//      → 每拍取 4 个 RGB565 像素,按 px_x[1:0] 选出 1 个
//      → px_data[15:0] 交 cv_sys 展开成 RGB888
//
//  【★ 乒乓换址与相位(改前必读)】
//    节拍全部来自 lcd_timing:
//      · `frame_start`  ── 进入场消隐首拍 → **预取第 0 行** 到 wr_sel
//      · `line_start`   ── 第 N 行取数起点 → **交换 rd/wr**,再取第 N+1 行到新 wr_sel
//    于是「扫描第 N 行时,另一个缓冲正在装第 N+1 行」,永远不打架。
//
//    行的先后由 `row_ptr` 记:
//      frame_start : row_ptr <= 0        (同时取第 0 行)
//      line_start  : 取 row_ptr+1;row_ptr <= row_ptr+1(599 → 0)
//                    ⚠️ row_ptr == 599 时**不取**(下一帧由 frame_start 取第 0 行)
//
//    ★ 像素输出相位(必须与 lcd_timing 的三级链对齐):
//      de_req 当拍 : px_x[1:0]==0 时读 BRAM(word_r),否则保持
//      下一拍 de_act: word_r 已有效,sel_r = 上一拍的 px_x[1:0] ⇒ 选出正确像素
//    ⇒ **px_data 是寄存输出,天然匹配"源必须寄存"这条约定**。
//
//  【时钟域】
//    M_AXI 与像素侧**同一个 clk**(= clk_axi,设计 50 MHz / 实测 41.6667 MHz)
//    ⇒ 不需要 cdc_sync,不需要异步 FIFO。这是 D24 里"两域同频"的直接收益。
//
//  【异常处理 —— 不得死等】(规格 §6.2 的硬性要求)
//    · `rresp != OKAY` → 置 axi_err(粘滞)
//    · AR 或 DATA 状态超过 WDT_LIMIT 拍无进展 → 中止、置 axi_err、回 IDLE
//      ⇒ 宁可丢几行画面并**把错误暴露给 PS**,也不能把状态机卡死
//    · 换进一个没装满的缓冲 → 置 underrun(粘滞)
//    ⚠️ 两个标志都是**粘滞**的 —— 瞬态错误不该被下一次正常刷新抹掉。
//       清除方式:PS 写 0x64(任意值)⇒ clr_sticky 脉冲,清 axi_err / underrun。
//       ★ 2026-10-03 才真正实现。此前文档写了、RTL 没写,导致上电第一帧读物理地址 0
//         拿到 DECERR 后 axi_err 永远为 1 —— **一个恒真的告警等于没有告警**。
//=============================================================================

module lcd_fb_reader #(
    parameter integer H_DISP      = 1024,       // 每行像素数
    parameter integer V_DISP      = 600,        // 每帧行数
    parameter integer BYTES_LINE  = 2048,       // = H_DISP × 2(RGB565)
    parameter integer BEATS_LINE  = 256,        // = BYTES_LINE / 8(64 bit 一拍),一行总拍数
    parameter integer BEATS_BURST = 16,         // ★ AXI3 单次突发上限就是 16 拍,不可再大
    parameter integer AXI_ADDR_W  = 32,
    parameter integer AXI_DATA_W  = 64,
    // AR/DATA 状态无进展的看门狗上限(拍)。50 MHz 下 8192 拍 ≈ 164 µs ≈ 6 行
    parameter integer WDT_LIMIT   = 8192
)(
    input  wire                    clk,
    input  wire                    rst_n,

    //-------------------------------------------------------------------------
    // 配置 / 节拍
    //-------------------------------------------------------------------------
    input  wire [AXI_ADDR_W-1:0]   fb_base,      // 当前前台帧缓存物理基址
    input  wire                    frame_start,  // 场消隐首拍(1 拍脉冲)
    input  wire                    line_start,   // 有效行取数起点(1 拍脉冲)

    //-------------------------------------------------------------------------
    // 取数使能 + 清粘滞(2026-10-03 新增,上板排查所得)
    //-------------------------------------------------------------------------
    // ★ 为什么需要 src_sel 门控:
    //   此前本模块**无条件**取数。复位后 fb_base = 0、row_ptr = 0,
    //   于是上电第一帧就去读物理地址 0 —— 拿到 DECERR ⇒ 置 axi_err。
    //   而 axi_err 是粘滞的、当时又没有清它的路径,结果「axi_err = 1」成了恒真条件,
    //   **在诊断上等于没有信息**(上板实测确认过这一点)。
    //   门控后:LCD_CTRL 复位值为 0(src_sel ≠ 2)⇒ 上电不取数 ⇒ axi_err 恢复信息量。
    input  wire [1:0]              src_sel,      // 来自 LCD_CTRL[1:0];== 2(DDR 帧缓存)才取数
    input  wire                    clr_sticky,   // PS 写 0x64 的清粘滞脉冲(1 拍),清 axi_err/underrun

    //-------------------------------------------------------------------------
    // 像素侧
    //-------------------------------------------------------------------------
    input  wire                    de_req,       // 取数请求:此刻 px_x 有效
    input  wire [10:0]             px_x,         // 0 .. H_DISP-1
    output wire [15:0]             px_data,      // RGB565,寄存输出,与 de_act 同拍

    //-------------------------------------------------------------------------
    // 状态(粘滞,交 lcd_vblank_ctrl 汇总进 LCD_FB_STATUS)
    //-------------------------------------------------------------------------
    output reg                     underrun,
    output reg                     axi_err,

    //-------------------------------------------------------------------------
    // 调试观测(2026-10-03 新增,只读;交 cv_axi_lite_slave 的 0x74/0x78/0x7C)
    //
    //  ★ 为什么要加这三个寄存器:
    //    上板出现「src=2 全黑」时,PS 侧能读到的只有 FB_STATUS,
    //    而它把两种**完全不同的故障**混成了同一个读数:
    //      假设 A:AXI 读事务根本没拿到数据(HP 口 / SmartConnect 层面挂)
    //      假设 B:数据拿到了,但读写乒乓选错缓冲,显示侧读到一块没写过的全零 BRAM
    //    两者都是「黑屏 + underrun=0」。**只有 PL 侧计数才能把二者分开。**
    //-------------------------------------------------------------------------
    //  ★ 2026-10-03 第二次上板后修订 0x74 —— 拍数已证明是满的(153600),但屏仍全黑。
    //    拍数只说明「握手发生了」,**看不见数据本身**。而"换成内核镜像地址仍然全黑"
    //    这条观测指向:数据可能就是**全零**(AXI 从设备拒绝地址时会回 DECERR + 全零数据,
    //    握手却照常完成)—— 那种情况下拍数照样是满帧。
    //    ⇒ 0x74 拆成两段:高 14 位放【内容证据】,低 18 位仍放拍数。
    output wire [31:0]             dbg_beats,    // 0x74:见模块末尾 §6 的位布局
    output reg  [31:0]             dbg_araddr,   // 0x78:最近一次 AR 握手成功时的地址
    output wire [31:0]             dbg_state,    // 0x7C:状态快照,布局见模块末尾

    //-------------------------------------------------------------------------
    // AXI4 只读主机(功能子集;常量字段如 arprot/arcache 在 cv_sys 里接)
    //-------------------------------------------------------------------------
    output reg  [AXI_ADDR_W-1:0]   m_axi_araddr,
    output reg  [3:0]              m_axi_arlen,     // ★ AXI3:4 bit(编码 = 拍数-1,最大 15 ⇒ 16 拍)
    output reg  [2:0]              m_axi_arsize,
    output reg  [1:0]              m_axi_arburst,
    output reg                     m_axi_arvalid,
    input  wire                    m_axi_arready,

    input  wire [AXI_DATA_W-1:0]   m_axi_rdata,
    input  wire                    m_axi_rvalid,
    input  wire                    m_axi_rlast,
    input  wire [1:0]              m_axi_rresp,
    output wire                    m_axi_rready
);

    //=========================================================================
    //  0. 常量
    //=========================================================================
    localparam integer WORDS_LINE  = H_DISP / 4;                    // 256:每 64 bit 一个字 = 4 像素
    localparam integer BURSTS_LINE = WORDS_LINE / BEATS_BURST;      //  16:一行拆几次突发
    localparam [9:0]   V_DISP_M1   = V_DISP - 1;                    // 599
    localparam [3:0]   BURSTS_M1   = BURSTS_LINE - 1;               //  15
    localparam [3:0]   BEATS_M1    = BEATS_BURST - 1;               //  15

    localparam [1:0] S_IDLE = 2'd0,
                     S_AR   = 2'd1,
                     S_DATA = 2'd2;

    // ⚠️ BURSTS_LINE 必须是 2 的幂(下面用"移位"代替乘法算突发起始地址)。
    //    1024 像素 / 4 / 16 = 16 ✓。若将来改分辨率让这里不再是 2 的幂,
    //    要把 `burst_off` 那两处 `7'b0` 改成乘法,否则突发地址会静默算错。

    // ⚠️ 出错的处置:**置 axi_err 后回 IDLE**,不进"死等"的错误态。
    //    理由:卡死的系统连"我卡住了"都报不出来。回 IDLE 后下一行继续取,
    //    画面降级(丢一两行/闪一下)但系统活着,且 axi_err **粘滞**,PS 一读就知道。
    //    ⇒ "能把错误暴露出来"优先于"把错误藏起来继续跑"。

    //=========================================================================
    //  1. 乒乓缓冲索引
    //     rd_sel:当前【正在显示】的缓冲
    //     wr_sel:当前【正在装入】的缓冲 —— 二者恒不相交
    //=========================================================================
    reg  rd_sel, wr_sel;

    // 本次突发写入哪一块:★ 在突发【发起时】锁存。
    //   不能直接用 wr_sel 做写地址 —— 若上一笔突发还没结束就来了 line_start,
    //   wr_sel 会中途翻转,那一笔的后半行会写进另一块 ⇒ 静默串行。
    reg  tgt_sel;

    // 每块的"装满"标志:b[0] / b[1]。换进一个没装满的块 ⇒ underrun
    reg  [1:0] buf_ready;

    //=========================================================================
    //  2. 行指针与预取请求
    //=========================================================================
    reg  [9:0] row_ptr;         // 当前正在显示的行号

    reg  [9:0] fetch_row;       // 本次要取的行
    reg        fetch_pend;      // 有预取请求待发

    // ⚠️ state / fsm_go 必须【先声明后用】。
    //    写在下面会让 fsm_go 被 Verilog 隐式声明成 1 位 wire,
    //    或直接报 VRFC 10-2938 —— 与 cv_sys.v 里记的是同一个坑。
    reg  [1:0]  state;

    // 取数使能:只有 LCD_CTRL 选中 DDR 帧缓存(2)时才工作。
    //   fsm_go 也一并门控 —— 否则 src_sel 在 fetch_pend=1 期间跌落的那一拍,
    //   状态机仍会发出一次多余的 AR(无害,但白白让 axi_err 有话说)。
    wire        fetch_en   = (src_sel == 2'd2);
    wire        fsm_go     = (state == S_IDLE) && fetch_pend && fetch_en;

    // 组合:由节拍推出的"要取哪一行"
    wire        want_fetch = fetch_en & (frame_start | (line_start & (row_ptr != V_DISP_M1)));
    wire [9:0]  want_row   = frame_start ? 10'd0 : (row_ptr + 10'd1);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            row_ptr <= 10'd0;
        end else if (frame_start) begin
            row_ptr <= 10'd0;
        end else if (line_start) begin
            row_ptr <= (row_ptr == V_DISP_M1) ? 10'd0 : (row_ptr + 10'd1);
        end
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            fetch_pend <= 1'b0;
            fetch_row  <= 10'd0;
        end else if (want_fetch) begin
            // ⚠️ 若上一笔还没发出去就被新的覆盖,会丢一行 ——
            //    这种退化由 underrun 如实暴露,不在这里偷偷补救
            fetch_pend <= 1'b1;
            fetch_row  <= want_row;
        end else if (fsm_go) begin
            fetch_pend <= 1'b0;
        end else if (!fetch_en) begin
            // 关掉取数时丢弃挂起的请求:否则重新使能的那一刻会拿着**陈旧的 fetch_row**
            // 立刻发一笔 AR。真正的重新开始由 frame_start 负责(row_ptr 同时归零)。
            fetch_pend <= 1'b0;
        end
    end

    //=========================================================================
    //  3. 突发状态机 —— AXI3:一行 = BURSTS_LINE(16)次突发,每次 16 拍
    //
    //     ★ 每次突发单独一个 AR,收满 16 拍再发下一次(**串行,不做多笔并发**)。
    //       理由:16 × (AR 握手 + 16 拍数据 + 尾巴)在 HP 口上约 300~900 拍,
    //       而一行有 1344 拍,**余量足够就没必要引入乱序跟踪**——
    //       并发读要管"哪一拍属于哪次突发",复杂度上去了收益却零。
    //       ⏸ 若上板后 LCD_FB_STATUS[19](underrun)报出来,第一个该动的就是这里。
    //=========================================================================
    reg  [3:0]  burst_idx;      // 本次突发在一行里的序号 0..15
    reg  [3:0]  beat_cnt;       // 本次突发内已收到的拍数 0..15
    reg  [9:0]  cur_row;        // ★ 本次取数的行号,发起时锁存(见下方注释)
    reg  [13:0] wdt;            // 看门狗(AR / DATA 无进展计数)

    // 行内地址 = fb_base + row × BYTES_LINE + burst_idx × 128
    //   BYTES_LINE = 2048 = 2^11、每次突发 16 拍 × 8 B = 128 = 2^7 ⇒ 纯移位,不引乘法器
    wire [AXI_ADDR_W-1:0] row_off     = {{(AXI_ADDR_W-21){1'b0}}, cur_row,          11'b0};
    wire [AXI_ADDR_W-1:0] burst_off   = {{(AXI_ADDR_W-11){1'b0}}, burst_idx,         7'b0};
    wire [AXI_ADDR_W-1:0] burst_off_n = {{(AXI_ADDR_W-11){1'b0}}, burst_idx + 4'd1,  7'b0};
    // 第 0 次突发用 fetch_row —— 那一刻 cur_row 还没锁上(两者在同一拍更新)
    wire [AXI_ADDR_W-1:0] row_off_f   = {{(AXI_ADDR_W-21){1'b0}}, fetch_row,        11'b0};

    // ★ 为什么要 `cur_row` 这个额外的寄存器:
    //   一次取数现在横跨 16 次突发、最长可持续几百拍。这期间若来了下一次
    //   `line_start`,上面的 `fetch_row` 会被**当场改写**,后半行的突发地址就会
    //   错到别的行上去 —— 而且是**静默**的(数据看着像正常图像,只是错行)。
    //   锁进 cur_row 后,一行之内行号恒定。
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state         <= S_IDLE;
            m_axi_arvalid <= 1'b0;
            m_axi_araddr  <= {AXI_ADDR_W{1'b0}};
            m_axi_arlen   <= BEATS_M1;
            m_axi_arsize  <= 3'b011;         // 8 字节
            m_axi_arburst <= 2'b01;          // INCR
            burst_idx     <= 4'd0;
            beat_cnt      <= 4'd0;
            cur_row       <= 10'd0;
            wdt           <= 14'd0;
            tgt_sel       <= 1'b0;
            buf_ready     <= 2'b00;
            axi_err       <= 1'b0;
        end else begin
            // ★ PS 写 0x64 清粘滞 —— 这个契约文档里早就写了(规格 §6.2),
            //   但此前**从未实现**,害得 axi_err 恒为 1。
            //   放在 case【之前】:同一拍若真的又出错,case 里的置位会覆盖它,
            //   ⇒「清除」不会把当拍的真实错误一起抹掉。
            if (clr_sticky) axi_err <= 1'b0;
            case (state)
                //---------------------------------------------------------
                S_IDLE: begin
                    m_axi_arvalid <= 1'b0;
                    wdt           <= 14'd0;
                    if (fsm_go) begin
                        m_axi_araddr  <= fb_base + row_off_f;    // 第 0 次突发
                        m_axi_arlen   <= BEATS_M1;               // 15 ⇒ 16 拍
                        m_axi_arsize  <= 3'b011;
                        m_axi_arburst <= 2'b01;
                        m_axi_arvalid <= 1'b1;
                        cur_row       <= fetch_row;              // ★ 锁行号
                        burst_idx     <= 4'd0;
                        beat_cnt      <= 4'd0;
                        tgt_sel       <= wr_sel;                 // ★ 锁存写入目标
                        buf_ready[wr_sel] <= 1'b0;               // ★ 这一块开始重装
                        state         <= S_AR;
                    end
                end
                //---------------------------------------------------------
                S_AR: begin
                    if (m_axi_arready) begin
                        m_axi_arvalid <= 1'b0;      // 握手完成,立刻撤 ARVALID
                        wdt           <= 14'd0;
                        beat_cnt      <= 4'd0;      // 进 S_DATA 前清零这一拍的计数
                        state         <= S_DATA;
                    end else if (wdt >= WDT_LIMIT[13:0]) begin
                        m_axi_arvalid <= 1'b0;      // ★ 不得死等:撤 ARVALID,回 IDLE
                        axi_err       <= 1'b1;
                        state         <= S_IDLE;
                    end else begin
                        wdt <= wdt + 14'd1;
                    end
                end
                //---------------------------------------------------------
                S_DATA: begin
                    if (m_axi_rvalid) begin
                        wdt <= 14'd0;
                        if (m_axi_rresp != 2'b00) axi_err <= 1'b1;   // ★ 错误也照收,不卡住
                        // 本次突发收满(rlast 是权威判据,beat_cnt 做兜底)
                        if ((beat_cnt == BEATS_M1) || m_axi_rlast) begin
                            if (burst_idx == BURSTS_M1) begin
                                buf_ready[tgt_sel] <= 1'b1;          // 整行装满了
                                state <= S_IDLE;
                            end else begin
                                m_axi_araddr  <= fb_base + row_off + burst_off_n;
                                m_axi_arvalid <= 1'b1;               // 下一笔突发立刻发起
                                burst_idx     <= burst_idx + 4'd1;
                                state         <= S_AR;
                            end
                        end else begin
                            beat_cnt <= beat_cnt + 4'd1;
                        end
                    end else if (wdt >= WDT_LIMIT[13:0]) begin
                        axi_err <= 1'b1;                             // ★ 不得死等
                        state   <= S_IDLE;
                    end else begin
                        wdt <= wdt + 14'd1;
                    end
                end
                //---------------------------------------------------------
                default: state <= S_IDLE;
            endcase
        end
    end

    // 只要在收数据就常开 RREADY(顺序读,无重排,不需要反压)
    assign m_axi_rready = (state == S_DATA);

    //=========================================================================
    //  4. 行缓冲
    //     512 word × 64 bit = 2 块 × 256 word。地址高位 = 缓冲索引。
    //     ★ 一个简单双口 BRAM(write 64 / read 64),综合应落 1 个 RAMB36。
    //     ⚠️ 写侧 bit 保持原样(不做字节序翻转)—— PS 用小端写 LVGL 的 RGB565,
    //        低 16 位是第 1 个像素。若上板发现像素两两互换,改这里即可。
    //=========================================================================
    reg [63:0] lbuf [0:511];

    // 写入索引 = {缓冲索引, 突发序号, 突发内拍号} = burst_idx×16 + beat_cnt
    //   ⇒ 与读侧的 {rd_sel_eff, px_x[9:2]} 是同一套 0..255 的字序 ✓
    always @(posedge clk) begin
        if ((state == S_DATA) && m_axi_rvalid)
            lbuf[{tgt_sel, burst_idx, beat_cnt}] <= m_axi_rdata;
    end

    //-------------------------------------------------------------------------
    //  读侧
    //    de_req 当拍:px_x[1:0]==0 时读一个字,其余拍保持
    //    下一拍 de_act:用上一拍的 px_x[1:0] 选出像素
    //-------------------------------------------------------------------------
    reg [63:0] word_r;
    reg [1:0]  sel_r;

    // 🔴 交换发生在 line_start,**而 line_start 与一行的第一拍 de_req 是【同一拍】**
    //    (两者都由 h_cnt == H_REQ_BEG 触发)。
    //    此时 rd_sel 还是【旧值】(交换要到本拍末尾才生效),
    //    照直用它会把**每行开头 4 个像素读成上一行的缓冲** —— 屏幕左边缘一条竖缝。
    //    ⇒ 取数地址必须用"本拍生效后"的 rd_sel:line_start 当拍就等于 wr_sel。
    wire rd_sel_eff = line_start ? wr_sel : rd_sel;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            word_r <= 64'd0;
            sel_r  <= 2'd0;
        end else if (de_req) begin
            sel_r <= px_x[1:0];
            if (px_x[1:0] == 2'd0)
                word_r <= lbuf[{rd_sel_eff, px_x[9:2]}];   // 每 4 个像素读一次
            // 其余拍保持 —— 同一 word 供连续 4 个像素使用
        end
    end

    assign px_data = (sel_r == 2'd0) ? word_r[15: 0] :
                     (sel_r == 2'd1) ? word_r[31:16] :
                     (sel_r == 2'd2) ? word_r[47:32] :
                                       word_r[63:48] ;

    //=========================================================================
    //  5. 乒乓交换 + underrun 判定
    //     ⚠️ 交换发生在 line_start,与"取下一行"同拍。
    //        取数写进【新】wr_sel(= 旧 rd_sel),而显示读【新】rd_sel(= 旧 wr_sel),
    //        两者刚好互换,永不冲突。
    //     ⚠️ 判 underrun 用【旧】wr_sel —— 那一刻它还装的是"即将被换上去"的那块。
    //=========================================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            rd_sel   <= 1'b0;
            wr_sel   <= 1'b1;
            underrun <= 1'b0;
        end else begin
            if (clr_sticky) underrun <= 1'b0;           // ★ 同上;line_start 的置位在后,优先
            if (line_start) begin
                if (!buf_ready[wr_sel]) underrun <= 1'b1;   // ★ 换上一块没装满的
                rd_sel <= wr_sel;
                wr_sel <= rd_sel;
            end
        end
    end

    //=========================================================================
    //  6. 调试观测(2026-10-03 新增)
    //
    //  ★ 语义统一为「本帧内是否发生过」,由 **frame_start 清零**,不由 PS 清零。
    //    理由:PS 读一次 0x7C 的时刻是随机的,而 line_start/de_req/arready 都是
    //    **1 拍脉冲**(占一行 1344 拍的 0.07%)—— 采样式读取根本采不到。
    //    改成"本帧内出现过"后,每帧稳定可读,一次读取就能定性,也不需要额外的清端口。
    //
    //  ★ 0x7C 位布局(修订后)
    //    [1:0]   state                    实时
    //    [2]     rd_sel                   实时 ← 假设 B 会看到它恒为 0
    //    [3]     wr_sel                   实时
    //    [5:4]   buf_ready                实时
    //    [6]     fetch_pend               实时
    //    [7]     fetch_en                 实时(= src_sel == 2)
    //    [8]     rresp_bad_frm            本帧出现过 rresp != OKAY
    //    [9]     rvalid_frm               本帧收到过读数据
    //    [10]    arready_frm              本帧 AR 握手成功过
    //    [11]    arvalid_frm              本帧发起过 AR
    //    [12]    line_start_frm           本帧 line_start 出现过
    //    [13]    de_req_frm               本帧 de_req 出现过
    //    [14]    wdt_expired_frm          本帧看门狗超时过
    //    [15]    both_ready_frm           本帧两块缓冲都装满过
    //    [23:16] wdt_expire_cnt           本帧看门狗超时次数(饱和 255)
    //    [27:24] burst_idx                实时
    //    [31:28] beat_cnt                 实时
    //
    //  ★ 判决表(src=2 全黑时)
    //    [11]=1 且 0x74 低 18 位 == 0  ⇒ AR 发出去了但一拍数据都没回来 ⇒ **假设 A**(HP/SmartConnect)
    //    0x74 低 18 位 ≈ 153600        ⇒ 数据确实进了 PL ⇒ 黑屏出在读侧 ⇒ **假设 B**
    //    [12]=0                        ⇒ line_start 根本没动 ⇒ 问题在 lcd_timing,不在这里
    //
    //  ★ 0x74 位布局(2026-10-03 二次修订 —— 拍数已证明是满的,改问"数据是什么")
    //    [17: 0] beats_cnt        本帧 m_axi_rvalid 拍数(满帧 = 256 × 600 = 153600)
    //    [19:18] rresp_lat        本帧【最后一次】非 OKAY 响应的原始编码
    //                             00=无错 01=EXOKAY 10=SLVERR 11=**DECERR**(地址被拒)
    //    [20]    axi_or_nz        本帧任一拍 rdata 有任一比特为 1
    //    [21]    px_or_nz         本帧 px_data 出现过任一比特为 1
    //    [31:22] 保留(0)
    //
    //  ★ 为什么要这三条内容证据:
    //    「拍数满帧 + 屏全黑」有两种物理成因,拍数分不开:
    //      ① rdata 本身全零 ⇒ 从设备拒了地址(DECERR/SLVERR),**握手照常完成**
    //      ② rdata 非零但读侧没取出来 ⇒ 行缓冲/像素选择错
    //    rresp_lat 与 axi_or_nz 一眼分开这两者:
    //      rresp_lat=11 且 axi_or_nz=0  ⇒ ①(问题在地址/互联,不在本模块)
    //      rresp_lat=00 且 axi_or_nz=1  ⇒ ②(数据是好的,问题在下面 §4/§5)
    //=========================================================================
    reg  [7:0] dbg_wdt_cnt;
    reg        dbg_rresp_bad, dbg_rvalid, dbg_arready, dbg_arvalid;
    reg        dbg_line_start, dbg_de_req, dbg_wdt_exp, dbg_both_ready;

    // ---- 内容证据(2026-10-03 二次修订新增)----------------------------------
    reg  [17:0] dbg_beats_cnt;               // 18 位够(153600 < 262144)
    reg  [1:0]  dbg_rresp_lat;
    reg         dbg_axi_or, dbg_px_or;

    // 看门狗超时:与下面 S_AR / S_DATA 里那两个 `else if (wdt >= WDT_LIMIT)` 分支
    //   逐字对应。超时当拍状态机就离开该状态且 wdt 归零 ⇒ 这个线只高 1 拍,不会重复计数。
    wire wdt_fire = ((state == S_AR)   && !m_axi_arready && (wdt >= WDT_LIMIT[13:0])) ||
                    ((state == S_DATA) && !m_axi_rvalid  && (wdt >= WDT_LIMIT[13:0]));

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            dbg_beats_cnt  <= 18'd0;
            dbg_rresp_lat  <= 2'b00;
            dbg_axi_or     <= 1'b0;
            dbg_px_or      <= 1'b0;
            dbg_araddr     <= {AXI_ADDR_W{1'b0}};
            dbg_wdt_cnt    <= 8'd0;
            dbg_rresp_bad  <= 1'b0;
            dbg_rvalid     <= 1'b0;
            dbg_arready    <= 1'b0;
            dbg_arvalid    <= 1'b0;
            dbg_line_start <= 1'b0;
            dbg_de_req     <= 1'b0;
            dbg_wdt_exp    <= 1'b0;
            dbg_both_ready <= 1'b0;
        end else if (frame_start) begin
            dbg_beats_cnt  <= 18'd0;        // ★ 每帧清零 ⇒ 满帧应读到 153600
            dbg_rresp_lat  <= 2'b00;
            dbg_axi_or     <= 1'b0;
            dbg_px_or      <= 1'b0;
            dbg_wdt_cnt    <= 8'd0;
            dbg_rresp_bad  <= 1'b0;
            dbg_rvalid     <= 1'b0;
            dbg_arready    <= 1'b0;
            dbg_arvalid    <= 1'b0;
            dbg_line_start <= 1'b0;
            dbg_de_req     <= 1'b0;
            dbg_wdt_exp    <= 1'b0;
            dbg_both_ready <= 1'b0;
        end else begin
            if ((state == S_DATA) && m_axi_rvalid) begin
                if (dbg_beats_cnt != 18'h3FFFF) dbg_beats_cnt <= dbg_beats_cnt + 18'd1;
                dbg_rvalid <= 1'b1;
                if (m_axi_rresp != 2'b00) begin
                    dbg_rresp_bad <= 1'b1;
                    dbg_rresp_lat <= m_axi_rresp;   // ★ 记住编码:SLVERR(10) 还是 DECERR(11)
                end
                if (|m_axi_rdata) dbg_axi_or <= 1'b1;   // ★ 数据里出现过 1 吗
            end
            // ★ px_data 是组合输出(由 word_r/sel_r 决定),不在 de_req 时也稳定可看 ——
            //   只要整帧内出现过一次非零,就说明「行缓冲确实读出来了东西」
            if (|px_data) dbg_px_or <= 1'b1;

            if (m_axi_arvalid && m_axi_arready) begin
                dbg_arready <= 1'b1;
                dbg_araddr  <= m_axi_araddr;
            end
            if (m_axi_arvalid)  dbg_arvalid    <= 1'b1;
            if (line_start)     dbg_line_start <= 1'b1;
            if (de_req)         dbg_de_req     <= 1'b1;
            if (buf_ready == 2'b11) dbg_both_ready <= 1'b1;
            if (wdt_fire) begin
                dbg_wdt_exp <= 1'b1;
                if (dbg_wdt_cnt != 8'hFF) dbg_wdt_cnt <= dbg_wdt_cnt + 8'd1;
            end
        end
    end

    // 0x74 = {保留 10 位, px_or_nz, axi_or_nz, rresp_lat, beats_cnt[17:0]}
    assign dbg_beats = {10'b0, dbg_px_or, dbg_axi_or, dbg_rresp_lat, dbg_beats_cnt};

    assign dbg_state = {
        beat_cnt,                       // [31:28]
        burst_idx,                      // [27:24]
        dbg_wdt_cnt,                    // [23:16]
        dbg_both_ready,                 // [15]
        dbg_wdt_exp,                    // [14]
        dbg_de_req,                     // [13]
        dbg_line_start,                 // [12]
        dbg_arvalid,                    // [11]
        dbg_arready,                    // [10]
        dbg_rvalid,                     // [9]
        dbg_rresp_bad,                  // [8]
        fetch_en,                       // [7]
        fetch_pend,                     // [6]
        buf_ready,                      // [5:4]
        wr_sel,                         // [3]
        rd_sel,                         // [2]
        state                           // [1:0]
    };

endmodule
