#include "clock/clock_draw.h"
#include "clock/clock_faces.h"

namespace {

using namespace clock_draw;

void draw_background(lgfx::LovyanGFX& lcd) {
    lcd.fillScreen(TFT_BLACK);
    fill_radial_disc(lcd, kCenterX, kCenterY, kDialRadius + 4, 0x0841, 0x0008);
    fill_radial_disc(lcd, kCenterX, kCenterY, kDialRadius - 8, 0x10A2, 0x0208);

    for (int i = 0; i < 18; ++i) {
        const float angle = (float)i * 20.0f + 7.0f;
        int x, y;
        polar_to_xy(kCenterX, kCenterY, angle, 72.0f + (i % 3) * 6.0f, x, y);
        lcd.fillCircle(x, y, 1, 0x528A);
    }

    lcd.drawCircle(kCenterX, kCenterY, kDialRadius, 0x3186);
    lcd.drawCircle(kCenterX, kCenterY, kDialRadius - 1, 0x1082);
}

void draw_static(lgfx::LovyanGFX& lcd, const ClockWallTime& /*wall*/) {
    draw_tick_ring(lcd, kCenterX, kCenterY, kDialRadius - 2,
                   0xE71C, 0x4208, 12, 6, 3, 1);

    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.setTextColor(0x8410, TFT_BLACK);
    lcd.setTextSize(1);
    lcd.drawString("12", kCenterX, kCenterY - 78);
    lcd.drawString("3", kCenterX + 78, kCenterY);
    lcd.drawString("6", kCenterX, kCenterY + 78);
    lcd.drawString("9", kCenterX - 78, kCenterY);
}

static bool mask_has(ClockHandMask mask, ClockHandMask bit) {
    return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit)) != 0;
}

constexpr float kSecondTail = 18.0f;
constexpr float kSecondTip = 88.0f;

void draw_hands(lgfx::LovyanGFX& lcd, const AnalogClockState& state, bool erase, ClockHandMask mask,
                int cx, int cy) {
    const uint16_t hour_c = erase ? 0x10A2 : 0xC618;
    const uint16_t min_c = erase ? 0x10A2 : 0xFFFF;
    const uint16_t sec_c = erase ? 0x10A2 : 0x07FF;

    if (mask_has(mask, ClockHandMask::Hour)) {
        draw_hand_triangle(lcd, cx, cy, state.hour_angle, 52.0f, 8.0f, hour_c);
    }
    if (mask_has(mask, ClockHandMask::Minute)) {
        draw_hand_line(lcd, cx, cy, state.minute_angle, 78.0f, 3, min_c);
    }
    if (mask_has(mask, ClockHandMask::Second)) {
        draw_second_hand(lcd, cx, cy, state.second_angle, kSecondTail, kSecondTip, sec_c);
    }

    if (!erase && mask_has(mask, ClockHandMask::Hub)) {
        lcd.fillCircle(cx, cy, 5, 0x3186);
        lcd.fillCircle(cx, cy, 2, 0xFFFF);
    }
}

const ClockFace kFace = {
    .id = "midnight",
    .name = "Midnight",
    .draw_background = draw_background,
    .draw_static = draw_static,
    .draw_digital_overlay = nullptr,
    .draw_hands = draw_hands,
    .second_tail_len = kSecondTail,
    .second_tip_len = kSecondTip,
    .erase_color = 0x10A2,
    .refresh_static_every_second = false,
    .use_static_compositor = false,
};

} // namespace

const ClockFace* clock_face_midnight() {
    return &kFace;
}
