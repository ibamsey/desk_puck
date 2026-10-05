#include "clock/clock_digital.h"
#include "clock/clock_draw.h"
#include "clock/clock_faces.h"

namespace {

using namespace clock_draw;

void draw_background(lgfx::LovyanGFX& lcd) {
    lcd.fillScreen(TFT_BLACK);
    fill_radial_disc(lcd, kCenterX, kCenterY, kDialRadius + 2, 0x1082, 0x0000);
    fill_radial_disc(lcd, kCenterX, kCenterY, kDialRadius - 6, 0x2104, 0x0841);
    lcd.drawCircle(kCenterX, kCenterY, kDialRadius, 0x528A);
}

void draw_static(lgfx::LovyanGFX& lcd, const ClockWallTime& /*wall*/) {
    draw_tick_ring(lcd, kCenterX, kCenterY, kDialRadius - 4, 0xDEFB, 0x4208, 10, 5, 2, 1);
}

void draw_digital_overlay(lgfx::LovyanGFX& lcd, const ClockWallTime& wall) {
    clock_digital::DigitalReadoutStyle style;
    style.time_color = 0x632C;
    style.date_color = 0x3186;
    style.bg_color = 0x2104;
    style.time_text_size = 2;
    style.date_text_size = 1;
    style.clear_background = false;
    clock_digital::draw_time_date_readout(lcd, wall, style);
}

static bool mask_has(ClockHandMask mask, ClockHandMask bit) {
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit)) != 0;
}

constexpr float kSecondTail = 16.0f;
constexpr float kSecondTip = 82.0f;

void draw_hands(lgfx::LovyanGFX& lcd, const AnalogClockState& state, bool erase, ClockHandMask mask,
                int cx, int cy) {
    (void)erase;
    const uint16_t hour_c = 0xC618;
    const uint16_t min_c = 0xFFFF;
    const uint16_t sec_c = 0xF800;

    if (mask_has(mask, ClockHandMask::Hour)) {
        draw_hand_triangle(lcd, cx, cy, state.hour_angle, 48.0f, 7.0f, hour_c);
    }
    if (mask_has(mask, ClockHandMask::Minute)) {
        draw_hand_line(lcd, cx, cy, state.minute_angle, 74.0f, 3, min_c);
    }
    if (mask_has(mask, ClockHandMask::Second)) {
        draw_second_hand(lcd, cx, cy, state.second_angle, kSecondTail, kSecondTip, sec_c);
    }

    if (!erase && mask_has(mask, ClockHandMask::Hub)) {
        lcd.fillCircle(cx, cy, 4, 0x528A);
        lcd.fillCircle(cx, cy, 2, 0xFFFF);
    }
}

const ClockFace kFace = {
    .id = "classic-digital",
    .name = "Classic Digital",
    .draw_background = draw_background,
    .draw_static = draw_static,
    .draw_digital_overlay = draw_digital_overlay,
    .draw_hands = draw_hands,
    .second_tail_len = kSecondTail,
    .second_tip_len = kSecondTip,
    .erase_color = 0x2104,
    .refresh_static_every_second = true,
    .use_static_compositor = true,
};

} // namespace

const ClockFace* clock_face_classic_digital() {
    return &kFace;
}
