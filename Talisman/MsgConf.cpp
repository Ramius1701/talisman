#include <fstream>
#include "Node.h"
#include "MsgConf.h"
#include "toml.hpp"
#include "Config.h"

MsgConf::MsgConf(std::string name, int sec_level, std::string mytagline)
{
	isloaded = false;
	this->name = name;
	this->sec_level = sec_level;
	tagline = mytagline;
}

bool MsgConf::load(Node *n, std::string filename) {
	Config* c = n->get_config();
	auto data = toml::parse_file(c->data_path() + "/" + filename + ".toml");

	auto areaitems = data.get_as<toml::array>("messagearea");

	for (size_t i = 0; i < areaitems->size(); i++) {
		auto itemtable = areaitems->get(i)->as_table();

		std::string myname;
		std::string myfile;
		int my_r_sec_level;
		int my_w_sec_level;
		std::string myoaddr;
		bool mynetmail;
		int my_qwk_base_no;
		bool myrealnames;

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

		auto o_addr = itemtable->get("aka");
		if (o_addr != nullptr) {
			myoaddr = o_addr->as_string()->value_or("");
		}
		else {
			myoaddr = "";
		}

		auto netmail = itemtable->get("netmail");
		if (netmail != nullptr) {
			mynetmail = netmail->as_boolean()->value_or(false);
		}
		else {
			mynetmail = false;
		}

		auto realnames = itemtable->get("real_names");
		if (realnames != nullptr) {
			myrealnames = realnames->as_boolean()->value_or(false);
		}
		else {
			myrealnames = false;
		}

		auto q_base_no = itemtable->get("qwk_base_no");
		if (q_base_no != nullptr) {
			my_qwk_base_no = q_base_no->as_integer()->value_or(-1);
		}
		else {
			my_qwk_base_no = -1;
		}

		if (myfile != "") {
			MsgArea a(n, myname, c->msg_path() + "/" + myfile, my_r_sec_level, my_w_sec_level, myoaddr, mynetmail, tagline, my_qwk_base_no, myrealnames);
			areas.push_back(a);
		}
	}
	isloaded = true;
	return true;
}

struct area_list_entry_t {
	std::string name;
	int total_msgs;
	int new_msgs;
};


int MsgConf::list_areas(Node* n, int sec) {
	bool fsr = n->get_user().get_attribute("fullscreenreader", "true") == "true";
	if (fsr == false || !n->hasANSI) {
		return list_areas_old(n, sec);
	}
	else {
		return list_areas_fsr(n, sec);
	}
}

int MsgConf::list(Node* n, int sec) {
	bool fsr = n->get_user().get_attribute("fullscreenreader", "true") == "true";
	if (fsr == false || !n->hasANSI) {
		return list_old(n, sec);
	}
	else {
		return list_fsr(n, sec);
	}
}


int MsgConf::list_areas_fsr(Node* n, int sec) {
	std::vector<struct area_list_entry_t> area_entries;
	uint32_t selected = stoi(n->get_user().get_attribute("cur_msg_area", "-1"));
	for (size_t i = 0; i < areas.size(); i++) {
		if (areas.at(i).get_r_sec_level() > sec) continue;
		struct area_list_entry_t entry;

		int lr = n->get_user().user_get_lastread(areas.at(i).get_file());

		entry.name = areas.at(i).get_name();
		entry.total_msgs = areas.at(i).get_total_msgs();
		entry.new_msgs = areas.at(i).get_total_msgs() - lr;

		area_entries.push_back(entry);
	}

	if (selected == -1 || selected >= areas.size()) {
		selected = 0;
	}

	bool redraw = true;
	int start = 0;

	while (true) {
		if (redraw) {
			n->cls();
			n->print_f("\x1b[1;1H%sAreas in conference: %s\x1b[K", n->get_config()->get_prompt_colour(), name.c_str());
			n->print_f("\x1b[%d;1H%sUse Arrow Keys to Move, ENTER to Select\x1b[K", n->get_term_height() - 1, n->get_config()->get_prompt_colour());

			for (size_t i = start; i - start < n->get_term_height() - 3 && i < areas.size(); i++) {
				if (i == selected) {
					if (area_entries.at(i).new_msgs > 0) {
						n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d\x1b[%d;%dHNew: %d", (i - start) + 2, area_entries.at(i).name.c_str(), (i - start) + 2, n->get_term_width() - 24, area_entries.at(i).total_msgs, (i - start) + 2, n->get_term_width() - 10, area_entries.at(i).new_msgs);
					}
					else {
						n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d", (i - start) + 2, area_entries.at(i).name.c_str(), (i - start) + 2, n->get_term_width() - 24, area_entries.at(i).total_msgs);
					}
				}
				else {
					if (area_entries.at(i).new_msgs > 0) {
						n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d\x1b[%d;%dH\x1b[1;33mNew: %d", (i - start) + 2, area_entries.at(i).name.c_str(), (i - start) + 2, n->get_term_width() - 24, area_entries.at(i).total_msgs, (i - start) + 2, n->get_term_width() - 10, area_entries.at(i).new_msgs);
					}
					else {
						n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d", (i - start) + 2, area_entries.at(i).name.c_str(), (i - start) + 2, n->get_term_width() - 24, area_entries.at(i).total_msgs);
					}
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
							start = selected;
							redraw = true;
						}
						else {
							if (area_entries.at(selected).new_msgs > 0) {
								n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d\x1b[%d;%dH\x1b[1;33mNew: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs, (selected - start) + 2, n->get_term_width() - 10, area_entries.at(selected).new_msgs);
							}
							else {
								n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs);
							}
							selected--;
							if (area_entries.at(selected).new_msgs > 0) {
								n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d\x1b[%d;%dHNew: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs, (selected - start) + 2, n->get_term_width() - 10, area_entries.at(selected).new_msgs);
							}
							else {
								n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs);
							}
						}
					}
				}
				else if (c == 'B') {
					if (selected < area_entries.size() - 1) {
						if (selected + 1 >= start + (n->get_term_height() - 3) - 1) {
							selected++;
							start++;
							redraw = true;
						}
						else {
							if (area_entries.at(selected).new_msgs > 0) {
								n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d\x1b[%d;%dH\x1b[1;33mNew: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs, (selected - start) + 2, n->get_term_width() - 10, area_entries.at(selected).new_msgs);
							}
							else {
								n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K\x1b[%d;%dH\x1b[1;32mTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs);
							}

							selected++;
							if (area_entries.at(selected).new_msgs > 0) {
								n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d\x1b[%d;%dHNew: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs, (selected - start) + 2, n->get_term_width() - 10, area_entries.at(selected).new_msgs);
							}
							else {
								n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K\x1b[%d;%dHTotal: %d", (selected - start) + 2, area_entries.at(selected).name.c_str(), (selected - start) + 2, n->get_term_width() - 24, area_entries.at(selected).total_msgs);
							}
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

int MsgConf::list_areas_old(Node* n, int sec)
{
	Config *c = n->get_config();
	int cur_area = 1;
	int lines = 0;

	while (true) {
		n->cls();

		for (size_t i = 0; i < areas.size(); i++) {
			if (areas.at(i).get_r_sec_level() > sec) continue;
			int lr = n->get_user().user_get_lastread(areas.at(i).get_file());
			if (i == stoi(n->get_user().get_attribute("cur_msg_area", "-1"))) {
				if (areas.at(i).get_total_msgs() - lr > 0) {
					n->print_f("|08[|14%3d|08]|11->|15%-32.32s |08%6d TOTAL |11%6d NEW|07\r\n", cur_area++, areas.at(i).get_name().c_str(), areas.at(i).get_total_msgs(), areas.at(i).get_total_msgs() - lr);
				}
				else {
					n->print_f("|08[|14%3d|08]|11->|15%-32.32s |08%6d TOTAL|07\r\n", cur_area++, areas.at(i).get_name().c_str(), areas.at(i).get_total_msgs());
				}
			}
			else {
				if (areas.at(i).get_total_msgs() - lr > 0) {
					n->print_f("|08[|14%3d|08]  |15%-32.32s |08%6d TOTAL |11%6d NEW|07\r\n", cur_area++, areas.at(i).get_name().c_str(), areas.at(i).get_total_msgs(), areas.at(i).get_total_msgs() - lr);
				}
				else {
					n->print_f("|08[|14%3d|08]  |15%-32.32s |08%6d TOTAL|07\r\n", cur_area++, areas.at(i).get_name().c_str(), areas.at(i).get_total_msgs());
				}
			}
			lines++;
			if (lines == n->get_term_height() - 2 && i != areas.size() - 1) {
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


int MsgConf::list_fsr(Node* n, int sec) {
	uint32_t selected = stoi(n->get_user().get_attribute("cur_msg_conf", "-1"));

	if (selected == -1 || selected >= n->get_config()->msgconfs.size()) {
		selected = 0;
	}

	bool redraw = true;
	int start = 0;

	while (true) {
		if (redraw) {
			n->cls();
			n->print_f("\x1b[1;1H%sConferences Available\x1b[K", n->get_config()->get_prompt_colour());
			n->print_f("\x1b[%d;1H%sUse Arrow Keys to Move, ENTER to Select\x1b[K", n->get_term_height() - 1, n->get_config()->get_prompt_colour());

			for (size_t i = start; i - start < n->get_term_height() - 3 && i < n->get_config()->msgconfs.size(); i++) {
				if (i == selected) {
					n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K", (i - start) + 2, n->get_config()->msgconfs.at(i).name.c_str());
				}
				else {
					n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K", (i - start) + 2, n->get_config()->msgconfs.at(i).name.c_str());
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
						if (selected -1 < start) {
							selected--;
							start = selected;
							redraw = true;
						}
						else {
							n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K", (selected - start) + 2, n->get_config()->msgconfs.at(selected).name.c_str());
							selected--;
							n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K", (selected - start) + 2, n->get_config()->msgconfs.at(selected).name.c_str());
						}
					}
				}
				else if (c == 'B') {
					if (selected < n->get_config()->msgconfs.size() - 1) {
						if (selected + 1>= start + (n->get_term_height() - 3) - 1) {
							selected++;
							start++;
							redraw = true;
						}
						else {
							n->print_f("\x1b[%d;1H\x1b[1;40;37m%s\x1b[K", (selected - start) + 2, n->get_config()->msgconfs.at(selected).name.c_str());
							selected++;
							n->print_f("\x1b[%d;1H\x1b[0;47;30m%s\x1b[K", (selected - start) + 2, n->get_config()->msgconfs.at(selected).name.c_str());
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


int MsgConf::list_old(Node* n, int sec)
{
	Config *c = n->get_config();
	int lines = 0;
	int cur_conf = 1;
	while (true) {
		n->cls();

		for (size_t i = 0; i < c->msgconfs.size(); i++) {
			if (c->msgconfs.at(i).get_sec_level() > sec) continue;
			if (i == stoi(n->get_user().get_attribute("cur_msg_conf", "-1"))) {
				n->print_f("|08[|14%3d|08]|11->|07%s\r\n", cur_conf++, c->msgconfs.at(i).get_name().c_str());
			}
			else {
				n->print_f("|08[|14%3d|08]  |07%s\r\n", cur_conf++, c->msgconfs.at(i).get_name().c_str());
			}
			lines++;
			if (lines == 24 && i != c->msgconfs.size() - 1) {
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

void MsgConf::scan(Node* n) {
	int lines = 0;
	n->print_f("\r\n");
	for (size_t conf = 0; conf < n->get_config()->msgconfs.size(); conf++) {
		if (n->get_user().get_sec_level() < n->get_config()->msgconfs.at(conf).get_sec_level()) {
			continue;
		}
		if (lines >= n->get_term_height() - 5) {
			n->print_f("|14Continue? (Y/N) : |07");
			if (tolower(n->getche()) == 'n') {
				return;
			}
			n->print_f("\r\n");
			lines = 0;
		}
		n->print_f("|08------------------------------------------------------------------------------|07\r\n");
		n->print_f("|14CONFERENCE: |15%s\r\n", n->get_config()->msgconfs.at(conf).get_name().c_str());
		n->print_f("|08------------------------------------------------------------------------------|07\r\n");
		lines += 3;
		if (lines >= n->get_term_height() - 2) {
			n->print_f("|14Continue? (Y/N) : |07");
			if (tolower(n->getche()) == 'n') {
				return;
			}
			n->print_f("\r\n");
			lines = 0;
		}
		for (size_t area = 0; area < n->get_config()->msgconfs.at(conf).areas.size(); area++) {
			if (n->get_user().get_sec_level() < n->get_config()->msgconfs.at(conf).areas.at(area).get_r_sec_level()) {
				continue;
			}
			int lr = n->get_user().user_get_lastread(n->get_config()->msgconfs.at(conf).areas.at(area).get_file());
			if (n->get_config()->msgconfs.at(conf).areas.at(area).get_total_msgs() - lr  == 0) {
				n->print_f("   |15%-32.32s |08%6d TOTAL |07\r\n", n->get_config()->msgconfs.at(conf).areas.at(area).get_name().c_str(), n->get_config()->msgconfs.at(conf).areas.at(area).get_total_msgs());
			}
			else {
				n->print_f(" |11* |15%-32.32s |08%6d TOTAL |11%6d NEW!|07\r\n", n->get_config()->msgconfs.at(conf).areas.at(area).get_name().c_str(), n->get_config()->msgconfs.at(conf).areas.at(area).get_total_msgs(), n->get_config()->msgconfs.at(conf).areas.at(area).get_total_msgs() - lr);
			}
			lines++;
			if (lines >= n->get_term_height() - 2) {
				n->print_f("|14Continue? (Y/N) : |07");
				if (tolower(n->getche()) == 'n') {
					return;
				}
				n->print_f("\r\n");
				lines = 0;
			}
		}
	}
	n->pause();
}
