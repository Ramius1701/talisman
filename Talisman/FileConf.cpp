#include <string>
#include <fstream>
#include <vector>
#include "FileConf.h"
#include "Config.h"
#include "Node.h"
#include "toml.hpp"
#include "FileArea.h"

bool FileConf::load(Node* n)
{
	Config* c = n->get_config();

	try {
		auto data = toml::parse_file(c->data_path() + "/" + config_file + ".toml");

		auto areaitems = data.get_as<toml::array>("filearea");

		for (size_t i = 0; i < areaitems->size(); i++) {
			auto itemtable = areaitems->get(i)->as_table();

			std::string myname;
			std::string mydatabase;
			std::string mypath;
			int my_d_sec_level;
			int my_u_sec_level;
			auto name = itemtable->get("name");
			if (name != nullptr) {
				myname = name->as_string()->value_or("Invalid Name");
			}
			else {
				myname = "Unknown Name";
			}
			auto database = itemtable->get("database");
			if (database != nullptr) {
				mydatabase = database->as_string()->value_or("");
			}
			else {
				mydatabase = "";
			}
			auto fpath = itemtable->get("file_path");
			if (fpath != nullptr) {
				mypath = fpath->as_string()->value_or("");
			}
			else {
				mypath = "";
			}

			auto d_sec_level = itemtable->get("download_sec_level");
			if (d_sec_level != nullptr) {
				my_d_sec_level = d_sec_level->as_integer()->value_or(10);
			}
			else {
				my_d_sec_level = 10;
			}

			auto u_sec_level = itemtable->get("upload_sec_level");
			if (u_sec_level != nullptr) {
				my_u_sec_level = u_sec_level->as_integer()->value_or(10);
			}
			else {
				my_u_sec_level = 10;
			}

			if (mydatabase == "" || mypath == "") {
				continue;
			}

			FileArea f(myname, mypath, mydatabase, my_d_sec_level, my_u_sec_level);

			areas.push_back(f);
		}
		return true;
	}
	catch (toml::parse_error) {
		return false;
	}
}

int FileConf::list(Node* n, int sec) {
	Config* c = n->get_config();
	int lines = 0;
	int cur_conf = 1;
	while (true) {
		n->cls();

		for (size_t i = 0; i < c->fileconfs.size(); i++) {
			if (c->fileconfs.at(i).get_sec_level() > sec) continue;
			if (i == stoi(n->get_user().get_attribute("cur_file_conf", "-1"))) {
				n->print_f("|08[|14%3d|08]|11->|07%s\r\n", cur_conf++, c->fileconfs.at(i).get_name().c_str());
			}
			else {
				n->print_f("|08[|14%3d|08]  |07%s\r\n", cur_conf++, c->fileconfs.at(i).get_name().c_str());
			}
			lines++;
			if (lines == 24 && i != c->fileconfs.size() - 1) {
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
					catch (std::invalid_argument) {
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


int FileConf::list_areas(Node* n, int sec)
{
	Config* c = n->get_config();
	int cur_area = 1;
	int lines = 0;

	while (true) {
		n->cls();

		for (size_t i = 0; i < areas.size(); i++) {
			if (areas.at(i).get_d_sec_level() > sec) continue;
			
			if (i == stoi(n->get_user().get_attribute("cur_file_area", "-1"))) {
				n->print_f("|08[|14%3d|08]|11->|15%-32.32s |08%6d TOTAL|07\r\n", cur_area++, areas.at(i).get_name().c_str(), areas.at(i).get_total_files(n));
			}
			else {
				n->print_f("|08[|14%3d|08]  |15%-32.32s |08%6d TOTAL|07\r\n", cur_area++, areas.at(i).get_name().c_str(), areas.at(i).get_total_files(n));
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
