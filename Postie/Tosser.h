#pragma once

#include "../Common/Squish.h"
#include "../Common/Logger.h"

class Config;

class Tosser
{
public:
	bool run(bool protinbound);
	NETADDR* get_echomail_addr(std::string ctrlbody, std::string msgbody);
private:
	Logger log;
	void areafix(Config *c, sq_msg_t* msg);
	bool update(std::string tag, std::string links);
	std::string get_msgid(std::string ctrlbody);
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};

