`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Module Name: tb_dac_1220
//
// 自检型测试台:只从 dac_sclk / dac_mosi / dac_cs 三根引脚上观察,不窥探 DUT 内部信号。
// 期望值全部来自 TI 手册 SBAS082G(Table 5/6/7/9/11/13),不是照抄 RTL。
//
// 输出:逐项 PASS/FAIL + 末尾汇总,可直接重定向到日志文件存档:
//     xsim tb_dac_1220 -R > run.log
//
// ⚠️ 三条加速参数把「毫秒级等待」压缩了 —— 「等多久」本次不验证,
//    验证的是协议层(复位图案 / 帧格式 / 字节序 / 寄存器语义)与状态机转移。
//    字节级时序(SPI_HALF_T=256、字节间隔 1024 拍、复位图案六段)保持真值。
//////////////////////////////////////////////////////////////////////////////////

module tb_dac_1220();

// ---------------- 时钟与 DUT 加速参数 ----------------
localparam CLK_PERIOD     = 20;        // 20 ns = 50 MHz(dac_1220 全部常量按 50 MHz 标定)
localparam PWR_WAIT_TB    = 1000;
localparam CAL_WAIT_TB    = 3000;
localparam SETTLE_WAIT_TB = 1500;

// ---------------- 手册窗口(Table 5;tXIN = 406.90 ns = 20.345 拍)----------------
localparam T16_MIN = 10416, T16_MAX = 16276;   // 512 ~ 800 tXIN  = 208.3 ~ 325.5 us
localparam T17_MIN =   204;                    // >= 10 tXIN      = 4.07 us
localparam T18_MIN = 20833, T18_MAX = 36621;   // 1024 ~ 1800 tXIN= 416.7 ~ 732.4 us
localparam T19_MIN = 41666, T19_MAX = 48828;   // 2048 ~ 2400 tXIN= 833.3 ~ 976.6 us
localparam T14_MIN =   834;                    // >= 41 tXIN(命令→命令,CS tied low)

// 字节切分阈值:字节内相邻下降沿 512 拍,字节间 >= 1500 拍 → 取 1000 拍
localparam GAP_TH = 1000;

// ---------------- 期望值(来自手册,不是抄 RTL)----------------
// 校准序列 = 写 CMR 2 字节 @ADR=4(0x24)+ 单独改 CMR 低字节(0x05)
localparam [7:0] EXP_CAL0 = 8'h24, EXP_CAL1 = 8'h20, EXP_CAL2 = 8'hA0,
                 EXP_CAL3 = 8'h05, EXP_CAL4 = 8'hA1;
// 写完后 CMR = 0x20A1:bit13=1(手册要求写 1)/ RES=1(20 位)/ DF=1(straight binary)/ MD=01(自校准)
localparam [15:0] EXP_CMR = 16'h20A1;

// ---------------- DUT ----------------
reg  clk    = 1'b0;
reg  rst_n  = 1'b0;
reg  set_in = 1'b0;
reg  [19:0] code = 20'h12350;

wire dac_sclk;
wire dac_mosi;
wire dac_cs;
wire init_done;

always #(CLK_PERIOD/2) clk = ~clk;

dac_1220 #(
	.PWR_WAIT    (PWR_WAIT_TB),
	.CAL_WAIT_T  (CAL_WAIT_TB),
	.SETTLE_WAIT (SETTLE_WAIT_TB)
) dut (
	.clk_50m  (clk),
	.rst_n    (rst_n),
	.dac_sclk (dac_sclk),
	.dac_cs   (dac_cs),
	.dac_mosi (dac_mosi),
	.code     (code),
	.set_in   (set_in),
	.init_done(init_done)
);

// ---------------- 计数与统计 ----------------
integer nchk, npass, nfail;
integer nbyte;                  // 已解出的字节数
integer frame_idx;              // 当前第几帧(从 1 起)
integer nh, nl;                 // 已记录的复位图案高/低段数
integer h_w [0:2];              // 复位图案三段高电平宽度(拍)
integer l_w [0:1];              // 复位图案两段低电平宽度(拍)
integer rx [0:15];              // 解出的字节
integer t_byte [0:15];          // 每个字节最后一个 SCLK 下降沿的时刻(ns)
integer t_init_done;            // init_done 拉高时刻(ns)
integer t_reset_end;            // 复位图案结束时刻(ns)
integer half_min, half_max;     // SCLK 半周期实测(拍)
integer t14_min;                // 字节间隔「下降沿→上升沿」实测最小值(拍)
integer nfrag;                  // 复位图案阶段的非字节片段
reg     cs_high_seen;           // dac_cs 是否曾经拉高
reg [19:0] exp_code [0:2];      // 每帧期望码值
// 引脚监视用
reg        sclk_d = 1'b0;
integer    gap_cnt;             // 距上一个 SCLK 沿的拍数
integer    bit_idx;             // 当前字节已采样位数
integer    nedge;               // SCLK 沿总数(前 6 个 = 复位图案)
reg [7:0]  sh;
integer    t_cur_byte_end;      // 当前字节最后一个 SCLK 下降沿的时刻(ns)

// ---------------- 检查记账 ----------------
task chk;
	input ok;
	input [8*128-1:0] label;    // 128 字节,够放中文注释(UTF-8 每字 3 字节)
	begin
		nchk = nchk + 1;
		if (ok) begin
			npass = npass + 1;
			$display("  PASS | %0s", label);
		end else begin
			nfail = nfail + 1;
			$display("  FAIL | %0s", label);
		end
	end
endtask

// ---------------- 寄存器模型:Table 6/7 解命令 + Table 11 映射地址 ----------------
reg [7:0] regimg [0:15];
integer   cmd_left;             // 当前命令还剩几个数据字节
integer   cmd_addr;
reg       expect_cmd;           // 1 = 下一个字节是命令字节
reg       dir_pending;          // 当前命令是不是「写 DIR 3 字节 @ADR=0」

task do_check_dir;
	reg [23:0] act24, exp24;
	begin
		act24 = {regimg[0], regimg[1], regimg[2]};
		exp24 = {exp_code[frame_idx], 4'b0000};      // 手册 p14:DIR = code20 << 4
		$display("  第 %0d 帧 DIR:code = 20'h%05h → DIR 实测 24'h%06h / 期望 24'h%06h",
		         frame_idx, exp_code[frame_idx], act24, exp24);
		chk(act24 === exp24, "DIR = code << 4 (datasheet p14 'left justified' / Table 9)");
	end
endtask

task parse_byte;
	input [7:0] b;
	integer mb;
	begin
		if (expect_cmd) begin
			if (b[7]) begin
				$display("  WARN | 收到读命令 0x%02h(本设计不做回读)", b);
			end else begin
				mb        = (b >> 5) & 2'b11;       // MB: 00=1 字节 01=2 10=3 11=保留
				cmd_left  = mb + 1;
				cmd_addr  = b[3:0];
				dir_pending = (mb == 2) && (b[3:0] == 4'd0);
				expect_cmd  = 1'b0;
			end
		end else begin
			if (cmd_addr < 16) regimg[cmd_addr] = b;
			cmd_addr = cmd_addr + 1;                // BD=0 → 地址自增
			cmd_left = cmd_left - 1;
			if (cmd_left == 0) begin
				expect_cmd = 1'b1;
				if (dir_pending) begin
					dir_pending = 1'b0;
					do_check_dir();
				end
			end
		end
	end
endtask

task push_byte;
	input [7:0] b;
	begin
		$display("  [字节 %0d] 0x%02h   @ %0.2f us", nbyte, b, $time/1000.0);
		rx[nbyte]     = b;
		t_byte[nbyte] = t_cur_byte_end;
		if (nbyte < 15) nbyte = nbyte + 1;
		parse_byte(b);
	end
endtask

// ---------------- 引脚监视 ----------------
always @(posedge clk) begin
	sclk_d  <= dac_sclk;
	gap_cnt  = gap_cnt + 1;

	// (1) 间隔超阈值 → 收尾当前字节
	if ((gap_cnt > GAP_TH) && (bit_idx > 0)) begin
		if (bit_idx == 8) push_byte(sh);
		else              nfrag = nfrag + 1;
		bit_idx = 0;
		sh      = 8'h00;
	end

	// (2) SCLK 沿
	if (sclk_d != dac_sclk) begin
		nedge = nedge + 1;
		if (nedge <= 6) begin                       // ---- 复位图案 ----
			// 第 1 个沿是「复位前那段空闲低电平」的结束,不算图案的一段 → 从第 2 个沿起记录
			if (sclk_d == 1'b1) begin
				if (nh < 3) begin h_w[nh] = gap_cnt; nh = nh + 1; end
			end else if (nedge > 2) begin
				if (nl < 2) begin l_w[nl] = gap_cnt; nl = nl + 1; end
			end
			if (nedge == 6) t_reset_end = $time;
		end else begin                              // ---- 数据阶段 ----
			if (sclk_d == 1'b1) begin               // 下降沿 → 采样 MOSI
				if ((bit_idx > 0) && (bit_idx < 8)) begin   // 字节内:gap = 前一段高 = SCLK 半周期
					if (gap_cnt < half_min) half_min = gap_cnt;
					if (gap_cnt > half_max) half_max = gap_cnt;
				end
				sh              = {sh[6:0], dac_mosi};
				bit_idx         = bit_idx + 1;
				t_cur_byte_end  = $time;
			end else begin                          // 上升沿
				if ((bit_idx == 0) && (nbyte > 0))  // 命令→命令 = t14
					if (gap_cnt < t14_min) t14_min = gap_cnt;
			end
		end
		gap_cnt = 0;
	end
end

always @(posedge clk) begin
	if (init_done && (t_init_done == 0)) t_init_done = $time;
	if (dac_cs === 1'b1)                 cs_high_seen = 1'b1;
end

// ---------------- 激励 ----------------
task pulse_set_in;
	begin
		@(posedge clk);
		set_in = 1'b1;
		repeat (4) @(posedge clk);
		set_in = 1'b0;
	end
endtask

// ---------------- 报告 ----------------
task report;
	reg ok;
	integer i;
	reg [23:0] d24;
	begin
		// ---- A. 复位图案 ----
		$display("");
		$display("A. 复位图案(手册 Table 5 窗口 @50 MHz)");
		$display("   段    实测        下限        上限        判定");
		$display("   t16   %0.2f us   %0.2f us   %0.2f us   %s",
		         h_w[0]*CLK_PERIOD/1000.0, T16_MIN*CLK_PERIOD/1000.0,
		         T16_MAX*CLK_PERIOD/1000.0, ((h_w[0]>=T16_MIN)&&(h_w[0]<=T16_MAX))?"PASS":"FAIL");
		$display("   t17   %0.2f us   %0.2f us   --          %s",
		         l_w[0]*CLK_PERIOD/1000.0, T17_MIN*CLK_PERIOD/1000.0,
		         (l_w[0]>=T17_MIN)?"PASS":"FAIL");
		$display("   t18   %0.2f us   %0.2f us   %0.2f us   %s",
		         h_w[1]*CLK_PERIOD/1000.0, T18_MIN*CLK_PERIOD/1000.0,
		         T18_MAX*CLK_PERIOD/1000.0, ((h_w[1]>=T18_MIN)&&(h_w[1]<=T18_MAX))?"PASS":"FAIL");
		$display("   (低段) %0.2f us   手册未单列", l_w[1]*CLK_PERIOD/1000.0);
		$display("   t19   %0.2f us   %0.2f us   %0.2f us   %s",
		         h_w[2]*CLK_PERIOD/1000.0, T19_MIN*CLK_PERIOD/1000.0,
		         T19_MAX*CLK_PERIOD/1000.0, ((h_w[2]>=T19_MIN)&&(h_w[2]<=T19_MAX))?"PASS":"FAIL");
		chk((h_w[0]>=T16_MIN)&&(h_w[0]<=T16_MAX), "t16 within [512,800] tXIN");
		chk(l_w[0]>=T17_MIN,                       "t17 >= 10 tXIN");
		chk((h_w[1]>=T18_MIN)&&(h_w[1]<=T18_MAX), "t18 within [1024,1800] tXIN");
		chk((h_w[2]>=T19_MIN)&&(h_w[2]<=T19_MAX), "t19 within [2048,2400] tXIN");

		// ---- B. 字节流 ----
		$display("");
		$display("B. 字节流");
		ok = (nbyte >= 5) && (rx[0]===EXP_CAL0) && (rx[1]===EXP_CAL1) && (rx[2]===EXP_CAL2)
		                  && (rx[3]===EXP_CAL3) && (rx[4]===EXP_CAL4);
		$display("   校准 5 字节  实测 %02h %02h %02h %02h %02h",
		         rx[0], rx[1], rx[2], rx[3], rx[4]);
		$display("                期望 %02h %02h %02h %02h %02h   %s",
		         EXP_CAL0, EXP_CAL1, EXP_CAL2, EXP_CAL3, EXP_CAL4, ok?"PASS":"FAIL");
		chk(ok, "cal sequence bytes = 24 20 A0 05 A1");

		for (i = 1; i <= 2; i = i + 1) begin
			d24 = {exp_code[i], 4'b0000};
			ok  = (rx[4+4*i-3]===8'h40) && (rx[4+4*i-2]===d24[23:16])
			   && (rx[4+4*i-1]===d24[15:8]) && (rx[4+4*i]===d24[7:0]);
			$display("   第 %0d 帧 DIR   实测 %02h %02h %02h %02h", i,
			         rx[4+4*i-3], rx[4+4*i-2], rx[4+4*i-1], rx[4+4*i]);
			$display("                期望 %02h %02h %02h %02h   (code=20'h%05h → DIR=24'h%06h)  %s",
			         8'h40, d24[23:16], d24[15:8], d24[7:0], exp_code[i], d24, ok?"PASS":"FAIL");
			chk(ok, "DIR frame bytes = 40 + code[19:12] + code[11:4] + {code[3:0],4'b0}");
		end

		// ---- C. 寄存器语义 ----
		$display("");
		$display("C. 寄存器语义(按 Table 6/7 解码命令后重建,与字节切分无关)");
		$display("   CMR 实测 16'h%04h / 期望 16'h%04h", {regimg[4], regimg[5]}, EXP_CMR);
		chk(regimg[4][5] === 1'b1,               "CMR bit13 = 1 (Table 13: write '1' to this bit)");
		chk(regimg[5][7] === 1'b1,               "CMR RES(bit7) = 1 -> 20-bit mode");
		chk(regimg[5][5] === 1'b1,               "CMR DF(bit5) = 1 -> straight binary");
		chk(regimg[5][1:0] === 2'b01,            "CMR MD(1:0) = 01 -> self-calibration mode");
		chk(regimg[4][7] === 1'b0,               "CMR ADPT(bit15) = 0 -> adaptive filtering on (default)");
		chk(regimg[4][6] === 1'b0,               "CMR CALPIN(bit14) = 0 -> output off during cal (default)");
		chk({regimg[4], regimg[5]} === EXP_CMR,  "CMR whole = 0x20A1");

		// ---- D. init_done 时刻 ----
		$display("");
		$display("D. init_done 时刻");
		$display("   复位图案结束 %0.2f us / 第 5 个校准字节 %0.2f us / init_done %0.2f us",
		         t_reset_end/1000.0, t_byte[4]/1000.0, t_init_done/1000.0);
		chk(t_init_done >= t_byte[4] + CAL_WAIT_TB*CLK_PERIOD,
		    "init_done after last cal byte + CAL_WAIT (not at reset-pattern end)");

		// ---- E. dac_cs ----
		chk(!cs_high_seen, "dac_cs stuck low since reset (D20 / datasheet p11 tie-low path)");

		// ---- F. 时序实测 ----
		$display("");
		$display("F. 时序实测");
		$display("   SCLK 半周期  %0d 拍 = %0.2f us → f_SCLK = %0.2f kHz",
		         half_min, half_min*CLK_PERIOD/1000.0, 1000.0/(2.0*half_min*CLK_PERIOD/1000.0));
		$display("   字节间隔 t14 %0d 拍 = %0.2f us(手册下限 41 tXIN = 16.68 us)",
		         t14_min, t14_min*CLK_PERIOD/1000.0);
		chk(half_min == 256,      "SCLK half period = SPI_HALF_T = 256 clk (5.12us, 97.66kHz)");
		chk(t14_min >= T14_MIN,   "byte gap t14 >= 41 tXIN");
		chk(half_min == half_max, "SCLK half period jitter = 0");

		// ---- 汇总 ----
		$display("");
		$display("============================================================");
		$display(" 检查项 %0d 项:PASS %0d / FAIL %0d", nchk, npass, nfail);
		if (nfail == 0) $display(" 结论:全部通过");
		else            $display(" 结论:存在 %0d 项失败,见上表", nfail);
		$display("============================================================");
	end
endtask

// ---------------- 主激励 ----------------
initial begin
	$display("============================================================");
	$display(" dac_1220 功能仿真自检报告");
	$display(" 时钟 50 MHz / PWR_WAIT=%0d CAL_WAIT_T=%0d SETTLE_WAIT=%0d(加速值)",
	         PWR_WAIT_TB, CAL_WAIT_TB, SETTLE_WAIT_TB);
	$display(" 期望值来源:TI SBAS082G Table 5/6/7/9/11/13");
	$display("============================================================");

	nchk=0; npass=0; nfail=0; nbyte=0; frame_idx=0;
	nh=0; nl=0; nedge=0; nfrag=0; bit_idx=0; sh=8'h00; gap_cnt=0;
	half_min=99999; half_max=0; t14_min=99999; t_init_done=0;
	cs_high_seen=1'b0; expect_cmd=1'b1; dir_pending=1'b0;
	exp_code[1] = 20'h12350;      // 三个字节全非零且互不相同 → 判别力强
	exp_code[2] = 20'hABCDE;

	rst_n  = 1'b0;
	set_in = 1'b0;
	repeat (10) @(posedge clk);
	rst_n = 1'b1;

	wait (init_done == 1'b1);
	$display("  init_done 于 %0.2f us 拉高 → 开始逐帧测量", $time/1000.0);
	$display("");

	// ---- 第 1 帧 ----
	frame_idx = 1;
	code      = exp_code[1];
	$display("-- 第 1 帧:code = 20'h%05h,set_in 打脉冲 --", code);
	pulse_set_in();
	wait (nbyte >= 9);
	repeat (SETTLE_WAIT_TB + 2500) @(posedge clk);   // 等建立窗口过去

	// ---- 第 2 帧(换码值 → 验证帧起始锁存)----
	frame_idx = 2;
	code      = exp_code[2];
	$display("");
	$display("-- 第 2 帧:code = 20'h%05h,set_in 再打脉冲 --", code);
	pulse_set_in();
	wait (nbyte >= 13);
	repeat (4000) @(posedge clk);                    // 等最后一个字节收尾

	report();
	$finish;
end

// ---------------- 看门狗 ----------------
initial begin
	#20_000_000;
	$display("");
	$display("**** 超时:20 ms 内没跑完,波形或状态机可能卡住 ****");
	$display("     已解出字节数 = %0d(期望 13),当前状态可能在等待 set_in 沿。", nbyte);
	$finish;
end

endmodule
