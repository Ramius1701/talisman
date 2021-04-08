#pragma once

#include "Config.h"
#include <sqlite3.h>
class Scanner
{
public:
	void run();
private:
	int username_to_id(const char* uname);
	bool open_user_database(sqlite3** db);
	Config config;
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};

