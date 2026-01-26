#include <stdio.h>
#include <stdint.h>
#include "../../includes/hal/hal-api.h"
#include "../../includes/peripherals/led.h"
#include "../../lib/address_map_arm.h"

// ------------------------------------------------------------
// Initialize LED driver
// ------------------------------------------------------------
int led_init(led_handle_t *led, hal_map_t *hal) {
    if (!led || !hal) return -1;
    led->hal = hal;
    return 0;
}

// ------------------------------------------------------------
// Write LED pattern (10 bits used on DE10-Standard)
// ------------------------------------------------------------
void led_write(led_handle_t *led, uint32_t value) {
    if (!led || !led->hal) return;
    hal_write32(led->hal, LEDR_BASE, value & 0x3FF);   // mask to 10 bits
}

// ------------------------------------------------------------
// Turn ALL LEDs OFF
// ------------------------------------------------------------
void led_clear(led_handle_t *led) {
    if (!led || !led->hal) return;
    hal_write32(led->hal, LEDR_BASE, 0x000);
}

// ------------------------------------------------------------
// Cleanup (no resources allocated, but left for symmetry)
// ------------------------------------------------------------
void led_cleanup(led_handle_t *led) {
    // nothing required
}
