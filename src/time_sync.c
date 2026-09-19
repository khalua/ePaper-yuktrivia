#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "time_sync.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"

static const char *TAG = "time";

// US Pacific with DST rules, matching the timezone we ask Open-Meteo for.
#define TZ_PACIFIC "PST8PDT,M3.2.0/2,M11.1.0/2"

static bool s_synced;

// The system clock keeps running across deep sleep (RTC timer), so on a wake
// that skips WiFi the time is still good even though s_synced is false. Anything
// before this epoch (2023-11-14) means the clock was never set.
#define MIN_VALID_EPOCH 1700000000

static bool clock_is_set(void)
{
    return s_synced || time(NULL) > MIN_VALID_EPOCH;
}

void time_sync_init_tz(void)
{
    setenv("TZ", TZ_PACIFIC, 1);
    tzset();
}

bool time_sync_start(int timeout_ms)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sntp init failed: %s", esp_err_to_name(err));
        return false;
    }

    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms)) != ESP_OK) {
        ESP_LOGW(TAG, "clock not set within %d ms", timeout_ms);
        return false;
    }

    time_sync_init_tz();
    s_synced = true;

    char buf[32];
    time_sync_format_now(buf, sizeof(buf));
    ESP_LOGI(TAG, "clock synced, local time is %s", buf);
    return true;
}

int time_sync_local_hour(void)
{
    if (!clock_is_set()) return -1;

    time_t now = time(NULL);
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    return tm_local.tm_hour;
}

int time_sync_seconds_until_hour(int hour)
{
    if (!clock_is_set()) return -1;

    time_t now = time(NULL);
    struct tm tm_local;
    localtime_r(&now, &tm_local);

    int secs_now    = tm_local.tm_hour * 3600 + tm_local.tm_min * 60 + tm_local.tm_sec;
    int secs_target = hour * 3600;
    int delta = secs_target - secs_now;
    if (delta <= 0) delta += 24 * 3600;   // already past today, so tomorrow
    return delta;
}

void time_sync_format_now(char *out, int out_len)
{
    if (!clock_is_set()) {
        snprintf(out, out_len, "--:--");
        return;
    }

    time_t now = time(NULL);
    struct tm tm_local;
    localtime_r(&now, &tm_local);

    // 12-hour clock without a leading zero (strftime's %l pads with a space).
    int hour = tm_local.tm_hour % 12;
    if (hour == 0) hour = 12;
    snprintf(out, out_len, "%d:%02d %s",
             hour, tm_local.tm_min, tm_local.tm_hour < 12 ? "AM" : "PM");
}

void time_sync_format_time(time_t t, char *out, int out_len)
{
    if (t <= MIN_VALID_EPOCH) {
        snprintf(out, out_len, "--");
        return;
    }

    struct tm tm_local;
    localtime_r(&t, &tm_local);

    static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    int hour = tm_local.tm_hour % 12;
    if (hour == 0) hour = 12;
    snprintf(out, out_len, "%s %d %d:%02d %s",
             months[tm_local.tm_mon], tm_local.tm_mday, hour, tm_local.tm_min,
             tm_local.tm_hour < 12 ? "AM" : "PM");
}
