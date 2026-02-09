#include "game/game.h"
#include <string.h>

// ----------------------------
// Constants
// ----------------------------
#define MAX_STEPS 64u

// Timing (ms)
#define WATCH_DURATION_MS   5000u
#define GO_DURATION_MS      2000u
#define RESULTS_DURATION_MS 4000u

// ----------------------------
// Helpers
// ----------------------------

// Enter state helper
static void enter_state(game_t *g, game_state_t st, uint32_t now_ms) {
    g->state = st;
    g->state_enter_ms = now_ms;
    g->out.state = st;
}

// Edge detection: pressed edges = transitions 0->1
static uint32_t button_edges(game_t *g, uint32_t curr_buttons) {
    uint32_t edges = (~g->prev_buttons) & curr_buttons;
    g->prev_buttons = curr_buttons;
    return edges;
}

// Mode -> bpm/seq_len mapping (your current plan)
static void mode_to_params(game_mode_t mode, uint32_t *bpm_out, uint32_t *len_out) {
    switch (mode) {
        case GAME_MODE_EASY:
            *bpm_out = 60;
            *len_out = 5;
            break;
        case GAME_MODE_MEDIUM:
            *bpm_out = 90;
            *len_out = 8;
            break;
        case GAME_MODE_HARD:
            *bpm_out = 120;
            *len_out = 10;
            break;
        case GAME_MODE_EXPERT:
            *bpm_out = 120;
            *len_out = 10;
            break;
        default:
            *bpm_out = 60;
            *len_out = 5;
            break;
    }
}

// Placeholder sequence generator (we’ll replace with sequence.c later)
// Keeps it deterministic and simple for now.
static void generate_sequence(game_t *g) {
    uint32_t n = g->seq_len;
    if (n > MAX_STEPS) n = MAX_STEPS;

    for (uint32_t i = 0; i < n; i++) {
        g->steps[i].lane = (uint8_t)(i % 4);
        g->steps[i].flash_all = false;
        g->steps[i].shake = false;
    }

    // Expert: add some shake notes (example: last step is shake + flash)
    if (g->mode == GAME_MODE_EXPERT && n > 0) {
        g->steps[n - 1].shake = true;
        g->steps[n - 1].flash_all = true;
    }
}

// Exit combo: SW[9:0] all 1s + KEY0 edge press
static bool exit_combo_edge(uint32_t switches, uint32_t pressed_edges) {
    bool sw_all = ((switches & 0x3FFu) == 0x3FFu);
    bool key0_edge = (pressed_edges & 0x1u) != 0; // KEY0 bit0
    return sw_all && key0_edge;
}

// Update output strings (simple for now)
static void set_output_text(game_t *g, const char *l1, const char *l2) {
    g->out.line1 = l1;
    g->out.line2 = l2;
}

// Ladder progression helper
static game_mode_t ladder_next(game_mode_t m) {
    switch (m) {
        case GAME_MODE_EASY:   return GAME_MODE_MEDIUM;
        case GAME_MODE_MEDIUM: return GAME_MODE_HARD;
        case GAME_MODE_HARD:   return GAME_MODE_EXPERT;
        case GAME_MODE_EXPERT: return GAME_MODE_EASY;
        default:               return GAME_MODE_EASY;
    }
}

// ----------------------------
// Public API
// ----------------------------
void game_init(game_t *g) {
    memset(g, 0, sizeof(*g));
    g->state = ST_IDLE;
    g->select_mode = SELECT_LADDER;
    g->mode = GAME_MODE_EASY;
    mode_to_params(g->mode, &g->bpm, &g->seq_len);

    g->out.state = g->state;
    g->out.mode = g->mode;
    g->out.bpm = g->bpm;
    g->out.seq_len = g->seq_len;
    g->out.score_0_100 = 0;

    set_output_text(g, "Rhythm Game", "KEY0: Start");
}

bool game_should_exit(const game_t *g) {
    return g ? g->should_exit : true;
}

const game_outputs_t *game_get_outputs(const game_t *g) {
    return g ? &g->out : NULL;
}

void game_update(game_t *g, fpga_if_t *fpga, const game_inputs_t *in, uint32_t now_ms) {
    if (!g || !fpga || !in) return;

    // Edge-detect buttons
    uint32_t edges = button_edges(g, in->buttons_raw);

    // Global exit combo (edge-based KEY0)
    if (exit_combo_edge(in->switches, edges)) {
        g->should_exit = true;
        fpga_if_stop(fpga);
        enter_state(g, ST_EXIT, now_ms);
        set_output_text(g, "Exiting...", "");
        return;
    }

    // Update select mode + free-select mode each tick (safe)
    // (game itself decides whether to honor changes mid-round)
    // You’ll read this in main and feed switches already; no direct switch module needed here.
    g->select_mode = ((in->switches >> 9) & 0x1u) ? SELECT_FREE : SELECT_LADDER;

    // If free-select and idle, decode difficulty from SW1..SW0 mapping
    if (g->state == ST_IDLE && g->select_mode == SELECT_FREE) {
        uint32_t sel = (in->switches & 0x3u);
        if (sel == 0x0) g->mode = GAME_MODE_EASY;
        else if (sel == 0x2) g->mode = GAME_MODE_MEDIUM;
        else if (sel == 0x1) g->mode = GAME_MODE_HARD;
        else g->mode = GAME_MODE_EXPERT;
    }

    // Update derived params (safe in idle; in-round we keep locked)
    if (g->state == ST_IDLE) {
        mode_to_params(g->mode, &g->bpm, &g->seq_len);
        g->out.mode = g->mode;
        g->out.bpm = g->bpm;
        g->out.seq_len = g->seq_len;
    }

    switch (g->state) {

        case ST_IDLE: {
            g->completed = false;
            g->out.score_0_100 = 0;
            g->score = 0;

            // UX hint text
            if (g->select_mode == SELECT_LADDER) {
                set_output_text(g, "Mode: LADDER", "KEY0: Start");
                g->mode = GAME_MODE_EASY; // start ladder at easy each time from idle
                mode_to_params(g->mode, &g->bpm, &g->seq_len);
                g->out.mode = g->mode;
                g->out.bpm = g->bpm;
                g->out.seq_len = g->seq_len;
            } else {
                set_output_text(g, "Mode: FREE", "KEY0: Start");
            }

            // KEY0 edge starts
            if (edges & 0x1u) {
                // Lock config for this round
                fpga_if_reset(fpga);
                fpga_if_configure(fpga, g->mode, g->bpm, g->seq_len);

                enter_state(g, ST_WATCH, now_ms);
                set_output_text(g, "WATCH", "Get ready...");
            }
        } break;

        case ST_WATCH: {
            // Wait fixed duration
            if ((now_ms - g->state_enter_ms) >= WATCH_DURATION_MS) {
                // Generate + load sequence
                generate_sequence(g);
                fpga_if_load_sequence(fpga, g->steps, g->seq_len);

                // Start preview on FPGA
                fpga_if_start_preview(fpga);
                enter_state(g, ST_PREVIEW, now_ms);
                set_output_text(g, "PREVIEW", "Watch pattern");
            }
        } break;

        case ST_PREVIEW: {
            // Wait FPGA done
            fpga_status_t st = fpga_if_read_status(fpga);
            if (st.done) {
                fpga_if_stop(fpga);
                enter_state(g, ST_GO, now_ms);
                set_output_text(g, "GO", "Match it!");
            }
        } break;

        case ST_GO: {
            if ((now_ms - g->state_enter_ms) >= GO_DURATION_MS) {
                // Prepare scoring sync
                g->last_beat_edge = 0;
                g->last_step_index = 0;
                g->score = 0;

                fpga_if_start_playback(fpga);
                enter_state(g, ST_PLAYBACK, now_ms);
                set_output_text(g, "PLAY", "Press keys");
            }
        } break;

        case ST_PLAYBACK: {
            fpga_status_t st = fpga_if_read_status(fpga);

            // Hook points for scoring (we’ll implement properly in scoring.c)
            // For now: increment score when window is active and any key edge occurs.
            if (st.window_active) {
                // Any key press gives a little credit (placeholder)
                if (edges & 0xFu) {
                    // distribute points roughly across steps
                    uint32_t per = (g->seq_len ? (100u / g->seq_len) : 0u);
                    if (g->score + per > 100u) g->score = 100u;
                    else g->score += per;
                }
            }

            // Shake step placeholder (no penalty here yet)
            if (st.shake_window_active && in->shake_detected) {
                // optional bump (placeholder)
            }

            if (st.done) {
                fpga_if_stop(fpga);
                g->out.score_0_100 = g->score;

                enter_state(g, ST_RESULTS, now_ms);
                set_output_text(g, "RESULTS", "Score shown");
            }
        } break;

        case ST_RESULTS: {
            // After results delay, either advance ladder or return to idle
            if ((now_ms - g->state_enter_ms) >= RESULTS_DURATION_MS) {
                if (g->select_mode == SELECT_LADDER) {
                    g->mode = ladder_next(g->mode);
                }
                enter_state(g, ST_IDLE, now_ms);
                set_output_text(g, "Rhythm Game", "KEY0: Start");
            }
        } break;

        case ST_EXIT: {
            g->should_exit = true;
        } break;

        default:
            enter_state(g, ST_IDLE, now_ms);
            break;
    }

    // Keep output snapshot updated
    g->out.mode = g->mode;
    g->out.bpm = g->bpm;
    g->out.seq_len = g->seq_len;
    g->out.score_0_100 = g->score;
}
