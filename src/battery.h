#ifndef BATTERY_H
#define BATTERY_H

#ifdef __cplusplus
extern "C" {
#endif

// The UI shows a warning at or below this charge level.
#define BATTERY_LOW_PCT 15

// Reads the battery once (averaged) and returns its charge as 0-100, or -1 if
// the ADC couldn't be read. The battery sits behind a 2:1 divider on GPIO4
// (ADC1 channel 3). Call before WiFi starts: radio transmit bursts sag the
// battery voltage and make the reading look lower than it is. On USB power the
// reading is high, so it just shows as full.
int battery_read_percent(void);

// Rough single-cell Li-ion state of charge from a resting voltage in mV.
// Exposed for testing.
int battery_percent_from_mv(int mv);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_H
