#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>

#define LCD_WIDTH 128
#define LCD_HEIGHT 64

//? LCD Struct for frame buffer management
typedef struct
{
    int Width;
    int Height;
    int BitPerPixel;
    int FrameSize;
    uint8_t *pFrame;
}lcd_canvas_t;

//? LCD Handle Struct
typedef struct 
{
    void *hps_virtual_base;
    lcd_canvas_t canvas;
    int initialized;

}lcd_handle_t;

int lcd_init(lcd_handle_t *lcd);
int lcd_cleanup(lcd_handle_t *lcd);
int lcd_clear(lcd_handle_t *lcd);
int lcd_write_text(lcd_handle_t *lcd, int x, int y, const char *text);
int lcd_backlight(lcd_handle_t *lcd, bool on);
int lcd_refresh(lcd_handle_t *lcd);

#endif