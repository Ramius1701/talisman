#include "Config.h"
#include <fstream>
#include "../Common/toml.hpp"

bool Config::load(std::string datapath) {
	try {
		auto data = toml::parse_file(datapath + "/binki.toml");

		auto _inbound = data["binki"]["inbound"].as_string();

		if (_inbound == nullptr) {
			inbound = "";
		}
		else {
			inbound = _inbound->value_or("");
		}

		auto _secinbound = data["binki"]["secure_inbound"].as_string();

		if (_secinbound == nullptr) {
			inbound_secure = "";
		}
		else {
			inbound_secure = _secinbound->value_or("");
		}

		auto _outbound = data["binki"]["outbound"].as_string();
		if (_outbound == nullptr) {
			outbound = "";
		}
		else {
			outbound = _outbound->value_or("");
		}

		auto _defaultzone = data["binki"]["default_zone"].as_integer();
		if (_defaultzone == nullptr) {
			defaultzone = 0;
		}
		else {
			defaultzone = _defaultzone->value_or(0);
		}

		auto addressitems = data.get_as<toml::array>("address");
		if (addressitems != nullptr) {
			for (size_t i = 0; i < addressitems->size(); i++) {
				struct address_t newaddr;

				auto itemtable = addressitems->get(i)->as_table();


			}
		}
	}
	catch (toml::parse_error) {
		return false;
	}


	return true;
}