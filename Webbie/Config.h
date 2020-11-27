#pragma once
#include <string>
class Config
{
public:
	bool parse();

	std::string wwwpath() {
		return _wwwpath;
	}
private:
	std::string _datapath;
	std::string _msgpath;
	std::string _logpath;
	std::string _opname;
	std::string _sysname;
	std::string _location;
	std::string _wwwpath;
};

