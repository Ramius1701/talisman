#include <fstream>
#include <iostream>
#include "toml.hpp"
#include "INIReader.h"
#include "Config.h"
#include "Protocol.h"
#include "FileConf.h"
#include "Archiver.h"

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
	_scriptpath = inir.Get("Paths", "Script Path", "scripts");
	_opname = inir.Get("Main", "Sysop Name", "Sysop");
	_sysname = inir.Get("Main", "System Name", "Talisman");
	_netmailsem = inir.Get("Paths", "Netmail Semaphore", "netmail.sem");
	_echomailsem = inir.Get("Paths", "Echomail Semaphore", "echomail.sem");
	_externaleditor = inir.Get("Paths", "External Editor", "");
	_logpath = inir.Get("Paths", "Log Path", "logs");

	try {
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
	}
	catch (toml::parse_error) {
		std::cerr << "Error parsing " << _datapath << "/msgconfs.toml" << std::endl;
		return false;
	}
	try {
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
	}
	catch (toml::parse_error) {
		std::cerr << "Error parsing " << _datapath << "/seclevels.toml" << std::endl;
		return false;
	}
	try {
		auto data3 = toml::parse_file(_datapath + "/protocols.toml");

		auto protitems = data3.get_as<toml::array>("protocol");

		for (size_t i = 0; i < protitems->size(); i++) {
			auto itemtable = protitems->get(i)->as_table();

			std::string myname;
			std::string myul_cmd;
			std::string mydl_cmd;
			bool mybatch;
			bool myprompt;

			auto name = itemtable->get("name");
			if (name != nullptr) {
				myname = name->as_string()->value_or("Invalid Name");
			}
			else {
				myname = "Unknown";
			}
			auto ul_cmd = itemtable->get("upload_command");
			if (ul_cmd != nullptr) {
				myul_cmd = ul_cmd->as_string()->value_or("");
			}
			else {
				myul_cmd = "";
			}
			auto dl_cmd = itemtable->get("download_command");
			if (dl_cmd != nullptr) {
				mydl_cmd = dl_cmd->as_string()->value_or("");
			}
			else {
				mydl_cmd = "";
			}

			auto batch = itemtable->get("batch");
			if (batch != nullptr) {
				mybatch = batch->as_boolean()->value_or(false);
			}
			else {
				mybatch = false;
			}
			auto prompt = itemtable->get("prompt");
			if (prompt != nullptr) {
				myprompt = prompt->as_boolean()->value_or(true);
			}
			else {
				myprompt = true;
			}

			Protocol* p = new Protocol(myname, mydl_cmd, myul_cmd, mybatch, myprompt);
			protocols.push_back(p);
		}
	}
	catch (toml::parse_error) {
		std::cerr << "Error parsing " << _datapath << "/protocols.toml" << std::endl;
		return false;
	}
	try {
		auto data3 = toml::parse_file(_datapath + "/archivers.toml");

		auto arcitems = data3.get_as<toml::array>("archiver");

		for (size_t i = 0; i < arcitems->size(); i++) {
			auto itemtable = arcitems->get(i)->as_table();

			std::string myname;
			std::string myext;
			std::string myunarc;
			std::string myarc;

			auto name = itemtable->get("name");
			if (name != nullptr) {
				myname = name->as_string()->value_or("Invalid Name");
			}
			else {
				myname = "Unknown";
			}

			auto ext = itemtable->get("extension");
			if (ext != nullptr) {
				myext = ext->as_string()->value_or("");
			}
			else {
				myext = "";
			}

			auto unarc = itemtable->get("unarc");
			if (unarc != nullptr) {
				myunarc = unarc->as_string()->value_or("");
			}
			else {
				myunarc = "";
			}
			auto arc = itemtable->get("arc");
			if (arc != nullptr) {
				myarc = arc->as_string()->value_or("");
			}
			else {
				myarc = "";
			}
			Archiver* a = new Archiver(myname, myext, myunarc, myarc);
			archivers.push_back(a);
		}
	}
	catch (toml::parse_error) {
		std::cerr << "Error parsing " << _datapath << "/protocols.toml" << std::endl;
		return false;
	}
	try {
		auto data = toml::parse_file(_datapath + "/fileconfs.toml");

		auto confitems = data.get_as<toml::array>("fileconf");

		for (size_t i = 0; i < confitems->size(); i++) {
			auto itemtable = confitems->get(i)->as_table();

			std::string myname;
			std::string myconfig;
			int mysec_level;

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

			auto sec_level = itemtable->get("sec_level");
			if (sec_level != nullptr) {
				mysec_level = sec_level->as_integer()->value_or(10);
			}
			else {
				mysec_level = 10;
			}

			FileConf f(myname, myconfig, mysec_level);

			if (f.load(n)) {
				fileconfs.push_back(f);
			}
		}
	}
	catch (toml::parse_error) {
		std::cerr << "Error parsing " << _datapath << "/msgconfs.toml" << std::endl;
		return false;
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

Protocol* Config::select_protocol(Node* n) {
	n->print_f("|14Available Protocols\r\n");
	n->print_f("|08----------------------------------------\r\n");
	for (size_t i = 0; i < protocols.size(); i++) {
		n->print_f("|15%2d|08. |14%s\r\n", i + 1, protocols.at(i)->get_name().c_str());
	}
	n->print_f("|15 Q|08. |14Quit\r\n");
	n->print_f("|08----------------------------------------\r\n");
	std::string res = n->get_string(2, false);
	if (res.size() > 0) {
		if (tolower(res.at(0)) == 'q') {
			return nullptr;
		}
		try {
			int prot = stoi(res);
			if (prot > 0 && prot <= protocols.size()) {
				return protocols.at(prot - 1);
			}
		}
		catch (std::invalid_argument) {

		}
		catch (std::out_of_range) {

		}
	}
	return nullptr;
}