`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 09/20/2026 03:53:41 PM
// Design Name: 
// Module Name: adc_9833
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


// AD9833 上电初始化（只写三线 SPI：SDATA = adc_mosi / SCLK = adc_sclk / FSYNC = adc_sync）
//   CPOL=1 / CPHA=0，16 位一帧，FSYNC 直接用 spi_master 的 cs
//   分帧：一次 start = 一帧 16 位，帧间由 DELAY 拉开间隔
module adc_9833#(
	parameter PWR_WAIT = 32'd10000000
)(
	input clk_50m,
	input rst_n,

	output wire adc_sclk,
	output wire adc_sync,
	output wire adc_mosi,

	input [27:0] code,
	input set_in,
	output reg init_done
    );
    
reg start;
reg [15:0] tx_data;
wire busy;
wire done;

spi_master #(
  .CPOL       (1),
  .CPHA       (0),
  .DATA_WIDTH (16),
  .MSB_FIRST  (1),
  .CS_SETUP   (1000), //cs拉低时长
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
  .sclk    (adc_sclk),
  .cs      (adc_sync),
  .mosi    (adc_mosi)
);

reg set_in_d;
reg set_in_d_d;
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

function [15:0] init_word_f;
	input [3:0] idx;
	case(idx)
		4'd0: init_word_f = 16'h2100;
		4'd1: init_word_f = 16'h570A;
		4'd2: init_word_f = 16'h428F;
		4'd3: init_word_f = 16'h2000;
		default: init_word_f = 16'h0000;
	endcase
endfunction

// 正常工作：改 FREQ0，28 位分两帧发。
// 手册表9：B28=1 时两次写入的 D15,D14 完全相同（都是 01），
//   第一次写入 = 14 个 LSB，第二次写入 = 14 个 MSB。
// 拿 init 的 0xA3D70A 反推：{01,0x170A}=0x570A、{01,0x028F}=0x428F
//   —— 与初始化那两帧逐字相同，说明两张表用的是同一条规则。
function [15:0] freq_word_f;
	input [3:0] idx;
	input [27:0] c;
	case(idx)
		4'd0: freq_word_f = {2'b01, c[13:0]};
		4'd1: freq_word_f = {2'b01, c[27:14]};
		default: freq_word_f = 16'h0000;
	endcase
endfunction


localparam IDLE = 8'd0,
		   INIT = 8'd1,
		   DELAY = 8'd2,
		   NORMAL = 8'd3,
		   GAP = 8'd4,
		   NORMAL_WAIT = 8'd5;

reg [7:0] state;
reg [31:0] delay_cnt;
reg [3:0] init_cnt;
reg [3:0] work_cnt;
reg [27:0] code_in_d;
reg set_pending;

always@(posedge clk_50m or negedge rst_n)
	if(!rst_n)
		begin
			state <= IDLE;
			init_done <= 0;
			delay_cnt <= 0;
			init_cnt <= 0;
			work_cnt <= 0;
			code_in_d <= 0;
			set_pending <= 0;
			start <= 0;
			tx_data <= 0;
		end
	else
		begin
			case (state)
				IDLE:
					begin
						init_done <= 0;
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
						if(init_cnt < 4)
							tx_data <= init_word_f(init_cnt);
						if(done)
							begin
								state <= DELAY;
								start <= 0;
							end
						else if(!busy)
							start <= 1;
						else
							state <= INIT;
					end
				DELAY:
					begin
						// 帧间隔：cs 已在 DONE 抬起，这里再等一次，
						// 让波形上四帧边界清楚，也保证 start 回 0 使沿检测复位
						if(delay_cnt < 1024 - 1)
							delay_cnt <= delay_cnt+1;
						else
							begin
								delay_cnt <= 0;
								if(init_cnt < 4-1)
									begin
										init_cnt <= init_cnt+1;
										state <= INIT;
									end
								else
									begin
										init_done <= 1;
										state <= NORMAL_WAIT;
									end
							end
					end
				NORMAL:
					begin
						if(work_cnt < 2)
							tx_data <= freq_word_f(work_cnt, code_in_d);
						if(done)
							begin
								state <= GAP;
								start <= 0;
							end
						else if(!busy)
							start <= 1;
						else
							state <= NORMAL;
					end
				GAP:
					begin
						// 两帧之间的间隔，与 DELAY 同长
						if(delay_cnt < 1024 - 1)
							delay_cnt <= delay_cnt+1;
						else
							begin
								delay_cnt <= 0;
								if(work_cnt < 2-1)
									begin
										work_cnt <= work_cnt+1;
										state <= NORMAL;
									end
								else
									state <= NORMAL_WAIT;
							end
					end
				NORMAL_WAIT:
					begin
						if(set_pending)
							begin
								// 触发时才锁存，避免两帧之间 code 变化把一帧撕成两个值
								code_in_d <= code;
								set_pending <= 0;
								work_cnt <= 0;
								state <= NORMAL;
							end
						else
							state <= NORMAL_WAIT;
					end
				default:
					state <= IDLE;
			endcase
			// set_in 上升沿无条件锁存（写在 case 后面 = 同拍优先级最高）：
			// 发射期间到来的沿不会丢，留到下一次进 NORMAL_WAIT 时消费
			if(set_in_d & !set_in_d_d)
				set_pending <= 1;
		end

endmodule
