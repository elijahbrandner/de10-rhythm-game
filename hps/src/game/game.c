#include "game/game.h"
#include "game/sequence.h"
#include <string.h>

// ----------------------------
// Constants
// ----------------------------
#define MAX_STEPS 64u

#define WATCH_DURATION_MS   5000u
#define GO_DURATION_MS      2000u
#define RESULTS_DURATION_MS 4000u

// ----------------------------
// Helpers
// ----------------------------

static void enter_state(game_t *g, game_state_t st, uint32_t now_ms) {
    g->state = st;
    g->state_enter_ms = now_ms;
    g->out.state = st;
}

static uint32_t button_edges(game_t *g, uint32_t curr_buttons) {
    uint32_t edges = (~g->prev_buttons) & curr_buttons;
    g->prev_buttons = curr_buttons;
    return edges;
}

static bool exit_combo_edge(uint32_t switches, uint32_t pressed_edges) {
    bool sw_all = ((switches & 0x3FFu) == 0x3FFu);
    bool key0_edge = (pressed_edges & 0x1u) != 0;
    return sw_all && key0_edge;
}

static void set_output_text(game_t *g, const char *l1, const char *l2) {
    g->out.line1 = l1;
    g->out.line2 = l2;
}

static game_mode_t ladder_next(game_mode_t m) {
    switch (m) {
        case GAME_MODE_EASY:   return GAME_MODE_MEDIUM;
        case GAME_MODE_MEDIUM: return GAME_MODE_HARD;
        case GAME_MODE_HARD:   return GAME_MODE_EXPERT;
        case GAME_MODE_EXPERT: return GAME_MODE_EASY;
        default:               return GAME_MODE_EASY;
    }
}

static int decode_lane_from_edges(uint32_t edges) {
    if (edges & (1u << 0)) return 0;
    if (edges & (1u << 1)) return 1;
    if (edges & (1u << 2)) return 2;
    if (edges & (1u << 3)) return 3;
    return -1;
}

static void reset_round_state(game_t *g) {
    memset(g->step_scored, 0, sizeof(g->step_scored));
    g->last_beat_edge = 0;
    g->last_step_index = 0;
    g->score = 0;
    g->completed = false;
    scoring_init(&g->scoring, g->seq_len);
}

static void finalize_step_if_needed(game_t *g, uint32_t idx) {
    if (!g) return;
    if (idx >= g->seq_len) return;
    if (g->step_scored[idx]) return;

    hit_grade_t grade;

    if (g->steps[idx].shake) {
        scoring_score_shake_step(&g->scoring, false, 0, 1, &grade);
    } else {
        scoring_miss_button_step(&g->scoring, &grade);
    }

    g->step_scored[idx] = true;
}

static void score_current_step_button(game_t *g, uint32_t idx, int pressed_lane) {
    if (!g) return;
    if (idx >= g->seq_len) return;
    if (g->step_scored[idx]) return;
    if (pressed_lane < 0 || pressed_lane > 3) return;

    hit_grade_t grade;
    scoring_score_button_step(&g->scoring,
                              g->steps[idx].lane,
                              (uint8_t)pressed_lane,
                              0,   // placeholder timing offset
                              1,   // placeholder window size
                              &grade);

    g->step_scored[idx] = true;
}

static void score_current_step_shake(game_t *g, uint32_t idx, bool shake_detected) {
    if (!g) return;
    if (idx >= g->seq_len) return;
    if (g->step_scored[idx]) return;

    hit_grade_t grade;
    scoring_score_shake_step(&g->scoring,
                             shake_detected,
                             0,   // placeholder timing offset
                             1,   // placeholder window size
                             &grade);

    if (shake_detected) {
        g->step_scored[idx] = true;
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

    sequence_mode_params(g->mode, &g->bpm, &g->seq_len);

    g->out.state = g->state;
    g->out.mode = g->mode;
    g->out.bpm = g->bpm;
    g->out.seq_len = g->seq_len;
    g->out.score_0_100 = 0;
    g->out.rating_text = "";

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

    uint32_t edges = button_edges(g, in->buttons_raw);

    if (exit_combo_edge(in->switches, edges)) {
        g->should_exit = true;
        fpga_if_stop(fpga);
        enter_state(g, ST_EXIT, now_ms);
        set_output_text(g, "Exiting...", "");
        return;
    }

    // Always update ladder/free select from SW9
    g->select_mode = ((in->switches >> 9) & 0x1u) ? SELECT_FREE : SELECT_LADDER;

    // Free-select difficulty only while idle
    if (g->state == ST_IDLE && g->select_mode == SELECT_FREE) {
        uint32_t sel = (in->switches & 0x3u);

        if (sel == 0x0)      g->mode = GAME_MODE_EASY;
        else if (sel == 0x2) g->mode = GAME_MODE_MEDIUM;
        else if (sel == 0x1) g->mode = GAME_MODE_HARD;
        else                 g->mode = GAME_MODE_EXPERT;
    }

    // Keep params synced while idle only
    if (g->state == ST_IDLE) {
        sequence_mode_params(g->mode, &g->bpm, &g->seq_len);
    }

    switch (g->state) {

        case ST_IDLE: {
            g->completed = false;
            g->score = 0;
            g->out.score_0_100 = 0;
            g->out.rating_text = "";

            if (g->select_mode == SELECT_LADDER) {
                set_output_text(g, "Mode: LADDER", "KEY0: Start");
            } else {
                set_output_text(g, "Mode: FREE", "KEY0: Start");
            }

            if (edges & 0x1u) {
                // In ladder mode, start from Easy only if currently not already in ladder flow.
                // Keeping your current selected mode is usually cleaner for testing.
                g->seed = now_ms;

                fpga_if_reset(fpga);
                fpga_if_configure(fpga, g->mode, g->bpm, g->seq_len);

                enter_state(g, ST_WATCH, now_ms);
                set_output_text(g, "WATCH", "Get ready...");
            }
        } break;

        case ST_WATCH: {
            if ((now_ms - g->state_enter_ms) >= WATCH_DURATION_MS) {
                uint32_t out_len = 0;

                if (sequence_generate(g->mode, g->seed, g->steps, MAX_STEPS, &out_len) != 0) {
                    fpga_if_stop(fpga);
                    enter_state(g, ST_RESULTS, now_ms);
                    g->score = 0;
                    g->out.score_0_100 = 0;
                    g->out.rating_text = "Sequence Error";
                    set_output_text(g, "ERROR", "Sequence failed");
                    break;
                }

                g->seq_len = out_len;

                reset_round_state(g);

                fpga_if_configure(fpga, g->mode, g->bpm, g->seq_len);

                if (fpga_if_load_sequence(fpga, g->steps, g->seq_len) != 0) {
                    fpga_if_stop(fpga);
                    enter_state(g, ST_RESULTS, now_ms);
                    g->score = 0;
                    g->out.score_0_100 = 0;
                    g->out.rating_text = "FPGA Load Error";
                    set_output_text(g, "ERROR", "Load failed");
                    break;
                }

                fpga_if_start_preview(fpga);
                enter_state(g, ST_PREVIEW, now_ms);
                set_output_text(g, "PREVIEW", "Watch pattern");
            }
        } break;

        case ST_PREVIEW: {
            fpga_status_t st = fpga_if_read_status(fpga);

            if (st.done) {
                fpga_if_stop(fpga);
                enter_state(g, ST_GO, now_ms);
                set_output_text(g, "GO", "Match it!");
            }
        } break;

        case ST_GO: {
            if ((now_ms - g->state_enter_ms) >= GO_DURATION_MS) {
                g->last_step_index = 0;
                g->last_beat_edge = 0;

                fpga_if_start_playback(fpga);
                enter_state(g, ST_PLAYBACK, now_ms);
                set_output_text(g, "PLAY", "Press keys");
            }
        } break;

        case ST_PLAYBACK: {
            fpga_status_t st = fpga_if_read_status(fpga);
            uint32_t idx = st.step_index;

            // If FPGA advanced to a new step, finalize previous unscored step
            if (idx != g->last_step_index) {
                if (g->last_step_index < g->seq_len) {
                    finalize_step_if_needed(g, g->last_step_index);
                }
                g->last_step_index = idx;
            }

            // Score current button note
            if (st.window_active && idx < g->seq_len && !g->steps[idx].shake) {
                int pressed_lane = decode_lane_from_edges(edges);
                if (pressed_lane >= 0) {
                    score_current_step_button(g, idx, pressed_lane);
                }
            }

            // Score current shake note
            if (st.shake_window_active && idx < g->seq_len && g->steps[idx].shake) {
                if (in->shake_detected) {
                    score_current_step_shake(g, idx, true);
                }
            }

            if (st.done) {
                // Finalize last step if it never got scored
                if (idx < g->seq_len) {
                    finalize_step_if_needed(g, idx);
                }

                fpga_if_stop(fpga);

                g->score = scoring_finalize_0_100(&g->scoring);
                g->out.score_0_100 = g->score;
                g->out.rating_text = scoring_rating(g->score);

                enter_state(g, ST_RESULTS, now_ms);
                set_output_text(g, "RESULTS", g->out.rating_text);
            }
        } break;

        case ST_RESULTS: {
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

        default: {
            enter_state(g, ST_IDLE, now_ms);
        } break;
    }

    g->out.mode = g->mode;
    g->out.bpm = g->bpm;
    g->out.seq_len = g->seq_len;
    g->out.score_0_100 = g->score;
}