#include <fstream>
#include "Node.h"
#include "MsgConf.h"
#include "toml.hpp"
#include "Config.h"

MsgConf::MsgConf(std::string name, int sec_level)
{
	isloaded = false;
	this->name = name;
	this->sec_level = sec_level;
}

bool MsgConf::load(Config *c, std::string filename) {
	auto data = toml::parse_file(c->data_path() + "/" + filename + ".toml");

	auto areaitems = data.get_as<toml::array>("messagearea");

	for (size_t i = 0; i < areaitems->size(); i++) {
		auto itemtable = areaitems->get(i)->as_table();

		std::string myname;
		std::string myfile;
		int my_r_sec_level;
		int my_w_sec_level;

		auto name = itemtable->get("name");
		if (name != nullptr) {
			myname = name->as_string()->value_or("Invalid Name");
		}
		else {
			myname = "Unknown Name";
		}
		auto file = itemtable->get("file");
		if (file != nullptr) {
			myfile = file->as_string()->value_or("");
		}
		else {
			myfile = "";
		}

		auto r_sec_level = itemtable->get("read_sec_level");
		if (r_sec_level != nullptr) {
			my_r_sec_level = r_sec_level->as_integer()->value_or(10);
		}
		else {
			my_r_sec_level = 10;
		}

		auto w_sec_level = itemtable->get("write_sec_level");
		if (w_sec_level != nullptr) {
			my_w_sec_level = w_sec_level->as_integer()->value_or(10);
		}
		else {
			my_w_sec_level = 10;
		}

		if (myfile != "") {
			MsgArea a(myname, myfile, my_r_sec_level, my_w_sec_level);
			areas.push_back(a);
		}
	}
	isloaded = true;
	return true;
}

int MsgConf::list_areas(Node* n, int sec)
{
	Config c = n->get_config();
	int cur_area = 1;
	int lines = 0;

	while (true) {
		n->cls();

		for (size_t i = 0; i < areas.size(); i++) {
			if (areas.at(i).get_r_sec_level() > sec) continue;
			if (i == stoi(n->get_user().get_attribute("cur_msg_area", "-1"))) {
				n->print_f("|08[|14%3d|08] |07%s |11<-|07\r\n", cur_area++, areas.at(i).get_name().c_str());
			}
			else {
				n->print_f("|08[|14%3d|08] |07%s\r\n", cur_area++, areas.at(i).get_name().c_str());
			}
			lines++;
			if (lines == 24 && i != areas.size() - 1) {
				n->print_f("|14Select |08[|151|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", cur_area - 1);
				std::string res = n->get_string(3, false);

				if (res.size() == 0) {
					lines = 0;
					n->print_f("\r\n");
					continue;
				}
				else if (tolower(res[0]) == 'q') {
					return 0;
				}
				else {
					try {
						return std::stoi(res);
					}
					catch (std::invalid_argument) {
						return 0;
					}
				}
			}
		}
		n->print_f("|14Select |08[|151|08-|15%d|08] |15?|08=|14List again|08, |15ENTER|08=|14Quit |07", cur_area - 1);
		std::string res = n->get_string(3, false);

		if (res.size() == 0) {
			return 0;
		}
		else if (res[0] == '?') {
			continue;
		}
		else {
			try {
				return std::stoi(res);
			}
			catch (std::invalid_argument) {
				return 0;
			}
		}
	}

	return 0;
}

int MsgConf::list(Node* n, int sec)
{
	Config c = n->get_config();
	int lines = 0;
	int cur_conf = 1;
	while (true) {
		n->cls();

		for (size_t i = 0; i < c.msgconfs.size(); i++) {
			if (c.msgconfs.at(i).get_sec_level() > sec) continue;
			if (i == stoi(n->get_user().get_attribute("cur_msg_conf", "-1"))) {
				n->print_f("|08[|14%3d|08] |07%s |11<-|07\r\n", cur_conf++, c.msgconfs.at(i).get_name().c_str());
			}
			else {
				n->print_f("|08[|14%3d|08] |07%s\r\n", cur_conf++, c.msgconfs.at(i).get_name().c_str());
			}
			lines++;
			if (lines == 24 && i != c.msgconfs.size() - 1) {
				n->print_f("|14Select |08[|151|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", cur_conf - 1);
				std::string res = n->get_string(3, false);

				if (res.size() == 0) {
					lines = 0;
					n->print_f("\r\n");
					continue;
				}
				else if (tolower(res[0]) == 'q') {
					return 0;
				}
				else {
					try {
						return std::stoi(res);
					}
					catch (std::invalid_argument){
						return 0;
					}
				}
			}
		}
		n->print_f("|14Select |08[|151|08-|15%d|08] |15?|08=|14List again|08, |15ENTER|08=|14Quit |07", cur_conf - 1);
		std::string res = n->get_string(3, false);

		if (res.size() == 0) {
			return 0;
		}
		else if (res[0] == '?') {
			continue;
		}
		else {
			try {
				return std::stoi(res);
			}
			catch (std::invalid_argument) {
				return 0;
			}
		}
	}
}
