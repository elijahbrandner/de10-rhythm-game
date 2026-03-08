#include "game/scoring.h"

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int scoring_init(scoring_ctx_t *ctx, uint32_t seq_len) {
    if (!ctx || seq_len == 0) return -1;

    ctx->seq_len = seq_len;
    ctx->total_points = 0;

    // base points per step (floor). Remainder can be handled later if you want.
    ctx->step_points_base = 100u / seq_len;
    if (ctx->step_points_base == 0) ctx->step_points_base = 1;

    return 0;
}

// Grade based on how early the press is within the window.
// You said: "HEX lights for the entire step; sooner press = better score."
static hit_grade_t grade_from_offset(uint32_t offset_ms, uint32_t window_ms) {
    if (window_ms == 0) return HIT_MISS;
    if (offset_ms > window_ms) return HIT_MISS;

    // thresholds (tune later)
    // Perfect: first 25% of window
    // Good:    first 55%
    // OK:      first 85%
    uint32_t p = (window_ms * 25u) / 100u;
    uint32_t g = (window_ms * 55u) / 100u;
    uint32_t o = (window_ms * 85u) / 100u;

    if (offset_ms <= p) return HIT_PERFECT;
    if (offset_ms <= g) return HIT_GOOD;
    if (offset_ms <= o) return HIT_OK;
    return HIT_MISS;
}

static int points_for_grade(uint32_t base, hit_grade_t grade) {
    // scale points by grade (tune later)
    switch (grade) {
        case HIT_PERFECT: return (int)base;
        case HIT_GOOD:    return (int)((base * 75u) / 100u);
        case HIT_OK:      return (int)((base * 40u) / 100u);
        case HIT_MISS:    return 0;
        default:          return 0;
    }
}

int scoring_score_button_step(scoring_ctx_t *ctx,
                              uint8_t expected_lane,
                              uint8_t pressed_lane,
                              uint32_t offset_ms,
                              uint32_t window_ms,
                              hit_grade_t *grade_out) {
    if (!ctx) return 0;

    // Wrong lane penalty: we wanted points taken away for wrong button.
    // Penalty size: 50% of base step points (tune later).
    const int wrong_penalty = (int)(ctx->step_points_base / 2u);

    if (pressed_lane != expected_lane) {
        if (grade_out) *grade_out = HIT_MISS;
        int delta = -wrong_penalty;
        // Apply but don't allow below 0 overall later in finalize (we clamp)
        ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);
        return delta;
    }

    hit_grade_t grade = grade_from_offset(offset_ms, window_ms);
    if (grade_out) *grade_out = grade;

    int delta = points_for_grade(ctx->step_points_base, grade);
    ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);
    return delta;
}

int scoring_miss_button_step(scoring_ctx_t *ctx, hit_grade_t *grade_out) {
    if (!ctx) return 0;
    if (grade_out) *grade_out = HIT_MISS;
    // no delta (0 points)
    return 0;
}

int scoring_score_shake_step(scoring_ctx_t *ctx,
                             bool detected,
                             uint32_t offset_ms,
                             uint32_t window_ms,
                             hit_grade_t *grade_out) {
    if (!ctx) return 0;

    // Shake notes: you wanted NO accidental punishment.
    // So: if detected = true, award OK/GOOD/PERFECT (generous).
    // If missed completely: small penalty (25% of base step points).
    const int miss_penalty = (int)(ctx->step_points_base / 4u);

    if (!detected) {
        if (grade_out) *grade_out = HIT_MISS;
        int delta = -miss_penalty;
        ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);
        return delta;
    }

    // If detected, grade it (but keep it generous)
    hit_grade_t grade = grade_from_offset(offset_ms, window_ms);
    if (grade == HIT_MISS) grade = HIT_OK; // generous: any in-window shake counts at least OK

    if (grade_out) *grade_out = grade;

    int delta = points_for_grade(ctx->step_points_base, grade);
    ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);
    return delta;
}

uint32_t scoring_finalize_0_100(const scoring_ctx_t *ctx) {
    if (!ctx) return 0;

    // total_points can exceed 100 due to rounding; clamp
    int s = (int)ctx->total_points;
    if (s < 0) s = 0;
    if (s > 100) s = 100;
    return (uint32_t)s;
}

const char *scoring_rating(uint32_t score) {
    if (score >= 93) return "Perfection";
    if (score >= 85) return "Excellent";
    if (score >= 75) return "Great";
    if (score >= 60) return "Good";
    if (score >= 45) return "Okay";
    if (score >= 25) return "Terrible";
    return "Just restart bro";
}
