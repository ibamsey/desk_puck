#ifndef DIARY_DIARY_TYPES_H
#define DIARY_DIARY_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define DIARY_MAX_EVENTS 50
#define DIARY_TITLE_LEN 48

/** One calendar entry for a single local day (minutes from local midnight). */
struct DiaryEvent {
    const char* title;
    int16_t start_min;
    int16_t end_min;
    bool all_day;
};

struct DiaryDay {
    const DiaryEvent* events;
    size_t event_count;
    int year;
    int month;
    int day;
};

struct DiaryEventBuf {
    char title[DIARY_TITLE_LEN + 1];
    int16_t start_min;
    int16_t end_min;
    bool all_day;
};

struct DiaryDayBuffer {
    DiaryEventBuf events[DIARY_MAX_EVENTS];
    size_t count = 0;
    int year = 0;
    int month = 0;
    int day = 0;

    void clear() {
        count = 0;
    }
};

#endif // DIARY_DIARY_TYPES_H
