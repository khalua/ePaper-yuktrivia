# yuktrivia -- Requirements

## Purpose
A desk display that shows a fresh joke and trivia question each day from
[API Ninjas](https://api-ninjas.com), on the Waveshare ESP32-S3-ePaper-1.54G.

## Content
Three pages, cycled in order and wrapping back to the first:

1. **Joke of the day**: `GET /v1/jokeoftheday`
2. **Trivia of the day**: `GET /v1/triviaoftheday`, showing the category and the question
3. **Trivia answer**: the answer from the same trivia response

Both endpoints need the `X-Api-Key` header. The key is stored only in the
gitignored `src/secrets.h`.

## Screen layout
200x200 px, black / white / red / yellow.
- Coloured header bar with the page title (yellow for the joke and trivia pages, red for the answer). The trivia header shows the API's category, or "TRIVIA" if it's empty.
- Word-wrapped body, in the largest font that fits.
- Page-position dots at the left, and a "Top button" hint on the right after a cold boot.
- A red `LOW BATTERY nn%` warning on the right of the dots row when charge is 15% or lower (replaces the hint). Nothing is shown above 15%.
- Footer on every page: `Updated <date> <time>` (US Pacific), the time the API was last called and the data refreshed.

## Update behaviour
- Content is fetched on first boot and then once a day, shortly after local midnight, and cached in RTC memory.
- A failed refresh keeps the previous content on screen and retries in 15 minutes.
- With no cached content and no WiFi, an error page is shown.

## Input
- **BOOT button** (GPIO0, the non-power button): advances to the next page. It wakes the board from deep sleep and redraws from the cache with no network access.
- **PWR button**: unchanged; hold ~1.5 s to power off.
- Taps during the 10-20 s panel refresh are ignored.

## Power
Battery-friendly: the board deep sleeps between events and the panel keeps its image with no power.

The battery is read from the on-board divider (GPIO4, ADC1 channel 3) at every wake, before WiFi starts. Charge is estimated from voltage, so it is approximate. The warning only appears when the screen redraws (a tap or the daily refresh). Charging can't be detected, and on USB power the reading shows as full.

## Out of scope
Anything beyond these three pages; other API Ninjas endpoints; touch input.
