#ifndef WIFI_SERIAL_WIFI_H
#define WIFI_SERIAL_WIFI_H

void serial_wifi_begin();

/** Read serial lines and handle wifi / tz commands. */
void serial_wifi_poll();

#endif // WIFI_SERIAL_WIFI_H
