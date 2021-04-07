#pragma once

#include "Config.h"

class SubReq
{
public:
	void add(const char *netname, int host, const char *subtype);
	void drop(const char *netname, int host, const char* subtype);
private:
	Config config;
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};
