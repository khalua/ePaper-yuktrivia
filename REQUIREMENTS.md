# yuktrivia -- Requirements

## Purpose
A desk display that shows three jokes each day from
[official-joke-api](https://official-joke-api.appspot.com), on the Waveshare ESP32-S3-ePaper-1.54G.

## Content
Three pages, one joke each, cycled in order and wrapping back to the first.
The jokes come from one request: `GET https://official-joke-api.appspot.com/random_ten`
(no API key). The first three usable jokes are kept, each shown as setup + punchline.

## Screen layout
200x200 px, black / white / red / yellow.
- Yellow header bar reading `JOKE n/3`.
- Word-wrapped body, in the largest font that fits.
- Three page-position dots at the left showing which joke is on screen, and a "Top button" hint on the right after a cold boot.
- A red `LOW BATTERY nn%` warning on the right of the dots row when charge is 15% or lower (replaces the hint). Nothing is shown above 15%.
- Footer on every page: `Updated <date> <time>` (US Pacific), the time the API was last called and the data refreshed.

## Update behaviour
- Three jokes are fetched on first boot and then once a day, shortly after local midnight, and cached in RTC memory. A refresh returns to joke 1.
- A failed refresh keeps the previous content on screen and retries in 15 minutes.
- With no cached content and no WiFi, an error page is shown.

## Input
- **BOOT button** (GPIO0, the non-power button): advances to the next joke. It wakes the board from deep sleep and redraws from the cache with no network access.
- **PWR button**: unchanged; hold ~1.5 s to power off.
- Taps during the 10-20 s panel refresh are ignored.

## Power
Battery-friendly: the board deep sleeps between events and the panel keeps its image with no power.

The battery is read from the on-board divider (GPIO4, ADC1 channel 3) at every wake, before WiFi starts. Charge is estimated from voltage, so it is approximate. The warning only appears when the screen redraws (a tap or the daily refresh). Charging can't be detected, and on USB power the reading shows as full.

## Out of scope
Anything beyond these three pages; touch input.
