#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#include "../includes/hal/hal-api.h"

#include "../includes/peripherals/button.h"
#include "../includes/peripherals/switch.h"
#include "../includes/peripherals/lcd.h"
#include "../includes/peripherals/accel.h"

#include "../includes/fpga/fpga_if.h"

#include "../includes/game/game.h"

// ------------------------------------------------------------
// Config
// ------------------------------------------------------------
#define LOOP_SLEEP_US 5000
#define LCD_UPDATE_MS 50u

// ------------------------------------------------------------
// Time helper
// ------------------------------------------------------------
static uint32_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
    return (uint32_t)ms;
}

// ------------------------------------------------------------
// Optional text helpers for console/debug
// ------------------------------------------------------------
static const char *mode_str(game_mode_t m) {
    switch (m) {
        case GAME_MODE_EASY:   return "Easy";
        case GAME_MODE_MEDIUM: return "Medium";
        case GAME_MODE_HARD:   return "Hard";
        case GAME_MODE_EXPERT: return "Expert";
        default:               return "Unknown";
    }
}

static const char *state_str(game_state_t s) {
    switch (s) {
        case ST_IDLE:     return "IDLE";
        case ST_WATCH:    return "WATCH";
        case ST_PREVIEW:  return "PREVIEW";
        case ST_GO:       return "GO";
        case ST_PLAYBACK: return "PLAYBACK";
        case ST_RESULTS:  return "RESULTS";
        case ST_EXIT:     return "EXIT";
        default:          return "?";
    }
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main(void) {
    // ----------------------------
    // HAL
    // ----------------------------
    hal_map_t hal = {0};
    if (hal_open(&hal) != 0) {
        printf("[MAIN] ERROR: hal_open failed.\n");
        return 1;
    }

    // ----------------------------
    // Peripherals
    // ----------------------------
    button_handle_t buttons = {0};
    if (button_init(&buttons, &hal) != 0) {
        printf("[MAIN] ERROR: button_init failed.\n");
        hal_close(&hal);
        return 1;
    }

    switch_handle_t switches = {0};
    if (switch_init(&switches, &hal) != 0) {
        printf("[MAIN] ERROR: switch_init failed.\n");
        button_cleanup(&buttons);
        hal_close(&hal);
        return 1;
    }

    lcd_handle_t lcd = {0};
    bool lcd_ok = (lcd_init(&lcd, &hal) == 0);
    if (!lcd_ok) {
        printf("[MAIN] WARNING: lcd_init failed. Continuing without LCD.\n");
    } else {
        lcd_clear(&lcd);
        lcd_write_line(&lcd, 0, "Rhythm-Based Timing Game");
        lcd_write_line(&lcd, 1, "Booting...");
    }

    accel_handle_t accel = {0};
    bool accel_ok = (accel_init(&accel, &hal) == 0);
    if (!accel_ok) {
        printf("[MAIN] WARNING: accel_init failed. Continuing without shake input.\n");
        if (lcd_ok) {
            lcd_write_line(&lcd, 1, "Accel unavailable");
        }
    }

    // ----------------------------
    // FPGA interface
    // ----------------------------
    fpga_if_t fpga = {0};
    if (fpga_if_init(&fpga, &hal) != 0) {
        printf("[MAIN] ERROR: fpga_if_init failed.\n");

        if (accel_ok) accel_cleanup(&accel);
        if (lcd_ok) {
            lcd_write_line(&lcd, 0, "ERROR");
            lcd_write_line(&lcd, 1, "FPGA IF init failed");
            lcd_cleanup(&lcd);
        }

        switch_cleanup(&switches);
        button_cleanup(&buttons);
        hal_close(&hal);
        return 1;
    }

    // ----------------------------
    // Game
    // ----------------------------
    game_t game;
    game_init(&game);

    printf("[MAIN] System initialized.\n");
    printf("[MAIN] Entering main loop.\n");

    uint32_t last_lcd_update = 0;
    game_state_t last_state = ST_IDLE;

    // ----------------------------
    // Main loop
    // ----------------------------
    while (!game_should_exit(&game)) {
        uint32_t t = now_ms();

        // Read switches
        uint32_t switch_state = 0;
        if (switch_read_all(&switches, &switch_state) != 0) {
            switch_state = 0;
        }

        // Read buttons
        uint32_t button_state = 0;
        if (button_read_all(&buttons, &button_state) != 0) {
            button_state = 0;
        }

        // Read accel shake flag
        int shake_detected_i = 0;
        if (accel_ok) {
            if (accel_poll_shake(&accel, &shake_detected_i) != 0) {
                shake_detected_i = 0;
            }
        }

        // Build game input snapshot
        game_inputs_t in = {
            .switches = switch_state,
            .buttons_raw = button_state,          // already normalized: pressed = 1
            .shake_detected = (shake_detected_i != 0)
        };

        // Update game
        game_update(&game, &fpga, &in, t);

        const game_outputs_t *out = game_get_outputs(&game);
        if (!out) {
            usleep(LOOP_SLEEP_US);
            continue;
        }

        // Console state transition debug
        if (out->state != last_state) {
            last_state = out->state;
            printf("[MAIN] State -> %s | Mode=%s | BPM=%u | SeqLen=%u | Score=%u\n",
                   state_str(out->state),
                   mode_str(out->mode),
                   out->bpm,
                   out->seq_len,
                   out->score_0_100);
        }

        // LCD update
        if (lcd_ok && (t - last_lcd_update) >= LCD_UPDATE_MS) {
            last_lcd_update = t;

            char line1[81];
            char line2[81];

            // Prefer game-provided UI text when available
            if (out->state == ST_RESULTS) {
                snprintf(line1, sizeof(line1),
                         "Score:%3u  %s",
                         out->score_0_100,
                         (out->rating_text ? out->rating_text : ""));

                snprintf(line2, sizeof(line2),
                         "%s",
                         (out->line2 ? out->line2 : ""));
            } else {
                snprintf(line1, sizeof(line1),
                         "%s | %s | %ubpm",
                         (out->line1 ? out->line1 : state_str(out->state)),
                         mode_str(out->mode),
                         out->bpm);

                snprintf(line2, sizeof(line2),
                         "%s",
                         (out->line2 ? out->line2 : ""));
            }

            lcd_write_line(&lcd, 0, line1);
            lcd_write_line(&lcd, 1, line2);
        }

        usleep(LOOP_SLEEP_US);
    }

    // ----------------------------
    // Shutdown
    // ----------------------------
    printf("[MAIN] Exit requested.\n");

    fpga_if_stop(&fpga);

    if (lcd_ok) {
        lcd_write_line(&lcd, 0, "Exited.");
        lcd_write_line(&lcd, 1, "");
    }

    if (accel_ok) accel_cleanup(&accel);
    if (lcd_ok) lcd_cleanup(&lcd);
    switch_cleanup(&switches);
    button_cleanup(&buttons);
    hal_close(&hal);

    return 0;
}