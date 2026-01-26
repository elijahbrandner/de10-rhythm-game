#include <stdio.h>
#include <stdint.h>
#include "../../includes/hal/hal-api.h"
#include "../../includes/peripherals/hex-display.h"
#include "../../lib/address_map_arm.h"

// -----------------------------------------------------------------------------
// Initialization / Cleanup
// -----------------------------------------------------------------------------
int hex_display_init(hex_display_handle_t *hex, hal_map_t *hal) {
    if (!hex || !hal) return -1;

    hex->hal       = hal;
    hex->hex03_reg = (volatile uint32_t *)hal_get_virtual_addr(hal, HEX3_HEX0_BASE);
    hex->hex45_reg = (volatile uint32_t *)hal_get_virtual_addr(hal, HEX5_HEX4_BASE);

    if (!hex->hex03_reg || !hex->hex45_reg) {
        hex->initialized = 0;
        return -1;
    }
    hex->initialized = 1;
    return 0;
}

int hex_display_cleanup(hex_display_handle_t *hex) {
    if (!hex) return -1;
    hex->hal       = NULL;
    hex->hex03_reg = NULL;
    hex->hex45_reg = NULL;
    hex->initialized = 0;
    return 0;
}

// -----------------------------------------------------------------------------
// Digit-Mode HEX Writing
// -----------------------------------------------------------------------------
// HPS writes ONLY decimal digits (0–9) into the low nibble of each byte.
// High nibble ignored by hardware.
// 0xF nibble = blank (decoder outputs all segments OFF).
// -----------------------------------------------------------------------------

int hex_display_write(const hex_display_handle_t *hex, int value) {
    if (!hex || !hex->initialized) return -1;

    int digits[6];
    for (int i = 0; i < 6; i++) {
        digits[i] = value % 10;
        value /= 10;

        if (digits[i] < 0 || digits[i] > 9) {
            digits[i] = 0; // safety fallback
        }
    }

    uint32_t word03 = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t d = (digits[i] & 0x0F);
        word03 |= (d << (8 * i));
    }

    uint32_t word45 = 0;
    for (int i = 4; i < 6; i++) {
        int idx = i - 4;
        uint32_t d = (digits[i] & 0x0F);
        word45 |= (d << (8 * idx));
    }

    *(hex->hex03_reg) = word03;
    *(hex->hex45_reg) = word45;
    return 0;
}

int hex_display_clear_digit(const hex_display_handle_t *hex, int digit) {
    if (!hex || !hex->initialized) return -1;
    if (digit < 0 || digit > 5) return -1;

    if (digit <= 3) {
        uint32_t word = *(hex->hex03_reg);

        word &= ~(0xFFu << (8 * digit));        // clear full byte
        word |=  (0x0Fu << (8 * digit));        // nibble F = blank

        *(hex->hex03_reg) = word;
    } else {
        int idx = digit - 4;
        uint32_t word = *(hex->hex45_reg);

        word &= ~(0xFFu << (8 * idx));
        word |=  (0x0Fu << (8 * idx));

        *(hex->hex45_reg) = word;
    }
    return 0;
}

int hex_display_clear_all(const hex_display_handle_t *hex) {
    if (!hex || !hex->initialized) return -1;

    *(hex->hex03_reg) = 0x0F0F0F0Fu;   // blank all lower digits
    *(hex->hex45_reg) = 0x00000F0Fu;   // blank upper digits

    return 0;
}
