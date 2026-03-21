#pragma once
#include <stdbool.h>

bool taffylabs_mirror_start(const char* STREAM_SERVER, const char* STREAM_KEY);
void taffylabs_mirror_stop(void);
