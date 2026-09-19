#include "battery.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "battery";

#define BATTERY_ADC_CHANNEL ADC_CHANNEL_3   // GPIO4 on the ESP32-S3
#define DIVIDER_RATIO       2               // battery -> pin is halved
#define SAMPLES             16

// Single-cell Li-ion, voltage (mV) -> percent. Piecewise-linear between points.
// Voltage is only a rough guide to charge, but it's enough for a low warning.
static const struct { int mv; int pct; } CURVE[] = {
    { 4200, 100 },
    { 4110,  90 },
    { 4020,  80 },
    { 3950,  70 },
    { 3870,  60 },
    { 3840,  50 },
    { 3800,  40 },
    { 3770,  30 },
    { 3730,  20 },
    { 3710,  15 },
    { 3690,  10 },
    { 3610,   5 },
    { 3270,   0 },
};
#define CURVE_LEN ((int)(sizeof CURVE / sizeof CURVE[0]))

int battery_percent_from_mv(int mv)
{
    if (mv >= CURVE[0].mv) return 100;
    if (mv <= CURVE[CURVE_LEN - 1].mv) return 0;

    for (int i = 1; i < CURVE_LEN; i++) {
        if (mv >= CURVE[i].mv) {
            int span_mv  = CURVE[i - 1].mv  - CURVE[i].mv;
            int span_pct = CURVE[i - 1].pct - CURVE[i].pct;
            return CURVE[i].pct + (mv - CURVE[i].mv) * span_pct / span_mv;
        }
    }
    return 0;
}

int battery_read_percent(void)
{
    adc_oneshot_unit_handle_t adc = NULL;
    adc_cali_handle_t cali = NULL;
    int result = -1;

    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    if (adc_oneshot_new_unit(&unit_cfg, &adc) != ESP_OK) {
        ESP_LOGW(TAG, "ADC init failed");
        return -1;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_oneshot_config_channel(adc, BATTERY_ADC_CHANNEL, &chan_cfg) != ESP_OK ||
        adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali) != ESP_OK) {
        ESP_LOGW(TAG, "ADC channel/calibration setup failed");
        goto done;
    }

    int total_mv = 0, good = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int raw = 0, mv = 0;
        if (adc_oneshot_read(adc, BATTERY_ADC_CHANNEL, &raw) == ESP_OK &&
            adc_cali_raw_to_voltage(cali, raw, &mv) == ESP_OK) {
            total_mv += mv;
            good++;
        }
    }
    if (good == 0) {
        ESP_LOGW(TAG, "no valid ADC samples");
        goto done;
    }

    int battery_mv = total_mv / good * DIVIDER_RATIO;
    result = battery_percent_from_mv(battery_mv);
    ESP_LOGI(TAG, "battery %d mV -> %d%%", battery_mv, result);

done:
    if (cali) adc_cali_delete_scheme_curve_fitting(cali);
    adc_oneshot_del_unit(adc);
    return result;
}
