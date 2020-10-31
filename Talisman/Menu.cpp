#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#include <fstream>
#include <sstream>
#include "Node.h"
#include "Menu.h"
#include "toml.hpp"
Menu::Menu(Node *n)
{
	isloaded = false;
	this->n = n;
}

bool Menu::load(std::string filename)
{
	try {
		auto data = toml::parse_file(filename);


		auto _gfile = data["menu"]["gfiles"].as_string();

		if (_gfile == nullptr) {
			gfile = "";
		}
		else {
			gfile = _gfile->value_or("");
		}

		auto menuitems = data.get_as<toml::array>("menuitem");

		for (size_t i = 0; i < menuitems->size(); i++) {
			auto itemtable = menuitems->get(i)->as_table();

			struct menuitem_t item;

			auto command = itemtable->get("command");
			if (command != nullptr) {
				item.command = command->as_string()->value_or("");
			}
			else {
				item.command = "";
			}
			auto data = itemtable->get("data");

			if (data != nullptr) {
				item.data = data->as_string()->value_or("");
			}
			else {
				item.data = "";
			}

			auto hotkey = itemtable->get("hotkey");
			if (hotkey != nullptr) {
				item.hotkey = hotkey->as_string()->value_or("");
			}
			else {
				item.hotkey = "";
			}

			auto sec_level = itemtable->get("sec_level");

			if (sec_level != nullptr) {
				item.sec_level = sec_level->as_integer()->value_or(0);
			}
			else {
				item.sec_level = 0;
			}
			items.push_back(item);
		}
	}
	catch (std::exception e) {
		return false;
	}
	isloaded = true;
	return true;

}

bool Menu::run() {
	if (!isloaded) {
		n->print_f("Menu is not loaded!\r\n");
		return false;
	}
	while (true) {
		if (gfile != "") {
			n->send_gfile(gfile);
		}

		std::stringstream ss;

		ss.str("");

		ss << "|15[";

		int longest_hotkey = 0;

		for (size_t i = 0; i < items.size(); i++) {
			ss << "|11" << items[i].hotkey;
			if (items[i].hotkey.size() > longest_hotkey) {
				longest_hotkey = items[i].hotkey.size();
			}
			if (i < items.size() - 1) {
				ss << "|15, ";
			}
		}

		ss << "|15]";

		n->print_f("\r\n|15Command %s|16: ", ss.str().c_str());
		std::string cmd = n->get_string(longest_hotkey, false);

		for (size_t i = 0; i < items.size(); i++) {
			if (strcasecmp(cmd.c_str(), items[i].hotkey.c_str()) == 0) {
				if (strcasecmp(items[i].command.c_str(), "goodbye") == 0) {
					return true;
				} else if (strcasecmp(items[i].command.c_str(), "prevmenu") == 0) {
					return false;
				} else if (strcasecmp(items[i].command.c_str(), "submenu") == 0) {
					Menu m(n);
					if (m.load(n->get_config().menu_path() + "/" + items[i].data + ".toml")) {
						if (m.run() == true) {
							return true;
						}
					}
				}
			}
		}
	}
}