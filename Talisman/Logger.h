#pragma once

#include <string>

class Logger
{
public:
	Logger();
	void load(std::string filename);
	void log(int severity, const char* fmt, ...);

private:
	bool is_loaded;
	std::string logfile;
};

