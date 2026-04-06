module led_controller (
    input  wire        clk,
    input  wire        beat_tick,
    input  wire [1:0]  mode,
    input  wire        enable,
    input  wire        rst,
    output wire [9:0]  ledr
);

    localparam integer IDLE_STEP_TICKS   = 50000000; // 1 second at 50 MHz
    localparam integer PULSE_PHASE_TICKS = 6000000;  // ~120 ms per phase

    // ----------------------------
    // Idle bounce state (4-LED bar)
    // ----------------------------
    reg [25:0] idle_counter = 0;
    reg [2:0]  idle_pos     = 0;  // 0..6 valid for 4-wide bar across 10 LEDs
    reg        idle_dir     = 0;  // 0 = move right, 1 = move left

    // ----------------------------
    // Gameplay pulse state
    // ----------------------------
    reg [22:0] pulse_counter = 0;
    reg [2:0]  pulse_phase   = 0;
    reg        pulse_active  = 0;

    // ----------------------------
    // Blink state
    // ----------------------------
    reg blink = 0;

    reg [9:0] leds;

    always @(posedge clk) begin
        if (rst || !enable) begin
            idle_counter  <= 0;
            idle_pos      <= 0;
            idle_dir      <= 0;

            pulse_counter <= 0;
            pulse_phase   <= 0;
            pulse_active  <= 0;

            blink         <= 0;
        end else begin
            case (mode)

                // --------------------------------
                // mode 0: off
                // --------------------------------
                2'd0: begin
                    pulse_counter <= 0;
                    pulse_phase   <= 0;
                    pulse_active  <= 0;
                end

                // --------------------------------
                // mode 1: 4-LED bar bounce
                // --------------------------------
                2'd1: begin
                    if (idle_counter >= IDLE_STEP_TICKS - 1) begin
                        idle_counter <= 0;

                        if (!idle_dir) begin
                            if (idle_pos == 3'd6) begin
                                idle_dir <= 1'b1;
                                idle_pos <= 3'd5;
                            end else begin
                                idle_pos <= idle_pos + 1'b1;
                            end
                        end else begin
                            if (idle_pos == 3'd0) begin
                                idle_dir <= 1'b0;
                                idle_pos <= 3'd1;
                            end else begin
                                idle_pos <= idle_pos - 1'b1;
                            end
                        end
                    end else begin
                        idle_counter <= idle_counter + 1'b1;
                    end
                end

                // --------------------------------
                // mode 2: gameplay pulse
                // --------------------------------
                2'd2: begin
                    if (beat_tick) begin
                        pulse_active  <= 1'b1;
                        pulse_phase   <= 3'd0;
                        pulse_counter <= 0;
                    end else if (pulse_active) begin
                        if (pulse_counter >= PULSE_PHASE_TICKS - 1) begin
                            pulse_counter <= 0;

                            if (pulse_phase == 3'd3) begin
                                pulse_active <= 1'b0;
                            end else begin
                                pulse_phase <= pulse_phase + 1'b1;
                            end
                        end else begin
                            pulse_counter <= pulse_counter + 1'b1;
                        end
                    end
                end

                // --------------------------------
                // mode 3: full blink on beat
                // --------------------------------
                2'd3: begin
                    if (beat_tick)
                        blink <= ~blink;
                end

            endcase
        end
    end

    always @(*) begin
        leds = 10'd0;

        if (!enable) begin
            leds = 10'd0;
        end else begin
            case (mode)
                2'd0: leds = 10'd0;

                // 4-wide bar starting at idle_pos
                2'd1: leds = (10'b0000001111 << idle_pos);

                // pulse / ripple
                2'd2: begin
                    if (!pulse_active) begin
                        leds = 10'd0;
                    end else begin
                        case (pulse_phase)
                            3'd0: leds = 10'b0000110000;
                            3'd1: leds = 10'b0001111000;
                            3'd2: leds = 10'b0011001100;
                            3'd3: leds = 10'b0110000110;
                            default: leds = 10'd0;
                        endcase
                    end
                end

                2'd3: leds = blink ? 10'b1111111111 : 10'd0;

                default: leds = 10'd0;
            endcase
        end
    end

    assign ledr = leds;

endmodule