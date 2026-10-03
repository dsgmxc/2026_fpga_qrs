`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Module Name: tb_adc_9833
//
// 自检型测试台:只从 adc_sclk / adc_sync / adc_mosi 三根引脚上观察,不窥探 DUT 内部信号。
// 期望值全部来自 AD9833 手册(中文 Rev. E):表 2 时序 / 表 5 控制字 / 表 6 控制位 /
// 表 9 频率字 28 位两次写入规则 —— 不是照抄 RTL。
//
// 输出:逐项 PASS/FAIL + 末尾汇总,可直接重定向到日志文件存档:
//     xsim tb_adc_9833 -R > run.log
//
// ⚠️ 只有一处加速参数 —— PWR_WAIT 被压缩,「上电等多久」本次不验证。
//    其余全部保持真值:SPI_HALF_T=256 / CS_SETUP=1000 / 帧间隔 1024 拍。
//    ⇒ 本测试台测出的 t7 / t8 / t4 是**真实时序值**,可与手册窗口直接比对
//      (这正是 dac_1220 那份测试台做不到的:那边压缩了三个毫秒级等待)。
//
// ⚠️ 采样的信号沿:CPOL=1 / CPHA=0 ⇒ 空闲高、**从机在下降沿取数**(手册 p13 图 4)。
//    故本测试台也在 SCLK 下降沿采样 SDATA,MSB first。
//
// ⚠️ chk() 的标签**必须写英文**:实测 xsim 2025.2 在 `%s` 里打印宽向量时,
//    非 ASCII 字节会被破坏(帧 = e5 b8 a7 打成 ff ff ff e5 ff a7)。
//    内联 $display 里的中文没有问题,所以中文说明照写,只把标签写成英文。
//////////////////////////////////////////////////////////////////////////////////

module tb_adc_9833();

// ---------------- 时钟与 DUT 加速参数 ----------------
localparam CLK_PERIOD  = 20;      // 20 ns = 50 MHz(adc_9833 全部常量按 50 MHz 标定)
localparam PWR_WAIT_TB = 1000;    // 真值 10_000_000(200 ms);此处压缩,本次不验证「等多久」

// ---------------- 手册时序窗口(表 2,p4;1 拍 = 20 ns)----------------
localparam SCLK_HALF_T = 256;                        // 与 DUT 例化参数一致
localparam T4_NS       = 2*SCLK_HALF_T*CLK_PERIOD;   // SCLK 周期 = 10240 ns(表2 下限 25 ns)
localparam T7_MIN_NS   =     5;                      // FSYNC → SCLK 下降沿建立时间
localparam T8_MIN_NS   =    10;                      // SCLK 下降沿 → FSYNC 上升沿保持时间
localparam T8_MAX_NS   = T4_NS - 5;                  // 表2:t8 max = t4 − 5 ns = 10235 ns
localparam T11_MIN_NS  =     5;                      // SCLK 高电平 → FSYNC 下降沿建立时间

// ---------------- 期望值(手算自手册,不是抄 RTL)----------------
// 表9 规则:B28(D13)=1 时,一个完整 28 位字由**两次连续写入**载入,
//   两次写入的 D15,D14 完全相同(频率寄存器 = 01);
//   **第一次 = 14 个 LSB,第二次 = 14 个 MSB**。
// 表6 位图: 0 0 B28 HLB FSELECT PSELECT 0 RESET SLEEP1 SLEEP12 OPBITEN 0 DIV2 0 MODE 0
//
// 初始化 4 帧:
//   0x2100 = 0010 0001 0000 0000 → 控制字(D15,D14=00), B28=1, RESET=1
//   0x570A = 0101 0111 0000 1010 → FREQ0 写入(D15,D14=01), 14 个 LSB = 0x170A
//   0x428F = 0100 0010 1000 1111 → FREQ0 写入(D15,D14=01), 14 个 MSB = 0x028F
//   0x2000 = 0010 0000 0000 0000 → 控制字(D15,D14=00), B28=1, RESET=0(释放复位)
//
// 目标 FREQREG = {0x028F, 0x170A} = 0x0A3D70A = 10 737 418
//   手册 p15:f_out = fMCLK/2^28 × FREQREG = 25 MHz × 10737418 / 268435456 = 999 999.978 Hz
localparam [15:0] EXP_INIT0 = 16'h2100,
                  EXP_INIT1 = 16'h570A,
                  EXP_INIT2 = 16'h428F,
                  EXP_INIT3 = 16'h2000;
localparam [27:0] FREQREG_A = 28'h0A3D70A;

// 三个测试码值 → 各自的两帧(全部手算后写死成常量 ⇒ 判据独立于 RTL)
//   CODE_A = 0x0A3D70A : & 0x3FFF = 0x170A → 0x570A ; >> 14 =  655 = 0x028F → 0x428F
//   CODE_B = 0x1234567 : & 0x3FFF = 0x0567 → 0x4567 ; >> 14 = 1165 = 0x048D → 0x448D
//   CODE_C = 0x0AAAAAA : & 0x3FFF = 0x2AAA → 0x6AAA ; >> 14 =  682 = 0x02AA → 0x42AA
localparam [27:0] CODE_A = 28'h0A3D70A,
                  CODE_B = 28'h1234567,
                  CODE_C = 28'h0AAAAAA;
localparam [15:0] EXP_A_LSB = 16'h570A, EXP_A_MSB = 16'h428F;
localparam [15:0] EXP_B_LSB = 16'h4567, EXP_B_MSB = 16'h448D;
localparam [15:0] EXP_C_LSB = 16'h6AAA, EXP_C_MSB = 16'h42AA;

// ---------------- DUT ----------------
reg  clk    = 1'b0;
reg  rst_n  = 1'b0;
reg  set_in = 1'b0;
reg  [27:0] code = 28'h0A3D70A;

wire adc_sclk;
wire adc_sync;
wire adc_mosi;
wire init_done;

always #(CLK_PERIOD/2) clk = ~clk;

adc_9833 #(
	.PWR_WAIT (PWR_WAIT_TB)
) dut (
	.clk_50m  (clk),
	.rst_n    (rst_n),
	.adc_sclk (adc_sclk),
	.adc_sync (adc_sync),
	.adc_mosi (adc_mosi),
	.code     (code),
	.set_in   (set_in),
	.init_done(init_done)
);

// ---------------- 计数与统计 ----------------
integer nchk, npass, nfail;
integer nframe;              // 已解出的完整帧数
integer nbit_cur;            // 当前帧已采位数
integer n_stray;             // 协议违例计数(帧外时钟 / 帧不满 16 位)
reg [15:0] frm [0:15];       // 解出的帧
integer t_frame_end [0:15];  // 每帧最后一个 SCLK 下降沿时刻(ns)
integer t_init_done;         // init_done 拉高时刻(ns)
integer n_init_done_rise;    // init_done 上升沿次数(应恰好 1)
// 时序实测(ns)
integer t_fall_last, t_rise_last;    // 上一次 SCLK 下降 / 上升沿时刻
integer t_sync_fall, t_sync_rise;    // 本帧 FSYNC 下降 / 上升沿时刻
integer half_min, half_max;          // SCLK 半周期实测
integer t7_min, t8_min, t8_max, t11_min;
integer t8_eff_min;                  // FSYNC 上升沿 → 下一帧首个 SCLK 下降沿(有效裕量)
integer t_gap_min;                   // FSYNC 上升沿 → 下一帧 FSYNC 下降沿(帧间空闲)
integer t_gap_prev;
reg     wait_first_fall;             // 等本帧第一个 SCLK 下降沿(用于量 t7)
reg     wait_fall_after_rise;        // 等 FSYNC 抬起后的第一个下降沿(用于量 t8_eff)
reg     sclk_not_high_at_fsync;      // t11 违例标志
reg     cs_idle_high_seen;           // 复位期间 FSYNC 是否保持高
// 引脚监视用
reg        sclk_d = 1'b1;
reg        sync_d = 1'b1;
reg        init_done_d = 1'b0;
reg [15:0] sh = 16'h0000;

// ---------------- 检查记账 ----------------
task chk;
	input ok;
	input [8*128-1:0] label;    // 标签写英文:xsim 的 %s 会破坏宽向量里的非 ASCII 字节
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

task push_frame;
	input [15:0] w;
	begin
		$display("  [帧 %0d] 0x%04h   @ %0.2f us", nframe, w, $time/1000.0);
		if (nframe < 16) begin
			frm[nframe]         = w;
			t_frame_end[nframe] = t_fall_last;
		end
		nframe = nframe + 1;
	end
endtask

// ---------------- 引脚监视 ----------------
always @(posedge clk) begin
	sclk_d      <= adc_sclk;
	sync_d      <= adc_sync;
	init_done_d <= init_done;

	if (init_done && !init_done_d) begin
		t_init_done      = $time;
		n_init_done_rise = n_init_done_rise + 1;
	end

	// ---- SCLK 沿 ----
	if (sclk_d != adc_sclk) begin
		if (sclk_d == 1'b1) begin                  // 下降沿
			if (adc_sync === 1'b0) begin
				if (wait_first_fall) begin         // 本帧第一个下降沿 → t7
					wait_first_fall = 1'b0;
					if (($time - t_sync_fall) < t7_min) t7_min = $time - t_sync_fall;
				end
				sh          = {sh[14:0], adc_mosi};   // 下降沿取数,MSB first
				nbit_cur    = nbit_cur + 1;
				t_fall_last = $time;
				if (nbit_cur == 16) begin
					push_frame(sh);
					nbit_cur = 0;
					sh       = 16'h0000;
				end
			end else begin
				n_stray = n_stray + 1;             // 帧外打时钟 = 协议违例
			end
			if (wait_fall_after_rise) begin        // FSYNC 抬起后的第一个下降沿
				wait_fall_after_rise = 1'b0;
				if ((($time  - t_sync_rise)) < t8_eff_min) t8_eff_min = $time - t_sync_rise;
			end
		end else begin                             // 上升沿
			if (($time - t_fall_last) < half_min) half_min = $time - t_fall_last;
			if (($time - t_fall_last) > half_max) half_max = $time - t_fall_last;
			t_rise_last = $time;
		end
	end

	// ---- FSYNC 沿 ----
	if (sync_d != adc_sync) begin
		if (sync_d == 1'b1) begin                  // FSYNC 下降沿 = 开一帧
			t_sync_fall     = $time;
			wait_first_fall = 1'b1;
			if (t_gap_prev != 0)
				if (($time - t_gap_prev) < t_gap_min) t_gap_min = $time - t_gap_prev;
			// t11:FSYNC 变低时 SCLK 必须处于高电平(表2 / p13 原文)
			if (($time - t_rise_last) < t11_min) t11_min = $time - t_rise_last;
			if (adc_sclk !== 1'b1) sclk_not_high_at_fsync = 1'b1;
		end else begin                             // FSYNC 上升沿 = 收帧
			t_gap_prev = $time;
			t_sync_rise = $time;
			// t8:末个 SCLK 下降沿 → FSYNC 上升沿
			if (($time - t_fall_last) < t8_min) t8_min = $time - t_fall_last;
			if (($time - t_fall_last) > t8_max) t8_max = $time - t_fall_last;
			if (nbit_cur != 0) n_stray = n_stray + 1;   // 帧没凑满 16 位就抬 FSYNC
			wait_fall_after_rise = 1'b1;
		end
	end
end

// ---------------- 复位期 FSYNC 空闲态监视 ----------------
always @(posedge clk)
	if (!rst_n && (adc_sync === 1'b1)) cs_idle_high_seen = 1'b1;

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
	reg [15:0] ctl_masked;
	reg [27:0] freqreg;
	real       f_out;
	begin
		// ---- A. 初始化 4 帧 ----
		$display("");
		$display("A. 初始化 4 帧(表 5 控制字 / 表 9 频率写入规则)");
		$display("   帧0 实测 0x%04h / 期望 0x%04h   %s", frm[0], EXP_INIT0, (frm[0]===EXP_INIT0)?"PASS":"FAIL");
		$display("   帧1 实测 0x%04h / 期望 0x%04h   %s", frm[1], EXP_INIT1, (frm[1]===EXP_INIT1)?"PASS":"FAIL");
		$display("   帧2 实测 0x%04h / 期望 0x%04h   %s", frm[2], EXP_INIT2, (frm[2]===EXP_INIT2)?"PASS":"FAIL");
		$display("   帧3 实测 0x%04h / 期望 0x%04h   %s", frm[3], EXP_INIT3, (frm[3]===EXP_INIT3)?"PASS":"FAIL");
		chk(nframe >= 4,          "at least 4 init frames emitted");
		chk(frm[0] === EXP_INIT0, "init frame0 = 0x2100");
		chk(frm[1] === EXP_INIT1, "init frame1 = 0x570A");
		chk(frm[2] === EXP_INIT2, "init frame2 = 0x428F");
		chk(frm[3] === EXP_INIT3, "init frame3 = 0x2000");

		// ---- B. 控制字位域 ----
		$display("");
		$display("B. 控制字位域(表 5 / 表 6;只看帧 0 与帧 3)");
		chk(frm[0][15:14] === 2'b00,          "frame0 D15,D14 = 00 -> control register");
		chk(frm[3][15:14] === 2'b00,          "frame3 D15,D14 = 00 -> control register");
		chk(frm[1][15:14] === 2'b01,          "frame1 D15,D14 = 01 -> FREQ0 write");
		chk(frm[2][15:14] === 2'b01,          "frame2 D15,D14 = 01 -> FREQ0 write");
		chk(frm[1][15:14] === frm[2][15:14],  "both freq frames share same D15,D14 (Table 9)");
		chk(frm[0][13] === 1'b1,              "frame0 B28(D13) = 1 -> 28-bit two-write load");
		chk(frm[3][13] === 1'b1,              "frame3 B28(D13) = 1");
		chk(frm[0][8]  === 1'b1,              "frame0 RESET(D8) = 1 -> power-on reset");
		chk(frm[3][8]  === 1'b0,              "frame3 RESET(D8) = 0 -> reset released");
		ctl_masked = frm[0] & 16'hFEFF;       // 帧0 抹掉 D8 后应与帧3 全等
		chk(ctl_masked === frm[3],            "frame0 vs frame3 differ only in RESET (f0^f3==0x0100)");
		chk(frm[0][12] === 1'b0,              "HLB(D12) = 0 (ignored when B28=1)");
		chk(frm[3][11] === 1'b0,              "FSELECT(D11) = 0 -> FREQ0 selected");
		chk(frm[3][10] === 1'b0,              "PSELECT(D10) = 0 -> PHASE0 selected");
		chk(frm[3][7]  === 1'b0,              "SLEEP1(D7) = 0 -> MCLK enabled");
		chk(frm[3][6]  === 1'b0,              "SLEEP12(D6) = 0 -> DAC active");
		chk(frm[3][5]  === 1'b0,              "OPBITEN(D5) = 0 -> DAC to VOUT");
		chk(frm[3][3]  === 1'b0,              "DIV2(D3) = 0");
		chk(frm[3][1]  === 1'b0,              "MODE(D1) = 0 -> sinusoidal output");
		chk((frm[3] & 16'h0015) === 16'h0000, "reserved D4 / D2 / D0 all zero");

		// ---- C. 频率字重建 + 输出频率 ----
		$display("");
		$display("C. 频率字重建(表9:第一帧 = 14 个 LSB,第二帧 = 14 个 MSB)");
		freqreg = {frm[2][13:0], frm[1][13:0]};
		f_out   = 25.0e6 * freqreg / 268435456.0;   // 手册 p15:f_out = fMCLK/2^28 × FREQREG
		$display("   FREQREG 实测 28'h%07h / 期望 28'h%07h", freqreg, FREQREG_A);
		$display("   f_out = 25 MHz x %0d / 2^28 = %0.4f Hz", freqreg, f_out);
		chk(freqreg === FREQREG_A,                     "init FREQREG rebuilt = 0xA3D70A");
		chk((f_out > 999999.0) && (f_out < 1000001.0), "f_out within 1 Hz of 1 MHz (target 999999.978 Hz)");

		// ---- D. 正常态:三次更新 ----
		$display("");
		$display("D. 正常态(分帧送码值)");
		$display("   更新1 code=28'h%07h 实测 %04h %04h / 期望 %04h %04h  %s",
		         CODE_A, frm[4], frm[5], EXP_A_LSB, EXP_A_MSB,
		         ((frm[4]===EXP_A_LSB)&&(frm[5]===EXP_A_MSB))?"PASS":"FAIL");
		$display("   更新2 code=28'h%07h 实测 %04h %04h / 期望 %04h %04h  %s",
		         CODE_B, frm[6], frm[7], EXP_B_LSB, EXP_B_MSB,
		         ((frm[6]===EXP_B_LSB)&&(frm[7]===EXP_B_MSB))?"PASS":"FAIL");
		$display("   更新3 code=28'h%07h 实测 %04h %04h / 期望 %04h %04h  %s",
		         CODE_C, frm[8], frm[9], EXP_C_LSB, EXP_C_MSB,
		         ((frm[8]===EXP_C_LSB)&&(frm[9]===EXP_C_MSB))?"PASS":"FAIL");
		chk((frm[4]===EXP_A_LSB) && (frm[5]===EXP_A_MSB), "update1 frames = 570A / 428F");
		chk((frm[6]===EXP_B_LSB) && (frm[7]===EXP_B_MSB), "update2 frames = 4567 / 448D");
		// 与初始化互证:更新1 喂的就是 init 那个频率字 → 两帧应逐字相同
		chk((frm[4]===frm[1]) && (frm[5]===frm[2]),
		    "update1 frames byte-identical to init frames 1/2 (same Table 9 rule)");
		chk((frm[8]===EXP_C_LSB) && (frm[9]===EXP_C_MSB),
		    "update3 = 6AAA / 42AA (trigger fired mid-transmission, edge not lost)");
		chk(nframe == 10,                                 "total frames = 10 (4 init + 3 updates x 2)");
		chk({frm[5][13:0], frm[4][13:0]} === CODE_A,      "update1 FREQREG rebuilt = 0x0A3D70A");
		chk({frm[7][13:0], frm[6][13:0]} === CODE_B,      "update2 FREQREG rebuilt = 0x1234567");
		chk({frm[9][13:0], frm[8][13:0]} === CODE_C,      "update3 FREQREG rebuilt = 0x0AAAAAA");

		// ---- E. 时序实测 ----
		$display("");
		$display("E. 时序实测(全部为真值,未加速)");
		$display("   SCLK 半周期 实测 %0.2f ns(设计值 %0d ns = SPI_HALF_T 拍,表2 t5/t6 下限 10 ns)",
		         half_min/1.0, SCLK_HALF_T*CLK_PERIOD);
		$display("   t4  SCLK 周期 实测 %0.2f ns(表2 下限 25 ns)", 2.0*half_min);
		$display("   t7  FSYNC->SCLK 下降沿 最小 %0.2f ns(表2 下限 %0d ns)", t7_min/1.0, T7_MIN_NS);
		$display("   t8  SCLK 下降沿->FSYNC 上升沿 最小 %0.2f / 最大 %0.2f ns(表2 窗口 %0d ~ %0d ns)",
		         t8_min/1.0, t8_max/1.0, T8_MIN_NS, T8_MAX_NS);
		$display("   t11 SCLK 高->FSYNC 下降沿 最小 %0.2f ns(表2 下限 %0d ns)", t11_min/1.0, T11_MIN_NS);
		$display("   帧间 FSYNC 高电平 最小 %0.2f ns", t_gap_min/1.0);
		$display("   FSYNC 上升沿 -> 下一帧首个 SCLK 下降沿 最小 %0.2f ns(这才是 t8 max 要保护的东西)",
		         t8_eff_min/1.0);
		chk(half_min == SCLK_HALF_T*CLK_PERIOD, "SCLK half period = SPI_HALF_T = 256 clk (5120 ns)");
		chk(half_min == half_max,               "SCLK half period jitter = 0");
		chk(half_min >= 10,                     "t5/t6 SCLK high/low duration >= 10 ns");
		chk(2*half_min >= 25,                   "t4 SCLK period >= 25 ns");
		chk(t7_min >= T7_MIN_NS,                "t7 >= 5 ns");
		chk(t8_min >= T8_MIN_NS,                "t8 >= 10 ns");
		chk(t8_max <= T8_MAX_NS,                "t8 <= t4 - 5 ns (LITERAL bound; see note in E)");
		chk(t8_eff_min >= T11_MIN_NS,           "next SCLK falling edge after FSYNC rise >= 5 ns");
		chk(!sclk_not_high_at_fsync,            "t11: SCLK high when FSYNC falls");
		chk(n_stray == 0,                       "no protocol violation (16 bits/frame, no stray SCLK)");

		// ---- F. init_done ----
		$display("");
		$display("F. init_done");
		$display("   第 4 帧结束 %0.2f us / init_done %0.2f us(相差 %0.2f us)",
		         t_frame_end[3]/1000.0, t_init_done/1000.0, (t_init_done - t_frame_end[3])/1000.0);
		chk(t_init_done > t_frame_end[3], "init_done asserted after frame3");
		chk(n_init_done_rise == 1,        "init_done rises exactly once");

		// ---- G. 复位空闲态 ----
		chk(cs_idle_high_seen, "FSYNC idle high during reset");

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
	$display(" adc_9833 功能仿真自检报告");
	$display(" 时钟 50 MHz / PWR_WAIT=%0d(加速值,真值 10000000)", PWR_WAIT_TB);
	$display(" SPI_HALF_T=256 / CS_SETUP=1000 / 帧间隔 1024 拍 —— 均为真值");
	$display(" 期望值来源:AD9833 手册中文 Rev. E 表 2 / 表 5 / 表 6 / 表 9 / p15");
	$display("============================================================");

	nchk=0; npass=0; nfail=0; nframe=0; nbit_cur=0; n_stray=0;
	t_init_done=0; n_init_done_rise=0;
	t_fall_last=0; t_rise_last=0; t_sync_fall=0; t_sync_rise=0; t_gap_prev=0;
	half_min=999999; half_max=0;
	t7_min=999999; t8_min=999999; t8_max=0; t11_min=999999;
	t8_eff_min=999999; t_gap_min=999999; wait_first_fall=1'b0;
	wait_fall_after_rise=1'b0;
	sclk_not_high_at_fsync=1'b0; cs_idle_high_seen=1'b0;

	rst_n  = 1'b0;
	set_in = 1'b0;
	repeat (10) @(posedge clk);          // 复位期先观察 FSYNC 空闲态
	rst_n = 1'b1;

	wait (init_done == 1'b1);
	$display(" init_done 于 %0.2f us 拉高 → 开始逐帧测量", $time/1000.0);
	$display("");

	// ---- 更新 1:喂 init 那个频率字,两帧应与 init 帧1/帧2 逐字相同 ----
	code = CODE_A;
	$display("-- 更新1:code = 28'h%07h --", code);
	pulse_set_in();
	wait (nframe >= 6);
	repeat (200) @(posedge clk);

	// ---- 更新 2 ----
	code = CODE_B;
	$display("");
	$display("-- 更新2:code = 28'h%07h --", code);
	pulse_set_in();
	wait (nframe >= 7);                  // 帧6 已收完,正处在更新2 两帧之间
	$display("   帧6 已收完 → 现在把更新3 的触发打在**发射中途**");
	repeat (20) @(posedge clk);

	// ---- 更新 3:触发落在更新2 的两帧之间 ----
	code = CODE_C;
	$display("");
	$display("-- 更新3:code = 28'h%07h(触发落在更新2 的 GAP 里)--", code);
	pulse_set_in();
	wait (nframe >= 10);
	repeat (500) @(posedge clk);

	report();
	$finish;
end

// ---------------- 看门狗 ----------------
initial begin
	#20_000_000;
	$display("");
	$display("**** 超时:20 ms 内没跑完,波形或状态机可能卡住 ****");
	$display("     已解出帧数 = %0d(期望 10),当前可能在等 set_in 沿。", nframe);
	report();
	$finish;
end

endmodule
