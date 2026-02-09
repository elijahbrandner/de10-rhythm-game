#ifndef SCORING_H
#define SCORING_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t seq_len;
    uint32_t total_points;     // accumulated points (can go negative internally in impl)
    uint32_t step_points_base; // base points per step (approx 100/seq_len)
} scoring_ctx_t;

typedef enum {
    HIT_PERFECT = 0,
    HIT_GOOD,
    HIT_OK,
    HIT_MISS
} hit_grade_t;

// init scoring for a round
int scoring_init(scoring_ctx_t *ctx, uint32_t seq_len);

// score a normal button step
// expected_lane: 0..3
// pressed_lane:  0..3
// offset_ms: how far from start of window the press occurred (0=earliest)
// window_ms: duration of scoring window
// returns points delta (can be negative for wrong button)
int scoring_score_button_step(scoring_ctx_t *ctx,
                              uint8_t expected_lane,
                              uint8_t pressed_lane,
                              uint32_t offset_ms,
                              uint32_t window_ms,
                              hit_grade_t *grade_out);

// score a missed button step (no press happened)
int scoring_miss_button_step(scoring_ctx_t *ctx, hit_grade_t *grade_out);

// score a shake step (Expert)
// detected = true if shake happened during shake window
// offset_ms/window_ms used to optionally grade early vs late; keep generous
int scoring_score_shake_step(scoring_ctx_t *ctx,
                             bool detected,
                             uint32_t offset_ms,
                             uint32_t window_ms,
                             hit_grade_t *grade_out);

// finalize to 0..100
uint32_t scoring_finalize_0_100(const scoring_ctx_t *ctx);

// optional: rating text based on 0..100
const char *scoring_rating(uint32_t score);

#endif // SCORING_H
