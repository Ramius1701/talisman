#include <string>
#include <fstream>
#include <vector>
#include "FileConf.h"
#include "Config.h"
#include "Node.h"
#include "../Common/toml.hpp"
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
	bool fsr = n->get_user().get_attribute("fullscreenreader", "true") == "true";
	if (fsr == false || !n->hasANSI) {
		return list_old(n, sec);
	}
	else {
		return list_fsr(n, sec);
	}
}

int FileConf::list_fsr(Node* n, int sec) {
	int selected = stoi(n->get_user().get_attribute("cur_file_conf", "-1"));

	if (selected == -1 || selected >= (int)n->get_config()->fileconfs.size()) {
		selected = 0;
	}

	bool redraw = true;
	int start = 0;

	while (true) {
		if (redraw) {
			n->print_f("\x1b[0;40;37m");
			n->cls();
			n->print_f("\x1b[1;1H%sFile Conferences Available\x1b[K", n->get_config()->get_prompt_colour());
			n->print_f("\x1b[%d;1H%sUse Arrow Keys to Move, ENTER to Select\x1b[K", n->get_term_height() - 1, n->get_config()->get_prompt_colour());

			for (size_t i = start; i - start < n->get_term_height() - 3 && i < n->get_config()->fileconfs.size(); i++) {
				if (i == selected) {
					n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K", (i - start) + 2, n->get_config()->fileconfs.at(i).name.c_str());
				}
				else {
					n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K", (i - start) + 2, n->get_config()->fileconfs.at(i).name.c_str());
				}
			}
			redraw = false;
		}
		char c = n->getch();

		if (c == '\x1b') {
			c = n->getch();
			if (c == '[') {
				c = n->getch();
				if (c == 'A') {
					if (selected > 0) {
						if (selected - 1 < start) {
							selected--;
							start = selected - (n->get_term_height() - 3);
							if (start < 0) {
								start = 0;
							}
							redraw = true;
						}
						else {
							n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K", (selected - start) + 2, n->get_config()->fileconfs.at(selected).name.c_str());
							selected--;
							n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K", (selected - start) + 2, n->get_config()->fileconfs.at(selected).name.c_str());
						}
					}
				}
				else if (c == 'B') {
					if (selected < (int)n->get_config()->fileconfs.size() - 1) {
						if (selected + 1 >= start + ((int)n->get_term_height() - 2) - 1) {
							selected++;
							start = selected;
							redraw = true;
						}
						else {
							n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K", (selected - start) + 2, n->get_config()->fileconfs.at(selected).name.c_str());
							selected++;
							n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K", (selected - start) + 2, n->get_config()->fileconfs.at(selected).name.c_str());
						}
					}


				}
				continue;
			}
		}
		else if (c == '\r') {
			n->print_f("\x1b[0;40;37m");
			return selected + 1;
		}
		else if (c == 'q' || c == 'Q') {
			n->print_f("\x1b[0;40;37m");
			return -1;
		}

	}
}


int FileConf::list_old(Node* n, int sec) {
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

struct filearea_list_entry_t {
	std::string name;
	int total_files;
};

int FileConf::list_areas(Node* n, int sec) 
{
	bool fsr = n->get_user().get_attribute("fullscreenreader", "true") == "true";
	if (fsr == false || !n->hasANSI) {
		return list_areas_old(n, sec);
	}
	else {
		return list_areas_fsr(n, sec);
	}
}

int FileConf::list_areas_fsr(Node* n, int sec)
{
	std::vector<struct filearea_list_entry_t > area_entries;
	int selected = stoi(n->get_user().get_attribute("cur_file_area", "-1"));
	for (size_t i = 0; i < areas.size(); i++) {
		if (areas.at(i).get_d_sec_level() > sec) continue;
		struct filearea_list_entry_t entry;

		entry.name = areas.at(i).get_name();
		entry.total_files = areas.at(i).get_total_files(n);

		area_entries.push_back(entry);
	}

	if (selected == -1 || selected >= (int)areas.size()) {
		selected = 0;
	}

	bool redraw = true;
	int start = 0;

	while (true) {
		if (redraw) {
			n->print_f("\x1b[0;40;37m");
			n->cls();
			n->print_f("\x1b[1;1H%sFile areas in conference: %s\x1b[K", n->get_config()->get_prompt_colour(), name.c_str());
			n->print_f("\x1b[%d;1H%sUse Arrow Keys to Move, ENTER to Select\x1b[K", n->get_term_height() - 1, n->get_config()->get_prompt_colour());

			for (size_t i = start; i - start < n->get_term_height() - 3 && i < area_entries.size(); i++) {
				if (i == selected) {
					n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d", (i - start) + 2, area_entries.at(i).name.c_str(), (i - start) + 2, n->get_term_width() - 24, area_entries.at(i).total_files);
				}
				else {
					n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d", (i - start) + 2, area_entries.at(i).name.c_str(), (i - start) + 2, n->get_term_width() - 24, area_entries.at(i).total_files);
				}
			}
			redraw = false;
		}
		char c = n->getch();

		if (c == '\x1b') {
			c = n->getch();
			if (c == '[') {
				c = n->getch();
				if (c == 'A') {
					if (selected > 0) {
						if (selected - 1 < start) {
							selected--;
							start = selected - (n->get_term_height() - 3);
							if (start < 0) {
								start = 0;
							}
							redraw = true;
						}
						else {
							n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_files);
							selected--;
							n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_files);
						}
					}
				}
				else if (c == 'B') {
					if (selected < (int)area_entries.size() - 1) {
						if (selected + 1 >= start + ((int)n->get_term_height() - 2) - 1) {
							selected++;
							start = selected;
							redraw = true;
						}
						else {
							n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_files);
							selected++;
							n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_files);
						}
					}
				}
			}
			continue;
				
		}
		else if (c == '\r') {
			n->print_f("\x1b[0;40;37m");
			return selected + 1;
		}
		else if (c == 'q' || c == 'Q') {
			n->print_f("\x1b[0;40;37m");
			return -1;
		}
	}
}


int FileConf::list_areas_old(Node* n, int sec)
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
