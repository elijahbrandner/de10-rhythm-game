module rhythm_fpga_circuits (
    input  wire        clk,
    input  wire [31:0] ctrl,
    output wire [31:0] hex3_hex0,
    output wire [15:0] hex5_hex4,
    output wire [9:0]  ledr
);

    wire [1:0] lane   = ctrl[1:0];
    wire [1:0] mode   = ctrl[3:2];
    wire [3:0] tempo  = ctrl[7:4];
    wire       enable = ctrl[8];
    wire       rst    = ctrl[9];

    wire beat_tick;

    beat_engine u_beat (
        .clk(clk),
        .tempo_sel(tempo),
        .enable(enable),
        .rst(rst),
        .beat_tick(beat_tick)
    );

    hex_prompt u_hex (
        .lane_select(lane),
        .mode(mode),
        .enable(enable),
        .hex3_hex0(hex3_hex0),
        .hex5_hex4(hex5_hex4)
    );

    led_controller u_led (
        .clk(clk),
        .beat_tick(beat_tick),
        .mode(mode),
        .enable(enable),
        .rst(rst),
        .ledr(ledr)
    );

endmodule