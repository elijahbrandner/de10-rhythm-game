#include <stdio.h>
#include <stdint.h>
#include "../../includes/peripherals/button.h"
#include "../../includes/hal/hal-api.h"
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
int button_init(button_handle_t *btn, hal_map_t *hal) {
    if (!btn) return -1;
    btn->hal = hal;


    if (!hal) return -1;
    btn->reg = (volatile uint32_t *)hal_get_virtual_addr(hal, KEY_BASE);
    if (!btn->reg) {
        btn->initialized = 0;
        return -1;
    }
    btn->initialized = 1;
    return 0;
}

int button_cleanup(button_handle_t *btn) {
    if (!btn) return -1;
    btn->reg = NULL;
    btn->initialized = 0;
    btn->hal = NULL;
    return 0;
}

int button_read_all(const button_handle_t *btn, uint32_t *state) {
    if (!btn || !state || !btn->initialized) return -1;
    *state = (*(btn->reg)) & BUTTON_ALL_MASK;
    return 0;
}

int button_read(const button_handle_t *btn, int button_number, int *pressed) {
    if (!btn || !pressed || !btn->initialized) return -1;
    if (button_number < 0 || button_number >= BUTTON_COUNT) return -1;

    uint32_t all = 0;
    if (button_read_all(btn, &all) != 0) return -1;

    *pressed = (int)((all >> button_number) & 0x1u);
    return 0;
}
