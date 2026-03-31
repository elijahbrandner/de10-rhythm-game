// Standard I/O library:
// Used for printf() to print debug/status messages
#include <stdio.h>

// Fixed-width integer types library
// Gives us types like uint32_t and uint64_t (predictable integer sizes)
#include <stdint.h>

// Boolean type library
#include <stdbool.h>

// Time library
// Used for struct timespec and clock_gettime()
#include <time.h>

// UNIX standard library, used for usleep() to pause loop briefly
#include <unistd.h>

// Hardware Abstraction Lyaer API
// Access to low level board/HPS-FPGA mapping functions
#include "../includes/hal/hal-api.h"

// Button driver
#include "../includes/peripherals/button.h"

// Switch peripheral driver
#include "../includes/peripherals/switch.h"

// LCD peripheral driver
#include "../includes/peripherals/lcd.h"

// Accerlerometer peripheral driver
#include "../includes/peripherals/accel.h"

// FPGA interface Layer
// Used to communicate game output/control data to the FPGA side
#include "../includes/fpga/fpga_if.h"

// Game logic layer
// Contains state machine, input/output structs, and game update functions
#include "../includes/game/game.h"

// ------------------------------------------------------------
// Config
// ------------------------------------------------------------

// Sleep time for main loop in microseconds (5 seconds)
// Prevents loop from running too fast and wasting cpu
#define LOOP_SLEEP_US 5000

// How often LCD should be refreshed in milliseconds
// Avoids rewriting LCD every single loop iteration
#define LCD_UPDATE_MS 50u

// ------------------------------------------------------------
// Time helper
// ------------------------------------------------------------

// Returns current monotimic time in milliseconds
// CLOCK_MONOTRONIC because it only moves forward and is not affected by 
// system clock changes. Makes it ideal for measuring elapsed time in games and embedded loops.
static uint32_t now_ms(void) {
    // Structure that will hold seconds + nanoseconds from the clock
    struct timespec ts;

    // Read monotronic clock into ts
    clock_gettime(CLOCK_MONOTONIC, &ts);

    // Convert seconds + nanoseconds into total milliseconds
    uint64_t ms = (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;

    // Return the result as a 32-bit unsigned integer
    return (uint32_t)ms;
}

// ------------------------------------------------------------
// Text helpers
// ------------------------------------------------------------

// Converts a game difficulty enum into a human-readable string
// Used for debug printing and LCD display
static const char *mode_str(game_mode_t m) {
    switch (m) {
        case GAME_MODE_EASY:   return "Easy";       // Easy
        case GAME_MODE_MEDIUM: return "Medium";     // Medium
        case GAME_MODE_HARD:   return "Hard";       // Hard
        case GAME_MODE_EXPERT: return "Expert";     // Expert
        default:               return "Unknown";    // Fallback if enum is invalid
    }
}

// Converts the selection mode enm into text
// This tells whether the player is using ladder mode or free select mode
static const char *select_mode_str(select_mode_t m) {
    switch (m) {
        case SELECT_LADDER: return "LADDER";     // Ladder progression mode
        case SELECT_FREE:   return "SELECT";    // Free difficulty selection mode
        default:            return "UNKNOWN";   // Fallback if enum is invalid
    }
}

// Converts the current game state enum into a readable string
// Useful for console debugging whenever state transitions occur
static const char *state_str(game_state_t s) {
    switch (s) {
        case ST_IDLE:      return "IDLE";       // Waiting for user/start
        case ST_WATCH:     return "WATCH";      // Short get-ready/watch phase
        case ST_PREVIEW:   return "PREVIEW";    // Pattern preview state
        case ST_GO:        return "GO";         // Waiting for start confirmation
        case ST_COUNTDOWN: return "COUNTDOWN";  // Countdown before playback begins
        case ST_PLAYBACK:  return "PLAYBACK";   // Main gameplay/input phase
        case ST_RESULTS:   return "RESULTS";    // Score / result display phase
        case ST_EXIT:      return "EXIT";       // Exit / Termination state
        default:           return "?";          // Fallback for unknown state
    }
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

// Main entry point of the program
int main(void) {
    // Create and zero-initialize the HAL context structure
    // This will hold the board memory mapping and related sources
    hal_map_t hal = {0};

    // Open the HAL mapping
    // If this fails, program cannot access needed hardware regions
    if (hal_open(&hal) != 0) {
        printf("[MAIN] ERROR: hal_open failed.\n");
        return 1;
    }

    // Create and zero-initialize the button driver handle
    button_handle_t buttons = {0};

    // Initialize button hardware access using the HAL context
    if (button_init(&buttons, &hal) != 0) {
        printf("[MAIN] ERROR: button_init failed.\n");

        // Clean up HAL before exiting because it was already opened
        hal_close(&hal);
        return 1;
    }

    // Create and zero initialize switch driver handle
    switch_handle_t switches = {0};

    // Initialize switch hardware access
    if (switch_init(&switches, &hal) != 0) {
        printf("[MAIN] ERROR: switch_init failed.\n");

        // Clean up any previously initialized resources before exiting
        button_cleanup(&buttons);
        hal_close(&hal);
        return 1;
    }

    // Create and zero initialize LCD driver handle
    lcd_handle_t lcd = {0};

    // Try to initialize the LCD
    // We store whether it succeeded because LCD failure is treated as non-fatal
    bool lcd_ok = (lcd_init(&lcd) == 0);

    // If the LCD failed, continue running without it
    if (!lcd_ok) {
        printf("[MAIN] WARNING: lcd_init failed. Continuing without LCD.\n");
    } else {
        // If lCD initializations worked, turn on the backlight
        lcd_backlight(&lcd, true);
        
        // Clear any previous LCD contents
        lcd_clear(&lcd);

        // Write boot message line 1
        lcd_write_text(&lcd, 0, 0,  "Rhythm Game");

        // Write boot message line 2
        lcd_write_text(&lcd, 0, 16, "Booting...");

        // Push the buffered text to the display
        lcd_refresh(&lcd);
    }

    // Create and zero-initialize the accelerometer handle
    accel_handle_t accel = {0};

    // try to initialize the accelerometer
    // Non fatal; game can continue without shake input
    bool accel_ok = (accel_init(&accel, &hal) == 0);

    // If accelerometery init fails, warn the user and continue
    if (!accel_ok) {
        printf("[MAIN] WARNING: accel_init failed. Continuing without shake input.\n");
        // If LCD is available, show shake input is unavailable
        if (lcd_ok) {
            lcd_clear(&lcd);
            lcd_write_text(&lcd, 0, 0,  "Rhythm Game");
            lcd_write_text(&lcd, 0, 16, "Accel unavailable");
            lcd_refresh(&lcd);
        }
    }

    // Create and zero-initialize the FPGA interface handle
    fpga_if_t fpga = {0};

    // Initialize communication with FPGA-side control/output registers
    // Required for the game, failure is fatal
    if (fpga_if_init(&fpga, &hal) != 0) {
        printf("[MAIN] ERROR: fpga_if_init failed.\n");

        // Cleanup accelerometer if it had init successfully
        if (accel_ok) accel_cleanup(&accel);

        // If LCD is available, show an error before shutting down
        if (lcd_ok) {
            lcd_clear(&lcd);
            lcd_write_text(&lcd, 0, 0,  "ERROR");
            lcd_write_text(&lcd, 0, 16, "FPGA init failed");
            lcd_refresh(&lcd);
            lcd_cleanup(&lcd);
        }
        
        // Clean up all earlier resources
        switch_cleanup(&switches);
        button_cleanup(&buttons);
        hal_close(&hal);
        return 1;
    }

    // Declare the main game state structure
    game_t game;

    // Init the game state machine and internal values
    game_init(&game);

    // Print console confirmation that startup succeeded
    printf("[MAIN] System initialized.\n");
    printf("[MAIN] Entering main loop.\n");

    // Track last LCD update time so we can throttle LCD refreshes
    uint32_t last_lcd_update = 0;

    // Track the previous game state so we only print transitions once
    game_state_t last_state = ST_IDLE;

    // Main program loop:
    // Runs until the game logic says the app should exit
    while (!game_should_exit(&game)) {
        // Get the current time in milliseconds for this frame/update
        uint32_t t = now_ms();

        // Variable to hold all switch bits read from hardware
        uint32_t switch_state = 0;

        // Read all switches
        // If read fails, fall back to 0 so game has safe input
        if (switch_read_all(&switches, &switch_state) != 0) {
            switch_state = 0;
        }

        // Variable to hold all button bits read from hardware
        uint32_t button_state = 0;
        // Read all Buttons
        // If the read fails, fall back to 0
        if (button_read_all(&buttons, &button_state) != 0) {
            button_state = 0;
        }

        // Integer flag for shake detection
        // Using int because that is what the accelerometer api expects
        int shake_detected_i = 0;
        // Only poll the accelerometer driver whether a shake was detected
        // If polling fails, treat it as a "no shake"
        if (accel_ok) {
            if (accel_poll_shake(&accel, &shake_detected_i) != 0) {
                shake_detected_i = 0;
            }
        }

        // Built the input structure that will be passed into game_update()
        game_inputs_t in = {
            .switches = switch_state,                       // Raw switch bit values
            .buttons_raw = button_state,                    // Raw button bit values
            .shake_detected = (shake_detected_i != 0)       // Convert int to bool
        };

        // Run one update step of the game state machine
        // Processes input, updates logic, writes new outputs to FPGA
        game_update(&game, &fpga, &in, t);

        // Retrieve the current game outputs after the update
        const game_outputs_t *out = game_get_outputs(&game);

        // If no outputs are available, sleep briefly and continue
        // This is a defensive check in case the game layer returns NULL
        if (!out) {
            usleep(LOOP_SLEEP_US);
            continue;
        }

        // Only print to the console when the state changes
        if (out->state != last_state) {
            // Update our remembered previous state
            last_state = out->state;

            // Print a helpful debug/status line showing the new state and core info
            printf("[MAIN] State -> %s | Mode=%s | BPM=%u | SeqLen=%u | Score=%u\n",
                   state_str(out->state),       // Current state as a string
                   mode_str(out->mode),         // Current difficulty as a string
                   out->bpm,                    // Current BPM
                   out->seq_len,                // Current sequence length
                   out->score_0_100);           // Current score
        }

        // Update the LCD only if:
        // 1. LCD exists
        // 2. Enough time has passed since the last LCD refresh
        if (lcd_ok && (t - last_lcd_update) >= LCD_UPDATE_MS) {
            // Record the time of this LCD update
            last_lcd_update = t;

            // Temporary text buffers for the 4 LCD lines
            char line1[32];
            char line2[32];
            char line3[32];
            char line4[32];

            // If the game is currently showing results
            // then display current round results on the LCD
            if (out->state == ST_RESULTS) {
                snprintf(line1, sizeof(line1), "Mode: %s", select_mode_str(game.select_mode));
                snprintf(line2, sizeof(line2), "Diff: %s", mode_str(out->mode));
                snprintf(line3, sizeof(line3), "Score: %3u", out->score_0_100);
                snprintf(line4, sizeof(line4), "%s",
                        (out->rating_text ? out->rating_text : ""));
            // If the game is idle and a round was completed previously
            // preserve and display the last score/rating
            } else if (out->state == ST_IDLE && game.completed) {
                snprintf(line1, sizeof(line1), "Mode: %s", select_mode_str(game.select_mode));
                snprintf(line2, sizeof(line2), "Diff: %s", mode_str(out->mode));
                snprintf(line3, sizeof(line3), "Last: %3u", game.last_score);
                snprintf(line4, sizeof(line4), "%s",
                        (game.last_rating_text ? game.last_rating_text : ""));
            // Otherwise, show general live game information
            } else {
                snprintf(line1, sizeof(line1), "Mode: %s", select_mode_str(game.select_mode));
                snprintf(line2, sizeof(line2), "Diff: %s", mode_str(out->mode));
                snprintf(line3, sizeof(line3), "BPM: %u", out->bpm);
                snprintf(line4, sizeof(line4), "%s",
                        (out->line2 ? out->line2 : ""));
            }

            // Clear the LCD before writing the new frame of text
            lcd_clear(&lcd);

            // Write the four lines to four vertical positions
            lcd_write_text(&lcd, 0, 0,  line1);     // First row
            lcd_write_text(&lcd, 0, 16, line2);     // Second row
            lcd_write_text(&lcd, 0, 32, line3);     // Third row
            lcd_write_text(&lcd, 0, 48, line4);     // Fourth row

            // Push the updated text to the LCD hardware
            lcd_refresh(&lcd);
        }

        // Pause briefly so the main loop does not spin too aggressively
        usleep(LOOP_SLEEP_US);
    }

    // The game requested exit, so announce shutdown to console
    printf("[MAIN] Exit requested.\n");

    // Clear FPGA outputs so LEDs/HEX/etc do not remain in a stale state
    fpga_if_clear(&fpga);

    // Clean up FPGA interface resources
    fpga_if_cleanup(&fpga);

    // If LCD is available, show an exit message and turn off backlight
    if (lcd_ok) {
        lcd_clear(&lcd);
        lcd_write_text(&lcd, 0, 0, "Exited.");
        lcd_refresh(&lcd);
        lcd_backlight(&lcd, false);
    }

    // Clean up accelerometer only if init successfully
    if (accel_ok) accel_cleanup(&accel);

    // Clean up LCD only if it init successfully
    if (lcd_ok) lcd_cleanup(&lcd);

    // Clean up switches driver
    switch_cleanup(&switches);

    // Clean up button driver
    button_cleanup(&buttons);

    // Close HAL mapping and release low-level hardwrae resources
    hal_close(&hal);

    return 0;
}