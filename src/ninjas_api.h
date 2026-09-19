#ifndef NINJAS_API_H
#define NINJAS_API_H

#include <stdbool.h>
#include "content.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fetches joke-of-the-day and trivia-of-the-day from API Ninjas into `out`
// (joke, category, question, answer, fetched_at, valid). Returns false, and
// leaves `out` untouched, if either request fails.
bool ninjas_fetch_all(content_t *out);

#ifdef __cplusplus
}
#endif

#endif // NINJAS_API_H
