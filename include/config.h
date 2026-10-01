#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Application identity
// ============================================================================

#define APP_NAME "Desk Puck"
#define APP_VERSION "0.3.0"

// ============================================================================
// Display (ESP32-2424S012 round GC9A01)
// ============================================================================

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_UPDATE_INTERVAL_MS 100

#ifndef PIN_LCD_SCLK
#define PIN_LCD_SCLK 6
#define PIN_LCD_MOSI 7
#define PIN_LCD_DC 2
#define PIN_LCD_CS 10
#define PIN_LCD_BL 3
#endif

// Capacitive touch (CST816S on ESP32-2424S012C)
#define PIN_TP_SDA 4
#define PIN_TP_SCL 5
#define PIN_TP_RST 1
#define PIN_TP_INT 0

#define TOUCH_GESTURE_DEBOUNCE_MS 300

/** Ignore repeat CST816S reports after a handled swipe (nav or clock face). */
#define GESTURE_ECHO_COOLDOWN_MS 1000

/** Seconds added to compile-time boot clock (e.g. 3600 for UTC+1). */
#define CLOCK_UTC_OFFSET_SEC 3600

#define CLOCK_TICK_INTERVAL_MS 200

// ============================================================================
// Debug
// ============================================================================

#define DEBUG_TOUCH 1
#define DEBUG_DISPLAY 0

#endif // CONFIG_H
