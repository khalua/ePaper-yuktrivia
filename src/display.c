#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "display";

#define EPD_PWR_PIN GPIO_NUM_6   // active LOW

// 4 colours = 2 bits per pixel.
#define IMAGE_SIZE ((DISPLAY_W / 4) * DISPLAY_H)

static uint8_t *s_image;

static void epaper_power_up(void)
{
    gpio_config_t c = {};
    c.intr_type = GPIO_INTR_DISABLE;
    c.mode = GPIO_MODE_OUTPUT;
    c.pin_bit_mask = 1ULL << EPD_PWR_PIN;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    c.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&c));
    gpio_set_level(EPD_PWR_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
}

bool display_init(bool clear_panel)
{
    epaper_power_up();

    s_image = (uint8_t *)heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_image) {
        ESP_LOGE(TAG, "failed to allocate framebuffer");
        return false;
    }

    epaper_port_init();
    if (clear_panel) {
        epaper_port_clear(EPD_1IN54G_WHITE);
    }

    Paint_NewImage(s_image, DISPLAY_W, DISPLAY_H, 0, EPD_1IN54G_WHITE);
    Paint_SetScale(4);
    Paint_SelectImage(s_image);
    Paint_Clear(EPD_1IN54G_WHITE);
    return true;
}

void display_push(void)
{
    epaper_port_display(s_image);
}

void display_shutdown(void)
{
    epaper_port_sleep();
}

// Expands each glyph pixel into a scale x scale block, straight into the
// framebuffer.
void display_text_scaled(int x, int y, const char *s, sFONT *font,
                         uint8_t colour, int scale)
{
    int bytes_per_row = (font->Width + 7) / 8;

    for (const char *p = s; *p; p++) {
        uint32_t offset = (uint32_t)(*p - ' ') * font->Height * bytes_per_row;
        const unsigned char *glyph = &font->table[offset];

        for (int row = 0; row < font->Height; row++) {
            for (int col = 0; col < font->Width; col++) {
                unsigned char byte = glyph[row * bytes_per_row + col / 8];
                if (!(byte & (0x80 >> (col % 8))))
                    continue;

                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        int px = x + col * scale + dx;
                        int py = y + row * scale + dy;
                        if (px >= 0 && px < DISPLAY_W && py >= 0 && py < DISPLAY_H)
                            Paint_SetPixel(px, py, colour);
                    }
                }
            }
        }
        x += font->Width * scale;
    }
}
