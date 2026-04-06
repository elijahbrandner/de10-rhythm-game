module hex_prompt (
    input  wire [1:0]  lane_select,
    input  wire [1:0]  mode,
    input  wire        enable,
    output wire [31:0] hex3_hex0,
    output wire [15:0] hex5_hex4
);

    // internal active-high segment pattern
    // top-level still inverts for DE10 active-low HEX outputs
    localparam [6:0] SEG_EIGHT = 7'b1111111;

    reg [31:0] hex_r;

    always @(*) begin
        hex_r = 32'd0;

        if (enable) begin
            // mode 2 = preview/playback: only active lane shown
            if (mode == 2'd2) begin
                case (lane_select)
                    2'd0: hex_r[ 6: 0] = SEG_EIGHT;
                    2'd1: hex_r[14: 8] = SEG_EIGHT;
                    2'd2: hex_r[22:16] = SEG_EIGHT;
                    2'd3: hex_r[30:24] = SEG_EIGHT;
                endcase
            end else begin
                // idle/watch/go/results: light all four
                hex_r[ 6: 0] = SEG_EIGHT;
                hex_r[14: 8] = SEG_EIGHT;
                hex_r[22:16] = SEG_EIGHT;
                hex_r[30:24] = SEG_EIGHT;
            end
        end
    end

    assign hex3_hex0 = hex_r;
    assign hex5_hex4 = 16'd0;

endmodule