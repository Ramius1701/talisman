#pragma once

#include "../Common/Squish.h"

class Tosser
{
public:
	bool run(bool protinbound);
	NETADDR* get_echomail_addr(std::string ctrlbody, std::string msgbody);
private:
	std::string get_msgid(std::string ctrlbody);
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};

