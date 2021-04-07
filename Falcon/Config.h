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

class Config
{
public:
	bool load(std::string datapath);
	std::vector<struct network_t> networks;

	std::string inbound() {
		return __inbound;
	}
private:
	std::string __inbound;

};

