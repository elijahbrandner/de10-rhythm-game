// Standard I/O (not strictly needed here, but often used for debugging)
#include <stdio.h>

// Fixed-width integer types (uint32_t, etc)
#include <stdint.h>

// Button interface header
// Defines:
// - button_handle_t
// - BUTTON_COUNT
// - BUTTON_ALL_MASK
#include "../../includes/peripherals/button.h"

// HAL interface for memory-mapped I/O access
#include "../../includes/hal/hal-api.h"

// Address map (contains KEY_BASE register address)
#include "../../lib/address_map_arm.h"

// NOTE:
// KEY buttons on DE10 are active-low in hardware.
// Quartus top-level already inverts them (~KEY[3:0]),
// so the register reads here are active-high:
//   pressed  = 1
//   released = 0

// -----------------------------------------------------------------------------
// API IMPLEMENTATION
// -----------------------------------------------------------------------------
// Initialize the button interface
//
// Parameters:
// - 0 on success
// - -1 on failure
int button_init(button_handle_t *btn, hal_map_t *hal) {
    // Validate handle pointer
    if (!btn) return -1;

    // Store HAL reference in the handle
    btn->hal = hal;

    // Validate HAL pointer
    if (!hal) return -1;

    // Get virtual address for the KEY register using HAL
    //
    // KEY_BASE is the offset inside the lightweight bridge
    // hal_get_virtual_addr() returns a pointer to that register
    btn->reg = (volatile uint32_t *)hal_get_virtual_addr(hal, KEY_BASE);
    
    // If mapping failed, mark as uninitialized
    if (!btn->reg) {
        btn->initialized = 0;
        return -1;
    }
    
    // Mark as initialized
    btn->initialized = 1;
    return 0;
}

// Cleanup button interface
//
// This does not unmap memory
// It just clears the handle
int button_cleanup(button_handle_t *btn) {
    // Validate pointer
    if (!btn) return -1;

    // Clear register pointer
    btn->reg = NULL;

    // Mark as not initialized
    btn->initialized = 0;

    // Clear HAL reference 
    btn->hal = NULL;
    return 0;
}


// Read the state of ALL buttons at once
//
// Parameters:
// - btn: button handlers
// - state: output bitmask of button states
//
// Returns:
// - 0 on success
// - -1 on failure
// 
// Result: each bit represents one button
// bit 0 = key0
// bit 1 = key1
// bit 2 = key2
// bit 3 = key3
//
// Value:
// 1 = pressed
// 0 = released
int button_read_all(const button_handle_t *btn, uint32_t *state) {
    // Validate inputs and ensure driver is initialized
    if (!btn || !state || !btn->initialized) return -1;

    // Read raw 32-bit register value and mask only valid button bits
    *state = (*(btn->reg)) & BUTTON_ALL_MASK;
    return 0;
}

// Read a single button state
// Parameters:
// - btn: button handle
// - button_number: which button (0-3)
// - pressed: output (1 = pressed, 0 = released)
//
// Returns:
// - 0 on success
// - -1 on failure
int button_read(const button_handle_t *btn, int button_number, int *pressed) {
    // validate inputs
    if (!btn || !pressed || !btn->initialized) return -1;

    // Ensure requested button index is valid
    if (button_number < 0 || button_number >= BUTTON_COUNT) return -1;

    // Read all button states first
    uint32_t all = 0;
    if (button_read_all(btn, &all) != 0) return -1;

    // Extract the specific button bit using shift + mask
    // 
    // Example:
    // If button_number = 2
    // shift right by 2, then mask lowest bit
    *pressed = (int)((all >> button_number) & 0x1u);
    return 0;
}
