#ifndef DIARY_DIARY_CACHE_H
#define DIARY_DIARY_CACHE_H

#include "diary/diary_types.h"

void diary_cache_begin();

bool diary_cache_load(DiaryDayBuffer& out);

bool diary_cache_save(const DiaryDayBuffer& day);

#endif // DIARY_DIARY_CACHE_H
