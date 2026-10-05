#include "clock/clock_digital.h"

#include "clock/clock_draw.h"

#include <cstdio>
#include <cstring>

namespace clock_digital {

namespace {

constexpr int kCenterX = clock_draw::kCenterX;

const char* weekday_short(int wday) {
    static const char* kNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (wday < 0 || wday > 6) {
        return "---";
    }
    return kNames[wday];
}

const char* month_short(int month) {
    static const char* kNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (month < 1 || month > 12) {
        return "---";
    }
    return kNames[month - 1];
}

} // namespace

void format_time_hm(char* out, size_t out_len, const ClockWallTime& wall) {
    if (!out || out_len < 6) {
        return;
    }
    snprintf(out, out_len, "%02d:%02d", wall.hour, wall.minute);
}

void format_time_12h_ampm(char* out, size_t out_len, const ClockWallTime& wall) {
    if (!out || out_len < 10) {
        return;
    }
    int h = wall.hour % 12;
    if (h == 0) {
        h = 12;
    }
    const char* ap = (wall.hour < 12) ? "am" : "pm";
    snprintf(out, out_len, "%d:%02d%s", h, wall.minute, ap);
}

void format_time_hms(char* out, size_t out_len, const ClockWallTime& wall) {
    if (!out || out_len < 9) {
        return;
    }
    snprintf(out, out_len, "%02d:%02d:%02d", wall.hour, wall.minute, wall.second);
}

static void format_date_short_fixed(char* out, size_t out_len, const ClockWallTime& wall) {
    if (!out || out_len < 13) {
        return;
    }
    snprintf(out, out_len, "%3s %02d %3s", weekday_short(wall.weekday), wall.day,
             month_short(wall.month));
}

void time_readout_rect(const DigitalReadoutStyle& style, int& x, int& y, int& w, int& h) {
    int kMaxTimeChars = 8;
    if (style.flags & kDigitalTimeFormat12h) {
        kMaxTimeChars = 10;
    }
    w = kMaxTimeChars * 6 * style.time_text_size + 8;
    h = 8 * style.time_text_size + 4;
    x = style.time_x - w / 2;
    y = style.time_y - h / 2;
}

void format_date_weekday_month_day(char* out, size_t out_len, const ClockWallTime& wall) {
    if (!out || out_len < 14) {
        return;
    }
    snprintf(out, out_len, "%s %s %02d", weekday_short(wall.weekday), month_short(wall.month), wall.day);
}

void date_readout_rect(const DigitalReadoutStyle& style, int& x, int& y, int& w, int& h) {
    int max_chars = 12;
    if (style.flags & kDigitalDateFormatWeekdayMonthDay) {
        max_chars = 13;
    } else if (style.flags & kDigitalDateFormatDayDd) {
        max_chars = 2;
    } else if (style.flags & kDigitalDateFormatDayD) {
        max_chars = 2;
    }
    w = max_chars * 6 * style.date_text_size + 8;
    h = 8 * style.date_text_size + 4;
    x = style.date_x - w / 2;
    y = style.date_y - h / 2;
}

void format_date_short(char* out, size_t out_len, const ClockWallTime& wall) {
    if (!out || out_len < 12) {
        return;
    }
    snprintf(out, out_len, "%s %02d %s", weekday_short(wall.weekday), wall.day,
             month_short(wall.month));
}

void format_day_dd(char* out, size_t out_len, const ClockWallTime& wall) {
    if (!out || out_len < 3) {
        return;
    }
    snprintf(out, out_len, "%02d", wall.day);
}

void draw_time_readout(lgfx::LovyanGFX& lcd, const ClockWallTime& wall, const DigitalReadoutStyle& style) {
    char time_buf[12];
    if (style.flags & kDigitalTimeFormat12h) {
        format_time_12h_ampm(time_buf, sizeof(time_buf), wall);
    } else {
        format_time_hms(time_buf, sizeof(time_buf), wall);
    }

    lcd.setTextDatum(textdatum_t::middle_center);
    int tx = 0;
    int ty = 0;
    int tw = 0;
    int th = 0;
    time_readout_rect(style, tx, ty, tw, th);
    if (style.clear_background) {
        lcd.fillRect(tx, ty, tw, th, style.bg_color);
    }
    lcd.setTextColor(style.time_color, style.bg_color);
    lcd.setTextSize(style.time_text_size);
    lcd.drawString(time_buf, style.time_x, style.time_y);
}

void draw_date_readout(lgfx::LovyanGFX& lcd, const ClockWallTime& wall, const DigitalReadoutStyle& style) {
    char date_buf[20];
    if (style.flags & kDigitalDateFormatWeekdayMonthDay) {
        format_date_weekday_month_day(date_buf, sizeof(date_buf), wall);
    } else if (style.flags & kDigitalDateFormatDayD) {
        snprintf(date_buf, sizeof(date_buf), "%d", wall.day);
    } else if (style.flags & kDigitalDateFormatDayDd) {
        format_day_dd(date_buf, sizeof(date_buf), wall);
    } else {
        format_date_short_fixed(date_buf, sizeof(date_buf), wall);
    }

    lcd.setTextDatum(textdatum_t::middle_center);
    int dx = 0;
    int dy = 0;
    int dw = 0;
    int dh = 0;
    date_readout_rect(style, dx, dy, dw, dh);
    if (style.clear_background) {
        lcd.fillRect(dx, dy, dw, dh, style.bg_color);
    }
    lcd.setTextColor(style.date_color, style.bg_color);
    lcd.setTextSize(style.date_text_size);
    lcd.drawString(date_buf, style.date_x, style.date_y);
}

void draw_time_date_readout(lgfx::LovyanGFX& lcd, const ClockWallTime& wall,
                            const DigitalReadoutStyle& style) {
    if (style.flags & kDigitalShowTime) {
        draw_time_readout(lcd, wall, style);
    }
    if (style.flags & kDigitalShowDate) {
        draw_date_readout(lcd, wall, style);
    }
}

} // namespace clock_digital
