#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "Squish.h"

class Archiver;

struct address_conf_t {
	NETADDR* aka;
};

struct route_conf_t {
	NETADDR* aka;
	std::string route;
	std::string flavour;
};


struct link_conf_t {
	NETADDR* aka;
	NETADDR* ouraka;
	std::string flavour;
	std::string archiver;
	std::string packetpwd;
	std::filesystem::path packetpath;
	FILE* fptr;
};

struct netmail_area_conf_t {
	NETADDR* aka;
	std::string file;
};

struct area_conf_t {
	NETADDR* aka;
	std::string file;
	std::string areatag;
	std::vector <struct link_conf_t*> links;
};

class Config
{
public:
	bool load(std::string filename);
	bool load_archivers(std::string datapath);
	std::vector<Archiver*> archivers;
	std::vector<struct address_conf_t> addresses;
	std::vector<struct link_conf_t> links;
	std::vector<struct area_conf_t> areas;
	std::vector<struct route_conf_t> routes;
	std::vector<struct netmail_area_conf_t> netmailareas;
	std::string packetdir() {
		return __packetdir;
	}

	std::string outbound() {
		return __outbound;
	}

	std::string inbound() {
		return __inbound;
	}
	std::string protinbound() {
		return __protinbound;
	}
private:
	std::string __inbound;
	std::string __protinbound;
	std::string __outbound;
	std::string __msgbasedir;
	std::string __packetdir;
};

