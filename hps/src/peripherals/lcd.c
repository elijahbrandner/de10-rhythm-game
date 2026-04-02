// Inlcude the LCD peripheral interface header
// This defines:
// - lcd_handle_t
// - LCD_WIDTH / LCD_HEIGHT
// - function declarations for the LCD API
#include "peripherals/lcd.h"

// Standard I/O for printf() and fprintf()
#include <stdio.h>

// Standard library for malloc() and free()
#include <stdlib.h>

// String utilities (included here though not heavily used in this file)
#include <string.h>

// UNIX/POSIX functions such as close()
#include <unistd.h>

// File control options for open()
#include <fcntl.h>

// Memory mapping support for mmap() and munmap()
#include <sys/mman.h>

// Include the necessary vendor LCD driver headers
//
// These are the lowlevel libraries provided for interacting with the LCD
// hardware and drawing into the LCD frame buffer
#include "../../includes/vendor/lcd/LCD_Lib.h"
#include "../../includes/vendor/lcd/lcd_graphic.h"
#include "../../includes/vendor/lcd/font.h"
#include "../../includes/vendor/lcd/LCD_Hw.h"

// HPS peripheral memory mapping (different from LW bridge used by HAL)
//
// Important:
// Your normal HAL maps the lightweight FPGA bridge
// The LCD hardware lives in a different HPS peripheral address space
// so this file uses its own /dev/mem + mmap setup
#define HW_REGS_BASE 0xFC000000
#define HW_REGS_SPAN 0x04000000

// Global HPS mapping for LCD (separate from HAL's LW bridge mapping)
//
// These globals let the LCD subsystem share one HPS mapping across all LCD
// operations instead of re-opening and re-mapping every single time
static int hps_fd = -1;                 // File descriptor for /dev/mem
static void *hps_virtual_base = NULL;   // Base virtual address returned by mmap()
static int hps_initialized = 0;         // Tracks whether HPS mapping is active

// Initialize the LCD subsystem
//
// Parameters:
// - lcd: pointer to LCD handle structure
//
// Returns: 0 on success and -1 on failure
//
// What this does:
// 1. Maps the HPS peripheral address space if needed 
// 2. Stores the mapped base in the LCD handle
// 3. Allocates a software frame buffer
// 4. Initializes the LCD hardware and graphics library
// 5. Clears the screen and enables the backlight
int lcd_init(lcd_handle_t *lcd) {
    // Validate input handle pointer
    if (!lcd) return -1;
    
    // Initialize HPS peripheral mapping (not using HAL - LCD needs different address space)
    //
    // This is separate from HAL because the LCD hardware uses a different 
    // address range than the lightweight FPGA bridge
    if (!hps_initialized) {
        // Open /dev/mem for raw physical memory access
        hps_fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (hps_fd == -1) {
            perror("ERROR: could not open /dev/mem for LCD");
            return -1;
        }
        
        // Map the HPS peripheral region into virtual memory
        hps_virtual_base = mmap(NULL, HW_REGS_SPAN, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, hps_fd, HW_REGS_BASE);
        // If mmap fails, clean up the file descriptor and return failure
        if (hps_virtual_base == MAP_FAILED) {
            perror("ERROR: mmap() failed for HPS peripherals");
            close(hps_fd);
            hps_fd = -1;
            return -1;
        }
        // Mark the shared HPS mapping as ready
        hps_initialized = 1;
    }
    
    // Store the HPS virtual base inside this LCD handle
    // The vendor LCD hardware functions will use this pointer
    lcd->hps_virtual_base = hps_virtual_base;
    
    // Initialize LCD canvas structure
    //
    // The canvas is a software-side frame buffer used by the drawing library
    // before the image is pushed to the actual LCD hardware
    lcd->canvas.Width = LCD_WIDTH;                  // Display width in pixels
    lcd->canvas.Height = LCD_HEIGHT;                // Display height in pixels
    lcd->canvas.BitPerPixel = 1;                    // 1-bit monochrome buffer
    lcd->canvas.FrameSize = lcd->canvas.Width * lcd->canvas.Height / 8; // Total bytes needed

    // Allocate memory for the frame buffer
    lcd->canvas.pFrame = (void *)malloc(lcd->canvas.FrameSize);
    
    // If allocation fails, return an error
    if (lcd->canvas.pFrame == NULL) {
        fprintf(stderr, "ERROR: failed to allocate lcd frame buffer\n");
        return -1;
    }
    
    // Initialize LCD hardware using the mapped HPS virtual base address
    LCDHW_Init(lcd->hps_virtual_base);

    // TUrn on the LCD backlight
    LCDHW_BackLight(true);

    // Initialize the higher-level LCD library
    LCD_Init();
    
    // Clear the display to white and immediately refresh it
    DRAW_Clear(&lcd->canvas, LCD_WHITE);
    DRAW_Refresh(&lcd->canvas);
    
    // Mark this LCD handle as initialized and ready to use
    lcd->initialized = 1;
    
    // Print confirmation to the console
    printf("LCD peripheral initialized successfully\n");
    return 0;
}

// Clean up the LCD subsystem
//
// Parameters:
// - lcd: pointer to LCD handle
//
// Returns:
// - 0 on success
// - -1 on failure
//
// What this does:
// 1. Clears the screen
// 2. Turns off the backlight
// 3. Frees the software frame buffer
// 4. Unmaps HPS peripherals and closes /dev/mem
int lcd_cleanup(lcd_handle_t *lcd) {
    // Validate handle and ensure LCD was initialized
    if (!lcd || !lcd->initialized) return -1;
    
    // Clear screen and turn off backlight so hardware is left in a clean state
    DRAW_Clear(&lcd->canvas, LCD_WHITE);

    // Free the allocated frame buffer memory if it exists
    DRAW_Refresh(&lcd->canvas);
    LCDHW_BackLight(false);
    
    // Free the allocated frame buffer memory if it exists
    if (lcd->canvas.pFrame) {
        free(lcd->canvas.pFrame);
        lcd->canvas.pFrame = NULL;
    }
    
    // Clear handle state
    lcd->hps_virtual_base = NULL;
    lcd->initialized = 0;
    
    // Close and unmap the shared HPS peripheral mapping if active
    if (hps_initialized) {
        // Unmap the HPS peripheral region
        if (munmap(hps_virtual_base, HW_REGS_SPAN) != 0) {
            perror("ERROR: munmap() failed for HPS peripherals");
            return -1;
        }
        // Close the /dev/mem file descriptor
        close(hps_fd);

        // Reset globals back to clean defaults
        hps_fd = -1;
        hps_virtual_base = NULL;
        hps_initialized = 0;
    }
    
    // Print confirmation to the console
    printf("LCD peripheral cleaned up successfully\n");
    return 0;
}

// clear the LCD display to white
//
// Parameters:
// - lcd: pointer to LCD handle
//
// Returns: 0 on success, -1 on failure
int lcd_clear(lcd_handle_t *lcd) {
    // Validate handle and initialized state
    if (!lcd || !lcd->initialized) return -1;
    
    // Clear the software canvas to white
    DRAW_Clear(&lcd->canvas, LCD_WHITE);

    // Push the cleared canvas to the LCD hardware
    DRAW_Refresh(&lcd->canvas);
    
    return 0;
}

// Draw a text string onto the LCD canvas
// Parameters:
// - lcd: pointer to LCD handle
// - x: horizontal pixel position
// - y: vertical pixel position
// - text: null-terminated string to draw
//
// Returns: 0 on success, -1 on failure
//
// Note:
// This writes into the canvas, but does not itself force a separate refresh
// beyond what the drawing library may require. Our main code often calls 
// lcd_refresh() after writing all lines
int lcd_write_text(lcd_handle_t *lcd, int x, int y, const char *text) {
    // Validate inputs
    if (!lcd || !lcd->initialized || !text) return -1;

    // Draw text in black using the 16x16 font
    DRAW_PrintString(&lcd->canvas, x, y, (char*)text, LCD_BLACK, &font_16x16);
    return 0;
}

// Turn the LCD backlight on or off
//
// Parameters:
// - lcd: pointer to LCD handle
// - on: true on enable backlight, false to disable it
//
// Returns: 0 on success, -1 on failure
int lcd_backlight(lcd_handle_t *lcd, bool on) {
    // Validate handle and intitialize state
    if (!lcd || !lcd->initialized) return -1;
    
    // Pass requrested backlight state to vendor hardware driver
    LCDHW_BackLight(on);
    
    return 0;
}

// Refresh the LCD so the current canvas contents appear on screen.
//
// Parameters:
// - lcd: pointer to LCD handle
//
// Returns:
// - 0 on success
// - -1 on failure
int lcd_refresh(lcd_handle_t *lcd) {
    // Validate handle and initialized state
    if (!lcd || !lcd->initialized) return -1;
    
    // Push the current canvas/frame buffer contents to the LCD hardware
    DRAW_Refresh(&lcd->canvas);
    
    return 0;
}
    