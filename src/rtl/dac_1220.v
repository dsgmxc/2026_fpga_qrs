`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 09/15/2026 09:56:38 PM
// Design Name: 
// Module Name: dac_1220
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module dac_1220 #(
	// 三个「毫秒级长等待」做成 parameter，仅供仿真加速覆写。
	// ⚠️ 字节级时序（SPI_HALF_T / 字节间隔 1024 / 复位图案六段）必须保持真值，不要跟着改。
	parameter PWR_WAIT    = 32'd10000000,  // 上电等待 200ms @50MHz，手册要求晶振起振≥25ms才能通信
	parameter CAL_WAIT_T  = 32'd30000000,  // 自校准等待 600ms @50MHz（手册 300–500ms，建议≥500ms）
	parameter SETTLE_WAIT = 32'd1500000    // 写 DIR 后建立等待 30ms @50MHz（20位 15ms，量程两端×2）
)(
	input clk_50m,
	input rst_n,

	output wire dac_sclk,
	output reg dac_cs,
	output wire dac_mosi,

	input [19:0] code,
	input set_in,
	output reg init_done
    );

//板载晶振2.4576mhz
reg start;
reg [7:0] tx_data;
wire busy;
wire done;
wire spi_sclk;
    
spi_master #(
  .CPOL       (0),
  .CPHA       (1),
  .DATA_WIDTH (8),
  .MSB_FIRST  (1),
  .CS_SETUP   (250), //cs拉低时长
  .SPI_HALF_T (256) //50mhz，100khz spi速率
) u_spi_master (
  .clk     (clk_50m),
  .rst_n   (rst_n),
  .start   (start),
  .tx_data (tx_data),
  .rx_data (),
  .busy    (busy),
  .done    (done),
  .miso    (1'd0),
  .sclk    (spi_sclk),
  .cs      (),
  .mosi    (dac_mosi)
);

reg set_in_d;
reg set_in_d_d;
reg [7:0] code_in_d [0:3];
always@(posedge clk_50m or negedge rst_n)begin
	if(!rst_n)
		begin
			set_in_d <= 1'd0;
			set_in_d_d <= 0;
		end
	else 
		begin
			set_in_d <= set_in;
			set_in_d_d <= set_in_d;
		end
end

reg man_sclk;
reg man_mode;
assign dac_sclk = man_mode ? man_sclk : spi_sclk;

// 四张查表用 function + case 实现 —— 纯 Verilog-2001，不含 initial 语句。
// 综合结果为常量选择逻辑（与原先的 initial 赋值表等价）。
// 表格顺序即执行顺序，索引从 0 起。

// 复位图案六段的间隔拍数
function [31:0] init_delay_f;
	input [3:0] idx;
	case(idx)
		4'd0: init_delay_f = 32'd13000;   // 单段前的小间隔
		4'd1: init_delay_f = 32'd500;
		4'd2: init_delay_f = 32'd28500;
		4'd3: init_delay_f = 32'd500;
		4'd4: init_delay_f = 32'd45000;
		4'd5: init_delay_f = 32'd500;
		default: init_delay_f = 32'd0;
	endcase
endfunction

// 复位图案六段的电平（1 = 高段，0 = 低段）
function init_sclk_f;
	input [3:0] idx;
	case(idx)
		4'd0: init_sclk_f = 1'b1;
		4'd1: init_sclk_f = 1'b0;
		4'd2: init_sclk_f = 1'b1;
		4'd3: init_sclk_f = 1'b0;
		4'd4: init_sclk_f = 1'b1;
		4'd5: init_sclk_f = 1'b0;
		default: init_sclk_f = 1'b0;
	endcase
endfunction

// 校准序列的字节间隔拍数（统一取 41×tXIN，覆盖 t9 / t14）
function [31:0] cal_delay_f;
	input [3:0] idx;
	case(idx)
		4'd0: cal_delay_f = 32'd1024;
		4'd1: cal_delay_f = 32'd1024;
		4'd2: cal_delay_f = 32'd1024;
		4'd3: cal_delay_f = 32'd1024;
		4'd4: cal_delay_f = 32'd1024;
		4'd5: cal_delay_f = 32'd1024;
		4'd6: cal_delay_f = CAL_WAIT_T;   // 600ms 自校准等待，可由测试台覆写加速
		default: cal_delay_f = 32'd0;
	endcase
endfunction

// 校准序列的命令/数据字节 —— CMR 最终值 = 0x2081
//   bit13 Reserved = 1   手册 Table 13 要求写 1
//   bit7  RES      = 1   → 20 位分辨率(DIR 是按 20 位左对齐发的,见 NORMAL_WAIT)
//   bit5  DF       = 0   → 偏移二进制补码
//   bit1:0 MD      = 01  → 自校准
//
// 🔴 2026-10-12 修正(原值 0x20A1:RES=0 十六位 / DF=1 直通二进制)
//   板上四点实测 0x7FFFF→+9.72V、0x80000→−10.0V、0x99999→−8.0V、0xA6666→−7.0V,
//   全部吻合**偏移二进制补码**(手册 Table 9:20 位补码 0 V 对应码 80000h),
//   而直通二进制下 0x7FFFF 应给 0 V。⇒ 芯片实际跑在 DF=0。
//   同时 PS 侧 `cv_bias_volts_to_code()` 现在也按补码走(旋转 0x80000),
//   **两边必须是同一个 DF**。改成 0 之后:
//     · 若这次 CMR 写落进了芯片 → 显式设为补码,与 PS 一致;
//     · 若仍没落进(见下方待查项)→ DF 上电默认就是 0,行为不变。
//   ⇒ **两种情况下都正确,零回归风险。**
//
// ⚠️ 待查(未结案):CMR 到底有没有写进去存疑 —— 若没写进去,则 MD=01 的自校准
//   也从未执行过,芯片一直跑在未校准状态。RES 改 1 后建立时间按手册是 15ms
//   (20 位),SETTLE_WAIT = 30ms 仍覆盖得住。
function [7:0] cal_byte_f;
	input [3:0] idx;
	case(idx)
		4'd0: cal_byte_f = 8'h24;   // 写 CMR,2 字节,ADR=4
		4'd1: cal_byte_f = 8'h20;   // CMR 高字节:bit13 = 1
		4'd2: cal_byte_f = 8'h80;   // CMR 低字节:RES=1 / DF=0 / MD=00
		4'd3: cal_byte_f = 8'h05;   // 写 CMR 低字节,1 字节,ADR=5
		4'd4: cal_byte_f = 8'h81;   // CMR = 0x2081,MD=01 → 进自校准
		default: cal_byte_f = 8'h00;
	endcase
endfunction



localparam IDLE = 8'd0,
		   INIT = 8'd1,
		   DELAY = 8'd2,
		   CAL = 8'd3,
		   CAL_WAIT = 8'd4,
		   NORMAL = 8'd5,
		   NORMAL_WAIT = 8'd6;
reg [7:0] state;
reg [31:0] delay_cnt;
reg [3:0] init_cnt;
reg [3:0] cal_cnt;
reg [3:0] work_cnt;
reg set_pending;//set_in 上升沿锁存标志，见 NORMAL_WAIT

always@(posedge clk_50m or negedge rst_n)begin 
	if(!rst_n)
		begin
			state <= IDLE;
			init_done <= 0;
			delay_cnt <= 0;
			man_sclk <= 0;
			man_mode <= 1;
			dac_cs <= 0;
			start <= 0;
			init_cnt <= 0;
			cal_cnt <= 0;
			tx_data <= 0;
			work_cnt <= 0;
			set_pending <= 0;
			code_in_d[0] <= 0;
			code_in_d[1] <= 0;
			code_in_d[2] <= 0;
			code_in_d[3] <= 0;
		end
	else
	case (state)
		IDLE:
			begin
				init_done <= 0;
				man_sclk <= 0;
				man_mode <= 1;
				dac_cs <= 0;
				start <= 0;
				init_cnt <= 0;
				if(delay_cnt < PWR_WAIT-1)
					delay_cnt <= delay_cnt+1;
				else
					begin
						delay_cnt <= 0;
						state <= INIT;
					end
			end
		INIT:
			begin
				if(init_cnt < 6)
					man_sclk <= init_sclk_f(init_cnt);
				state <= DELAY;
			end
		CAL:
			begin
				if(cal_cnt < 6)
					begin
						tx_data <= cal_byte_f(cal_cnt - 1);
						if(done)
							begin
								state <= DELAY;
								start <= 0;
							end
						else if(!busy)
							start <= 1;
						else
							state <= CAL;
					end
				else
					begin
						delay_cnt <= 0;
						state <= CAL_WAIT;
					end
			end
		DELAY:
			begin
				if(init_cnt < 6)
					if(delay_cnt < init_delay_f(init_cnt)-1)
						delay_cnt <= delay_cnt+1;
					else
						begin
							delay_cnt <=0;
							init_cnt <= init_cnt+1;
							state <= INIT;
						end
				else if(!init_done)
					begin
						man_mode <= 0;//复位图案结束，sclk 交还 spi_master
						if(cal_cnt < 6)
							if(delay_cnt < cal_delay_f(cal_cnt)-1)
								delay_cnt <= delay_cnt+1;
							else
								begin
									delay_cnt <=0;
									cal_cnt <= cal_cnt+1;
									state <= CAL;
								end
					end
				else
					begin
						if(work_cnt < 4-1)
							begin
								if(delay_cnt < 1024 - 1)
									delay_cnt <= delay_cnt+1;
								else
									begin
										delay_cnt <= 0;
										work_cnt <= work_cnt + 1;
										state <= NORMAL;
									end
							end
						else
							begin
								work_cnt <= 0;
								state <= NORMAL_WAIT;
							end
					end
			end
		CAL_WAIT:
			begin
				if(delay_cnt < cal_delay_f(6)-1)
					delay_cnt <= delay_cnt+1;
				else
					begin
						delay_cnt <= 0;
						init_done <= 1;
						state <= NORMAL_WAIT;
					end
			end
		NORMAL:
			begin
				if(done)
					begin
						state <= DELAY;
						start <= 0;
					end
				else if(!busy)
					begin
						tx_data <= code_in_d[work_cnt];
						start <= 1;
					end
				else
					state <= NORMAL;
			end
		NORMAL_WAIT:
			begin
				//沿无条件锁存，不受建立窗口门控 —— 否则 init_done 之后
				//立即到来的首个触发会被 SETTLE_WAIT 倒计时整个吃掉。
				if(set_in_d & !set_in_d_d)
					set_pending <= 1;
				if(delay_cnt < SETTLE_WAIT)
					delay_cnt <= delay_cnt+1;
				else
					if(set_pending)
						begin
							code_in_d[0] <= 8'b0100_0000;
							// 手册 p14:DIR 24 位、码值「左对齐」(left justified)
							//   → DIR[23:0] = code20 << 4
							//   三个数据字节依次 = code[19:12] / code[11:4] / {code[3:0],4'b0}
							//   (Table 9 算例,以 0x80000 为例对比两行:
							//      补码行   code=80000h → **芯片 VOUT = 0 V** → 模块 −10 V
							//      直通行   code=80000h → **芯片 VOUT = 2.5 V** → 模块  0 V
							//    ⇒ 同一个码,两种格式差**整个负半量程(10 V 模块级)**。
							//    本项目走补码(CMR.DF=0),故 0x80000 = −100 V 偏压,0x00000 才是 0 V。
							//    这里只做左对齐,格式由 CMR 定,见 cal_byte_f)
							code_in_d[1] <= code[19:12];
							code_in_d[2] <= code[11:4];
							code_in_d[3] <= {code[3:0], 4'b0000};
							set_pending <= 0;
							delay_cnt <= 0;
							state <= NORMAL;
						end
					else
						begin
							state <= NORMAL_WAIT;
						end
			end
		default:
			state <= IDLE;
	
	endcase

end


endmodule
