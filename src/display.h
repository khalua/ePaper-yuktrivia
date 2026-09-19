#ifndef YUKTRIVIA_DISPLAY_H
#define YUKTRIVIA_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "epaper_port.h"   // EPD_1IN54G_* colours, EXAMPLE_LCD_WIDTH/HEIGHT
#include "GUI_Paint.h"
#include "Fonts/fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_W EXAMPLE_LCD_WIDTH
#define DISPLAY_H EXAMPLE_LCD_HEIGHT

// Powers the panel, allocates the 4-colour framebuffer in PSRAM and selects it
// for GUI_Paint. Call exactly once per boot: the vendor init also sets up the
// SPI bus and aborts if run twice. `clear_panel` runs a full white refresh
// (~20s); skip it when the next push overwrites the whole screen anyway.
bool display_init(bool clear_panel);

// Sends the framebuffer to the panel (full refresh, 10-20s).
void display_push(void);

// Puts the panel into its own deep sleep. Call before cutting power; the
// panel can't be woken again without a reboot.
void display_shutdown(void);

// Draws text at an integer scale (the vendor fonts top out at 24px).
void display_text_scaled(int x, int y, const char *s, sFONT *font,
                         uint8_t colour, int scale);

#ifdef __cplusplus
}
#endif

#endif // YUKTRIVIA_DISPLAY_H
