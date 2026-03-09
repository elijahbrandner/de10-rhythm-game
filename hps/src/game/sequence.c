#include "game/sequence.h"
#include <string.h>

#define MAX_STEPS 64u

// Simple xorshift RNG (fast, deterministic, no libc rand dependency)
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    if (x == 0) x = 0xA3C59AC3u; // avoid zero-lock
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void sequence_mode_params(game_mode_t mode, uint32_t *bpm_out, uint32_t *len_out)
{
    if (!bpm_out || !len_out) return;

    switch (mode)
    {
        case GAME_MODE_EASY:
            *bpm_out = 45;
            *len_out = 5;
            break;

        case GAME_MODE_MEDIUM:
            *bpm_out = 60;
            *len_out = 8;
            break;

        case GAME_MODE_HARD:
            *bpm_out = 75;
            *len_out = 10;
            break;

        case GAME_MODE_EXPERT:
            *bpm_out = 75;
            *len_out = 12;
            break;

        default:
            *bpm_out = 60;
            *len_out = 8;
            break;
    }
}

// Insert N shake notes in expert, avoiding consecutive shake notes if possible
static void insert_shakes(uint32_t seed, fpga_step_t *steps, uint32_t len) {
    if (!steps || len == 0) return;

    uint32_t rng = seed ^ 0x5EED1234u;

    // Choose how many shakes (tweak later)
    uint32_t shakes = (len >= 10) ? 2 : 1;

    for (uint32_t k = 0; k < shakes; k++) {
        // Try a handful of times to find a non-consecutive spot
        for (uint32_t tries = 0; tries < 16; tries++) {
            uint32_t idx = (xorshift32(&rng) % len);

            // avoid first step sometimes (optional), and avoid duplicates
            if (steps[idx].shake) continue;

            bool left_shake  = (idx > 0)     ? steps[idx - 1].shake : false;
            bool right_shake = (idx + 1 < len) ? steps[idx + 1].shake : false;

            if (left_shake || right_shake) continue;

            steps[idx].shake = true;
            steps[idx].flash_all = true; // shake cue uses all segments
            break;
        }
    }
}

int sequence_generate(game_mode_t mode, uint32_t seed, fpga_step_t *out_steps, uint32_t out_cap, uint32_t *out_len) {
    if (!out_steps || !out_len) return -1;

    uint32_t bpm = 0, len = 0;
    sequence_mode_params(mode, &bpm, &len);

    if (len > out_cap || len > MAX_STEPS) return -1;

    // init
    memset(out_steps, 0, sizeof(fpga_step_t) * len);

    uint32_t rng = seed ^ 0xC0FFEE11u;

    // Generate lanes, avoid too many repeats in a row
    uint8_t prev_lane = 0xFF;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t lane = (uint8_t)(xorshift32(&rng) & 0x3u);

        // simple repeat-avoid: if same as previous, re-roll once
        if (lane == prev_lane) {
            lane = (uint8_t)(xorshift32(&rng) & 0x3u);
        }

        out_steps[i].lane = lane;
        out_steps[i].flash_all = false;
        out_steps[i].shake = false;
        prev_lane = lane;
    }

    // Expert: add shake notes
    if (mode == GAME_MODE_EXPERT) {
        insert_shakes(seed, out_steps, len);
    }

    *out_len = len;
    return 0;
}
