#include "board_power.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char *TAG = "power";

static board_wake_t s_wake = BOARD_WAKE_COLD_BOOT;

#define EPD_PWR_PIN     GPIO_NUM_6     // active LOW
#define VBAT_PWR_PIN    GPIO_NUM_17    // active HIGH -- battery power latch
#define PWR_BUTTON_PIN  GPIO_NUM_18    // active LOW (pressed = 0), internal pull-up
#define BOOT_BUTTON_PIN GPIO_NUM_0     // active LOW; the non-power button (cycles content)

#define SHUTDOWN_HOLD_MS 1500

void board_power_init(void)
{
    // Read the wake source first, before pin state gets reconfigured.
    uint32_t causes = esp_sleep_get_wakeup_causes();
    if (causes & BIT(ESP_SLEEP_WAKEUP_EXT1)) {
        uint64_t pins = esp_sleep_get_ext1_wakeup_status();
        s_wake = (pins & (1ULL << BOOT_BUTTON_PIN)) ? BOARD_WAKE_BOOT_BUTTON
                                                    : BOARD_WAKE_PWR_BUTTON;
    } else if (causes & BIT(ESP_SLEEP_WAKEUP_TIMER)) {
        s_wake = BOARD_WAKE_TIMER;
    } else {
        s_wake = BOARD_WAKE_COLD_BOOT;
    }

    // A previous deep sleep left pin states latched; release them before we
    // reconfigure anything, or gpio_config() silently won't take effect.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis(VBAT_PWR_PIN);

    gpio_config_t c = {};
    c.intr_type = GPIO_INTR_DISABLE;
    c.mode = GPIO_MODE_OUTPUT;
    c.pin_bit_mask = 1ULL << VBAT_PWR_PIN;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    c.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&c));

    // Hold the battery rail on. On USB this is harmless; on battery, skipping
    // it means the board powers off as soon as the PWR button is released.
    gpio_set_level(VBAT_PWR_PIN, 1);

    gpio_config_t btn = {};
    btn.intr_type = GPIO_INTR_DISABLE;
    btn.mode = GPIO_MODE_INPUT;
    btn.pin_bit_mask = 1ULL << PWR_BUTTON_PIN;
    btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&btn));

    gpio_config_t boot = btn;
    boot.pin_bit_mask = 1ULL << BOOT_BUTTON_PIN;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&boot));

    static const char *names[] = { "cold boot", "deep-sleep timer", "BOOT button", "PWR button" };
    ESP_LOGI(TAG, "boot (%s)", names[s_wake]);
}

board_wake_t board_wake_reason(void)
{
    return s_wake;
}

void board_wait_boot_button_release(int timeout_ms)
{
    // ext1 wake is level-triggered: sleeping while the button is still down
    // would wake us again immediately.
    int waited = 0;
    while (gpio_get_level(BOOT_BUTTON_PIN) == 0 && waited < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(20));
        waited += 20;
    }
    vTaskDelay(pdMS_TO_TICKS(50));   // let contact bounce settle
}

bool board_power_handle_shutdown_button(void)
{
    if (gpio_get_level(PWR_BUTTON_PIN) != 0) {
        return false;  // not pressed
    }

    ESP_LOGI(TAG, "PWR held at boot, checking for long-press shutdown");
    int held_ms = 0;
    while (gpio_get_level(PWR_BUTTON_PIN) == 0 && held_ms < SHUTDOWN_HOLD_MS) {
        vTaskDelay(pdMS_TO_TICKS(50));
        held_ms += 50;
    }

    if (held_ms < SHUTDOWN_HOLD_MS) {
        ESP_LOGI(TAG, "PWR released after %d ms -- not a shutdown", held_ms);
        return false;
    }

    ESP_LOGW(TAG, "PWR held %d ms -- powering off", held_ms);
    gpio_set_level(EPD_PWR_PIN, 1);    // panel rail off too
    gpio_set_level(VBAT_PWR_PIN, 0);   // release the latch: board loses power

    // If still alive (e.g. running on USB, no battery), there is nothing left
    // to do -- just sit here rather than falling through into a normal cycle.
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void board_deep_sleep_seconds(uint64_t seconds)
{
    // E-paper keeps its image with no power, so cut the panel rail.
    gpio_set_level(EPD_PWR_PIN, 1);

    // Keep the battery latch asserted while the CPU is asleep, otherwise the
    // board switches itself off and never wakes.
    gpio_hold_en(VBAT_PWR_PIN);
    gpio_deep_sleep_hold_en();

    // Both buttons wake the board. BOOT cycles the content; PWR is armed so a
    // long-press during sleep is caught by board_power_handle_shutdown_button()
    // on the next boot rather than requiring a real power cycle. The pull-ups
    // have to be set on the RTC side to survive deep sleep.
    const gpio_num_t wake_pins[] = { BOOT_BUTTON_PIN, PWR_BUTTON_PIN };
    uint64_t wake_mask = 0;
    for (size_t i = 0; i < sizeof wake_pins / sizeof wake_pins[0]; i++) {
        rtc_gpio_init(wake_pins[i]);
        rtc_gpio_set_direction(wake_pins[i], RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pulldown_dis(wake_pins[i]);
        rtc_gpio_pullup_en(wake_pins[i]);
        wake_mask |= 1ULL << wake_pins[i];
    }
    esp_sleep_enable_ext1_wakeup_io(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);

    ESP_LOGI(TAG, "deep sleeping for %llu seconds (BOOT + PWR wake armed)", seconds);
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    esp_deep_sleep_start();
}
