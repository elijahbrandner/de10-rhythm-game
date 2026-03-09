#include "game/game.h"
#include "game/sequence.h"

#include <string.h>

// ----------------------------
// Constants
// ----------------------------
#define MAX_STEPS 64u
#define WATCH_DURATION_MS    2000u
#define RESULTS_DURATION_MS  4000u

// fraction of beat used for active scoring/prompt window
#define STEP_WINDOW_NUM      85u
#define STEP_WINDOW_DEN      100u

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

static uint32_t bpm_to_step_ms(uint32_t bpm) {
    if (bpm == 0) return 1000u;
    return 60000u / bpm;
}

static uint32_t step_window_ms(uint32_t step_ms) {
    uint32_t w = (step_ms * STEP_WINDOW_NUM) / STEP_WINDOW_DEN;
    if (w == 0) w = 1;
    return w;
}

static void reset_round_state(game_t *g) {
    memset(g->step_scored, 0, sizeof(g->step_scored));
    g->last_step_index = 0;
    g->last_beat_edge = 0;
    g->score = 0;
    g->completed = false;
    scoring_init(&g->scoring, g->seq_len);
}

static void fpga_show_step(fpga_if_t *fpga, game_mode_t mode, uint8_t lane, fpga_led_mode_t led_mode) {
    fpga_if_set_lane(fpga, (fpga_lane_t)(lane & 0x3u));
    fpga_if_set_led_mode(fpga, led_mode);
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(mode));
    fpga_if_set_enable(fpga, true);
    fpga_if_commit(fpga);
}

static void fpga_idle_visual(fpga_if_t *fpga, game_mode_t mode) {
    fpga_if_set_lane(fpga, FPGA_LANE_0);
    fpga_if_set_led_mode(fpga, FPGA_LED_CHASE);
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(mode));
    fpga_if_set_enable(fpga, true);
    fpga_if_commit(fpga);
}

static void fpga_results_visual(fpga_if_t *fpga, game_mode_t mode, uint32_t score) {
    fpga_if_set_lane(fpga, FPGA_LANE_0);
    fpga_if_set_tempo(fpga, fpga_if_game_mode_to_tempo(mode));
    fpga_if_set_enable(fpga, true);

    if (score >= 75u) fpga_if_set_led_mode(fpga, FPGA_LED_BLINK);
    else              fpga_if_set_led_mode(fpga, FPGA_LED_PULSE);

    fpga_if_commit(fpga);
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

static void score_button_step(game_t *g, uint32_t idx, int pressed_lane, uint32_t offset_ms, uint32_t window_ms) {
    if (!g) return;
    if (idx >= g->seq_len) return;
    if (g->step_scored[idx]) return;
    if (pressed_lane < 0 || pressed_lane > 3) return;

    hit_grade_t grade;
    scoring_score_button_step(&g->scoring,
                              g->steps[idx].lane,
                              (uint8_t)pressed_lane,
                              offset_ms,
                              window_ms,
                              &grade);

    g->step_scored[idx] = true;
}

static void score_shake_step(game_t *g, uint32_t idx, bool shake_detected, uint32_t offset_ms, uint32_t window_ms) {
    if (!g) return;
    if (idx >= g->seq_len) return;
    if (g->step_scored[idx]) return;

    hit_grade_t grade;
    scoring_score_shake_step(&g->scoring,
                             shake_detected,
                             offset_ms,
                             window_ms,
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
        fpga_if_clear(fpga);
        enter_state(g, ST_EXIT, now_ms);
        set_output_text(g, "Exiting...", "");
        return;
    }

    g->select_mode = ((in->switches >> 9) & 0x1u) ? SELECT_FREE : SELECT_LADDER;

    if (g->state == ST_IDLE && g->select_mode == SELECT_FREE) {
        uint32_t sel = (in->switches & 0x3u);

        if (sel == 0x0)      g->mode = GAME_MODE_EASY;
        else if (sel == 0x2) g->mode = GAME_MODE_MEDIUM;
        else if (sel == 0x1) g->mode = GAME_MODE_HARD;
        else                 g->mode = GAME_MODE_EXPERT;
    }

    if (g->state == ST_IDLE) {
        sequence_mode_params(g->mode, &g->bpm, &g->seq_len);
    }

    switch (g->state) {

        case ST_IDLE: {
            g->completed = false;
            g->score = 0;
            g->out.score_0_100 = 0;
            g->out.rating_text = "";

            fpga_idle_visual(fpga, g->mode);

            if (g->select_mode == SELECT_LADDER) {
                set_output_text(g, "Idle", "KEY0: Start");
            } else {
                set_output_text(g, "Select", "KEY0: Start");
            }

            if (edges & 0x1u) {
                g->seed = now_ms;
                fpga_if_reset_pulse(fpga);

                enter_state(g, ST_WATCH, now_ms);
                set_output_text(g, "WATCH", "Get ready...");
            }
        } break;

        case ST_WATCH: {
            fpga_show_step(fpga, g->mode, 0, FPGA_LED_CHASE);

            if ((now_ms - g->state_enter_ms) >= WATCH_DURATION_MS) {
                uint32_t out_len = 0;

                if (sequence_generate(g->mode, g->seed, g->steps, MAX_STEPS, &out_len) != 0) {
                    fpga_if_clear(fpga);
                    enter_state(g, ST_RESULTS, now_ms);
                    g->score = 0;
                    g->out.score_0_100 = 0;
                    g->out.rating_text = "Sequence Error";
                    set_output_text(g, "ERROR", "Sequence failed");
                    break;
                }

                g->seq_len = out_len;
                reset_round_state(g);

                enter_state(g, ST_PREVIEW, now_ms);
                set_output_text(g, "PREVIEW", "Watch pattern");
            }
        } break;

        case ST_PREVIEW: {
            uint32_t step_ms = bpm_to_step_ms(g->bpm);
            uint32_t elapsed = now_ms - g->state_enter_ms;
            uint32_t idx = elapsed / step_ms;

            if (idx >= g->seq_len) {
                // Hold in READY state until player presses KEY0
                fpga_show_step(fpga, g->mode, 0, FPGA_LED_CHASE);
                enter_state(g, ST_GO, now_ms);
                set_output_text(g, "READY", "KEY0 to begin");
                break;
            }

            fpga_led_mode_t led_mode = g->steps[idx].shake ? FPGA_LED_BLINK : FPGA_LED_PULSE;
            fpga_show_step(fpga, g->mode, g->steps[idx].lane, led_mode);
        } break;

        case ST_GO: {
            // Player-controlled start state
            fpga_show_step(fpga, g->mode, 0, FPGA_LED_CHASE);

            if (edges & 0x1u) {
                g->last_step_index = 0;
                enter_state(g, ST_PLAYBACK, now_ms);
                set_output_text(g, "PLAY", "Press keys");
            }
        } break;

        case ST_PLAYBACK: {
            uint32_t step_ms   = bpm_to_step_ms(g->bpm);
            uint32_t window_ms = step_window_ms(step_ms);
            uint32_t elapsed   = now_ms - g->state_enter_ms;
            uint32_t idx       = elapsed / step_ms;
            uint32_t offset_ms = elapsed % step_ms;

            if (idx >= g->seq_len) {
                if (g->seq_len > 0) {
                    finalize_step_if_needed(g, g->seq_len - 1);
                }

                fpga_if_clear(fpga);

                g->score = scoring_finalize_0_100(&g->scoring);
                g->out.score_0_100 = g->score;
                g->out.rating_text = scoring_rating(g->score);

                fpga_results_visual(fpga, g->mode, g->score);

                enter_state(g, ST_RESULTS, now_ms);
                set_output_text(g, "RESULTS", g->out.rating_text);
                break;
            }

            if (idx != g->last_step_index) {
                if (g->last_step_index < g->seq_len) {
                    finalize_step_if_needed(g, g->last_step_index);
                }
                g->last_step_index = idx;
            }

            fpga_led_mode_t led_mode = g->steps[idx].shake ? FPGA_LED_BLINK : FPGA_LED_PULSE;
            fpga_show_step(fpga, g->mode, g->steps[idx].lane, led_mode);

            if (offset_ms <= window_ms) {
                if (g->steps[idx].shake) {
                    if (in->shake_detected) {
                        score_shake_step(g, idx, true, offset_ms, window_ms);
                    }
                } else {
                    int pressed_lane = decode_lane_from_edges(edges);
                    if (pressed_lane >= 0) {
                        score_button_step(g, idx, pressed_lane, offset_ms, window_ms);
                    }
                }
            }
        } break;

        case ST_RESULTS: {
            fpga_results_visual(fpga, g->mode, g->score);

            if ((now_ms - g->state_enter_ms) >= RESULTS_DURATION_MS) {
                fpga_if_clear(fpga);

                if (g->select_mode == SELECT_LADDER) {
                    g->mode = ladder_next(g->mode);
                }

                enter_state(g, ST_IDLE, now_ms);
                set_output_text(g, "Rhythm Game", "KEY0: Start");
            }
        } break;

        case ST_EXIT: {
            g->should_exit = true;
            fpga_if_clear(fpga);
        } break;

        default: {
            fpga_if_clear(fpga);
            enter_state(g, ST_IDLE, now_ms);
        } break;
    }

    g->out.mode = g->mode;
    g->out.bpm = g->bpm;
    g->out.seq_len = g->seq_len;
    g->out.score_0_100 = g->score;
}