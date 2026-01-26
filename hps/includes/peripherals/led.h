#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "../hal/hal-api.h"

// Handle for LED peripheral
typedef struct {
    hal_map_t *hal;   // Pointer to HAL memory map
} led_handle_t;

// Initialize LED driver
int  led_init(led_handle_t *led, hal_map_t *hal);

// Write a 10-bit LED pattern (LEDR9-0)
void led_write(led_handle_t *led, uint32_t value);

// Turn all LEDs OFF
void led_clear(led_handle_t *led);

// Cleanup (not strictly needed but kept for modularity)
void led_cleanup(led_handle_t *led);

#endif // LED_H