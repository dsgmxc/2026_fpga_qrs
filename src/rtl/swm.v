module swm(
    input rst_n,
    input clk,

    input [1:0] sel,
    output reg k1,k2,k3,k4,k5,k6,k7
);

reg [1:0] sel_d;

always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
		sel_d <=0;
	else 
		sel_d <= sel;
end

always@(posedge clk or negedge rst_n)begin
	if(!rst_n)
		begin
			k1 <= 0;
			k2 <= 0;
			k3 <= 0;
			k4 <= 0;
			k5 <= 0;
			k6 <= 0;
			k7 <= 0;
        end
	else 
        case (sel_d)
            0:
            begin
                k1 <= 1;
                k2 <= 1;
                k3 <= 1;
                k4 <= 1;
                k5 <= 1;
                k6 <= 1;
                k7 <= 1;
            end

            1:
            begin
                k1 <= 1;
                k2 <= 0;
                k3 <= 0;
                k4 <= 0;
                k5 <= 0;
                k6 <= 1;
                k7 <= 1;
            end

            2:
            begin
                k1 <= 1;
                k2 <= 0;
                k3 <= 0;
                k4 <= 0;
                k5 <= 1;
                k6 <= 0;
                k7 <= 1;
            end

            3:
            begin
                k1 <= 0;
                k2 <= 0;
                k3 <= 1;
                k4 <= 1;
                k5 <= 1;
                k6 <= 1;
                k7 <= 0;
            end

            default:
            begin
                k1 <= 0;
                k2 <= 0;
                k3 <= 0;
                k4 <= 0;
                k5 <= 0;
                k6 <= 0;
                k7 <= 0;
            end

        endcase
end

endmodule
