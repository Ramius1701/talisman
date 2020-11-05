#include <filesystem>
#include <sys/stat.h>
#include <sstream>
#include <fstream>
#include <cstring>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#include "Bulletins.h"
#include "Config.h"
#include "Node.h"
#include "toml.hpp"

void Bulletins::display(Node* n) {
	int hotkeylen = 0;
	struct tm ftm;
	struct stat s;
	std::stringstream ss;
	bool found;
	int total = 0;

	if (!isloaded) {
		return;
	}

	for (size_t i = 0; i < bullets.size(); i++) {
		if (bullets.at(i).hotkey.size() > hotkeylen) {
			hotkeylen = bullets.at(i).hotkey.size();
		}
	}

	while (true) {
		n->cls();
		n->send_gfile("bulletins");
		ss.str("");
		ss << "|14View Bulletin |08[";

		for (size_t i = 0; i < bullets.size(); i++) {
			found = false;
			if (n->hasANSI) {
				std::filesystem::path b(n->get_config()->gfile_path() + "/" + bullets.at(i).file + ".ans");
				if (stat(b.u8string().c_str(), &s) == 0) {
					found = true;
				}
			}
			if (!found) {
				std::filesystem::path b(n->get_config()->gfile_path() + "/" + bullets.at(i).file + ".asc");
				if (stat(b.u8string().c_str(), &s) == 0) {
					found = true;
				}
			}

			if (found) {
				total++;
				time_t t = s.st_mtime;

#ifdef _MSC_VER
				localtime_s(&ftm, &t);
#else
				localtime_r(&t, &ftm);
#endif
				n->print_f(" |15%*s |14%-32.32s |08Updated: |10%04d-%02d-%02d %02d:%02d\r\n", hotkeylen, bullets.at(i).hotkey.c_str(), bullets.at(i).name.c_str(), ftm.tm_year + 1900, ftm.tm_mon + 1, ftm.tm_mday, ftm.tm_hour, ftm.tm_min);
				ss << "|15" << bullets.at(i).hotkey;

				if (i < bullets.size() - 1) {
					ss << "|08,";
				}
			}
		}

		if (total == 0) {
			return;
		}

		ss << "|08] : |07";

		n->print_f("%s", ss.str().c_str());
		std::string cmd = n->get_string(hotkeylen, false);

		if (cmd.size() == 0) {
			return;
		}
		bool disp = false;
		for (size_t i = 0; i < bullets.size(); i++) {
			if (strcasecmp(cmd.c_str(), bullets.at(i).hotkey.c_str()) == 0) {
				n->cls();
				n->send_gfile(bullets.at(i).file, true);
				n->print_f("|14Press any key...|07");
				n->getch();
				disp = true;
				break;
			}
		}

		if (!disp) {
			return;
		}
	}

}

bool Bulletins::load(Node* n) {
	Config* c = n->get_config();
	try {
		auto data = toml::parse_file(c->data_path() + "/bulletins.toml");
		auto bullitems = data.get_as<toml::array>("bulletin");

		for (size_t i = 0; i < bullitems->size(); i++) {
			auto itemtable = bullitems->get(i)->as_table();

			std::string myname;
			std::string myfile;
			std::string myhotkey;
			int myseclevel;
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
			auto hotkey = itemtable->get("hotkey");
			if (hotkey != nullptr) {
				myhotkey = hotkey->as_string()->value_or("");
			}
			else {
				myhotkey = "";
			}
			auto seclevel = itemtable->get("sec_level");
			if (seclevel != nullptr) {
				myseclevel = seclevel->as_integer()->value_or(10);
			}
			else {
				myseclevel = 10;
			}

			if (myfile != "" && myhotkey != "") {
				struct bulletin_t b;

				b.name = myname;
				b.file = myfile;
				b.hotkey = myhotkey;
				b.seclevel = myseclevel;

				bullets.push_back(b);
			}
		}
		isloaded = true;
		return true;
	}
	catch (toml::parse_error) {
		return false;
	}
}