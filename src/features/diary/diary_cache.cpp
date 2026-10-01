#include "diary/diary_cache.h"

#include <Preferences.h>
#include <string.h>

static const char kMagic[] = "DPCK";
static const uint8_t kVersion = 1;

void diary_cache_begin() {}

bool diary_cache_load(DiaryDayBuffer& out) {
    out.clear();
    Preferences p;
    if (!p.begin("diary", true)) {
        return false;
    }
    const size_t len = p.getBytesLength("day");
    if (len < 16 || len > 4096) {
        p.end();
        return false;
    }
    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf) {
        p.end();
        return false;
    }
    p.getBytes("day", buf, len);
    p.end();

    size_t off = 0;
    if (memcmp(buf + off, kMagic, 4) != 0) {
        free(buf);
        return false;
    }
    off += 4;
    if (buf[off++] != kVersion) {
        free(buf);
        return false;
    }
    const int y = (int)buf[off] * 256 + buf[off + 1];
    off += 2;
    const int mo = buf[off++];
    const int d = buf[off++];
    const uint8_t n = buf[off++];
    if (n > DIARY_MAX_EVENTS) {
        free(buf);
        return false;
    }
    out.year = y;
    out.month = mo;
    out.day = d;
    for (uint8_t i = 0; i < n; ++i) {
        if (off + 1 >= len) {
            break;
        }
        const uint8_t tlen = buf[off++];
        if (off + tlen + 5 > len) {
            break;
        }
        DiaryEventBuf& ev = out.events[out.count++];
        const size_t copy = tlen > DIARY_TITLE_LEN ? DIARY_TITLE_LEN : tlen;
        memcpy(ev.title, buf + off, copy);
        ev.title[copy] = '\0';
        off += tlen;
        ev.start_min = (int16_t)(buf[off] | (buf[off + 1] << 8));
        off += 2;
        ev.end_min = (int16_t)(buf[off] | (buf[off + 1] << 8));
        off += 2;
        ev.all_day = buf[off++] != 0;
    }
    free(buf);
    return out.count > 0 || n == 0;
}

bool diary_cache_save(const DiaryDayBuffer& day) {
    size_t len = 4 + 1 + 2 + 1 + 1 + 1;
    for (size_t i = 0; i < day.count; ++i) {
        len += 1 + strlen(day.events[i].title) + 2 + 2 + 1;
    }
    if (len > 4096) {
        return false;
    }
    uint8_t* buf = (uint8_t*)malloc(len);
    if (!buf) {
        return false;
    }
    size_t off = 0;
    memcpy(buf + off, kMagic, 4);
    off += 4;
    buf[off++] = kVersion;
    buf[off++] = (uint8_t)((day.year >> 8) & 0xff);
    buf[off++] = (uint8_t)(day.year & 0xff);
    buf[off++] = (uint8_t)day.month;
    buf[off++] = (uint8_t)day.day;
    buf[off++] = (uint8_t)day.count;
    for (size_t i = 0; i < day.count; ++i) {
        const char* title = day.events[i].title;
        const uint8_t tlen = (uint8_t)strlen(title);
        buf[off++] = tlen;
        memcpy(buf + off, title, tlen);
        off += tlen;
        buf[off++] = (uint8_t)(day.events[i].start_min & 0xff);
        buf[off++] = (uint8_t)((day.events[i].start_min >> 8) & 0xff);
        buf[off++] = (uint8_t)(day.events[i].end_min & 0xff);
        buf[off++] = (uint8_t)((day.events[i].end_min >> 8) & 0xff);
        buf[off++] = day.events[i].all_day ? 1 : 0;
    }
    Preferences p;
    if (!p.begin("diary", false)) {
        free(buf);
        return false;
    }
    p.putBytes("day", buf, off);
    p.end();
    free(buf);
    return true;
}
