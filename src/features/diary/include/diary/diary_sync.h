#ifndef DIARY_DIARY_SYNC_H
#define DIARY_DIARY_SYNC_H

#include "diary/diary_types.h"

enum class DiarySyncState {
    Idle,
    WaitingToken,
    Fetching,
    Done,
    Error,
};

void diary_sync_begin();

void diary_sync_poll();

void diary_sync_request();

DiarySyncState diary_sync_state();

const char* diary_sync_last_error();

/** True while a fetch is in progress. */
bool diary_sync_busy();

/** Last successfully synced day (RAM). */
const DiaryDayBuffer& diary_sync_day();

unsigned long diary_sync_last_ok_ms();

#endif // DIARY_DIARY_SYNC_H
