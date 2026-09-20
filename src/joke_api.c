#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "joke_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "joke_api";

// No API key needed. Returns an array of 10 unique {"setup", "punchline"} jokes.
// Plain HTTP on purpose: the data is public, and the HTTPS chain (GTS Root R1
// cross-signed by GlobalSign) fails the ESP certificate bundle.
#define JOKES_URL "http://official-joke-api.appspot.com/random_ten"

#define RESPONSE_MAX 4096
#define CONNECT_ATTEMPTS 3

// Anything before this is a clock that was never set (2023-11-14).
#define MIN_VALID_EPOCH 1700000000

typedef struct {
    char *buf;
    int   len;
} response_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    response_t *r = (response_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && r) {
        // Truncate rather than overflow; these bodies are a few hundred bytes.
        int space = RESPONSE_MAX - 1 - r->len;
        int n = evt->data_len < space ? evt->data_len : space;
        if (n > 0) {
            memcpy(r->buf + r->len, evt->data, n);
            r->len += n;
            r->buf[r->len] = '\0';
        }
    }
    return ESP_OK;
}

// GETs `url` and returns the parsed JSON, or NULL.
// The caller frees the result with cJSON_Delete().
static cJSON *get_json(const char *url)
{
    char *body = calloc(1, RESPONSE_MAX);
    if (!body) {
        ESP_LOGE(TAG, "out of memory for response buffer");
        return NULL;
    }
    response_t resp = { .buf = body, .len = 0 };

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .timeout_ms = 15000,
    };

    // Right after WiFi comes up, DNS or the first socket can fail transiently
    // (ESP_ERR_HTTP_CONNECT), so retry a few times before giving up.
    esp_err_t err = ESP_FAIL;
    int status = 0;
    for (int attempt = 1; attempt <= CONNECT_ATTEMPTS; attempt++) {
        resp.len = 0;
        body[0] = '\0';
        esp_http_client_handle_t client = esp_http_client_init(&config);
        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err == ESP_OK && status == 200) break;
        ESP_LOGW(TAG, "GET %s attempt %d/%d failed: %s, status=%d", url, attempt,
                 CONNECT_ATTEMPTS, esp_err_to_name(err), status);
        if (attempt < CONNECT_ATTEMPTS) vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "GET %s failed: %s, status=%d", url, esp_err_to_name(err), status);
        free(body);
        return NULL;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse failed for %s", url);
    }
    return root;
}

static bool starts_with(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

// Copies `src` into `dst` as printable ASCII. The fonts have no glyphs outside
// 0x20-0x7E, so common Unicode punctuation is mapped to look-alikes, HTML
// entities are decoded, and anything else becomes '?'.
static void sanitise(char *dst, size_t dst_len, const char *src)
{
    size_t o = 0;
    bool last_space = true;   // collapses runs and trims leading whitespace

    #define PUT(c) do { if (o + 1 < dst_len) dst[o++] = (c); } while (0)
    #define PUTS(str) do { for (const char *q_ = (str); *q_; q_++) PUT(*q_); last_space = false; } while (0)

    for (const unsigned char *p = (const unsigned char *)src; *p; ) {
        if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
            if (!last_space) PUT(' ');
            last_space = true;
            p++;
        } else if (*p == '&') {
            const char *s = (const char *)p;
            if      (starts_with(s, "&quot;")) { PUTS("\""); p += 6; }
            else if (starts_with(s, "&amp;"))  { PUTS("&");  p += 5; }
            else if (starts_with(s, "&lt;"))   { PUTS("<");  p += 4; }
            else if (starts_with(s, "&gt;"))   { PUTS(">");  p += 4; }
            else if (starts_with(s, "&#039;") || starts_with(s, "&#39;") ||
                     starts_with(s, "&apos;")) {
                PUTS("'");
                p += starts_with(s, "&#039;") ? 6 : (starts_with(s, "&#39;") ? 5 : 6);
            } else { PUTS("&"); p++; }
        } else if (*p >= 0x20 && *p < 0x7F) {
            PUT((char)*p);
            last_space = false;
            p++;
        } else if (*p >= 0xC0) {
            // Multi-byte UTF-8: decode the code point, then map it.
            int extra = *p >= 0xF0 ? 3 : (*p >= 0xE0 ? 2 : 1);
            unsigned cp = *p & (0x3F >> extra);
            p++;
            for (int i = 0; i < extra && (*p & 0xC0) == 0x80; i++, p++) {
                cp = (cp << 6) | (*p & 0x3F);
            }
            switch (cp) {
                case 0x2018: case 0x2019: case 0x201B: case 0x2032: PUTS("'");   break;
                case 0x201C: case 0x201D: case 0x2033:              PUTS("\"");  break;
                case 0x2010: case 0x2011: case 0x2012: case 0x2013:
                case 0x2014: case 0x2015: case 0x2212:              PUTS("-");   break;
                case 0x2026:                                        PUTS("..."); break;
                case 0x00A0: case 0x2009: case 0x200A: case 0x202F:
                    if (!last_space) PUT(' ');
                    last_space = true;
                    break;
                case 0x00E9: case 0x00E8: case 0x00EA: case 0x00EB:  PUTS("e"); break;
                case 0x00E1: case 0x00E0: case 0x00E2: case 0x00E4:
                case 0x00E3: case 0x00E5:                            PUTS("a"); break;
                case 0x00ED: case 0x00EC: case 0x00EE: case 0x00EF:  PUTS("i"); break;
                case 0x00F3: case 0x00F2: case 0x00F4: case 0x00F6:
                case 0x00F5:                                         PUTS("o"); break;
                case 0x00FA: case 0x00F9: case 0x00FB: case 0x00FC:  PUTS("u"); break;
                case 0x00F1:                                         PUTS("n"); break;
                case 0x00E7:                                         PUTS("c"); break;
                default: PUTS("?"); break;
            }
        } else {
            // Stray continuation byte or control character.
            PUTS("?");
            p++;
        }
    }
    #undef PUTS
    #undef PUT

    if (o > 0 && dst[o - 1] == ' ') o--;   // trim trailing space
    dst[o] = '\0';
}

static bool copy_string_field(cJSON *obj, const char *name, char *dst, size_t dst_len)
{
    cJSON *item = cJSON_GetObjectItem(obj, name);
    if (!cJSON_IsString(item) || !item->valuestring) {
        dst[0] = '\0';
        return false;
    }
    sanitise(dst, dst_len, item->valuestring);
    return true;
}

bool joke_fetch_all(content_t *out)
{
    content_t tmp;
    memset(&tmp, 0, sizeof tmp);

    cJSON *root = get_json(JOKES_URL);
    if (!root) return false;

    int n = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        if (n >= JOKE_COUNT) break;
        char setup[200], punchline[200];
        if (!copy_string_field(item, "setup", setup, sizeof setup) ||
            !copy_string_field(item, "punchline", punchline, sizeof punchline) ||
            !setup[0] || !punchline[0]) {
            continue;
        }
        snprintf(tmp.jokes[n], sizeof tmp.jokes[n], "%s %s", setup, punchline);
        n++;
    }
    cJSON_Delete(root);

    if (n < JOKE_COUNT) {
        ESP_LOGE(TAG, "only %d usable jokes in response", n);
        return false;
    }

    time_t now = time(NULL);
    tmp.fetched_at = now > MIN_VALID_EPOCH ? now : 0;
    tmp.valid = true;
    tmp.page = 0;   // a new set of jokes starts at the first one

    *out = tmp;
    for (int i = 0; i < JOKE_COUNT; i++) {
        ESP_LOGI(TAG, "joke %d: %s", i + 1, out->jokes[i]);
    }
    return true;
}
