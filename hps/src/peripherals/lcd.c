#include "../../includes/peripherals/lcd.h"
#include "../../lib/address_map_arm.h"

#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#define CHAR_STRIDE 128  // typical character buffer stride (cols) in DE1/DE10 examples

static inline void lcd_putc(lcd_handle_t *lcd, int x, int y, char c) {
    // char buffer is byte-addressed: offset = y*CHAR_STRIDE + x
    lcd->char_buf[(y * CHAR_STRIDE) + x] = (uint8_t)c;
}

int lcd_init(lcd_handle_t *lcd, hal_map_t *hal) {
    if (!lcd || !hal || hal->fd < 0) return -1;

    memset(lcd, 0, sizeof(*lcd));
    lcd->hal = hal;
    lcd->char_buf_span = FPGA_CHAR_SPAN;

    // Map FPGA character buffer region (separate from LW bridge)
    void *v = mmap(NULL,
                   lcd->char_buf_span,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED,
                   hal->fd,
                   FPGA_CHAR_BASE);

    if (v == MAP_FAILED) {
        lcd->char_buf = NULL;
        lcd->initialized = 0;
        return -1;
    }

    lcd->char_buf = (volatile uint8_t *)v;
    lcd->initialized = 1;

    // Clear the display area we care about
    lcd_clear(lcd);
    return 0;
}

int lcd_cleanup(lcd_handle_t *lcd) {
    if (!lcd) return -1;

    if (lcd->char_buf) {
        munmap((void *)lcd->char_buf, lcd->char_buf_span);
        lcd->char_buf = NULL;
    }

    lcd->initialized = 0;
    lcd->hal = NULL;
    lcd->char_buf_span = 0;
    return 0;
}

int lcd_clear(lcd_handle_t *lcd) {
    if (!lcd || !lcd->initialized || !lcd->char_buf) return -1;

    for (int y = 0; y < LCD_ROWS; y++) {
        for (int x = 0; x < LCD_COLS; x++) {
            lcd_putc(lcd, x, y, ' ');
        }
    }
    return 0;
}

int lcd_write_line(lcd_handle_t *lcd, int line, const char *text) {
    if (!lcd || !lcd->initialized || !lcd->char_buf || !text) return -1;
    if (line < 0 || line >= LCD_ROWS) return -1;

    // Write up to LCD_COLS chars; pad with spaces
    int i = 0;
    for (; i < LCD_COLS && text[i] != '\0'; i++) {
        lcd_putc(lcd, i, line, text[i]);
    }
    for (; i < LCD_COLS; i++) {
        lcd_putc(lcd, i, line, ' ');
    }
    return 0;
}
