`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Module Name : tb_spi_master
// Description : spi_master 自检测试台
//
//   四种 SPI 模式组合 + 两种位序均可通过参数切换:
//     xelab tb_spi_master -generic_top "CPOL=1" -generic_top "CPHA=0"
//
//   两个方向分别验证,两个图案必须不同 —— 否则"MISO 根本没接"也会判 PASS:
//       MOSI 通路   slave_rx_cap === pat_tx
//       MISO 通路   rx_data      === pat_rx
//
//   ⚠️ 从机模型只依据 SPI 模式语义实现,不引用 DUT 内部任何信号
//      (bit_cnt / state / t_flag 均不碰)。但它是"第二个实现",
//      若与 DUT 同错会双双通过 —— 故四模式全测,不依赖"猜对模式"。
//////////////////////////////////////////////////////////////////////////////////

module tb_spi_master #(
    parameter CPOL      = 0,
    parameter CPHA      = 0,
    parameter MSB_FIRST = 1
)();

localparam DATA_WIDTH = 16;
localparam SPI_HALF_T = 10;
localparam CS_SETUP   = 100;

localparam CLK_PERIOD = 10;                              // 100 MHz
localparam WD_TIMEOUT = 20000;                           // 单帧看门狗拍数
localparam SCLK_IDLE  = (CPOL != 0) ? 1'b1 : 1'b0;

//====================================================================
// 时基与复位
//====================================================================
reg  clk   = 1'b0;
reg  rst_n = 1'b0;
always #(CLK_PERIOD/2) clk = ~clk;

reg  [DATA_WIDTH-1:0] tx_data = 0;
reg                   start   = 0;
wire [DATA_WIDTH-1:0] rx_data;
wire                  busy;
wire                  sclk, cs, mosi, miso;

//====================================================================
// DUT
//====================================================================
spi_master #(
  .CPOL       (CPOL),
  .CPHA       (CPHA),
  .DATA_WIDTH (DATA_WIDTH),
  .MSB_FIRST  (MSB_FIRST),
  .CS_SETUP   (CS_SETUP),
  .SPI_HALF_T (SPI_HALF_T)
) dut (
  .clk     (clk),
  .rst_n   (rst_n),
  .start   (start),
  .tx_data (tx_data),
  .rx_data (rx_data),
  .busy    (busy),
  .miso    (miso),
  .sclk    (sclk),
  .cs      (cs),
  .mosi    (mosi)
);

//====================================================================
// 从机模型 —— 只依据 SPI 模式语义,独立于 DUT 实现
//====================================================================
reg [1:0] sclk_sr;
always @(posedge clk or negedge rst_n)
  if(!rst_n) sclk_sr <= {SCLK_IDLE, SCLK_IDLE};
  else       sclk_sr <= {sclk_sr[0], sclk};

wire sclk_rise =  sclk_sr[0] & ~sclk_sr[1];   // 真实上升沿(延迟 1 拍)
wire sclk_fall = ~sclk_sr[0] &  sclk_sr[1];   // 真实下降沿(延迟 1 拍)

reg cs_r;
always @(posedge clk or negedge rst_n)
  if(!rst_n) cs_r <= 1'b1;
  else       cs_r <= cs;

wire cs_fall = ~cs &  cs_r;   // CS 拉低 = 帧开始
wire cs_rise =  cs & ~cs_r;   // CS 拉高 = 帧结束

// 前导沿 = 每位的第一个沿:CPOL=0 是上升沿,CPOL=1 是下降沿
wire lead_edge  = CPOL ? sclk_fall : sclk_rise;
wire trail_edge = CPOL ? sclk_rise : sclk_fall;

// CPHA=0 → 前导沿采样、后随沿换位(第 0 位在 CS 拉低时给)
// CPHA=1 → 后随沿采样、前导沿换位
wire sample_edge = CPHA ? trail_edge : lead_edge;
wire shift_edge  = CPHA ? lead_edge  : trail_edge;

reg [DATA_WIDTH-1:0] slave_tx     = 0;   // tb 预设的"从机回送图案"
reg [DATA_WIDTH-1:0] slave_rx_cap = 0;   // 帧结束时锁存,给 tb 断言
reg [DATA_WIDTH-1:0] slave_rx     = 0;   // 从机收到的
reg [4:0]            drv_cnt      = 0;   // 已驱动位数 = 下一位的位号
reg [4:0]            rcv_cnt      = 0;   // 已采样位数
reg                  slave_miso   = 0;

integer sclk_edges = 0;

always @(posedge clk or negedge rst_n) begin
  if(!rst_n) begin
      sclk_edges   <= 0;
      slave_rx     <= 0;
      slave_rx_cap <= 0;
      drv_cnt      <= 0;
      rcv_cnt      <= 0;
      slave_miso   <= 0;
  end else begin
      // 只统计帧内(cs 为低)的边沿:CPOL=1 时上电首拍 sclk 由 X 变 1,
      // 不加这个门会多算一个沿
      if((sclk_rise | sclk_fall) && !cs) sclk_edges <= sclk_edges + 1;

      if(cs_fall) begin
          slave_rx <= 0;
          rcv_cnt  <= 0;
          drv_cnt  <= 0;
          // CPHA=0:第 0 位必须在第一个沿之前就绪 → CS 拉低时就要给
          if(!CPHA) begin
              slave_miso <= MSB_FIRST ? slave_tx[DATA_WIDTH-1] : slave_tx[0];
              drv_cnt    <= 1;
          end
      end else begin
          // 换位沿:送下一位
          if(shift_edge && (drv_cnt < DATA_WIDTH)) begin
              slave_miso <= MSB_FIRST ? slave_tx[DATA_WIDTH-1-drv_cnt]
                                      : slave_tx[drv_cnt];
              drv_cnt    <= drv_cnt + 1'b1;
          end
          // 采样沿:把 mosi 移进来
          if(sample_edge) begin
              slave_rx <= MSB_FIRST ? {slave_rx[DATA_WIDTH-2:0], mosi}
                                    : {mosi, slave_rx[DATA_WIDTH-1:1]};
              rcv_cnt  <= rcv_cnt + 1'b1;
          end
      end

      if(cs_rise) slave_rx_cap <= slave_rx;
  end
end

// ⚠️ 冒烟自测时才换回下面这行(必须同时注释掉本行):
//    assign miso = mosi;
//    两个 assign 同时驱动同一根网线 → 网线解析成 X,不是"回环"。
assign miso = slave_miso;

//====================================================================
// 记分板
//====================================================================
integer pass_cnt = 0;
integer fail_cnt = 0;

task check;
    input [8*32-1:0] name;
    input            ok;
    begin
        if(ok) begin
            pass_cnt = pass_cnt + 1;
            $display("  [PASS] %0s", name);
        end else begin
            fail_cnt = fail_cnt + 1;
            $display("  [FAIL] %0s", name);
        end
    end
endtask

//--------------------------------------------------------------------
// 发一帧:start 脉冲 → 等 busy 起 → 等 busy 落 → 断言
// 卡死时看门狗直接 $finish,不让仿真挂到天亮
//--------------------------------------------------------------------
task run_frame;
    input [DATA_WIDTH-1:0] pat_tx;
    input [DATA_WIDTH-1:0] pat_rx;
    integer wd;
    begin
        $display("--- frame: tx=%h  rx(slave)=%h ---", pat_tx, pat_rx);

        slave_tx = pat_rx;             // 阻塞赋值,必须早于 cs 拉低

        @(posedge clk);
        tx_data <= pat_tx;
        start   <= 1'b1;
        @(posedge clk);
        start   <= 1'b0;

        wd = 0;
        while(busy !== 1'b1 && wd < WD_TIMEOUT) begin @(posedge clk); wd = wd + 1; end
        if(busy !== 1'b1) begin
            $display("  [FATAL] busy did not rise -- watchdog timeout");
            fail_cnt = fail_cnt + 1;
            $finish;
        end

        wd = 0;
        while(busy !== 1'b0 && wd < WD_TIMEOUT) begin @(posedge clk); wd = wd + 1; end
        if(busy !== 1'b0) begin
            $display("  [FATAL] busy stuck high -- FSM hung?");
            fail_cnt = fail_cnt + 1;
            $finish;
        end

        repeat(4) @(posedge clk);       // 等 cs_rise 把 slave_rx 锁存好

        check("MOSI: slave_rx_cap == pat_tx", slave_rx_cap === pat_tx);
        check("MISO: rx_data      == pat_rx", rx_data      === pat_rx);
        check("slave rx bit count  == DATA_WIDTH", rcv_cnt === DATA_WIDTH);
        check("slave tx bit count  == DATA_WIDTH", drv_cnt === DATA_WIDTH);
    end
endtask

//====================================================================
// 激励序列
//====================================================================
initial begin
    $dumpfile("tb_spi_master.vcd");
    $dumpvars(0, tb_spi_master);

    $display("=========================================");
    $display("  MODE: CPOL=%0d  CPHA=%0d  MSB_FIRST=%0d", CPOL, CPHA, MSB_FIRST);
    $display("=========================================");

    rst_n = 1'b0;
    repeat(10) @(posedge clk);
    rst_n = 1'b1;
    repeat(5)  @(posedge clk);

    // 图案选取的依据(经变异测试实测,不是猜的):
    //   · A5A5 / 8001 / 0000 / FFFF 都是【回文或均匀图案】—— 位序反了照样相等,
    //     实测只能用它们抓到 2/6 帧。故改用下面这组【非回文 / 单比特】图案。
    //   · 两个图案必须不同 —— 否则 MISO 断路也判 PASS。
    run_frame(16'h0001, 16'h8000);   // 单比特在两端 —— 位序反了必错
    run_frame(16'h8000, 16'h0001);   // 反向再来一次
    run_frame(16'hAAAA, 16'h5555);   // 交替但【非回文】(AAAA 倒过来是 5555)
    run_frame(16'h1234, 16'h5678);
    run_frame(16'h0000, 16'hFFFF);   // 全 0/全 1 —— 只证明"确实发了",不测位序
    run_frame(16'h5678, 16'h1234);   // ← 连续两帧:抓"只能发一帧"

    // 只比数据会漏掉"多发/少发一个沿但数据恰好凑对"
    check("sclk edges == frames * 2 * DATA_WIDTH",
          sclk_edges === 6*2*DATA_WIDTH);

    $display("=========================================");
    $display("  PASS = %0d   FAIL = %0d", pass_cnt, fail_cnt);
    if(fail_cnt == 0) $display("  RESULT: ALL PASS");
    else              $display("  RESULT: FAIL");
    $display("=========================================");
    $finish;
end

// 全局看门狗:仿真没正常结束就强制停
initial begin
    #(CLK_PERIOD * 1000000);
    $display("  [FATAL] global watchdog -- sim did not finish normally");
    $finish;
end

endmodule
