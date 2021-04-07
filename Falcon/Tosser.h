#pragma once
#include <sqlite3.h>
#include <vector>
#include "Config.h"

class Tosser
{
public:
	void run();
	bool import_email(int to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent);
	bool import_email(std::string to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent);
	bool open_user_database(sqlite3** db);
private:
	Config config;
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};

