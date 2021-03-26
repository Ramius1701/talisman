#include <stdarg.h>
#include <ctime>
#include "Logger.h"

Logger::Logger()
{
	is_loaded = false;
}

void Logger::load(std::string filename)
{
	logfile = filename;
	is_loaded = true;
}

void Logger::log(int severity, const char* fmt, ...)
{
	static const char* sev[] = { "ERROR", " INFO", "DEBUG" };
	char buffer[2048];
	va_list args;
	time_t now = time(NULL);
	struct tm time_tm;
	if (!is_loaded) return;

	va_start(args, fmt);
	vsnprintf(buffer, sizeof buffer, fmt, args);
	va_end(args);

#if _MSC_VER
	localtime_s(&time_tm, &now);
#else
	localtime_r(&now, &time_tm);
#endif

	FILE* fptr = fopen(logfile.c_str(), "a");
	if (fptr) {
		fprintf(fptr, "%04d-%02d-%02d %02d:%02d:%02d %s: %s\n", time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec, sev[severity], buffer);
		fclose(fptr);
	}
}
