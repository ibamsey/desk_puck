#include "wifi/wifi_store.h"

#include <Preferences.h>
#include <stdio.h>
#include <string.h>

static Preferences s_prefs;
static WifiCredential s_cache[WIFI_STORE_MAX_NETWORKS];
static size_t s_count = 0;
static bool s_loaded = false;

static void load_from_nvs() {
    if (s_loaded) {
        return;
    }
    s_count = 0;
    if (!s_prefs.begin("wifi", true)) {
        s_loaded = true;
        return;
    }
    const uint8_t n = s_prefs.getUChar("count", 0);
    const size_t cap = n > WIFI_STORE_MAX_NETWORKS ? WIFI_STORE_MAX_NETWORKS : n;
    for (size_t i = 0; i < cap; ++i) {
        char key_ssid[8];
        char key_pass[8];
        snprintf(key_ssid, sizeof(key_ssid), "s%u", (unsigned)i);
        snprintf(key_pass, sizeof(key_pass), "p%u", (unsigned)i);
        String ssid = s_prefs.getString(key_ssid, "");
        String pass = s_prefs.getString(key_pass, "");
        if (ssid.length() == 0) {
            continue;
        }
        strncpy(s_cache[s_count].ssid, ssid.c_str(), WIFI_STORE_SSID_MAX);
        s_cache[s_count].ssid[WIFI_STORE_SSID_MAX] = '\0';
        strncpy(s_cache[s_count].password, pass.c_str(), WIFI_STORE_PASS_MAX);
        s_cache[s_count].password[WIFI_STORE_PASS_MAX] = '\0';
        s_count++;
    }
    s_prefs.end();
    s_loaded = true;
}

static bool save_to_nvs() {
    if (!s_prefs.begin("wifi", false)) {
        return false;
    }
    s_prefs.putUChar("count", (uint8_t)s_count);
    for (size_t i = 0; i < WIFI_STORE_MAX_NETWORKS; ++i) {
        char key_ssid[8];
        char key_pass[8];
        snprintf(key_ssid, sizeof(key_ssid), "s%u", (unsigned)i);
        snprintf(key_pass, sizeof(key_pass), "p%u", (unsigned)i);
        if (i < s_count) {
            s_prefs.putString(key_ssid, s_cache[i].ssid);
            s_prefs.putString(key_pass, s_cache[i].password);
        } else {
            s_prefs.remove(key_ssid);
            s_prefs.remove(key_pass);
        }
    }
    s_prefs.end();
    return true;
}

static int find_ssid_index(const char* ssid) {
    for (size_t i = 0; i < s_count; ++i) {
        if (strcmp(s_cache[i].ssid, ssid) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void wifi_store_begin() {
    load_from_nvs();
}

size_t wifi_store_count() {
    load_from_nvs();
    return s_count;
}

bool wifi_store_get(size_t index, WifiCredential& out) {
    load_from_nvs();
    if (index >= s_count) {
        return false;
    }
    out = s_cache[index];
    return true;
}

bool wifi_store_add(const char* ssid, const char* password) {
    if (!ssid || ssid[0] == '\0') {
        return false;
    }
    load_from_nvs();
    const int idx = find_ssid_index(ssid);
    if (idx >= 0) {
        strncpy(s_cache[idx].password, password ? password : "", WIFI_STORE_PASS_MAX);
        s_cache[idx].password[WIFI_STORE_PASS_MAX] = '\0';
        return save_to_nvs();
    }
    if (s_count >= WIFI_STORE_MAX_NETWORKS) {
        return false;
    }
    strncpy(s_cache[s_count].ssid, ssid, WIFI_STORE_SSID_MAX);
    s_cache[s_count].ssid[WIFI_STORE_SSID_MAX] = '\0';
    strncpy(s_cache[s_count].password, password ? password : "", WIFI_STORE_PASS_MAX);
    s_cache[s_count].password[WIFI_STORE_PASS_MAX] = '\0';
    s_count++;
    return save_to_nvs();
}

bool wifi_store_remove(const char* ssid) {
    load_from_nvs();
    const int idx = find_ssid_index(ssid);
    if (idx < 0) {
        return false;
    }
    for (size_t i = (size_t)idx + 1; i < s_count; ++i) {
        s_cache[i - 1] = s_cache[i];
    }
    s_count--;
    return save_to_nvs();
}

void wifi_store_promote(const char* ssid) {
    const int idx = find_ssid_index(ssid);
    if (idx <= 0) {
        return;
    }
    const WifiCredential tmp = s_cache[idx];
    for (int i = idx; i > 0; --i) {
        s_cache[i] = s_cache[i - 1];
    }
    s_cache[0] = tmp;
    save_to_nvs();
}
