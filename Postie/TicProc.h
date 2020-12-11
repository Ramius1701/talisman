#pragma once

#include <vector>
#include <string>
#include <sqlite3.h>
#include "Squish.h"

struct ticfile_t {
	std::string area;
	std::string password;
	std::string file;
	std::string lname;
	std::vector<std::string> desc;
	std::string shortdesc;
	std::string replaces;
	std::string from;
	std::vector<NETADDR*> seenbys;
	std::vector<std::string> path;
	uint32_t crc;
};


class TicProc
{
public:
	bool run();
private:
	bool check_crc(const char *, uint32_t crc);
	bool add_file_to_area(struct ticfile_t* tic, std::filesystem::path srcfile, std::filesystem::path destfile, std::string database);
	bool open_database(std::string filename, sqlite3** db);
	std::string _datapath;
	std::string _logpath;
	std::string _tmppath;
};

