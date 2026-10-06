#ifndef CLOCK_CLOCK_DIGITAL_H
#define CLOCK_CLOCK_DIGITAL_H

#include "clock/wall_time.h"
#include "LGFX_config.h"

namespace clock_digital {

/** Bits for DigitalReadoutStyle::flags / AssetDigitalReadoutMeta::flags (packer + face.json). */
constexpr uint8_t kDigitalShowTime = 0x01;
constexpr uint8_t kDigitalShowDate = 0x02;
/** Date string: 0 = short (Mon 05 Oct), 0x04 = day DD, 0x08 = day D (no pad). */
constexpr uint8_t kDigitalDateFormatDayDd = 0x04;
constexpr uint8_t kDigitalDateFormatDayD = 0x08;
constexpr uint8_t kDigitalTimeFormat12h = 0x10;
constexpr uint8_t kDigitalDateFormatWeekdayMonthDay = 0x20;
/** Weekday abbrev at time_x/y + day-of-month at date_x/y (WatchMaker split windows). */
constexpr uint8_t kDigitalDateFormatSplitWeekdayDay = 0x40;

/** "HH:MM" plus NUL (6 chars). */
void format_time_hm(char* out, size_t out_len, const ClockWallTime& wall);

/** 12-hour "h:MMam/pm" (WatchMaker {dh}:{dmz}{da}). */
void format_time_12h_ampm(char* out, size_t out_len, const ClockWallTime& wall);

/** "HH:MM:SS" plus NUL (9 chars). */
void format_time_hms(char* out, size_t out_len, const ClockWallTime& wall);

/** "Mon 05 Oct" style plus NUL (needs ≥ 16 bytes). */
void format_date_short(char* out, size_t out_len, const ClockWallTime& wall);

/** Two-digit day of month (WatchMaker {dd}). */
void format_day_dd(char* out, size_t out_len, const ClockWallTime& wall);

/** "Mon Mar 05" (WatchMaker {ddw} {dnnn} {dd}). */
void format_date_weekday_month_day(char* out, size_t out_len, const ClockWallTime& wall);

/** Three-letter weekday, upper case (WatchMaker string.sub(...,{ddww}...,1,3)). */
void format_weekday_abbrev_upper(char* out, size_t out_len, const ClockWallTime& wall);

struct DigitalReadoutStyle {
    uint16_t time_color = 0x4208;
    uint16_t date_color = 0x3186;
    uint16_t bg_color = 0x0000;
    int time_text_size = 2;
    int date_text_size = 1;
    int time_x = 120;
    int time_y = 52;
    int date_x = 120;
    int date_y = 188;
    /** kDigitalShowTime | kDigitalShowDate | optional kDigitalDateFormat* */
    uint8_t flags = kDigitalShowTime | kDigitalShowDate;
    /** Solid fill behind text; disable when the dial under the label is already correct. */
    bool clear_background = true;
};

/** Bounding boxes (max-width) of the time / date labels, for dirty-rect compositing. */
void time_readout_rect(const DigitalReadoutStyle& style, int& x, int& y, int& w, int& h);
void weekday_readout_rect(const DigitalReadoutStyle& style, int& x, int& y, int& w, int& h);
void date_readout_rect(const DigitalReadoutStyle& style, int& x, int& y, int& w, int& h);

/** Draw time + date centred above/below the dial hub (behind analogue hands). */
void draw_time_date_readout(lgfx::LovyanGFX& lcd, const ClockWallTime& wall,
                            const DigitalReadoutStyle& style);
void draw_time_readout(lgfx::LovyanGFX& lcd, const ClockWallTime& wall, const DigitalReadoutStyle& style);
void draw_weekday_readout(lgfx::LovyanGFX& lcd, const ClockWallTime& wall, const DigitalReadoutStyle& style);
void draw_date_readout(lgfx::LovyanGFX& lcd, const ClockWallTime& wall, const DigitalReadoutStyle& style);

} // namespace clock_digital

#endif // CLOCK_CLOCK_DIGITAL_H
