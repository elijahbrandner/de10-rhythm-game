#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "../includes/hal/hal-api.h"

#include "../includes/peripherals/button.h"
#include "../includes/peripherals/switch.h"
#include "../includes/peripherals/lcd.h"

#include "../includes/fpga/fpga_if.h"

#include "../includes/game/game.h"
#include "../includes/game/sequence.h"
#include "../includes/game/scoring.h"

// ------------------------------------------------------------
// Config knobs
// ------------------------------------------------------------

// If your KEYs behave inverted (pressed reads as 0), set this to 1.
// (DE10 keys are often active-low.)
#ifndef KEY_ACTIVE_LOW
#define KEY_ACTIVE_LOW 1
#endif

// Reduce CPU usage (microseconds)
#define LOOP_SLEEP_US 5000

static uint32_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
    return (uint32_t)ms;
}

// Helper to format mode as text
static const char *mode_str(game_mode_t m) {
    switch (m) {
        case GAME_MODE_EASY:   return "Easy";
        case GAME_MODE_MEDIUM: return "Medium";
        case GAME_MODE_HARD:   return "Hard";
        case GAME_MODE_EXPERT: return "Expert";
        default:               return "Unknown";
    }
}

// Helper to format state as text
static const char *state_str(game_state_t s) {
    switch (s) {
        case ST_IDLE:     return "IDLE";
        case ST_WATCH:    return "WATCH";
        case ST_PREVIEW:  return "PREVIEW";
        case ST_GO:       return "GO";
        case ST_PLAYBACK: return "PLAY";
        case ST_RESULTS:  return "RESULTS";
        case ST_EXIT:     return "EXIT";
        default:          return "?";
    }
}

int main(void) {
    // ----------------------------
    // HAL + peripherals
    // ----------------------------
    hal_map_t hal = {0};
    if (hal_open(&hal) != 0) {
        printf("[MAIN] Failed to open HAL.\n");
        return 1;
    }

    button_handle_t buttons = {0};
    if (button_init(&buttons, &hal) != 0) {
        printf("[MAIN] Failed to init buttons.\n");
        hal_close(&hal);
        return 1;
    }

    switch_handle_t sw = {0};
    if (switch_init(&sw, &hal) != 0) {
        printf("[MAIN] Failed to init switches.\n");
        button_cleanup(&buttons);
        hal_close(&hal);
        return 1;
    }

    lcd_handle_t lcd = {0};
    if (lcd_init(&lcd, &hal) != 0) {
        printf("[MAIN] WARNING: LCD/char buffer init failed. Continuing without LCD.\n");
        // We can continue even if LCD fails.
    } else {
        lcd_clear(&lcd);
        lcd_write_line(&lcd, 0, "Rhythm-Based Timing Game");
        lcd_write_line(&lcd, 1, "Booting...");
    }

    // ----------------------------
    // FPGA interface + game
    // ----------------------------
    fpga_if_t fpga = {0};
    if (fpga_if_init(&fpga, &hal) != 0) {
        printf("[MAIN] Failed to init FPGA interface.\n");
        if (lcd.initialized) lcd_write_line(&lcd, 1, "FPGA IF init failed");
        switch_cleanup(&sw);
        button_cleanup(&buttons);
        if (lcd.initialized) lcd_cleanup(&lcd);
        hal_close(&hal);
        return 1;
    }

    game_t game;
    game_init(&game);

    // Seed for sequences (we can mix in switches later too)
    uint32_t seed = now_ms();

    // ----------------------------
    // Main loop
    // ----------------------------
    printf("[MAIN] Running...\n");

    uint32_t last_lcd_update = 0;

    while (!game_should_exit(&game)) {
        uint32_t t = now_ms();

        // Read inputs
        uint32_t sw_state = 0;
        uint32_t key_state_raw = 0;

        (void)switch_read_all(&sw, &sw_state);
        (void)button_read_all(&buttons, &key_state_raw);

#if KEY_ACTIVE_LOW
        // Normalize so pressed=1 (more intuitive for game logic)
        uint32_t key_state = (~key_state_raw) & 0xFu;
#else
        uint32_t key_state = key_state_raw & 0xFu;
#endif

        game_inputs_t in = {
            .switches = sw_state,
            .buttons_raw = key_state,
            .shake_detected = false  // accel later
        };

        // Update game
        game_update(&game, &fpga, &in, t);

        // If we just entered WATCH phase, generate & preload the sequence now
        // so it's ready when preview starts (safe and clean).
        // (This avoids burying sequence generation inside game.c if you prefer.)
        const game_outputs_t *out = game_get_outputs(&game);

        // Light LCD output (throttle to reduce flicker)
        if (lcd.initialized && (t - last_lcd_update) >= 50u) {
            last_lcd_update = t;

            char line1[81];
            char line2[81];

            // Line 1: State + Mode + BPM
            snprintf(line1, sizeof(line1),
                     "%s | Mode:%s | %ubpm",
                     state_str(out->state),
                     mode_str(out->mode),
                     out->bpm);

            // Line 2: Score + rating (score meaningful after results)
            uint32_t score = out->score_0_100;
            const char *rating = scoring_rating(score);
            snprintf(line2, sizeof(line2),
                     "Score:%3u/100  %s",
                     score, rating);

            lcd_write_line(&lcd, 0, line1);
            lcd_write_line(&lcd, 1, line2);
        }

        // Optional: preload sequence at start of WATCH (once per round)
        // We can detect WATCH entry by checking if state is WATCH and timer is near entry.
        // Easiest: if state == WATCH and (t - state_enter) < small window, but we didn't expose state_enter.
        // So for now, we’ll generate/load sequence in game.c when WATCH ends (already present).
        // We'll refactor cleanly once we add real scoring.

        usleep(LOOP_SLEEP_US);
    }

    // ----------------------------
    // Cleanup
    // ----------------------------
    printf("[MAIN] Exit requested.\n");

    fpga_if_stop(&fpga);

    switch_cleanup(&sw);
    button_cleanup(&buttons);

    if (lcd.initialized) {
        lcd_write_line(&lcd, 0, "Exited.");
        lcd_write_line(&lcd, 1, "");
        lcd_cleanup(&lcd);
    }

    hal_close(&hal);
    return 0;
}
