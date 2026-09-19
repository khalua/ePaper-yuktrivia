#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ui.h"
#include "display.h"
#include "time_sync.h"
#include "battery.h"

#define MARGIN        8
#define HEADER_H      28
#define BODY_Y0       34
#define BODY_Y1       164            // last usable body row, exclusive of the dots
#define DOTS_Y        174
#define FOOTER_LINE_Y 181
#define FOOTER_Y      185
#define LINE_GAP      2
#define MAX_LINES     16

// Largest first. The vendor fonts top out at 24px, so long text drops down a size.
static sFONT *const FONTS[] = { &Font24, &Font20, &Font16, &Font12 };

// Greedy word wrap of `s` into at most `max_lines` lines of `cols` characters.
// Words longer than a line are split. Returns the line count and sets
// *truncated if text was left over.
static int wrap(const char *s, int cols, int max_lines,
                const char **starts, int *lens, bool *truncated)
{
    int n = 0;
    const char *p = s;

    while (*p && n < max_lines) {
        while (*p == ' ') p++;
        if (!*p) break;

        int remaining = (int)strlen(p);
        int len = remaining;
        if (remaining > cols) {
            len = cols;                       // hard split unless a space helps
            for (int i = cols; i > 0; i--) {  // last space at or before the limit
                if (p[i] == ' ') { len = i; break; }
            }
        }
        starts[n] = p;
        lens[n] = len;
        n++;
        p += len;
    }

    while (*p == ' ') p++;
    *truncated = *p != '\0';
    return n;
}

static void draw_header(const char *title, uint8_t bg, uint8_t fg)
{
    Paint_DrawRectangle(0, 0, DISPLAY_W - 1, HEADER_H, bg, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    // Font16 is 11px wide; keep the title inside the bar.
    char buf[24];
    int max_chars = (DISPLAY_W - 2 * MARGIN) / Font16.Width;
    snprintf(buf, sizeof buf, "%.*s", max_chars, title);
    // Paint_DrawString_EN's colour arguments are (background, foreground) in
    // practice -- same convention the weather app used.
    Paint_DrawString_EN(MARGIN, 6, buf, &Font16, bg, fg);
}

static void draw_body(const char *text)
{
    int body_h = BODY_Y1 - BODY_Y0;
    int width = DISPLAY_W - 2 * MARGIN;

    const char *starts[MAX_LINES];
    int lens[MAX_LINES];
    bool truncated = false;
    int n = 0;
    sFONT *font = &Font12;

    for (size_t i = 0; i < sizeof FONTS / sizeof FONTS[0]; i++) {
        sFONT *f = FONTS[i];
        int lines_fit = body_h / (f->Height + LINE_GAP);
        if (lines_fit > MAX_LINES) lines_fit = MAX_LINES;

        n = wrap(text, width / f->Width, lines_fit, starts, lens, &truncated);
        font = f;
        if (!truncated) break;   // fits at this size; otherwise fall through to a smaller font
    }

    char line[48];
    for (int i = 0; i < n; i++) {
        int len = lens[i] < (int)sizeof line - 4 ? lens[i] : (int)sizeof line - 4;
        memcpy(line, starts[i], len);
        line[len] = '\0';
        if (truncated && i == n - 1) {
            // Even the smallest font overflowed: mark the cut.
            if (len > 3) len -= 3;
            line[len] = '\0';
            strcat(line, "...");
        }
        Paint_DrawString_EN(MARGIN, BODY_Y0 + i * (font->Height + LINE_GAP), line, font,
                            EPD_1IN54G_WHITE, EPD_1IN54G_BLACK);
    }
}

// Dots sit at the left so the right side is free for the hint or low-battery
// warning.
static void draw_dots(int page)
{
    const int spacing = 14;
    int x0 = MARGIN + 3;
    for (int i = 0; i < PAGE_COUNT; i++) {
        Paint_DrawCircle(x0 + i * spacing, DOTS_Y, 3, EPD_1IN54G_BLACK, DOT_PIXEL_1X1,
                         i == page ? DRAW_FILL_FULL : DRAW_FILL_EMPTY);
    }
}

// Right-aligned text on the dots row.
static void draw_row_text(const char *text, uint8_t colour)
{
    int x = DISPLAY_W - MARGIN - (int)strlen(text) * Font12.Width;
    Paint_DrawString_EN(x, DOTS_Y - 6, text, &Font12, EPD_1IN54G_WHITE, colour);
}

static void draw_footer(time_t fetched_at)
{
    Paint_DrawLine(MARGIN, FOOTER_LINE_Y, DISPLAY_W - MARGIN, FOOTER_LINE_Y,
                   EPD_1IN54G_BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    char when[24];
    time_sync_format_time(fetched_at, when, sizeof when);
    char buf[40];
    snprintf(buf, sizeof buf, "Updated %s", when);
    Paint_DrawString_EN(MARGIN, FOOTER_Y, buf, &Font12, EPD_1IN54G_WHITE, EPD_1IN54G_BLACK);
}

void ui_render_page(const content_t *c, int page, bool show_hint, int battery_pct)
{
    Paint_Clear(EPD_1IN54G_WHITE);

    switch (page) {
    case PAGE_JOKE:
        draw_header("JOKE OF THE DAY", EPD_1IN54G_YELLOW, EPD_1IN54G_BLACK);
        draw_body(c->joke);
        break;
    case PAGE_TRIVIA: {
        // The API sometimes returns an empty category.
        char title[sizeof c->category];
        if (c->category[0]) {
            size_t i;
            for (i = 0; c->category[i] && i < sizeof title - 1; i++) {
                title[i] = (char)toupper((unsigned char)c->category[i]);
            }
            title[i] = '\0';
        } else {
            strcpy(title, "TRIVIA");
        }
        draw_header(title, EPD_1IN54G_YELLOW, EPD_1IN54G_BLACK);
        draw_body(c->question);
        break;
    }
    case PAGE_ANSWER:
    default:
        draw_header("ANSWER", EPD_1IN54G_RED, EPD_1IN54G_WHITE);
        draw_body(c->answer);
        break;
    }

    draw_dots(page);
    if (battery_pct >= 0 && battery_pct <= BATTERY_LOW_PCT) {
        // The warning takes priority over the hint; it's the one that matters.
        char buf[24];
        snprintf(buf, sizeof buf, "LOW BATTERY %d%%", battery_pct);
        draw_row_text(buf, EPD_1IN54G_RED);
    } else if (show_hint) {
        draw_row_text("Top button", EPD_1IN54G_BLACK);
    }
    draw_footer(c->fetched_at);
}

void ui_render_message(const char *title, const char *msg)
{
    Paint_Clear(EPD_1IN54G_WHITE);
    draw_header(title, EPD_1IN54G_YELLOW, EPD_1IN54G_BLACK);
    draw_body(msg);
}
