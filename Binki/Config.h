#pragma once
#include <string>
#include <vector>
#include "../Common/Squish.h"

struct link_t {
	std::string network;
	NETADDR* addr;
	std::string outbox;
	std::string password;
	std::string host;
	int port;
	bool crammd5;
};

struct address_t {
	NETADDR* addr;
	std::string domain;
};

class Config
{
public:
	bool load(std::string datapath);

	int defaultzone;

	std::string outbound;
	std::string inbound;
	std::string inbound_secure;
	std::string semaphore;

	std::vector<struct address_t> addresses;
	std::vector<struct link_t> links;
};

