#include "wifi/serial_wifi.h"

#include "time/wall_clock.h"
#include "wifi/wifi_station.h"
#include "wifi/wifi_store.h"

#include <WiFi.h>

static char s_line[256];
static size_t s_line_len = 0;

static void print_prompt() {
    Serial.print("> ");
}

static bool parse_quoted_pair(const char* rest, char* ssid, size_t ssid_len, char* pass,
                              size_t pass_len) {
    if (rest[0] != '"') {
        return false;
    }
    const char* ssid_end = strchr(rest + 1, '"');
    if (!ssid_end || ssid_end[1] != ' ') {
        return false;
    }
    const size_t ssid_sz = (size_t)(ssid_end - (rest + 1));
    if (ssid_sz >= ssid_len) {
        return false;
    }
    memcpy(ssid, rest + 1, ssid_sz);
    ssid[ssid_sz] = '\0';
    rest = ssid_end + 2;
    if (rest[0] != '"') {
        return false;
    }
    const char* pass_end = strchr(rest + 1, '"');
    if (!pass_end) {
        return false;
    }
    const size_t pass_sz = (size_t)(pass_end - (rest + 1));
    if (pass_sz >= pass_len) {
        return false;
    }
    memcpy(pass, rest + 1, pass_sz);
    pass[pass_sz] = '\0';
    return true;
}

static void cmd_wifi_list() {
    Serial.printf("[WIFI] saved count=%u\n", (unsigned)wifi_store_count());
    for (size_t i = 0; i < wifi_store_count(); ++i) {
        WifiCredential c;
        if (wifi_store_get(i, c)) {
            Serial.printf("[WIFI]   [%u] %s\n", (unsigned)i, c.ssid);
        }
    }
}

static void cmd_wifi_add(const char* args) {
    char ssid[WIFI_STORE_SSID_MAX + 1];
    char pass[WIFI_STORE_PASS_MAX + 1];
    while (*args == ' ') {
        args++;
    }
    if (args[0] == '"') {
        if (!parse_quoted_pair(args, ssid, sizeof(ssid), pass, sizeof(pass))) {
            Serial.println("[WIFI] usage: wifi add \"SSID\" \"pass\"  OR  wifi add SSID pass");
            return;
        }
    } else {
        const char* sp = strchr(args, ' ');
        if (!sp) {
            Serial.println("[WIFI] usage: wifi add SSID pass");
            return;
        }
        const size_t sl = (size_t)(sp - args);
        if (sl >= sizeof(ssid)) {
            Serial.println("[WIFI] ssid too long");
            return;
        }
        memcpy(ssid, args, sl);
        ssid[sl] = '\0';
        strncpy(pass, sp + 1, sizeof(pass) - 1);
        pass[sizeof(pass) - 1] = '\0';
        char* nl = strchr(pass, '\r');
        if (nl) {
            *nl = '\0';
        }
        nl = strchr(pass, '\n');
        if (nl) {
            *nl = '\0';
        }
    }
    if (!wifi_store_add(ssid, pass)) {
        Serial.println("[WIFI] add failed (full or invalid)");
        return;
    }
    Serial.printf("[WIFI] added/updated %s\n", ssid);
    wifi_station_request_reconnect();
}

static void cmd_wifi_remove(const char* arg) {
    char ssid[WIFI_STORE_SSID_MAX + 1];
    while (*arg == ' ') {
        arg++;
    }
    if (arg[0] == '"') {
        const char* end = strchr(arg + 1, '"');
        if (!end) {
            Serial.println("[WIFI] usage: wifi remove SSID");
            return;
        }
        const size_t n = (size_t)(end - arg - 1);
        if (n >= sizeof(ssid)) {
            return;
        }
        memcpy(ssid, arg + 1, n);
        ssid[n] = '\0';
    } else {
        strncpy(ssid, arg, sizeof(ssid) - 1);
        ssid[sizeof(ssid) - 1] = '\0';
        char* sp = strchr(ssid, ' ');
        if (sp) {
            *sp = '\0';
        }
    }
    if (!wifi_store_remove(ssid)) {
        Serial.println("[WIFI] remove failed (not found)");
        return;
    }
    Serial.printf("[WIFI] removed %s\n", ssid);
    wifi_station_request_reconnect();
}

static void cmd_wifi_status() {
    if (wifi_station_is_connected()) {
        Serial.printf("[WIFI] connected ssid=%s ip=%s rssi=%d synced=%s\n",
                      wifi_station_ssid(), WiFi.localIP().toString().c_str(),
                      (int)wifi_station_rssi(), wall_clock_is_synced() ? "yes" : "no");
    } else {
        Serial.printf("[WIFI] disconnected saved=%u\n", (unsigned)wifi_store_count());
    }
}

static void cmd_time_status() {
    time_t epoch = time(nullptr);
    struct tm tm;
    localtime_r(&epoch, &tm);
    Serial.printf("[TIME] epoch=%lu synced=%s tz=%s\n", (unsigned long)epoch,
                  wall_clock_is_synced() ? "yes" : "no", wall_clock_tz_posix());
    if (epoch > 100000) {
        Serial.printf("[TIME] local %04d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900,
                      tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
}

static void cmd_tz_set(const char* tz) {
    while (*tz == ' ') {
        tz++;
    }
    if (tz[0] == '\0') {
        Serial.println("[TZ] usage: tz set <POSIX_TZ_string>");
        return;
    }
    if (!wall_clock_set_tz_posix(tz)) {
        Serial.println("[TZ] set failed");
        return;
    }
    Serial.printf("[TZ] ok %s\n", wall_clock_tz_posix());
}

static void dispatch_line(char* line) {
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (line[0] == '\0') {
        return;
    }
    if (strncmp(line, "wifi list", 9) == 0) {
        cmd_wifi_list();
    } else if (strncmp(line, "wifi add ", 9) == 0) {
        cmd_wifi_add(line + 9);
    } else if (strncmp(line, "wifi remove ", 12) == 0) {
        cmd_wifi_remove(line + 12);
    } else if (strncmp(line, "wifi status", 11) == 0) {
        cmd_wifi_status();
    } else if (strncmp(line, "wifi scan", 9) == 0) {
        wifi_station_debug_scan();
    } else if (strncmp(line, "tz set ", 7) == 0) {
        cmd_tz_set(line + 7);
    } else if (strncmp(line, "tz show", 7) == 0) {
        Serial.printf("[TZ] %s\n", wall_clock_tz_posix());
    } else if (strncmp(line, "time status", 11) == 0) {
        cmd_time_status();
    } else if (strncmp(line, "help", 4) == 0) {
        Serial.println("[CMD] wifi list | wifi add SSID pass | wifi add \"S\" \"p\"");
        Serial.println("[CMD] wifi remove SSID | wifi status | wifi scan");
        Serial.println("[CMD] tz set POSIX | tz show | time status | help");
    } else {
        Serial.println("[CMD] unknown (try help)");
    }
}

void serial_wifi_begin() {
    s_line_len = 0;
    Serial.println("[CMD] serial wifi/tz ready (help)");
    print_prompt();
}

void serial_wifi_poll() {
    while (Serial.available() > 0) {
        const char c = (char)Serial.read();
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            Serial.println();
            s_line[s_line_len] = '\0';
            dispatch_line(s_line);
            s_line_len = 0;
            print_prompt();
            continue;
        }
        if (c == '\b' || c == 127) {
            if (s_line_len > 0) {
                s_line_len--;
                Serial.print("\b \b");
            }
            continue;
        }
        if (c >= 32 && c < 127 && s_line_len + 1 < sizeof(s_line)) {
            s_line[s_line_len++] = c;
            Serial.write(c);
        }
    }
}
