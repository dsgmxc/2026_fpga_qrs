`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 09/22/2026 09:10:56 PM
// Design Name: 
// Module Name: ad9238
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
module ad9238(
	input		wire ADC_CLK,
	input		wire RST_N,
	input		wire [11:0]	ADC_IN,	
	input   wire	OTR,
	output	wire	PDN,
	output  wire    OEB_B,
	output	wire	[11:0]	ADC_OUTA,
	output	wire	[11:0]	ADC_OUTB
);

//ADC采集到数据和电压转换公式为：ADC_DATA * (20/4096)
//采集到的数据通过 Signal_Tap II 查看
reg		[11:0] ADC_BUFA;  //ADCA数据缓存器
reg		[11:0] ADC_BUFB;  //ADCB数据缓存器

//ADCA数据读取
always@(posedge ADC_CLK or negedge RST_N)
begin
	if(!RST_N)begin
		ADC_BUFA	<= 12'd0;
	end
	else begin
		ADC_BUFA 	<= 12'hFFF - (ADC_IN + 12'd2048);
	end
end

//ADCB数据读取
always@(negedge ADC_CLK or negedge RST_N)
begin
	if(!RST_N)begin
		ADC_BUFB	<= 12'd0;
	end
	else begin
		ADC_BUFB 	<= 12'hFFF - (ADC_IN + 12'd2048);
	end
end

assign OEB_B    = 1'b0;
assign PDN      = 1'b0;
assign ADC_OUTA = ADC_BUFA;
assign ADC_OUTB = ADC_BUFB;


endmodule
