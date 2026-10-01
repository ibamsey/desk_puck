#ifndef WIFI_WIFI_STATION_H
#define WIFI_WIFI_STATION_H

#include <Arduino.h>

void wifi_station_begin();

/** Non-blocking connect/reconnect FSM. Call every loop. */
void wifi_station_poll();

bool wifi_station_is_connected();

const char* wifi_station_ssid();

int32_t wifi_station_rssi();

/** Force rescan on next poll (e.g. after wifi add). */
void wifi_station_request_reconnect();

/** Run one WiFi scan and print visible saved SSIDs (blocking scan — debug only). */
void wifi_station_debug_scan();

#endif // WIFI_WIFI_STATION_H
