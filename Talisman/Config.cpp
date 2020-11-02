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
	_tmppath = inir.Get("Paths", "Temp Path", "temp");
	_opname = inir.Get("Main", "Sysop Name", "Sysop");
	_sysname = inir.Get("Main", "System Name", "Talisman");
	_netmailsem = inir.Get("Paths", "Netmail Semaphore", "netmail.sem");
	_echomailsem = inir.Get("Paths", "Echomail Semaphore", "echomail.sem");
	_externaleditor = inir.Get("Paths", "External Editor", "");
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

	auto data2 = toml::parse_file(_datapath + "/seclevels.toml");

	auto secitems = data2.get_as<toml::array>("seclevel");

	for (size_t i = 0; i < secitems->size(); i++) {
		auto itemtable = secitems->get(i)->as_table();

		std::string myname;
		int mysec_level;
		int mytimeonline;
		int mytimeout;

		auto name = itemtable->get("name");
		if (name != nullptr) {
			myname = name->as_string()->value_or("Invalid Name");
		}
		else {
			myname = "Unknown Name";
		}
		auto seclvl = itemtable->get("sec_level");
		if (seclvl != nullptr) {
			mysec_level = seclvl->as_integer()->value_or(0);
		}
		else {
			mysec_level = 0;
		}

		auto timeon = itemtable->get("mins_per_day");
		if (timeon != nullptr) {
			mytimeonline = timeon->as_integer()->value_or(0);
		}
		else {
			mytimeonline = 0;
		}
		auto timeout = itemtable->get("timeout_mins");
		if (timeout != nullptr) {
			mytimeout = timeout->as_integer()->value_or(0);
		}
		else {
			mytimeout = 0;
		}

		if (mysec_level != 0) {
			struct sec_level_t slvl;
			slvl.level = mysec_level;
			slvl.name = myname;
			slvl.timeout = mytimeout;
			slvl.time_online = mytimeonline;
			seclevels.push_back(slvl);
		}
	}
	return true;
}

struct sec_level_t* Config::get_sec_level_info(int seclvl) {
	for (size_t i = 0; i < seclevels.size(); i++) {
		if (seclevels.at(i).level == seclvl) {
			return &seclevels.at(i);
		}
	}
	return NULL;
}