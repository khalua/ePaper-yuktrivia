#ifndef CONTENT_H
#define CONTENT_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Three jokes, one per page, in tap order. PAGE_COUNT wraps back to the first.
#define JOKE_COUNT 3
#define PAGE_COUNT JOKE_COUNT

// Everything the UI needs, kept in RTC memory across deep sleep so a button
// tap can redraw without touching WiFi. All text is sanitised to printable
// ASCII (the vendor fonts cover 0x20-0x7E only).
typedef struct {
    char   jokes[JOKE_COUNT][400];   // setup and punchline joined
    time_t fetched_at;               // when the API was last called successfully; 0 if the clock wasn't set
    int    page;                     // joke currently on screen
    bool   valid;                    // true once a fetch has succeeded
} content_t;

#ifdef __cplusplus
}
#endif

#endif // CONTENT_H
