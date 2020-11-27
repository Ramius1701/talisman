#pragma once
class Tosser
{
public:
	bool run();
private:
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};

