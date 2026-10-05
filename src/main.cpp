#include <Arduino.h>

#include "app_shell.h"
#include "config.h"
#include "diary/diary_sync.h"
#include "diary/gcal_auth.h"
#include "display.h"
#include "time/wall_clock.h"
#include "weather/local_weather.h"
#include "touch_input.h"
#include "wifi/serial_wifi.h"
#include "wifi/wifi_station.h"

static Display display;
static TouchInput touch_input;
static AppShell app_shell;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.printf("\n[APP] %s v%s starting...\n", APP_NAME, APP_VERSION);

    if (!display.begin()) {
        Serial.println("[APP] Display init failed");
        while (true) {
            delay(1000);
        }
    }

    touch_input.begin();
    wifi_station_begin();
    wall_clock_begin();
    local_weather_begin();
    serial_wifi_begin();
    gcal_auth_begin();
    diary_sync_begin();
    app_shell.begin(display);
    Serial.printf("[APP] free heap: %u bytes\n", (unsigned)ESP.getFreeHeap());
    Serial.println("[APP] Swipe up/down on clock to change watch face.");
}

void loop() {
    InputEvent event = InputEvent::None;
    if (touch_input.poll(event)) {
        app_shell.handleInput(event);
    }

    serial_wifi_poll();
    wifi_station_poll();
    wall_clock_poll();
    local_weather_poll();
    gcal_auth_poll();
    diary_sync_poll();

    app_shell.tick(millis());
    delay(5);
}
