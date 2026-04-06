#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// lcd.h
//
// Provides an interface for controlling the LCD display on the DE10 board.
//
// This module handles:
// - frame buffer management (software-side canvas)
// - text rendering
// - screen clearing and refresh
// - backlight control
//
// The LCD uses HPS-side memory mapping (not the FPGA LW bridge).
// -----------------------------------------------------------------------------

// LCD display resolution (pixels)
#define LCD_WIDTH 128
#define LCD_HEIGHT 64

// -----------------------------------------------------------------------------
// LCD Canvas (frame buffer)
// -----------------------------------------------------------------------------

// Represents a software frame buffer used for drawing before updating the LCD.
//
// The drawing library writes into this buffer, and the contents are pushed
// to the LCD hardware using a refresh call.
typedef struct
{
    int Width;          // Dispaly width in pixels
    int Height;         // Display height in pixels
    int BitPerPixel;    // Bits per pixel (typically 1 for monochrome)
    int FrameSize;      // Total buffer size in bytes
    uint8_t *pFrame;    // Pointer to allocated frame buffer memory
}lcd_canvas_t;

// -----------------------------------------------------------------------------
// LCD Handle
// -----------------------------------------------------------------------------

// Main LCD control structure.
//
// Stores:
// - HPS virtual base address (for hardware access)
// - canvas (frame buffer)
// - initialization status
typedef struct 
{
    void *hps_virtual_base; // Mapped HPS peripheral base address
    lcd_canvas_t canvas;    // Frame buffer for drawing
    int initialized;        // Initialization flag

}lcd_handle_t;

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------


// Initialize the LCD subsystem and allocate frame buffer.
// Returns 0 on success, -1 on failure.
int lcd_init(lcd_handle_t *lcd);

// Clean up LCD resources and free memory.
// Returns 0 on success, -1 on failure.
int lcd_cleanup(lcd_handle_t *lcd);

// Clear the display (fills screen with white).
// Returns 0 on success, -1 on failure.
int lcd_clear(lcd_handle_t *lcd);

// Draw text at specified (x, y) position on the screen.
// Returns 0 on success, -1 on failure.
int lcd_write_text(lcd_handle_t *lcd, int x, int y, const char *text);

// Turn LCD backlight on or off.
// Returns 0 on success, -1 on failure.
int lcd_backlight(lcd_handle_t *lcd, bool on);

// Refresh the display to show current frame buffer contents.
// Returns 0 on success, -1 on failure.
int lcd_refresh(lcd_handle_t *lcd);

#endif