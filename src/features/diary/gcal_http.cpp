#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace gcal_http {

static WiFiClientSecure s_client;

WiFiClientSecure& client() {
    static bool inited = false;
    if (!inited) {
        // TODO: embed CA bundle (see ESP32 WiFiClientSecure README) for stricter TLS.
        s_client.setInsecure();
        inited = true;
    }
    return s_client;
}

bool post_form(const char* url, const char* body, String& response_out, int& http_code_out) {
    HTTPClient http;
    WiFiClientSecure& c = client();
    if (!http.begin(c, url)) {
        return false;
    }
    http.setTimeout(12000);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http_code_out = http.POST(body);
    if (http_code_out > 0) {
        response_out = http.getString();
    } else {
        response_out = "";
        Serial.printf("[GCAL] POST failed: %s\n", http.errorToString(http_code_out).c_str());
    }
    http.end();
    return http_code_out > 0;
}

bool get_bearer(const char* url, const char* bearer, String& response_out, int& http_code_out) {
    HTTPClient http;
    WiFiClientSecure& c = client();
    if (!http.begin(c, url)) {
        return false;
    }
    http.setTimeout(15000);
    http.addHeader("Authorization", String("Bearer ") + bearer);
    http_code_out = http.GET();
    if (http_code_out > 0) {
        response_out = http.getString();
    } else {
        response_out = "";
    }
    http.end();
    return http_code_out > 0;
}

} // namespace gcal_http
