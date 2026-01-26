#include "../../includes/hal/hal-api.h"
#include "../../includes/peripherals/switch.h"
#include "../../lib/address_map_arm.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// --------------------------------------------------------------------
// API
// --------------------------------------------------------------------

// Global HAL mapping - shared across switch operations
int switch_init(switch_handle_t *sw, hal_map_t *hal) {
    if (!sw) return -1;
    sw->hal = hal;
    if (!hal) return -1;

    sw->reg = (volatile uint32_t *) hal_get_virtual_addr(hal, SW_BASE);
    if (!sw->reg) {
        sw->initialized = 0;
        return -1;
    }
    sw->initialized = 1;
    return 0;
}

int switch_cleanup(switch_handle_t *sw) {
    if (!sw || !sw->initialized) return -1;
    sw->reg = NULL;
    sw->initialized = 0;
    sw->hal = NULL;
    return 0;
}

int switch_read_all(const switch_handle_t *sw, uint32_t *state) {
    if (!sw || !state || !sw->initialized) return -1;
    *state = (*(sw->reg)) & SWITCH_ALL_MASK;
    return 0;
}


int switch_read(const switch_handle_t *sw, int switch_number, int *bit_state) {
    if (!sw || !bit_state || !sw->initialized) return -1;
    if (switch_number < 0 || switch_number > 9) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    *bit_state = (int)((all >> switch_number) & 0x1u);
    return 0;
}

int switch_read_input_value(const switch_handle_t *sw) {
    if (!sw || !sw->initialized) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    // SW[6:0] -> numeric input (0..127), then clamp 0..99)
    int value = (int)(all & SWITCH_INPUT_MASK);
    if (value > 99) value = 99;
    return value;
}

int switch_read_mode(const switch_handle_t *sw) {
    if (!sw || !sw->initialized) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    // SW9 -> mode (0 = countdown, 1 = stopwatch)
    return ((all & SWITCH_MODE_MASK) ? SWITCH_MODE_STOPWATCH : SWITCH_MODE_COUNTDOWN);
}

int switch_read_bit(const switch_handle_t *sw, int bit_index) {
    if (!sw || !sw->initialized) return -1;
    if (bit_index < 0 || bit_index > 31) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    return (int)((all >> bit_index) & 0x1u);
}