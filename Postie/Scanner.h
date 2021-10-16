#pragma once

#include <string>
#include "Config.h"

struct seenby_t {
	uint16_t net;
	uint16_t node;
};


class Scanner
{
public:
	bool run();
	static std::string get_bundle_name(NETADDR* orig, NETADDR* dest, std::string packetpath, bool bundle_ts, std::string data_path);
	static bool append_flo_file(struct link_conf_t* link, Config* c, std::string bundlefname, std::string flowtype);
	static void write_msg_to_pkt(struct area_conf_t* area, struct link_conf_t* link, sq_msg_t* msg, bool local);
	static void write_netmail_to_pkt(NETADDR* orig, NETADDR* dest, sq_msg_t* msg, bool local, FILE* fptr, std::string flavour);
	static void initialize_packet(struct link_conf_t* link, std::string working_path, NETADDR* pktorig);
	static bool matchroute(std::string route, NETADDR* aka);
	static std::string initialize_netmail_packet(NETADDR* dest, std::string working_path, NETADDR* myaka, FILE** fptr);
	static std::vector<struct seenby_t> parse_seenbys(std::string msgbuf);
	static bool check_seenby(std::vector<struct seenby_t>* seenbys, NETADDR* node);
private:
	static void add_seenby(std::vector<struct seenby_t>* seenbys, NETADDR* node);
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};

