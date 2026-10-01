#include "clock/clock_draw.h"
#include "clock/clock_faces.h"

namespace {

using namespace clock_draw;

void draw_background(LGFX_Device& lcd) {
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

void draw_static(LGFX_Device& lcd) {
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

void draw_hands(LGFX_Device& lcd, const AnalogClockState& state, bool erase) {
    const uint16_t hour_c = erase ? 0x10A2 : 0xC618;
    const uint16_t min_c = erase ? 0x10A2 : 0xFFFF;
    const uint16_t sec_c = erase ? 0x10A2 : 0x07FF;

    draw_hand_triangle(lcd, kCenterX, kCenterY, state.hour_angle, 52.0f, 8.0f, hour_c);
    draw_hand_line(lcd, kCenterX, kCenterY, state.minute_angle, 78.0f, 3, min_c);
    draw_second_hand(lcd, kCenterX, kCenterY, state.second_angle, 18.0f, 88.0f, sec_c);

    if (!erase) {
        lcd.fillCircle(kCenterX, kCenterY, 5, 0x3186);
        lcd.fillCircle(kCenterX, kCenterY, 2, 0xFFFF);
    }
}

const ClockFace kFace = {
    .id = "midnight",
    .name = "Midnight",
    .draw_background = draw_background,
    .draw_static = draw_static,
    .draw_hands = draw_hands,
    .erase_color = 0x10A2,
};

} // namespace

const ClockFace* clock_face_midnight() {
    return &kFace;
}
