# yuktrivia

Firmware for the **Waveshare ESP32-S3-ePaper-1.54G**, a 200×200 four-colour
(black / white / red / yellow) e-paper board with an ESP32-S3, 8 MB flash and
8 MB PSRAM.

Shows three jokes from [official-joke-api](https://official-joke-api.appspot.com)
(no API key) and cycles through them with the **BOOT** button (the non-power
button): joke 1, joke 2, joke 3, then back to 1. The dots at the bottom left
show which joke you're on.

Every page has an `Updated <date> <time>` footer showing when the API was last
called. The three jokes are fetched on first boot and once a day after local midnight,
then cached in RTC memory. A BOOT tap wakes the board, redraws the next page
from the cache (no WiFi, ~15 s panel refresh) and goes back to sleep. Taps
during that refresh are ignored. See [REQUIREMENTS.md](REQUIREMENTS.md).

## Layout

```
platformio.ini          Board, port, flash size, partitions (PlatformIO + ESP-IDF)
sdkconfig.defaults      Minimal ESP-IDF config (PSRAM, TLS cert bundle)
partitions.csv          3 MB factory app partition
src/
  main.cpp              Wake dispatcher: cold boot / timer / BOOT tap
  joke_api.c            Joke API request, JSON parsing, text cleanup
  ui.c                  Page layout: header, wrapped body, dots, footer
  content.h             The cached content struct (three jokes)
  battery.c             Battery voltage -> percent (GPIO4 / ADC1 ch3)
  display.c             Panel power, framebuffer, push, scaled text
  board_power.c         Battery latch, PWR-button power-off, deep sleep
  wifi_sta.c            WiFi station connect (credentials from secrets.h)
  time_sync.c           SNTP + US Pacific timezone
  epaper_port.c         Panel driver  ┐ from the vendor SDK's
  GUI_Paint.c, Fonts/   Drawing/fonts ┘ ESP-IDF example 09_E_Paper_Test
  idf_component.yml     Managed components (cJSON)
ESP32-S3-ePaper-1.54G/  Vendor SDK clone (gitignored, reference only)
```

The vendor SDK has examples for the RTC, temperature/humidity sensor, SD card,
audio and battery ADC under `ESP32-S3-ePaper-1.54G/Example/ESP-IDF_5.5.1/`. To
re-fetch it:

```bash
git clone https://github.com/waveshareteam/ESP32-S3-ePaper-1.54G.git
```

> Use the **1.54G** SDK only. The monochrome ESP32-S3-ePaper-1.54 SDK uses a
> different panel controller and won't drive this display.

## Build and flash

Requires [PlatformIO](https://platformio.org).

```bash
cp src/secrets.example.h src/secrets.h   # then set WIFI_SSID, WIFI_PASSWORD
pio run -t upload
pio device monitor
```

`src/secrets.h` is gitignored; the WiFi password is compiled into the firmware, so
don't share the built `.bin`.

Check `upload_port` / `monitor_port` in `platformio.ini` against
`ls /dev/cu.usbmodem*`.

If the board is deep sleeping, auto-reset won't reach the bootloader. Power it
off (hold **PWR** ~1.5 s, or disconnect the battery), then hold **BOOT** while
plugging in USB, keep holding ~2 s, release, and upload straight away.

## Hardware notes

- Colours: `EPD_1IN54G_BLACK/WHITE/YELLOW/RED` = 0/1/2/3. The framebuffer is
  2 bits per pixel, `(200 / 4) × 200` bytes, allocated in PSRAM.
- A full refresh takes 10–20 s. `display_init()` must run only once per boot.
- GPIO6 = panel power (active low), GPIO17 = battery latch, GPIO18 = PWR button,
  GPIO4 = battery voltage (2:1 divider). A red `LOW BATTERY` warning shows at 15% or lower.
- A new panel ships with a factory demo image that persists with no power.
