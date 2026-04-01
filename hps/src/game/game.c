// Main game header
// Contains the game_t structure, enums, input/output structs
#include "game/game.h"

// Includes the sequence generator header
// Provides funcitons for building the rhythm sequence/pattern for a round
#include "game/sequence.h"

// Include string utilities
#include <string.h>

// ----------------------------
// Constants
// ----------------------------

// Max number of sequence steps the system can hold
// Acts a safety cap when generating patterns
#define MAX_STEPS 64u

// Duration of the WATCH state in millisec
// This is the short "get ready" phase before pattern preview begins
#define WATCH_DURATION_MS    2000u

// Duration of the RESULTS state in millisec
// Determines how long results stay visible before returning to idle
#define RESULTS_DURATION_MS  4000u

// Fraction of each beat that is considered the active scoring window
// Here, the playable/scoring window is 85% of the full beat length
#define STEP_WINDOW_NUM      85u
#define STEP_WINDOW_DEN      100u

// Total duration of the countdown before playback begins (3 seconds)
#define COUNTDOWN_DURATION_MS  3000u
#define COUNTDOWN_STEP_MS      1000u

// ----------------------------
// Helpers
// ----------------------------

// Changes current game state and records the time that state began
// Exists because:
// Anytime the game changes state, we want to consistently update:
// g->state
// g->state_enter_ms
// g->out.state
// This keeps state transitions clean and centralized
static void enter_state(game_t *g, game_state_t st, uint32_t now_ms) {
    g->state = st;                  // Update internal state
    g->state_enter_ms = now_ms;     // Record when this state began
    g->out.state = st;              // Mirror state into public outputs
}


// Detects rising edges on button input
// Rising edge: button was NOT pressed last time, 
// but IS pressed now
// Usefule bc we usually want actions to happen once
// when button is pressed, but not continuously while it is held down
static uint32_t button_edges(game_t *g, uint32_t curr_buttons) {
    // Compute edges:
    // This gives bits that changed from 0 -> 1
    uint32_t edges = (~g->prev_buttons) & curr_buttons;
    // Save the current button state for then next update cycle
    g->prev_buttons = curr_buttons;
    // Return the edge bitmask
    return edges;
}

// Checks whether the exit combo was pressed

// Exit Combo rule here:
// all 10 switches must be ON
// key0 must be newly pressed this frame
// Lets user force the game to exit using a special combo
static bool exit_combo_edge(uint32_t switches, uint32_t pressed_edges) {
    // Check if all 10 switch bits are high
    bool sw_all = ((switches & 0x3FFu) == 0x3FFu);
    // Check if KEY0 had a rising edge
    bool key0_edge = (pressed_edges & 0x1u) != 0;
    // Exit when both are true
    return sw_all && key0_edge;
}

// Helper to set the two output text lines used by the game
// This keeps text updates simpleer and avoids repeated assignments
static void set_output_text(game_t *g, const char *l1, const char *l2) {
    g->out.line1 = l1;  // First display text line
    g->out.line2 = l2;  // Second display text line
}

// Returns the next difficulty in ladder mode
// Ladder progression:
// Easy -> Medium -> Hard -> Expert -> Easy
// If something unexpected happens, default back to EASY
static game_mode_t ladder_next(game_mode_t m) {
    switch (m) {
        case GAME_MODE_EASY:   return GAME_MODE_MEDIUM;
        case GAME_MODE_MEDIUM: return GAME_MODE_HARD;
        case GAME_MODE_HARD:   return GAME_MODE_EXPERT;
        case GAME_MODE_EXPERT: return GAME_MODE_EASY;
        default:               return GAME_MODE_EASY;
    }
}

// Decodes which lane button was newly pressed from the edge bitmask
// Assumes:
// KEY0 -> lane 0
// KEY1 -> lane 1
// KEY2 -> lane 2
// KEY3 -> lane 3
// Returns: 0, 1, 2,3, for valid lane press
// -1 if no lane was pressed
static int decode_lane_from_edges(uint32_t edges) {
    if (edges & (1u << 0)) return 0; 
    if (edges & (1u << 1)) return 1;
    if (edges & (1u << 2)) return 2;
    if (edges & (1u << 3)) return 3;
    return -1;                          // No valid lane edge detected
}


// Converts BPM (Beats per minute) into millisec per beat/step
// Formula
// step_ms = 60000 / bpm
// If bpm is ever 0 fall back to 10000 ms to avoid divide by zero
static uint32_t bpm_to_step_ms(uint32_t bpm) {
    if (bpm == 0) return 1000u;
    return 60000u / bpm;
}

// Computes the active scoring window size in millisec
// If a beat lasts 1000 ms and the active window is 85%
// then the scoring window is 850 ms
static uint32_t step_window_ms(uint32_t step_ms) {
    // Calculate window as a fraction of the step time
    uint32_t w = (step_ms * STEP_WINDOW_NUM) / STEP_WINDOW_DEN;

    // Ensure the window is never zero
    // This protects against corner cases with very small step_ms
    if (w == 0) w = 1;
    return w;
}

// Resets all round-specific tracking data before a new playable round
// This clears old scoring information and reinitializes the scoring engine
static void reset_round_state(game_t *g) {
    // Clear pre-step scored flags so each step starts unscored
    memset(g->step_scored, 0, sizeof(g->step_scored));

    // Reset last tracked step index
    g->last_step_index = 0;

    // Reset stored beat edge tracking
    g->last_beat_edge = 0;

    // Reset current score
    g->score = 0;

    // Mark that the current round is not yet commpleted
    g->completed = false;

    // Reinitialize the scoring subsystem for this round's sequence length
    scoring_init(&g->scoring, g->seq_len);
}

// Send a specific gameplay step to the FPGA so it can show the correct lane
// LED mode, and tempo
// This is used during preview and playback
static void fpga_show_step(fpga_if_t *fpga, game_mode_t mode, uint8_t lane, fpga_led_mode_t led_mode) {
    // Set which lane should be highlighted on the FPGA side
    fpga_if_set_lane(fpga, (fpga_lane_t)(lane & 0x3u));

    // Set LED behavior (pulse, blink, etc)
    fpga_if_set_led_mode(fpga, led_mode);
    
    // Set tempo based on current difficulty mode
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(mode));

    // Enable FPGA output
    fpga_if_set_enable(fpga, true);

    // Commit all pending settings to hardware
    fpga_if_commit(fpga);
}

// Sets the FPGA to its idle visualization mode
// In idle, the game is not showing a playable step
// So it uses a neutral lane + chase animaiton
static void fpga_idle_visual(fpga_if_t *fpga, game_mode_t mode) {
    fpga_if_set_lane(fpga, FPGA_LANE_0);                            // Neutral / default lane
    fpga_if_set_led_mode(fpga, FPGA_LED_CHASE);                     // Idle chase animation
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(mode));      // Tempo tied to mode
    fpga_if_set_enable(fpga, true);                                 // Keep FPGA visuals active 
    fpga_if_commit(fpga);                                           // Apply changes
}

// Sets the FPGA visuals used during the results screen
// If the score is high enough, use blink
// Otherwise use pulse
// This gives some visual feedback based on performance
static void fpga_results_visual(fpga_if_t *fpga, game_mode_t mode, uint32_t score) {
    fpga_if_set_lane(fpga, FPGA_LANE_0);                            // Neutral lane in results
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(mode));      // Keep mode tmepo
    fpga_if_set_enable(fpga, true);                                 // Enable output

    // Choose LED effect based on final score
    if (score >= 75u) fpga_if_set_led_mode(fpga, FPGA_LED_BLINK);
    else              fpga_if_set_led_mode(fpga, FPGA_LED_PULSE);

    // Apply the result visuals
    fpga_if_commit(fpga);
}

// Finalizes a step as a miss if it was never scored during its valid window.
// This is important because each ste pneeds a final judgement
// If the player never it it correctly, it should count as missed
static void finalize_step_if_needed(game_t *g, uint32_t idx) {
    // Defensive null check
    if (!g) return;

    // Ignore invalid step index
    if (idx >= g->seq_len) return;

    // If this step was already scored, do nothing
    if (g->step_scored[idx]) return;

    // Store the grade assigned by the scoring system
    hit_grade_t grade;

    // If this step was a shake step, finalize it as a missed shake
    if (g->steps[idx].shake) {
        scoring_score_shake_step(&g->scoring, false, 0, 1, &grade);
    } else {
        // Otherwise finalize it as a missed button press
        scoring_miss_button_step(&g->scoring, &grade);
    }
    // Mark this step as resolved / scored so it will not be finalized again
    g->step_scored[idx] = true;
}

// Scores a button step when the player presses a lane
// This function validates the input and passes the judgment to the scoring system
static void score_button_step(game_t *g, uint32_t idx, int pressed_lane, uint32_t offset_ms, uint32_t window_ms) {
    // Defensive null check
    if (!g) return;

    // Ignore invalid sequence index
    if (idx >= g->seq_len) return;

    // Ignore already-scored steps
    if (g->step_scored[idx]) return;

    // Ignore invalid lane numbers
    if (pressed_lane < 0 || pressed_lane > 3) return;

    // Variable to receive the hit quality/grade from the scoring system
    hit_grade_t grade;

    // Score the button step using:
    // Score the button step using:
    // - expected lane
    // - actual pressed lane
    // - timing offset
    // - allowed window
    scoring_score_button_step(&g->scoring,
                              g->steps[idx].lane,
                              (uint8_t)pressed_lane,
                              offset_ms,
                              window_ms,
                              &grade);
    // Mark the step as scored so it cannot be scored twice
    g->step_scored[idx] = true;
}

// Scores a shake step if shake input was detected
// Shake steps behave slightly differently
// they only get marked scored if an actual shake occurs
static void score_shake_step(game_t *g, uint32_t idx, bool shake_detected, uint32_t offset_ms, uint32_t window_ms) {
    // Defensive null check
    if (!g) return;

    // Ignore invalid sequence index
    if (idx >= g->seq_len) return;

    // Ignore already-scored steps
    if (g->step_scored[idx]) return;

    // Variable to receive the hit grade
    hit_grade_t grade;

    // Ask the scoring system to evaluate the shake event
    scoring_score_shake_step(&g->scoring,
                             shake_detected,
                             offset_ms,
                             window_ms,
                             &grade);

    // Only mark the step scored if a shake actually occurred
    // If no shake happened, it may still need to be finalized later as a miss
    if (shake_detected) {
        g->step_scored[idx] = true;
    }
}

// ----------------------------
// Public API
// ----------------------------

// Initialize the entire game structure to a safe startup state
void game_init(game_t *g) {
    // Zero out the full game structure so everything starts clean
    memset(g, 0, sizeof(*g));

    // Initialize state is idle
    g->state = ST_IDLE;

    // Default selection mode is ladder mode
    g->select_mode = SELECT_LADDER;

    // Default difficulty start at easy
    g->mode = GAME_MODE_EASY;

    // Initialize score related values
    g->score = 0;
    g->last_score = 0;
    g->last_rating_text = "";
    g->completed = false;

    // Set BPM and sequence length based on the default mode
    sequence_mode_params(g->mode, &g->bpm, &g->seq_len);

    // Initialize public outputs so the rest of the system has valid data immediately
    g->out.state = g->state;
    g->out.mode = g->mode;
    g->out.bpm = g->bpm;
    g->out.seq_len = g->seq_len;
    g->out.score_0_100 = 0;
    g->out.rating_text = "";

    // Initial display text shwon to the user.
    set_output_text(g, "Rhythm Game", "KEY0: Start");
}

// returns whether the game has requested exit
// If g is NULL, return true as a safe default
bool game_should_exit(const game_t *g) {
    return g ? g->should_exit : true;
}

// Returns a pointer to the current game outputs
// If g is NULL, return NULL
const game_outputs_t *game_get_outputs(const game_t *g) {
    return g ? &g->out : NULL;
}


// Main game update function
//
// Heart of the game stat emachine
// It:
// - reads processed inputs
// - updates the game state
// - updates scoring
// - updates outputs
// - drives FPGA visuals
void game_update(game_t *g, fpga_if_t *fpga, const game_inputs_t *in, uint32_t now_ms) {
    // Defensive guard: if anything critical is NULL, do nothing
    if (!g || !fpga || !in) return;

    // Detect newly-pressed button edges from the raw button state
    uint32_t edges = button_edges(g, in->buttons_raw);

    // Check for the forced exit combo
    if (exit_combo_edge(in->switches, edges)) {
        g->should_exit = true;                  // Mark exit requested
        fpga_if_clear(fpga);                    // Clear FPGA visuals
        enter_state(g, ST_EXIT, now_ms);        // Move to exit state
        set_output_text(g, "Exiting...", "");
        return;
    }

    // Determine current selection mode from switch 9
    // SW9 = 1 -> free select
    // SW9 = 0 -> Ladder mode
    g->select_mode = ((in->switches >> 9) & 0x1u) ? SELECT_FREE : SELECT_LADDER;

    // Only allow difficulty selection from switches while in IDLE and in FREE mode
    if (g->state == ST_IDLE && g->select_mode == SELECT_FREE) {
        // Read the lowest two switches for difficulty slection
        uint32_t sel = (in->switches & 0x3u);

        // Map switch combo to difficulty
        if (sel == 0x0)      g->mode = GAME_MODE_EASY;
        else if (sel == 0x2) g->mode = GAME_MODE_MEDIUM;
        else if (sel == 0x1) g->mode = GAME_MODE_HARD;
        else                 g->mode = GAME_MODE_EXPERT;
    }

    // While idle, refresh BPM and sequence length in case mode changed
    if (g->state == ST_IDLE) {
        sequence_mode_params(g->mode, &g->bpm, &g->seq_len);
    }

    // Main FSM switch:
    // Execute behavior based on current state
    switch (g->state) {

        // Reset current score display values while idle
        case ST_IDLE: {
            g->score = 0;
            g->out.score_0_100 = 0;
            g->out.rating_text = "";

            // Show idle visuals on FPGA
            fpga_idle_visual(fpga, g->mode);

            // Text differs slightly depending on ladder vs free select
            if (g->select_mode == SELECT_LADDER) {
                set_output_text(g, "Idle", "KEY0: Start");
            } else {
                set_output_text(g, "Select", "KEY0: Start");
            }

            // KEY0 edge starts a new round
            if (edges & 0x1u) {
                // Save a seed based on current time for sequence generation
                g->seed = now_ms;

                // Clear completed flag because a new round is starting
                g->completed = false;

                // Reset the FPGA rhythm engine/pulse logic
                fpga_if_reset_pulse(fpga);

                // Move to WATCH state
                enter_state(g, ST_WATCH, now_ms);
                set_output_text(g, "WATCH", "Get ready...");
            }
        } break;

        case ST_WATCH: {
            // Show a neutral chase animation during watch/get ready
            fpga_show_step(fpga, g->mode, 0, FPGA_LED_CHASE);

            // After WATCH duration expires, generate the sequence and move on
            if ((now_ms - g->state_enter_ms) >= WATCH_DURATION_MS) {
                // Will receive the actual generated length
                uint32_t out_len = 0;

                // Try to generate the sequence
                if (sequence_generate(g->mode, g->seed, g->steps, MAX_STEPS, &out_len) != 0) {
                    // If generation fails, clear visuals and jump to results with error text
                    fpga_if_clear(fpga);
                    enter_state(g, ST_RESULTS, now_ms);
                    g->score = 0;
                    g->out.score_0_100 = 0;
                    g->out.rating_text = "Sequence Error";
                    set_output_text(g, "ERROR", "Sequence failed");
                    break;
                }

                // Save the actual generated sequence length
                g->seq_len = out_len;

                // Reset all round-specific tracking data
                reset_round_state(g);

                // Move to preview state so the user can watch the pattern
                enter_state(g, ST_PREVIEW, now_ms);
                set_output_text(g, "PREVIEW", "Watch pattern");
            }
        } break;

        case ST_PREVIEW: {
            // Compute beat duration for the current BPM
            uint32_t step_ms = bpm_to_step_ms(g->bpm);

            // Compute how long we have been inside preview
            uint32_t elapsed = now_ms - g->state_enter_ms;

            // Determine which sequence step should currently be shown
            uint32_t idx = elapsed / step_ms;

            // If preview has shown every step, move to GO state
            if (idx >= g->seq_len) {
                fpga_show_step(fpga, g->mode, 0, FPGA_LED_CHASE);
                enter_state(g, ST_GO, now_ms);
                set_output_text(g, "READY", "KEY0 to begin");
                break;
            }

            // Choose LED behavior based on whether this is a shake step
            fpga_led_mode_t led_mode = g->steps[idx].shake ? FPGA_LED_BLINK : FPGA_LED_PULSE;
            fpga_show_step(fpga, g->mode, g->steps[idx].lane, led_mode);
        } break;

        case ST_GO: {
            // Show neutral chase visual while waiting for player confirmation
            fpga_show_step(fpga, g->mode, 0, FPGA_LED_CHASE);

            // KEY0 begins the countdown
            if (edges & 0x1u) {
                enter_state(g, ST_COUNTDOWN, now_ms);
                set_output_text(g, "Starting", "3");
            }
        } break;

        case ST_COUNTDOWN: {
            // How long we have been in countdown
            uint32_t elapsed = now_ms - g->state_enter_ms;

            // After full countdown duration, begin actual playback
            if (elapsed >= COUNTDOWN_DURATION_MS) {
                // Reset last step tracking so playback starts clean
                g->last_step_index = 0;

                // Enter playback state
                enter_state(g, ST_PLAYBACK, now_ms);
                set_output_text(g, "PLAY", "Press keys");
                break;
            }

            // Determine which countdown number should be shown
            // Example:
            // 0..999 ms --> 3
            // 1000..1999 -> 2
            // 2000..2999 -> 1
            uint32_t remaining = 3u - (elapsed / COUNTDOWN_STEP_MS);

            // Update text based on remaining countdown value
            if (remaining > 2u) {
                set_output_text(g, "Starting", "3");
            } else if (remaining > 1u) {
                set_output_text(g, "Starting", "2");
            } else {
                set_output_text(g, "Starting", "1");
            }

            // Duration countdown, show a neutral/fixed visual cue
            fpga_if_set_lane(fpga, FPGA_LANE_0);                            // Neutral lane
            fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(g->mode));   // Current mode tempo
            fpga_if_set_enable(fpga, true);                                 // Enable output
            fpga_if_set_led_mode(fpga, FPGA_LED_BLINK);                     // Countdown blink effect  
            fpga_if_commit(fpga);                                           // Apply changes
        } break;

        case ST_PLAYBACK: {
            // Compute beat duration in ms
            uint32_t step_ms   = bpm_to_step_ms(g->bpm);

            // Compute allowed scoring window
            uint32_t window_ms = step_window_ms(step_ms);

            // COmpute elapsed time since playback started
            uint32_t elapsed   = now_ms - g->state_enter_ms;

            // Current active step index
            uint32_t idx       = elapsed / step_ms;

            // Time offset inside the current step
            uint32_t offset_ms = elapsed % step_ms;

            // If playback has passed the final step, finish the round
            if (idx >= g->seq_len) {
                // Finalize the last step if it still was never scored
                if (g->seq_len > 0) {
                    finalize_step_if_needed(g, g->seq_len - 1);
                }

                // Clear any active FPGA visuals from playback
                fpga_if_clear(fpga);

                // Compute final score from the scoring system
                g->score = scoring_finalize_0_100(&g->scoring);

                // Save round result for idle display later
                g->last_score = g->score;
                g->last_rating_text = scoring_rating(g->score);
                g->completed = true;

                // Mirror final values into outputs
                g->out.score_0_100 = g->score;
                g->out.rating_text = g->last_rating_text;

                // Show result visuals on FPGA
                fpga_results_visual(fpga, g->mode, g->score);

                // MOve to results state
                enter_state(g, ST_RESULTS, now_ms);
                set_output_text(g, "RESULTS", g->out.rating_text);
                break;
            }

            // If we just moved from one step to the next
            // finalize the previous step if it was missed
            if (idx != g->last_step_index) {
                if (g->last_step_index < g->seq_len) {
                    finalize_step_if_needed(g, g->last_step_index);
                }
                g->last_step_index = idx;
            }

            // Choose LED behavior depending on whether current step is shake-based
            fpga_led_mode_t led_mode = g->steps[idx].shake ? FPGA_LED_BLINK : FPGA_LED_PULSE;

            // Show the active step visually on the FPGA
            fpga_show_step(fpga, g->mode, g->steps[idx].lane, led_mode);

            // Only allow scoring during the active timing window
            if (offset_ms <= window_ms) {
                // Handle shake step
                if (g->steps[idx].shake) {
                    if (in->shake_detected) {
                        score_shake_step(g, idx, true, offset_ms, window_ms);
                    }
                } else {
                    // Handle button lane step
                    int pressed_lane = decode_lane_from_edges(edges);
                    if (pressed_lane >= 0) {
                        score_button_step(g, idx, pressed_lane, offset_ms, window_ms);
                    }
                }
            }
        } break;

        case ST_RESULTS: {
            // Continuously show result visuals while in results state
            fpga_results_visual(fpga, g->mode, g->score);

            // After results duration expires, return to idle
            if ((now_ms - g->state_enter_ms) >= RESULTS_DURATION_MS) {
                fpga_if_clear(fpga);

                // In ladder mode, advance to the next difficulty automatically
                if (g->select_mode == SELECT_LADDER) {
                    g->mode = ladder_next(g->mode);
                }

                // return to idle state
                enter_state(g, ST_IDLE, now_ms);
                set_output_text(g, "Rhythm Game", "KEY0: Start");
            }
        } break;

        case ST_EXIT: {
            // Ensure exit flag is set and visuals are cleared
            g->should_exit = true;
            fpga_if_clear(fpga);
        } break;

        default: {
            // Fallback recovery:
            // if somehow the stae is invalid, clear FPGA and return to IDLE
            fpga_if_clear(fpga);
            enter_state(g, ST_IDLE, now_ms);
        } break;
    }

    // Refresh public output values at the end of every update
    // This ensures main.c and other modules always see the latest state
    g->out.mode = g->mode;
    g->out.bpm = g->bpm;
    g->out.seq_len = g->seq_len;
    g->out.score_0_100 = g->score;
    g->out.state = g->state;
}