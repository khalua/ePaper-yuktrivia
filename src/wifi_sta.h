#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Brings up NVS, the network stack and Wi-Fi in station mode, then blocks
// until connected or the timeout expires. Returns true once we have an IP.
bool wifi_sta_connect(int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STA_H
