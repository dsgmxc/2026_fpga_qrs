`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 09/12/2026 08:49:59 PM
// Design Name: 
// Module Name: qrs
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


module qrs(
	input  wire clk_in1,
	input  wire rst_n,

	output wire dac_sclk,
	output wire dac_cs,
	output wire dac_mosi,
	
	output wire adc_sclk,
	output wire adc_sync,
	output wire adc_mosi,
	
	output wire clk_65m,

	// RGB LCD 接口（ATK-MD0700R-1024600-IPS，24bit 并行，DE 模式）
	output wire        lcd_de,
	output wire        lcd_hs,
	output wire        lcd_vs,
	output wire        lcd_bl,
	output wire        lcd_clk,
	output wire        lcd_rst,
	output wire [23:0] lcd_rgb
    );

wire sys_clk_100m;
wire spi_clk_50m;

wire locked;
clk_wiz_0 u_clk_wiz_0
(
.sys_clk_100m(sys_clk_100m),
.spi_clk_50m(spi_clk_50m),
.clk_65m(clk_65m), 
.resetn(rst_n), 
.locked(locked),       

.clk_in1(clk_in1)      
);

wire rst_n_sys = rst_n & locked;

ila_0 u_ila_0 (
	.clk(spi_clk_50m), 
	.probe0(rst_n), 
	.probe1(dac_sclk), 
	.probe2(dac_cs), 
	.probe3(dac_mosi), 
	.probe4(adc_sclk), 
	.probe5(adc_sync), 
	.probe6(adc_mosi) 
);

wire [19:0] code = 20'hC0000;

wire set_in;
wire init_done;

reg init_done_d;
always @(posedge spi_clk_50m or negedge rst_n_sys)
	if(!rst_n_sys)
		init_done_d <= 1'b0;
	else
		init_done_d <= init_done;

assign set_in = init_done & ~init_done_d;

dac_1220 dut (
	.clk_50m  (spi_clk_50m),
	.rst_n    (rst_n_sys),
	.dac_sclk (dac_sclk),
	.dac_cs   (dac_cs),
	.dac_mosi (dac_mosi),
	.code     (code),
	.set_in   (set_in),
	.init_done(init_done)
);

wire [27:0] adc_code = 28'h0A3D70A;

adc_9833 u_adc_9833 (
	.clk_50m  (spi_clk_50m),
	.rst_n    (rst_n_sys),
	.adc_sclk (adc_sclk),
	.adc_sync (adc_sync),
	.adc_mosi (adc_mosi),
	.code     (adc_code),
	.set_in   (set_in),
	.init_done()
);

//========================================================================================
// RGB LCD 显示（ATK-MD0700R-1024600-IPS）
//   时序参数已核对：PCLK 50MHz / H: 20-140-1024-160 = 1344 / V: 3-20-600-12 = 635
//                  刷新率 = 50e6 / (1344*635) ≈ 58.6 Hz
//   屏型号固定，ID 直接硬编码为 16'h7016，故不需要 rd_id 读 ID、也不需要 clk_div 分频。
//   PCLK 直接取 MMCM 的 50MHz 输出（clk_wiz CLKOUT2）。
//========================================================================================
wire [15:0] lcd_id = 16'h7016;      // 1024x600 屏 ID

wire [10:0] pixel_xpos;
wire [10:0] pixel_ypos;
wire [10:0] h_disp;
wire [10:0] v_disp;
wire [23:0] pixel_data;

lcd_display u_lcd_display (
	.lcd_pclk   (spi_clk_50m),
	.rst_n      (rst_n_sys  ),
	.pixel_xpos (pixel_xpos ),
	.pixel_ypos (pixel_ypos ),
	.h_disp     (h_disp     ),
	.v_disp     (v_disp     ),
	.pixel_data (pixel_data )
);

lcd_driver u_lcd_driver (
	.lcd_pclk   (spi_clk_50m),
	.rst_n      (rst_n_sys  ),
	.lcd_id     (lcd_id     ),
	.pixel_data (pixel_data ),
	.pixel_xpos (pixel_xpos ),
	.pixel_ypos (pixel_ypos ),
	.h_disp     (h_disp     ),
	.v_disp     (v_disp     ),
	.data_req   (           ),
	.lcd_de     (lcd_de     ),
	.lcd_hs     (lcd_hs     ),
	.lcd_vs     (lcd_vs     ),
	.lcd_bl     (lcd_bl     ),
	.lcd_clk    (lcd_clk    ),
	.lcd_rst    (lcd_rst    ),
	.lcd_rgb    (lcd_rgb    )
);

endmodule
