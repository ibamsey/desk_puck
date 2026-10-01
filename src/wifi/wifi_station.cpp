#include "wifi/wifi_station.h"

#include "wifi/wifi_store.h"

#include <WiFi.h>

enum class StaState {
    Idle,
    Scanning,
    Connecting,
    Connected,
    Backoff,
};

static StaState s_state = StaState::Idle;
static unsigned long s_next_action_ms = 0;
static unsigned long s_connect_started_ms = 0;
static char s_connected_ssid[WIFI_STORE_SSID_MAX + 1] = {};
static int s_target_index = -1;

static void clear_connected_ssid() {
    s_connected_ssid[0] = '\0';
}

static bool pick_best_network(int& out_index, int32_t& out_rssi) {
    out_index = -1;
    out_rssi = -1000;
    const int n = WiFi.scanNetworks(false, true);
    if (n <= 0) {
        return false;
    }
    for (size_t si = 0; si < wifi_store_count(); ++si) {
        WifiCredential cred;
        if (!wifi_store_get(si, cred)) {
            continue;
        }
        for (int i = 0; i < n; ++i) {
            if (WiFi.SSID(i) != cred.ssid) {
                continue;
            }
            const int32_t rssi = WiFi.RSSI(i);
            if (rssi > out_rssi) {
                out_rssi = rssi;
                out_index = (int)si;
            }
        }
    }
    WiFi.scanDelete();
    return out_index >= 0;
}

void wifi_station_begin() {
    wifi_store_begin();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    s_state = StaState::Idle;
    s_next_action_ms = millis();
    clear_connected_ssid();
}

void wifi_station_request_reconnect() {
    if (WiFi.isConnected()) {
        WiFi.disconnect(false, false);
    }
    s_state = StaState::Idle;
    s_next_action_ms = millis();
    clear_connected_ssid();
}

void wifi_station_poll() {
    const unsigned long now = millis();

    if (WiFi.isConnected()) {
        if (s_state != StaState::Connected) {
            s_state = StaState::Connected;
            strncpy(s_connected_ssid, WiFi.SSID().c_str(), WIFI_STORE_SSID_MAX);
            s_connected_ssid[WIFI_STORE_SSID_MAX] = '\0';
            wifi_store_promote(s_connected_ssid);
            Serial.printf("[WIFI] connected ssid=%s ip=%s rssi=%d\n", s_connected_ssid,
                          WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
        return;
    }

    if (s_state == StaState::Connected) {
        Serial.println("[WIFI] disconnected");
        clear_connected_ssid();
        s_state = StaState::Backoff;
        s_next_action_ms = now + 3000;
    }

    if (now < s_next_action_ms) {
        return;
    }

    if (wifi_store_count() == 0) {
        s_state = StaState::Backoff;
        s_next_action_ms = now + 10000;
        return;
    }

    switch (s_state) {
        case StaState::Idle:
        case StaState::Backoff:
            s_state = StaState::Scanning;
            WiFi.scanNetworks(true, true);
            s_next_action_ms = now + 100;
            break;

        case StaState::Scanning: {
            const int pending = WiFi.scanComplete();
            if (pending == WIFI_SCAN_RUNNING) {
                s_next_action_ms = now + 50;
                break;
            }
            int32_t best_rssi = -1000;
            if (pending >= 0) {
                pick_best_network(s_target_index, best_rssi);
            } else {
                WiFi.scanNetworks(true, true);
                s_next_action_ms = now + 100;
                break;
            }
            if (s_target_index < 0) {
                s_state = StaState::Backoff;
                s_next_action_ms = now + 15000;
                break;
            }
            WifiCredential cred;
            if (!wifi_store_get((size_t)s_target_index, cred)) {
                s_state = StaState::Backoff;
                s_next_action_ms = now + 5000;
                break;
            }
            Serial.printf("[WIFI] connecting ssid=%s rssi=%ld\n", cred.ssid, (long)best_rssi);
            WiFi.begin(cred.ssid, cred.password);
            s_connect_started_ms = now;
            s_state = StaState::Connecting;
            s_next_action_ms = now + 200;
            break;
        }

        case StaState::Connecting:
            if (WiFi.isConnected()) {
                s_state = StaState::Connected;
                break;
            }
            if (now - s_connect_started_ms > 20000) {
                Serial.println("[WIFI] connect timeout");
                WiFi.disconnect(false, false);
                s_state = StaState::Backoff;
                s_next_action_ms = now + 5000;
            } else {
                s_next_action_ms = now + 200;
            }
            break;

        case StaState::Connected:
            break;
    }
}

bool wifi_station_is_connected() {
    return WiFi.isConnected();
}

const char* wifi_station_ssid() {
    return s_connected_ssid;
}

int32_t wifi_station_rssi() {
    return WiFi.isConnected() ? WiFi.RSSI() : 0;
}

void wifi_station_debug_scan() {
    const int n = WiFi.scanNetworks(false, true);
    Serial.printf("[WIFI] scan found %d networks\n", n);
    for (size_t si = 0; si < wifi_store_count(); ++si) {
        WifiCredential cred;
        if (!wifi_store_get(si, cred)) {
            continue;
        }
        bool visible = false;
        int32_t rssi = 0;
        for (int i = 0; i < n; ++i) {
            if (WiFi.SSID(i) == cred.ssid) {
                visible = true;
                rssi = WiFi.RSSI(i);
                break;
            }
        }
        Serial.printf("[WIFI]   saved[%u] %s %s rssi=%ld\n", (unsigned)si, cred.ssid,
                      visible ? "visible" : "not seen", (long)rssi);
    }
    WiFi.scanDelete();
}
