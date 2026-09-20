#ifndef JOKE_API_H
#define JOKE_API_H

#include <stdbool.h>
#include "content.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fetches JOKE_COUNT jokes from official-joke-api into `out` (jokes, fetched_at,
// valid) and resets out->page to 0. Returns false, and leaves `out` untouched,
// if the request fails or returns too few usable jokes.
bool joke_fetch_all(content_t *out);

#ifdef __cplusplus
}
#endif

#endif // JOKE_API_H
