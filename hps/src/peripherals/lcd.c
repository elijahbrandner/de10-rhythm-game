#include "peripherals/lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

// Include the necessary LCD driver headers
#include "../../includes/vendor/lcd/LCD_Lib.h"
#include "../../includes/vendor/lcd/lcd_graphic.h"
#include "../../includes/vendor/lcd/font.h"
#include "../../includes/vendor/lcd/LCD_Hw.h"

// HPS peripheral memory mapping (different from LW bridge used by HAL)
#define HW_REGS_BASE 0xFC000000
#define HW_REGS_SPAN 0x04000000

// Global HPS mapping for LCD (separate from HAL's LW bridge mapping)
static int hps_fd = -1;
static void *hps_virtual_base = NULL;
static int hps_initialized = 0;

int lcd_init(lcd_handle_t *lcd) {
    if (!lcd) return -1;
    
    // Initialize HPS peripheral mapping (not using HAL - LCD needs different address space)
    if (!hps_initialized) {
        hps_fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (hps_fd == -1) {
            perror("ERROR: could not open /dev/mem for LCD");
            return -1;
        }
        
        hps_virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, hps_fd, HW_REGS_BASE);
        if (hps_virtual_base == MAP_FAILED) {
            perror("ERROR: mmap() failed for HPS peripherals");
            close(hps_fd);
            hps_fd = -1;
            return -1;
        }
        hps_initialized = 1;
    }
    
    // Store the HPS virtual base for LCD operations
    lcd->hps_virtual_base = hps_virtual_base;
    
    // Initialize LCD canvas
    lcd->canvas.Width = LCD_WIDTH;
    lcd->canvas.Height = LCD_HEIGHT;
    lcd->canvas.BitPerPixel = 1;
    lcd->canvas.FrameSize = lcd->canvas.Width * lcd->canvas.Height / 8;
    lcd->canvas.pFrame = (void *)malloc(lcd->canvas.FrameSize);
    
    if (lcd->canvas.pFrame == NULL) {
        fprintf(stderr, "ERROR: failed to allocate lcd frame buffer\n");
        return -1;
    }
    
    // Initialize LCD hardware using the HPS virtual base
    LCDHW_Init(lcd->hps_virtual_base);
    LCDHW_BackLight(true);
    LCD_Init();
    
    // Clear the screen
    DRAW_Clear(&lcd->canvas, LCD_WHITE);
    DRAW_Refresh(&lcd->canvas);
    
    lcd->initialized = 1;
    
    printf("LCD peripheral initialized successfully\n");
    return 0;
}

int lcd_cleanup(lcd_handle_t *lcd) {
    if (!lcd || !lcd->initialized) return -1;
    
    // Clear screen and turn off backlight
    DRAW_Clear(&lcd->canvas, LCD_WHITE);
    DRAW_Refresh(&lcd->canvas);
    LCDHW_BackLight(false);
    
    // Free frame buffer
    if (lcd->canvas.pFrame) {
        free(lcd->canvas.pFrame);
        lcd->canvas.pFrame = NULL;
    }
    
    lcd->hps_virtual_base = NULL;
    lcd->initialized = 0;
    
    // Close HPS peripheral mapping
    if (hps_initialized) {
        if (munmap(hps_virtual_base, HW_REGS_SPAN) != 0) {
            perror("ERROR: munmap() failed for HPS peripherals");
            return -1;
        }
        close(hps_fd);
        hps_fd = -1;
        hps_virtual_base = NULL;
        hps_initialized = 0;
    }
    
    printf("LCD peripheral cleaned up successfully\n");
    return 0;
}

int lcd_clear(lcd_handle_t *lcd) {
    if (!lcd || !lcd->initialized) return -1;
    
    DRAW_Clear(&lcd->canvas, LCD_WHITE);
    DRAW_Refresh(&lcd->canvas);
    
    return 0;
}

int lcd_write_text(lcd_handle_t *lcd, int x, int y, const char *text) {
    if (!lcd || !lcd->initialized || !text) return -1;
    
    DRAW_Clear(&lcd->canvas, LCD_WHITE);
    DRAW_PrintString(&lcd->canvas, x, y, (char*)text, LCD_BLACK, &font_16x16);
    DRAW_Refresh(&lcd->canvas);
    
    return 0;
}

int lcd_backlight(lcd_handle_t *lcd, bool on) {
    if (!lcd || !lcd->initialized) return -1;
    
    LCDHW_BackLight(on);
    
    return 0;
}

int lcd_refresh(lcd_handle_t *lcd) {
    if (!lcd || !lcd->initialized) return -1;
    
    DRAW_Refresh(&lcd->canvas);
    
    return 0;
}
    