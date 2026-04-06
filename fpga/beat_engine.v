module beat_engine (
    input  wire        clk,
    input  wire [3:0]  tempo_sel,
    input  wire        enable,
    input  wire        rst,
    output wire        beat_tick
);

    function [31:0] bpm_cycles;
        input [3:0] sel;
        begin
            case (sel)
                4'd0:    bpm_cycles = 32'd66666666; // 45 BPM
                4'd1:    bpm_cycles = 32'd50000000; // 60 BPM
                4'd2:    bpm_cycles = 32'd40000000; // 75 BPM
                default: bpm_cycles = 32'd50000000; // default 60 BPM
            endcase
        end
    endfunction

    reg [31:0] counter = 0;
    reg        tick_r  = 0;

    wire [31:0] period = bpm_cycles(tempo_sel);

    always @(posedge clk) begin
        tick_r <= 0;

        if (rst || !enable) begin
            counter <= 0;
        end else begin
            if (counter >= period - 1) begin
                counter <= 0;
                tick_r  <= 1;
            end else begin
                counter <= counter + 1;
            end
        end
    end

    assign beat_tick = tick_r;

endmodule