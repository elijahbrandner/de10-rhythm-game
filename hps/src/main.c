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
// Text helpers
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

static const char *select_mode_str(select_mode_t m) {
    switch (m) {
        case SELECT_LADDER: return "LADDER";
        case SELECT_FREE:   return "SELECT";
        default:            return "UNKNOWN";
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
    hal_map_t hal = {0};
    if (hal_open(&hal) != 0) {
        printf("[MAIN] ERROR: hal_open failed.\n");
        return 1;
    }

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
    bool lcd_ok = (lcd_init(&lcd) == 0);
    if (!lcd_ok) {
        printf("[MAIN] WARNING: lcd_init failed. Continuing without LCD.\n");
    } else {
        lcd_backlight(&lcd, true);
        lcd_clear(&lcd);
        lcd_write_text(&lcd, 0, 0,  "Rhythm Game");
        lcd_write_text(&lcd, 0, 16, "Booting...");
        lcd_refresh(&lcd);
    }

    accel_handle_t accel = {0};
    bool accel_ok = (accel_init(&accel, &hal) == 0);
    if (!accel_ok) {
        printf("[MAIN] WARNING: accel_init failed. Continuing without shake input.\n");
        if (lcd_ok) {
            lcd_clear(&lcd);
            lcd_write_text(&lcd, 0, 0,  "Rhythm Game");
            lcd_write_text(&lcd, 0, 16, "Accel unavailable");
            lcd_refresh(&lcd);
        }
    }

    fpga_if_t fpga = {0};
    if (fpga_if_init(&fpga, &hal) != 0) {
        printf("[MAIN] ERROR: fpga_if_init failed.\n");

        if (accel_ok) accel_cleanup(&accel);
        if (lcd_ok) {
            lcd_clear(&lcd);
            lcd_write_text(&lcd, 0, 0,  "ERROR");
            lcd_write_text(&lcd, 0, 16, "FPGA init failed");
            lcd_refresh(&lcd);
            lcd_cleanup(&lcd);
        }

        switch_cleanup(&switches);
        button_cleanup(&buttons);
        hal_close(&hal);
        return 1;
    }

    game_t game;
    game_init(&game);

    printf("[MAIN] System initialized.\n");
    printf("[MAIN] Entering main loop.\n");

    uint32_t last_lcd_update = 0;
    game_state_t last_state = ST_IDLE;

    while (!game_should_exit(&game)) {
        uint32_t t = now_ms();

        uint32_t switch_state = 0;
        if (switch_read_all(&switches, &switch_state) != 0) {
            switch_state = 0;
        }

        uint32_t button_state = 0;
        if (button_read_all(&buttons, &button_state) != 0) {
            button_state = 0;
        }

        int shake_detected_i = 0;
        if (accel_ok) {
            if (accel_poll_shake(&accel, &shake_detected_i) != 0) {
                shake_detected_i = 0;
            }
        }

        game_inputs_t in = {
            .switches = switch_state,
            .buttons_raw = button_state,
            .shake_detected = (shake_detected_i != 0)
        };

        game_update(&game, &fpga, &in, t);

        const game_outputs_t *out = game_get_outputs(&game);
        if (!out) {
            usleep(LOOP_SLEEP_US);
            continue;
        }

        if (out->state != last_state) {
            last_state = out->state;
            printf("[MAIN] State -> %s | Mode=%s | BPM=%u | SeqLen=%u | Score=%u\n",
                   state_str(out->state),
                   mode_str(out->mode),
                   out->bpm,
                   out->seq_len,
                   out->score_0_100);
        }

        if (lcd_ok && (t - last_lcd_update) >= LCD_UPDATE_MS) {
            last_lcd_update = t;

            char line1[32];
            char line2[32];
            char line3[32];
            char line4[32];

            if (out->state == ST_RESULTS) {
                snprintf(line1, sizeof(line1), "Mode: %s", select_mode_str(game.select_mode));
                snprintf(line2, sizeof(line2), "Diff: %s", mode_str(out->mode));
                snprintf(line3, sizeof(line3), "Score: %3u", out->score_0_100);
                snprintf(line4, sizeof(line4), "%s",
                        (out->rating_text ? out->rating_text : ""));
            } else if (out->state == ST_IDLE && game.completed) {
                snprintf(line1, sizeof(line1), "Mode: %s", select_mode_str(game.select_mode));
                snprintf(line2, sizeof(line2), "Diff: %s", mode_str(out->mode));
                snprintf(line3, sizeof(line3), "Last: %3u", game.last_score);
                snprintf(line4, sizeof(line4), "%s",
                        (game.last_rating_text ? game.last_rating_text : ""));
            } else {
                snprintf(line1, sizeof(line1), "Mode: %s", select_mode_str(game.select_mode));
                snprintf(line2, sizeof(line2), "Diff: %s", mode_str(out->mode));
                snprintf(line3, sizeof(line3), "BPM: %u", out->bpm);
                snprintf(line4, sizeof(line4), "%s",
                        (out->line2 ? out->line2 : ""));
            }

            lcd_clear(&lcd);
            lcd_write_text(&lcd, 0, 0,  line1);
            lcd_write_text(&lcd, 0, 16, line2);
            lcd_write_text(&lcd, 0, 32, line3);
            lcd_write_text(&lcd, 0, 48, line4);
            lcd_refresh(&lcd);
        }

        usleep(LOOP_SLEEP_US);
    }

    printf("[MAIN] Exit requested.\n");

    fpga_if_clear(&fpga);
    fpga_if_cleanup(&fpga);

    if (lcd_ok) {
        lcd_clear(&lcd);
        lcd_write_text(&lcd, 0, 0, "Exited.");
        lcd_refresh(&lcd);
        lcd_backlight(&lcd, false);
    }

    if (accel_ok) accel_cleanup(&accel);
    if (lcd_ok) lcd_cleanup(&lcd);
    switch_cleanup(&switches);
    button_cleanup(&buttons);
    hal_close(&hal);

    return 0;
}