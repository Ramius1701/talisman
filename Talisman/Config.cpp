#include <fstream>
#include "toml.hpp"
#include "INIReader.h"
#include "Config.h"

Config::Config() {

}

bool Config::load(Node *n, std::string filename) {
	INIReader inir(filename);

	if (inir.ParseError() != 0) {
		return false;
	}

	_gfilepath = inir.Get("Paths", "GFile Path", "gfiles");
	_datapath = inir.Get("Paths", "Data Path", "data");
	_menupath = inir.Get("Paths", "Menu Path", "menus");
	_mainmenu = inir.Get("Main", "Root Menu", "main");
	_msgpath = inir.Get("Paths", "Message Path", "msgs");

	auto data = toml::parse_file(_datapath + "/msgconfs.toml");

	auto confitems = data.get_as<toml::array>("messageconf");

	for (size_t i = 0; i < confitems->size(); i++) {
		auto itemtable = confitems->get(i)->as_table();

		std::string myname;
		std::string myconfig;
		int mysec_level;
		std::string mytagline;

		auto name = itemtable->get("name");
		if (name != nullptr) {
			myname = name->as_string()->value_or("Invalid Name");
		}
		else {
			myname = "Unknown Name";
		}
		auto conf = itemtable->get("config");
		if (conf != nullptr) {
			myconfig = conf->as_string()->value_or("");
		}
		else {
			myconfig = "";
		}

		auto tagline = itemtable->get("tagline");
		if (tagline != nullptr) {
			mytagline = tagline->as_string()->value_or("");
		}
		else {
			mytagline = "";
		}


		auto sec_level = itemtable->get("sec_level");
		if (sec_level != nullptr) {
			mysec_level = sec_level->as_integer()->value_or(10);
		}
		else {
			mysec_level = 10;
		}

		MsgConf c(myname, mysec_level, mytagline);

		if (c.load(n, myconfig)) {
			msgconfs.push_back(c);
		}
	}


	return true;
}