// yuktrivia -- Waveshare ESP32-S3-ePaper-1.54(G), four-colour 200x200 e-paper.
//
// Shows three jokes from official-joke-api and cycles them with the BOOT button:
//   joke 1 -> joke 2 -> joke 3 -> back to 1. The page dots show which one.
//
// Jokes are fetched once a day (and on first boot) and cached in RTC memory,
// so a BOOT tap wakes from deep sleep, redraws the next page from the cache
// with no WiFi, and sleeps again.
//
// Before flashing: copy src/secrets.example.h to src/secrets.h and fill in
// your WiFi credentials.

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "esp_log.h"

#include "battery.h"
#include "board_power.h"
#include "content.h"
#include "display.h"
#include "joke_api.h"
#include "time_sync.h"
#include "ui.h"
#include "wifi_sta.h"

static const char *TAG = "main";

#define REFRESH_HOUR        0              // refetch after local midnight...
#define REFRESH_MINUTE_S    (5 * 60)       // ...plus a few minutes of slack
#define SLEEP_RETRY         (15 * 60)      // fetch failed; try again sooner
#define SLEEP_NO_CLOCK      (24 * 60 * 60) // clock never set; daily is the best guess
#define BUTTON_RELEASE_MS   3000

// Survives deep sleep (not a full power loss, which just triggers a refetch).
RTC_DATA_ATTR static content_t s_content;

// Seconds until the next daily refresh, from the RTC-kept clock. Recomputed on
// every wake so BOOT taps don't push the refresh back.
static uint64_t seconds_until_refresh(void)
{
    int s = time_sync_seconds_until_hour(REFRESH_HOUR);
    if (s < 0) {
        return SLEEP_NO_CLOCK;
    }
    return (uint64_t)s + REFRESH_MINUTE_S;
}

// Connects, syncs the clock and refreshes s_content. Returns true on success;
// on failure the previous content is left alone.
static bool refresh_content(void)
{
    if (!wifi_sta_connect(30000)) {
        ESP_LOGW(TAG, "WiFi failed");
        return false;
    }
    time_sync_start(10000);   // not fatal: the stamp just shows "--"
    return joke_fetch_all(&s_content);
}

extern "C" void app_main(void)
{
    // First, before anything else: on battery this is what keeps the board
    // powered at all.
    board_power_init();

    // Hold PWR ~1.5s to power the board off. No-op if PWR isn't held.
    board_power_handle_shutdown_button();

    // The timezone isn't kept across deep sleep; needed even when offline.
    time_sync_init_tz();

    // Measure before WiFi starts: radio transmit bursts sag the battery voltage.
    int battery_pct = battery_read_percent();

    // Give the USB serial console a moment to re-enumerate.
    vTaskDelay(pdMS_TO_TICKS(2000));

    board_wake_t wake = board_wake_reason();
    bool first_boot = !s_content.valid;
    ESP_LOGI(TAG, "starting (wake=%d, cache %s, page %d)", (int)wake,
             s_content.valid ? "valid" : "empty", s_content.page);

    // A PWR tap that wasn't a long-press just goes back to sleep; the panel
    // already shows the current page.
    if (wake == BOARD_WAKE_PWR_BUTTON && s_content.valid) {
        board_deep_sleep_seconds(seconds_until_refresh());
    }

    bool fetch_failed = false;
    bool need_fetch = first_boot || wake == BOARD_WAKE_TIMER || wake == BOARD_WAKE_COLD_BOOT;
    if (need_fetch) {
        fetch_failed = !refresh_content();
    }

    if (wake == BOARD_WAKE_BOOT_BUTTON && s_content.valid) {
        s_content.page = (s_content.page + 1) % PAGE_COUNT;
    }

    // A failed timer refresh with a good cache changes nothing on screen, so
    // skip the 15s panel refresh and just retry later.
    if (wake == BOARD_WAKE_TIMER && fetch_failed && s_content.valid) {
        board_deep_sleep_seconds(SLEEP_RETRY);
    }

    // A refresh that failed while a cache exists keeps showing the old data --
    // slightly stale beats blank -- so only an empty cache gets the error page.
    bool show_error = !s_content.valid;

    // Skip the ~20s clear when we already have the next page to draw over it.
    if (!display_init(false)) {
        ESP_LOGE(TAG, "display init failed");
        board_deep_sleep_seconds(SLEEP_RETRY);
    }

    if (show_error) {
        ui_render_message("YUKTRIVIA", "No data - check WiFi");
    } else {
        ui_render_page(&s_content, s_content.page, wake == BOARD_WAKE_COLD_BOOT, battery_pct);
    }
    display_push();
    display_shutdown();

    // Only a BOOT wake can still have the button held (level-triggered wake).
    if (wake == BOARD_WAKE_BOOT_BUTTON) {
        board_wait_boot_button_release(BUTTON_RELEASE_MS);
    }

    board_deep_sleep_seconds(fetch_failed ? SLEEP_RETRY : seconds_until_refresh());
}
