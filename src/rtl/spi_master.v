`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 09/13/2026 04:33:15 PM
// Design Name: 
// Module Name: spi_master
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

/*
SPI 主机模块
参数:
CPOL - 时钟极性（0-空闲低，1-空闲高）
CPHA - 时钟相位 （0-第一个边沿采样，1-第二个边沿采样）
DATA_WIDTH - 每帧数据位宽
FRAME_NUM - 连续帧数
MSB_FIRST - （1-MSB先发，0-LSB先发）
SPI_HALF_T - spi时钟半周期
*/

module spi_master #(
	parameter CPOL =0,
	parameter CPHA =0,
	parameter DATA_WIDTH =8,
	parameter MSB_FIRST =1,
	parameter CS_SETUP =1000,
	parameter SPI_HALF_T =4
)(
	input wire clk,
	input wire rst_n,
	
	input wire start,
	input wire [DATA_WIDTH-1:0] tx_data,
	output reg [DATA_WIDTH-1:0] rx_data,
	output reg busy,
	output reg done,
	
	input wire miso,
	output reg sclk,
	output reg cs,
	output reg mosi
    );
    
    
localparam BIT_CNT_WIDTH = $clog2(2*DATA_WIDTH+1);
localparam FLAG_CNT_WIDTH = $clog2(DATA_WIDTH+1);
localparam SCLK_IDLE = (CPOL != 0) ? 1'b1 : 1'b0;
localparam TH = 2*DATA_WIDTH ;
reg [BIT_CNT_WIDTH - 1:0] bit_cnt;
reg [31:0] cs_delay;
reg [31:0] delay_cnt;

reg [FLAG_CNT_WIDTH-1:0] t_flag;
reg [FLAG_CNT_WIDTH-1:0] r_flag;
reg [DATA_WIDTH-1:0] tx_shift;
reg [DATA_WIDTH-1:0] rx_shift;
reg miso_r;
reg start_r;
reg start_r_d;

always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
		miso_r <= 1'd0;
	else 
		miso_r <= miso;
end

always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
	begin
		start_r <= 1'd0;
		start_r_d <= 1'd0;
	end
	else 
		begin
			start_r <= start;
			start_r_d <= start_r;
		end
end


localparam IDLE = 8'd0,
		   START = 8'd1,
		   SCLK_TOG = 8'd2,
		   DONE = 8'd3;
reg [7:0] state;


always@(posedge clk or negedge rst_n)begin 
	if(!rst_n)
		begin
			cs <= 1'd1;
			sclk <= SCLK_IDLE;
			bit_cnt <= 1'd0;
			state <= IDLE;
			busy <= 1'd0;
			delay_cnt <= 1'd0;
			cs_delay <= 0;
			tx_shift <= 0;
			rx_data <= 0;
			done <= 0;
		end
	else
	case (state)
		IDLE: 
			begin
				cs <= 1'd1;
				sclk <= SCLK_IDLE;
				done <= 0;
				if(start_r & ~start_r_d)
					begin
						tx_shift <= tx_data;
						state <= START;
						rx_data <= 0;
					end
				else
					state <= IDLE;
			end
		START:
			begin
				cs <= 1'd0;
				busy <= 1'd1;
				delay_cnt <= 0;
				if(cs_delay < CS_SETUP - 1)
					cs_delay <= cs_delay + 1;
				else
					begin
						cs_delay <= 0;
						state <= SCLK_TOG;
					end
			end

		SCLK_TOG:
			begin
				if(delay_cnt < SPI_HALF_T -1)
					delay_cnt <= delay_cnt + 1;
				else
					begin
						delay_cnt <= 0;
						if(bit_cnt < TH)
							begin
								bit_cnt <= bit_cnt +1;
								state <= SCLK_TOG;
								sclk <= ~sclk;
							end
						else
							begin
								state <= DONE;
								bit_cnt <= 0;
							end
					end
			end
		DONE:
			begin
				cs <= 1'd1;
				rx_data <= rx_shift;
				busy <= 1'd0;
				state <= IDLE;
				done <= 1;
			end
		default:
			state <= IDLE;
		endcase 
end

// 沿判定：FSM 在 delay_cnt == SPI_HALF_T-1 那一拍翻 sclk，此刻 bit_cnt 就是这一沿的序号。
// 帧尾还要多等半个周期才跳 DONE，那一拍不翻沿，用 bit_cnt < 2*DATA_WIDTH 排掉。
wire edge_now = (state == SCLK_TOG) & (delay_cnt == SPI_HALF_T - 1)
              & (bit_cnt < 2*DATA_WIDTH);
// 采样沿：CPHA=0 是前沿（偶数沿），CPHA=1 是后沿（奇数沿）
wire rx_now   = edge_now & (CPHA ? bit_cnt[0] : ~bit_cnt[0]);


always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
		t_flag <= 1'd0;
	else 
		if((delay_cnt == SPI_HALF_T - 1) & (bit_cnt < 2*DATA_WIDTH - 1) &
		   (CPHA ? ~bit_cnt[0] : bit_cnt[0]))
			t_flag <= CPHA ? (bit_cnt >> 1) : ((bit_cnt + 1) >> 1);
		else if(state != SCLK_TOG)
			t_flag <= 1'd0;          
end

always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
		mosi <= 1'd0;
	else 
		if(MSB_FIRST)
			mosi <= tx_shift[DATA_WIDTH - t_flag - 1];
		else
			mosi <= tx_shift[t_flag];
end

// r_flag：给 rx 用的位号。rx 是在采样沿那一拍写的，位号必须比沿早一拍准备好，
// 所以用 SPI_HALF_T - 2（比 t_flag 早一拍），位号就是这一沿的 bit_cnt>>1。
always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
		r_flag <= 1'd0;
	else 
		if((delay_cnt == SPI_HALF_T - 2) & (bit_cnt < 2*DATA_WIDTH) &
		   (CPHA ? bit_cnt[0] : ~bit_cnt[0]))
			r_flag <= bit_cnt >> 1;
		else if(state != SCLK_TOG)
			r_flag <= 1'd0;
end

always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
		rx_shift <= 0;
	else if(rx_now)              // 原来每拍都写，会把已经收对的那一位冲掉
		if(MSB_FIRST)
			rx_shift[DATA_WIDTH - r_flag - 1] <= miso_r;
		else
			rx_shift[r_flag] <= miso_r;
end

endmodule