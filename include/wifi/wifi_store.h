#ifndef WIFI_WIFI_STORE_H
#define WIFI_WIFI_STORE_H

#include <stddef.h>
#include <stdint.h>

/** Max saved networks (multi-location). */
#define WIFI_STORE_MAX_NETWORKS 5
#define WIFI_STORE_SSID_MAX 32
#define WIFI_STORE_PASS_MAX 64

struct WifiCredential {
    char ssid[WIFI_STORE_SSID_MAX + 1];
    char password[WIFI_STORE_PASS_MAX + 1];
};

void wifi_store_begin();

size_t wifi_store_count();

bool wifi_store_get(size_t index, WifiCredential& out);

/** Add new or update password for existing SSID. Returns false if list full (new SSID). */
bool wifi_store_add(const char* ssid, const char* password);

bool wifi_store_remove(const char* ssid);

/** Move SSID to front after successful connect (most-recent priority). */
void wifi_store_promote(const char* ssid);

#endif // WIFI_WIFI_STORE_H
