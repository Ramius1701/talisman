#pragma once

#include <vector>
#include <string>

struct ticfile_t {
	std::string area;
	std::string password;
	std::string file;
	std::string lname;
	std::vector<std::string> desc;
	std::string shortdesc;
	std::string replaces;
	uint32_t crc;
};


class TicProc
{
public:
	bool run();
private:
	bool check_crc(std::string filename);
	std::string _datapath;
	std::string _logpath;
	std::string _tmppath;
};

