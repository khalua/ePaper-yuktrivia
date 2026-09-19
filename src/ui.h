#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "content.h"

#ifdef __cplusplus
extern "C" {
#endif

// Draws one page (a page_t) into the framebuffer: coloured header, wrapped
// body, page dots, and an "Updated <date> <time>" footer from c->fetched_at.
// `show_hint` adds a "Top button" reminder. `battery_pct` is 0-100, or -1 if
// unknown; a red "LOW BATTERY" warning replaces the hint at or below
// BATTERY_LOW_PCT and nothing is shown above it. Call display_push() afterwards.
void ui_render_page(const content_t *c, int page, bool show_hint, int battery_pct);

// Draws a message on an otherwise empty page (errors, "Connecting...").
void ui_render_message(const char *title, const char *msg);

#ifdef __cplusplus
}
#endif

#endif // UI_H
