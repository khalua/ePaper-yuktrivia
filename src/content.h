#ifndef CONTENT_H
#define CONTENT_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// The three pages, in tap order. PAGE_COUNT wraps back to PAGE_JOKE.
typedef enum {
    PAGE_JOKE = 0,
    PAGE_TRIVIA,
    PAGE_ANSWER,
    PAGE_COUNT
} page_t;

// Everything the UI needs, kept in RTC memory across deep sleep so a button
// tap can redraw without touching WiFi. All text is sanitised to printable
// ASCII (the vendor fonts cover 0x20-0x7E only).
typedef struct {
    char   joke[400];
    char   category[40];   // may be empty -- the API sometimes omits it
    char   question[300];
    char   answer[120];
    time_t fetched_at;     // when the API was last called successfully; 0 if the clock wasn't set
    int    page;           // page_t currently on screen
    bool   valid;          // true once a fetch has succeeded
} content_t;

#ifdef __cplusplus
}
#endif

#endif // CONTENT_H
