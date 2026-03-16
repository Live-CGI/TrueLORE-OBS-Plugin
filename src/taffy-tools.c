#include "taffy_time.h"

#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

void taffy_now_iso8601(char *buffer, size_t buffer_size)
{
#ifdef _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);

	snprintf(buffer, buffer_size,
		"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
		st.wYear,
		st.wMonth,
		st.wDay,
		st.wHour,
		st.wMinute,
		st.wSecond,
		st.wMilliseconds);

#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	struct tm tm_utc;
	gmtime_r(&ts.tv_sec, &tm_utc);

	int ms = ts.tv_nsec / 1000000;

	snprintf(buffer, buffer_size,
		"%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
		tm_utc.tm_year + 1900,
		tm_utc.tm_mon + 1,
		tm_utc.tm_mday,
		tm_utc.tm_hour,
		tm_utc.tm_min,
		tm_utc.tm_sec,
		ms);
#endif
}