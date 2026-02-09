#include "../../includes/peripherals/switch.h"
#include "../../includes/hal/hal-api.h"
#include "../../lib/address_map_arm.h"
#include <stdint.h>

int switch_init(switch_handle_t *sw, hal_map_t *hal) {
    if (!sw || !hal) return -1;

    sw->hal = hal;
    sw->reg = (volatile uint32_t *) hal_get_virtual_addr(hal, SW_BASE);
    if (!sw->reg) {
        sw->initialized = 0;
        return -1;
    }

    sw->initialized = 1;
    return 0;
}

int switch_cleanup(switch_handle_t *sw) {
    if (!sw) return -1;
    sw->reg = NULL;
    sw->initialized = 0;
    sw->hal = NULL;
    return 0;
}

int switch_read_all(const switch_handle_t *sw, uint32_t *switch_state) {
    if (!sw || !switch_state || !sw->initialized) return -1;
    *switch_state = (*(sw->reg)) & SWITCH_ALL_MASK;
    return 0;
}

int switch_read_bit(const switch_handle_t *sw, int bit_index, int *bit_state) {
    if (!sw || !bit_state || !sw->initialized) return -1;
    if (bit_index < 0 || bit_index > 9) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    *bit_state = (int)((all >> bit_index) & 0x1u);
    return 0;
}

int switch_read_select_mode(const switch_handle_t *sw, select_mode_t *mode_out) {
    if (!sw || !mode_out || !sw->initialized) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    *mode_out = (all & SWITCH_FREESEL_MASK) ? SELECT_FREE : SELECT_LADDER;
    return 0;
}

int switch_read_game_mode(const switch_handle_t *sw, game_mode_t *mode_out) {
    if (!sw || !mode_out || !sw->initialized) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    // Ladder mode (SW9=0): always start at Easy; game advances internally
    if ((all & SWITCH_FREESEL_MASK) == 0) {
        *mode_out = GAME_MODE_EASY;
        return 0;
    }

    // Free select mapping:
    // 00 -> Easy
    // 10 -> Medium
    // 01 -> Hard
    // 11 -> Expert
    uint32_t sel = all & SWITCH_DIFFICULTY_MASK;

    switch (sel) {
        case 0x0: *mode_out = GAME_MODE_EASY; break;
        case 0x2: *mode_out = GAME_MODE_MEDIUM; break;
        case 0x1: *mode_out = GAME_MODE_HARD; break;
        case 0x3: *mode_out = GAME_MODE_EXPERT; break;
        default:  *mode_out = GAME_MODE_EASY; break;
    }

    return 0;
}

int switch_all_on(const switch_handle_t *sw, int *all_on_out) {
    if (!sw || !all_on_out || !sw->initialized) return -1;

    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    *all_on_out = ((all & SWITCH_ALL_MASK) == SWITCH_ALL_MASK) ? 1 : 0;
    return 0;
}
