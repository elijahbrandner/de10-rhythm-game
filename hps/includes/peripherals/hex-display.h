#ifndef HEX_DISPLAY_H
#define HEX_DISPLAY_H

#include <stdint.h>
#include "../hal/hal-api.h"

// -----------------------------------------------------------------------------
// HEX Display Driver (Digit Mode)
// -----------------------------------------------------------------------------
// The HPS writes raw decimal digits (0–9) into the low nibble of each byte in
// the HEX PIO registers. The FPGA (decoder_7seg modules) convert each digit
// into the correct 7-segment pattern.
// -----------------------------------------------------------------------------

typedef struct {
    hal_map_t        *hal;
    volatile uint32_t *hex03_reg;   // base for HEX0–HEX3 digits
    volatile uint32_t *hex45_reg;   // base for HEX4–HEX5 digits
    int               initialized;
} hex_display_handle_t;

int hex_display_init(hex_display_handle_t *hex, hal_map_t *hal);
int hex_display_cleanup(hex_display_handle_t *hex);

int hex_display_write(const hex_display_handle_t *hex, int value);
int hex_display_clear_digit(const hex_display_handle_t *hex, int digit);
int hex_display_clear_all(const hex_display_handle_t *hex);

#endif
