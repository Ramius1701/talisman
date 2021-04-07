#include <fstream>
#include "Config.h"
#include "../Common/toml.hpp"


static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

bool Config::load(std::string datapath) {
	try {
		auto data = toml::parse_file(datapath + "/falcon.toml");
		
		auto _inbound = data["falcon"]["inbound"].as_string();

		if (_inbound == nullptr) {
			__inbound = "";
		}
		else {
			__inbound = _inbound->value_or("");
		}


		// iterate over nets
		auto networkitems = data.get_as<toml::array>("network");

		if (networkitems != nullptr) {
			for (size_t i = 0; i < networkitems->size(); i++) {

				struct network_t newnet;

				auto itemtable = networkitems->get(i)->as_table();
				
				auto _outbox = itemtable->get("outbox");

				if (_outbox == nullptr) {
					newnet.outbox = "";
				}
				else {
					newnet.outbox = _outbox->as_string()->value_or("");
				}

				auto _netname = itemtable->get("name");

				if (_netname == nullptr) {
					newnet.name = "";
				}
				else {
					newnet.name = _netname->as_string()->value_or("");
				}

				auto _emailbase = itemtable->get("emailbase");

				if (_emailbase == nullptr) {
					newnet.emailbase = "";
				}
				else {
					newnet.emailbase = _emailbase->as_string()->value_or("");
				}


				auto _mynode = itemtable->get("mynode");

				if (_mynode == nullptr) {
					newnet.mynode = 0;
				}
				else {
					newnet.mynode = _mynode->as_integer()->value_or(0);
				}

				auto _upnode = itemtable->get("uplink");

				if (_upnode == nullptr) {
					newnet.upnode = 0;
				}
				else {
					newnet.upnode = _upnode->as_integer()->value_or(0);
				}

				if (newnet.name == "" || newnet.emailbase == "" || newnet.outbox == "" || newnet.mynode == 0 || newnet.upnode == 0) {
					continue;
				}
				else {
					networks.push_back(newnet);
				}
			}
		}
	}
	catch (toml::parse_error) {
		return false;
	}

	return true;
}