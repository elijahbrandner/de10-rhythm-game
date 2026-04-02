// Include the switch peripheral interface header
// This defines:
// - switch_handle_t
// -switch-related masks
// - select_mode_t
// - game_mode_t
#include "../../includes/peripherals/switch.h"

// Include HAL interface for memory-mapped register access
#include "../../includes/hal/hal-api.h"

// Include base address definitions such as SW_BASE
#include "../../lib/address_map_arm.h"

// Fixed width integer support for uint32_t
#include <stdint.h>

// In ladder mode (SW9 = 0), switch helper returns Easy as the
// starting/default mode. Actual ladder progression is handled by game.c.

// Initialize the switch driver
// 
// Parameters:
// - sw: pointer to switch handle structure
// - hal: pointer to HAL mapping context
//
// Returns: 0 on success, -1 on failure
// What this does:
// 1. stores the HAL pointer
// 2. resolves the virtual address of the switch register
// 3. marks the handle initialized if successful
int switch_init(switch_handle_t *sw, hal_map_t *hal) {
    // Validate required poitners
    if (!sw || !hal) return -1;

    // Store HAL reference in the switch handle
    sw->hal = hal;

    // Resolve the virtual address for the switch register
    // SW_BASE is the offset of the switch register within the LW bridge
    sw->reg = (volatile uint32_t *) hal_get_virtual_addr(hal, SW_BASE);

    // If the register pointer could not be obtained, fail initialization
    if (!sw->reg) {
        sw->initialized = 0;
        return -1;
    }

    // Mark the handle ready for use
    sw->initialized = 1;
    return 0;
}

// Clean up switch driver
//
// THis only clears the handle state
// It does not unmap memory directly because that is HAL's job
int switch_cleanup(switch_handle_t *sw) {
    // Validate pointer
    if (!sw) return -1;

    // Clear register pointer
    sw->reg = NULL;
    // Mark handle as uninitialized
    sw->initialized = 0;
    // Clear HAL reference
    sw->hal = NULL;
    return 0;
}

// Read the full switch state register
//
// Parameters:
// - sw: pointer to switch handle
// - switch_state: output bitmask of switch values
//
// returns:
// - 0 on success, -1 on failure
//
// Result:
// Each bit represents one hardware switch
// This masks the result so only valid switvh bits are returned
int switch_read_all(const switch_handle_t *sw, uint32_t *switch_state) {
    // Validate pointers and make sure the driver is initialized
    if (!sw || !switch_state || !sw->initialized) return -1;

    // Read the switch register and keep only valid switch bits
    *switch_state = (*(sw->reg)) & SWITCH_ALL_MASK;
    return 0;
}


// Read one specific switch bit
// 
// Parameters:
// - sw: pointer to switch handle
// - bit_index: switch number to read (0 through 9)
// - bit_state: output value (1 = on, 0 = off)
//
// Returns:
// 0 on success, -1 failure
int switch_read_bit(const switch_handle_t *sw, int bit_index, int *bit_state) {
    // Validate inputs and initialized state
    if (!sw || !bit_state || !sw->initialized) return -1;
    
    // Only SW0 through SW9 are valid
    if (bit_index < 0 || bit_index > 9) return -1;

    // Read the full switch bitmaks first 
    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    // Extract the requested bit using shift + mask
    *bit_state = (int)((all >> bit_index) & 0x1u);
    return 0;
}

// Read the current selection mode from switches
//
// Parameters:
// - sw: pointer to switch handle
// - mode_out: output selection mode
//
// Returns:
// 0 on success, -1 on failure
// 
// Behavior:
// SW9 determines whether the game uses:
// - ladder mode
// - free select mode
int switch_read_select_mode(const switch_handle_t *sw, select_mode_t *mode_out) {
    // validate inputs
    if (!sw || !mode_out || !sw->initialized) return -1;

    // read all switch bits
    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    // If the free-select mask bit is set, use free mode
    // Otherwise use ladder mode
    *mode_out = (all & SWITCH_FREESEL_MASK) ? SELECT_FREE : SELECT_LADDER;
    return 0;
}

// Read the desired game difficulty mode from switches
//
// Parameters:
// - sw: pointer to switch handle
// - mode_out: output difficulty mode
//
// Returns:
// 0 on success, -1 on failure
//
// In ladder mode, this function always returns Easy
// The actual automatic progression through difficulties is handled in game.c
//
// In free select mode, the lower switch bit map to difficulty
int switch_read_game_mode(const switch_handle_t *sw, game_mode_t *mode_out) {
    // Validate inputs
    if (!sw || !mode_out || !sw->initialized) return -1;

    // Read all switch bits
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
    //
    // ONly the difficulty selection bits are kept
    uint32_t sel = all & SWITCH_DIFFICULTY_MASK;

    // Decode difficulty from the selected switch combination
    switch (sel) {
        case 0x0: *mode_out = GAME_MODE_EASY; break;
        case 0x2: *mode_out = GAME_MODE_MEDIUM; break;
        case 0x1: *mode_out = GAME_MODE_HARD; break;
        case 0x3: *mode_out = GAME_MODE_EXPERT; break;
        default:  *mode_out = GAME_MODE_EASY; break;
    }

    return 0;
}

// Check whether all switches are ON.
//
// Parameters:
// - sw: pointer to switch handle
// - all_on_out: output integer (1 if all switches are on, else 0)
//
// Returns:
// - 0 on success
// - -1 on failure
//
// This is useful for special commands like your exit combo.
int switch_all_on(const switch_handle_t *sw, int *all_on_out) {
    // Validate inputs
    if (!sw || !all_on_out || !sw->initialized) return -1;

    // Read all switch bits
    uint32_t all = 0;
    if (switch_read_all(sw, &all) != 0) return -1;

    // Compare masked switch state against the mask representing all switches on
    *all_on_out = ((all & SWITCH_ALL_MASK) == SWITCH_ALL_MASK) ? 1 : 0;
    return 0;
}
