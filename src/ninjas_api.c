#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "ninjas_api.h"
#include "secrets.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "ninjas";

#define JOKE_URL   "https://api.api-ninjas.com/v1/jokeoftheday"
#define TRIVIA_URL "https://api.api-ninjas.com/v1/triviaoftheday"

#define RESPONSE_MAX 2048

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

// GETs `url` with the API key and returns the parsed JSON, or NULL.
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
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "X-Api-Key", NINJAS_API_KEY);
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

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

// Both endpoints return a one-element array, but accept a bare object too.
static cJSON *first_object(cJSON *root)
{
    if (cJSON_IsArray(root)) {
        return cJSON_GetArrayItem(root, 0);
    }
    return cJSON_IsObject(root) ? root : NULL;
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

bool ninjas_fetch_all(content_t *out)
{
    content_t tmp;
    memset(&tmp, 0, sizeof tmp);

    cJSON *joke_root = get_json(JOKE_URL);
    if (!joke_root) return false;
    cJSON *joke = first_object(joke_root);
    bool joke_ok = joke && copy_string_field(joke, "joke", tmp.joke, sizeof tmp.joke);
    cJSON_Delete(joke_root);
    if (!joke_ok || !tmp.joke[0]) {
        ESP_LOGE(TAG, "no \"joke\" in response");
        return false;
    }

    cJSON *trivia_root = get_json(TRIVIA_URL);
    if (!trivia_root) return false;
    cJSON *trivia = first_object(trivia_root);
    bool trivia_ok = trivia &&
        copy_string_field(trivia, "question", tmp.question, sizeof tmp.question) &&
        copy_string_field(trivia, "answer", tmp.answer, sizeof tmp.answer);
    // The category is sometimes empty or missing; the UI falls back to "TRIVIA".
    if (trivia) copy_string_field(trivia, "category", tmp.category, sizeof tmp.category);
    cJSON_Delete(trivia_root);
    if (!trivia_ok || !tmp.question[0] || !tmp.answer[0]) {
        ESP_LOGE(TAG, "missing question/answer in trivia response");
        return false;
    }

    time_t now = time(NULL);
    tmp.fetched_at = now > MIN_VALID_EPOCH ? now : 0;
    tmp.valid = true;
    tmp.page = out->page;   // a refetch shouldn't move the user off their page

    *out = tmp;
    ESP_LOGI(TAG, "joke: %s", out->joke);
    ESP_LOGI(TAG, "trivia [%s]: %s -> %s", out->category, out->question, out->answer);
    return true;
}
