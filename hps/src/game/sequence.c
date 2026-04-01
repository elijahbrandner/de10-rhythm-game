// Include sequence system header
// THis defines:
// - game_mode_t
// - fpga_step_t
// - function declarations related to sequence generation
#include "game/sequence.h"

// Include string utilities for memset()
#include <string.h>

// Max number of steps the sequenc generator is allowed to produce
// This acts as a hard safety cap
#define MAX_STEPS 64u



// Simple xorshift RNG (fast, deterministic, no libc rand dependency)
//
// We use this instead of rand() because:
// - fast
// - deterministic
// - lightweight
// - no dependency on libc rand()
// - good enough for game pattern generation
//
// It updates the state in place and returns the new pseudo-random value
static uint32_t xorshift32(uint32_t *state) {
    // Copy current RNG state
    uint32_t x = *state;
    
    // xorshift generators get stuck if the state is ever 0
    // So if state is 0, force it to a nonzero fallback seed
    if (x == 0) x = 0xA3C59AC3u; // avoid zero-lock

    // Apply xorshift bit-mixing steps
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    // Save updated RNG state back through the pointer
    *state = x;

    // Return new random value
    return x;
}

// Return the BPNM and sequence length associated with a given difficulty
//
// Parameters:
// - mode: selected game difficulty
// - bpm_out: pointer where BPM should be written
// - len_out: pointer where sequence length should be written
//
// This function is used so that difficulty settings remain centralized
// in one place instead of being scattered across the codebase
void sequence_mode_params(game_mode_t mode, uint32_t *bpm_out, uint32_t *len_out)
{
    // Defensive check:
    // both output pointers must be valid
    if (!bpm_out || !len_out) return;

    // Choose BPM and length based on difficulty
    switch (mode)
    {
        case GAME_MODE_EASY:
            *bpm_out = 45;  // Slower tempo for easier play
            *len_out = 5;   // Shorter pattern
            break;

        case GAME_MODE_MEDIUM:
            *bpm_out = 60; // Moderate tmepo
            *len_out = 6; // Slightly longer pattern
            break;

        case GAME_MODE_HARD:
            *bpm_out = 75; // Faster tempo
            *len_out = 7; // longer pattern
            break;

        case GAME_MODE_EXPERT:
            *bpm_out = 75; // Same tempo as hard
            *len_out = 8; // Longest pattern here
            break;

        default:
            // Fallback if mode is invalid
            *bpm_out = 60; 
            *len_out = 6;
            break;
    }
}

// Insert shake notes into a generated sequence
// 
// Current design:
// - only used for expert mode
// - tries to avoid placing shake notes next to each other
// - marks flash_all so the visual cue is stronger for shake steps
//
// Parameters:
// - seed: base seed for deterministic placement
// - steps: array of generated steps
// - len: number of valid steps in the array
static void insert_shakes(uint32_t seed, fpga_step_t *steps, uint32_t len) {
    // Validate inputs
    if (!steps || len == 0) return;

    // Derive a local RNG seed specifically for shake insertion
    // XOR with a constant to shake placement is deterministic
    // but not identical to lane-gernation randomness
    uint32_t rng = seed ^ 0x5EED1234u;

    
    // decide how many shake notes to insert
    //
    // Current rule:
    // - if sequence length is 10 or more, insert 2
    // - otherwise insert 1
    //
    // Since our current max difficulties are shorter than 10,
    // expert currently gets 1 shake notes
    uint32_t shakes = (len >= 10) ? 2 : 1;

    // try to place each requested shake note
    for (uint32_t k = 0; k < shakes; k++) {

        // Try a limited number of times to find a vlid position
        // This prevents an infinfite loop if no ideal spot is available
        for (uint32_t tries = 0; tries < 16; tries++) {
            // Pick a candidate index in te sequence
            uint32_t idx = (xorshift32(&rng) % len);

            // If this step is already a shake note, skip it
            if (steps[idx].shake) continue;

            // Check neighboring steps so we can avoid consecutive shake notes
            bool left_shake  = (idx > 0)     ? steps[idx - 1].shake : false;
            bool right_shake = (idx + 1 < len) ? steps[idx + 1].shake : false;

            // If neither neighbor is already a shake note, skip this index
            if (left_shake || right_shake) continue;

            // Mark this step as a shake step
            steps[idx].shake = true;

            // Enable "flash all" cue for shake notes so they stand out visually
            steps[idx].flash_all = true; // shake cue uses all segments

            // Stop searching for this shake note: placement succeeded
            break;
        }
    }
}

// Gerneates a new sequence/pattern for the current round
//
// Parameters:
// - mode: selected difficulty
// - seed: random seed, typically derived from time or round start
// - out_steps: destination array for generated sequence steps
// - out_cap:  capacity of out_steps array
// - out_len: output pointer that receives final generated length
//
// Returns: 0 on success, -1 on failure
//
// What this function does:
// 1. Looks up the BPM and length for selected mode
// 2. Clears the output array
// 3. Generated random lane values
// 4. Avoids immediate repeats when possible
// 5. Adds shake notes in expert mode
// 6. Returns te final length
int sequence_generate(game_mode_t mode, uint32_t seed, fpga_step_t *out_steps, uint32_t out_cap, uint32_t *out_len) {
    // Validate required output pointers
    if (!out_steps || !out_len) return -1;

    // Get BPM and sequence length for this mode
    uint32_t bpm = 0, len = 0;
    sequence_mode_params(mode, &bpm, &len);

    // Ensure requested length fits inside provided output buffer
    // and does not exceed the hard-coded maximum
    if (len > out_cap || len > MAX_STEPS) return -1;

    // Initialize the used portion of the output array to zero
    //
    // This ensures:
    // - all lanes start clean
    // - shake flags start false
    // - flash flags start false
    memset(out_steps, 0, sizeof(fpga_step_t) * len);

    // Create a deterministic RNG seed for lane generation
    // XOR with a different constant than shake insertion
    // so the two behaviors are decorrelated
    uint32_t rng = seed ^ 0xC0FFEE11u;

    // Track the previous lane so we can reduce repeated notes
    uint8_t prev_lane = 0xFF;

    // Generate one step at a time
    for (uint32_t i = 0; i < len; i++) {
        // Pick a random lane from 0 to 3 using the low 2 bits
        uint8_t lane = (uint8_t)(xorshift32(&rng) & 0x3u);

        // Basic repeat-avoid logic:
        // if this lane matches the previous one, reroll once
        //
        // This does not guarantee no repeats
        // but it reduces the number of immeidiate duplicates
        // without making the logic more complex
        if (lane == prev_lane) {
            lane = (uint8_t)(xorshift32(&rng) & 0x3u);
        }

        // Store the lane into this step
        out_steps[i].lane = lane;

        // Normal lane notes do not flash all segments by default
        out_steps[i].flash_all = false;

        // Normal lane notes for the next iteration's repeat check
        out_steps[i].shake = false;

        // Remember this lane for the next iteration's repeat check
        prev_lane = lane;
    }

    // Expert: add shake notes after generating lanes
    if (mode == GAME_MODE_EXPERT) {
        insert_shakes(seed, out_steps, len);
    }

    // Return the final generated sequence length
    *out_len = len;
    return 0; // Success
}
