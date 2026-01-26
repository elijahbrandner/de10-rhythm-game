#ifndef SWITCH_H
#define SWITCH_H

#include <stdint.h>
#include "../hal/hal-api.h" // for hal_map_t

// --------------------------------------------------------------------
// DE10-Standard Switch Map
// --------------------------------------------------------------------
//  SW[6:0] -> binary numeric input (0-127; clamped to 0-99)
//  SW[8:7] -> unused (reserved)
//  SW[9]   -> mode select (0 = countdown, 1 = stopwatch)
// --------------------------------------------------------------------

// Bit masks
#define SWITCH_INPUT_MASK   0x7Fu       //bits 6:0 (numeric input)
#define SWITCH_MODE_MASK    (1u << 9)   // bit 9 (mode select)
#define SWITCH_ALL_MASK     0x3FFu      // bits 9:0 (all switches)

// Optional helper for readability
typedef enum {
    SWITCH_MODE_COUNTDOWN = 0,
    SWITCH_MODE_STOPWATCH = 1
} switch_mode_t;

// --------------------------------------------------------------------
// Switch Handle Structure
// --------------------------------------------------------------------
typedef struct {
    hal_map_t *hal;             // Reference to the HAL map 
    volatile uint32_t *reg;     // Virtual register address (from HAL)
    int initialized;            // 1 = ready, 0 = not initialized
} switch_handle_t;

// --------------------------------------------------------------------
// Core API
// --------------------------------------------------------------------

// Initialize the switch handle
// In hardware mode: Resolves address using hal_get_virtual_addr()
// In simulation mode: sets reg = NULL and marks as initialized
// Returns 0 on success, -1 on failure
int switch_init(switch_handle_t *sw, hal_map_t *hal);

// Cleanup the swtich handle (currently a no-op but for completeness)
int switch_cleanup(switch_handle_t *sw);

// Read the entire 10-bit switch value into *switch_state
// Returns 0 on success, -1 on failure
int switch_read_all(const switch_handle_t *sw, uint32_t *switch_state);

// Read a specific switch bit (0-9)
// Returns 0 or 1 in *state
int switch_read(const switch_handle_t *sw, int switch_number, int *state);

// --------------------------------------------------------------------
// Milestone 3 / Hardware-Ready Helpers
// --------------------------------------------------------------------

// Reads binary input from SW[6:0] and clamps to 0-99
int switch_read_input_value(const switch_handle_t *sw);

// Reads mode bit from SW9 (0 = Countdown, 1 = Stopwatch)
int switch_read_mode(const switch_handle_t *sw);

// Reads any individual bit value (for convenience)
int switch_read_bit(const switch_handle_t *sw, int bit_index);

#endif // SWITCH_H