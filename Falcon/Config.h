#pragma once
#include <string>
#include <vector>

struct network_t {
	std::string name;
	std::string outbox;
	std::string emailbase;
	int mynode;
	int upnode;
};

struct area_t {
	std::string netname;
	std::string basefile;
	std::string subtype;
	int mynode;
	int hostnode;
};

class Config
{
public:
	bool load(std::string datapath);
	std::vector<struct network_t> networks;
	std::vector<struct area_t> areas;

	std::string inbound() {
		return __inbound;
	}
private:
	std::string __inbound;

};

