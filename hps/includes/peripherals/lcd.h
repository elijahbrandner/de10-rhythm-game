#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "../hal/hal-api.h"

// This "LCD" is implemented using the FPGA character buffer (VGA char buffer style).
// We'll treat it like a simple 2-line text display for your game UI.

#ifndef LCD_COLS
#define LCD_COLS 80
#endif

#ifndef LCD_ROWS
#define LCD_ROWS 2
#endif

typedef struct {
    hal_map_t *hal;
    int initialized;

    // mapped char buffer (FPGA_CHAR_BASE)
    volatile uint8_t *char_buf;
    uint32_t char_buf_span;
} lcd_handle_t;

// Initialize LCD/char buffer mapping (uses hal->fd)
int lcd_init(lcd_handle_t *lcd, hal_map_t *hal);

// Cleanup mapping
int lcd_cleanup(lcd_handle_t *lcd);

// Clear top LCD_ROWS lines (fills with spaces)
int lcd_clear(lcd_handle_t *lcd);

// Write text to a line (0..LCD_ROWS-1). Text is truncated/padded to LCD_COLS.
int lcd_write_line(lcd_handle_t *lcd, int line, const char *text);

#endif // LCD_H
