#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pins the timezone to US Pacific. Call on every boot: the timezone isn't kept
// across deep sleep, so without this an offline wake would read UTC.
void time_sync_init_tz(void);

// Starts SNTP and blocks until the clock is set (or the timeout expires),
// then pins the timezone to US Pacific so localtime() reads correctly.
bool time_sync_start(int timeout_ms);

// Current local hour (0-23), or -1 if the clock was never synced.
int time_sync_local_hour(void);

// Seconds until the next occurrence of `hour`:00 local time.
// Returns -1 if the clock was never synced.
int time_sync_seconds_until_hour(int hour);

// Formats the current local time into `out` (e.g. "3:42 PM").
// Writes "--:--" if the clock was never synced.
void time_sync_format_now(char *out, int out_len);

// Formats `t` as local date and time (e.g. "Sep 19 3:42 PM"), or "--" if `t`
// is 0 / was never a real time.
void time_sync_format_time(time_t t, char *out, int out_len);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_H
