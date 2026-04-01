// Scoring system header file
// defines:
// - scoring_ctx_t
// - hit_grade_t
// - funciton declarations
// - integer/boolean types used by this file
#include "game/scoring.h"

// Helper funciton to clamp an integer value into a safe range
//
// If v is below lo, return lo
// if v is above hi, return hi
// Otherwise return v unchanged
// 
// Useful for preventing score totals from going negative
// or growing beyond some safe upper bound
static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;      // Too small -> clamp to lower limit
    if (v > hi) return hi;      // Too large -> clamp to upper limit
    return v;                   // Already in range
}

// Initialize the scoring context for a new round
// 
// Parameters:
// - ctx: pointer to the scoring context structure
// - seq_len: number of steps in the current sequence
//
// returns:
// - 0 on success
// - -1 if inputs are invalid
//
// Purpose:
// This prepares the scoring system to track a new round by:
// - storing the sequence length
// - resetting accumulated points
// - computing a base point value for each step
int scoring_init(scoring_ctx_t *ctx, uint32_t seq_len) {
    // Reject invalid inputs:
    // - ctx must exist
    // - sequence length cannot be 0
    if (!ctx || seq_len == 0) return -1;

    // Score the number of steps in the sequence
    ctx->seq_len = seq_len;

    // Reset total points at the start of a round
    ctx->total_points = 0;

    // COmpute the base number of points each step is worth
    // Example:
    // If seq_len = 5, base = 100 / 5 = 20 points per step
    // 
    // this uses integer division, so some remainder might be lost
    // This is okay for now because the final score is clamped later
    ctx->step_points_base = 100u / seq_len;

    // Safety fallback:
    // If seq_len is very large, integer division could produce 0
    // In that case, force each step to be worth at least 1 point
    if (ctx->step_points_base == 0) ctx->step_points_base = 1;

    return 0; // Init succeeded
}

// Determine a hit grade based on how early the player responded
// within the active timing window
//
// Design intent:
// The note/prompt is visible for the full step,
// and pressing sooner in that valid window should earn a better grade
//
// Reutrns one of:
// - HIT_PERFECT
// - HIT_GOOD
// - HIT_OK
// - HIT_MISS
static hit_grade_t grade_from_offset(uint32_t offset_ms, uint32_t window_ms) {
    // If the window is invalid, treat it as a miss
    if (window_ms == 0) return HIT_MISS;

    // If the input happened after the valid window, it is a miss
    if (offset_ms > window_ms) return HIT_MISS;

    // Define grading thresholds as percentages of the window
    //
    // thresholds (current tuning)
    // Perfect: first 40% of window
    // Good:    first 65%
    // OK:      first 85%
    uint32_t p = (window_ms * 40u) / 100u;
    uint32_t g = (window_ms * 65u) / 100u;
    uint32_t o = (window_ms * 85u) / 100u;

    if (offset_ms <= p) return HIT_PERFECT;
    if (offset_ms <= g) return HIT_GOOD;
    if (offset_ms <= o) return HIT_OK;
    return HIT_MISS;
}

// Converts a hit grade into an actual point amount
// 
// Parameters:
// - base: the base number of points for one step
// - grade: the quality of the hit
//
// Curent scoring scale
// - PERFECT = 100% of base
// - GOOD    = 75% of base
// - OK      = 40% of base
// - MISS    = 0
//
// This can be tuned later if we want game to feel stricter or more genrous
static int points_for_grade(uint32_t base, hit_grade_t grade) {
    // scale points by grade (tune later)
    switch (grade) {
        case HIT_PERFECT: return (int)base;                     // Full value
        case HIT_GOOD:    return (int)((base * 75u) / 100u);    // 75% of base
        case HIT_OK:      return (int)((base * 40u) / 100u);    // 40% of base
        case HIT_MISS:    return 0;                             // No points
        default:          return 0;                             // Safe fallback
    }
}

// Scores a normal button step
// 
// Parameters:
// - ctx: scoring context
// - expected_lane: which lane the game expected
// - pressed_lane: which lane the player actually pressed
// - offset_ms : timing offset within the active window
// - window_ms : size of the active scoring window
// - grade_out : optional pointer to receive the resulting hit grade
//
// Behavior:
// - If the player pressed the wrong lane, apply a penalty
// - If the lane is correct, compute grade from timing and award points
//
// Returns:
// - position value for awarded points
// - negative value for penalty
// - 0 for no change
int scoring_score_button_step(scoring_ctx_t *ctx,
                              uint8_t expected_lane,
                              uint8_t pressed_lane,
                              uint32_t offset_ms,
                              uint32_t window_ms,
                              hit_grade_t *grade_out) {
    // If no scoring context exists, do nothing
    if (!ctx) return 0;

    // Wrong lane penalty:
    // The design here is that pressing the wrong button should hurt the score
    //
    // current penalty:
    // 50 % of the base step points
    //
    // Example:
    // If a step is worth 20 base points,
    // Wrong button = -10 points
    const int wrong_penalty = (int)(ctx->step_points_base / 2u);

    // If the player pressed the wrong lane
    if (pressed_lane != expected_lane) {
        // Report this as a miss if caller wants the grade
        if (grade_out) *grade_out = HIT_MISS;

        // Apply negative score delta
        int delta = -wrong_penalty;

        // UPdate total points safely
        // Clamp prevents total_points from going below 0 or above a huge safe cap
        ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);

        // Return how much the score changed
        return delta;
    }

    // If the lane was correct, determine timing grade
    hit_grade_t grade = grade_from_offset(offset_ms, window_ms);

    // If caller provided a place to store the grade, write it there
    if (grade_out) *grade_out = grade;

    // Convert the grade into awarded points
    int delta = points_for_grade(ctx->step_points_base, grade);

    // Add those points to total_points safely
    ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);

    // Return awarded delta
    return delta;
}

// Handles a missed button step
// 
// THis is used when a player never hit the note during its valid window
// 
// Current behavior:
// - mark the step as a miss
// - award 0 points
// - do not apply an additional penalty
//
// Returns 0 because no score changes occur
int scoring_miss_button_step(scoring_ctx_t *ctx, hit_grade_t *grade_out) {
    // If no context exists, do nothing
    if (!ctx) return 0;

    // Report miss grade if requested
    if (grade_out) *grade_out = HIT_MISS;

    // no delta (0 points) No point change for a simple missed button note
    return 0;
}

// Scores a shake step
// Parameters:
// - ctx: scoring context
// - detected: whether a shake was detected
// - offset_ms: timing offset inside the window
// - window_ms: active timing window size
// - grade_out: optional returned hit grade
//
// Design choice:
// Shake notes are intentioanlly more forgiving
// We dont want accidental punishment
//
// Current behavior:
// - If shake is missed completely: small penalty
// - If shake is detected: score it using timing
//   but even weak timing can still be upgraded to at least OK
//
// Returns:
// - positive value for awareded points
// - negative value for miss penalty
// - 0 for no change
int scoring_score_shake_step(scoring_ctx_t *ctx,
                             bool detected,
                             uint32_t offset_ms,
                             uint32_t window_ms,
                             hit_grade_t *grade_out) {
    // If no context exists, do nothing
    if (!ctx) return 0;

    // Penalty for missing a shake note entirely:
    //
    // Current timing:
    // 25% of base step points
    //
    // Example:
    // If base = 20, missed shake penalty = -5
    const int miss_penalty = (int)(ctx->step_points_base / 4u);

    // If no shake was detected
    if (!detected) {
        // Report miss grade if requested
        if (grade_out) *grade_out = HIT_MISS;

        // Apply small negative penalty
        int delta = -miss_penalty;

        // Update total points safely with clamping
        ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);

        // Return penalty amount
        return delta;
    }

    // If detected, grade it by timing (but keep it generous)
    hit_grade_t grade = grade_from_offset(offset_ms, window_ms);

    // Generous rule:
    // If timing logic somehow says MISS, still upgrade it to OK
    // This makes shake notes more forgiving and less frustrating
    if (grade == HIT_MISS) grade = HIT_OK;

    // Return grade if caller wants it
    if (grade_out) *grade_out = grade;

    // Convert grade into awarded points
    int delta = points_for_grade(ctx->step_points_base, grade);

    // Add points safely
    ctx->total_points = (uint32_t)clamp_int((int)ctx->total_points + delta, 0, 1000000);

    // Return awarded points
    return delta;
}

// Finalizes the total score and returns it in the 0-100 range
//
// Since step scoring can involve integer division and penalties,
// total_points may end up silghtly outside the desired final ragne
//
// This function clamps the result to [0, 100]
uint32_t scoring_finalize_0_100(const scoring_ctx_t *ctx) {
    // If context is invalid, return 0 as safe fallback
    if (!ctx) return 0;

    // Copy total_points into a signed integer for easier clamping logic
    int s = (int)ctx->total_points;

    // Clamp low end
    if (s < 0) s = 0;

    // Clamp high end
    if (s > 100) s = 100;

    // Return final score as unsigned integer
    return (uint32_t)s;
}

// Converts a numeric score into a text rating
//
// These strings are used for results/feedback screens
//
// Current rating bands:
// 93+ = perfection
// 85+ = Excellent
// 75+ = Great
// 60+ = Good
// 45+ = Okay
// 25+ = Terrible
// else = Just restart bro
const char *scoring_rating(uint32_t score) {
    if (score >= 93) return "Perfection";
    if (score >= 85) return "Excellent";
    if (score >= 75) return "Great";
    if (score >= 60) return "Good";
    if (score >= 45) return "Okay";
    if (score >= 25) return "Terrible";
    return "Just restart bro";
}
