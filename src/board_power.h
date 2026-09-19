#ifndef BOARD_POWER_H
#define BOARD_POWER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Must be called first thing at boot. On battery there is no hardware power
// switch: VBAT_PWR (GPIO17) latches the rail on, and the board cuts power the
// moment it goes low. Also releases the pin holds left by a previous deep sleep.
void board_power_init(void);

typedef enum {
    BOARD_WAKE_COLD_BOOT = 0,   // power-on or reset
    BOARD_WAKE_TIMER,           // deep-sleep timer expired
    BOARD_WAKE_BOOT_BUTTON,     // BOOT (GPIO0) tapped during deep sleep
    BOARD_WAKE_PWR_BUTTON,      // PWR (GPIO18) pressed during deep sleep
} board_wake_t;

// Why this boot happened. Valid after board_power_init().
board_wake_t board_wake_reason(void);

// Blocks until the BOOT button is released (or the timeout passes), then waits
// out contact bounce. Call before deep sleeping after a BOOT wake, otherwise the
// still-held button wakes the board again straight away.
void board_wait_boot_button_release(int timeout_ms);

// Call once, right after board_power_init(). If PWR is currently held down,
// blocks for up to ~1.5s confirming it's a genuine long-press (not a wake
// bounce); if confirmed, releases the VBAT latch and the board loses power
// (does not return). Returns false immediately if PWR isn't held, or if it's
// released before the hold time -- so this is cheap to call unconditionally.
bool board_power_handle_shutdown_button(void);

// Cuts the e-paper rail, keeps the battery latch held through sleep, arms a
// PWR-button wakeup alongside the timer, and deep sleeps for the given number
// of seconds. Does not return -- waking runs app_main() again from the top.
void board_deep_sleep_seconds(uint64_t seconds);

#ifdef __cplusplus
}
#endif

#endif // BOARD_POWER_H
