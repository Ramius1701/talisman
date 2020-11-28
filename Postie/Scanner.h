#pragma once

#include <string>
#include "Config.h"

class Scanner
{
public:
	bool run();
	static std::string get_bundle_name(struct link_conf_t* link, std::string packetpath);
	static bool append_flo_file(struct link_conf_t* link, Config* c, std::string bundlefname, std::string flowtype);
	static void write_msg_to_pkt(struct area_conf_t* area, struct link_conf_t* link, sq_msg_t* msg, bool local);
	static void write_netmail_to_pkt(struct link_conf_t* link, sq_msg_t* msg, bool local);
	static void initialize_packet(struct link_conf_t* link, std::string working_path, NETADDR* pktorig);
	static bool matchroute(std::string route, NETADDR* aka);
private:
	std::string _datapath;
	std::string _logpath;
	std::string _msgpath;
	std::string _tmppath;
};

