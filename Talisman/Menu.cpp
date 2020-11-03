#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#include <fstream>
#include <sstream>
#include "Node.h"
#include "Menu.h"
#include "CallLog.h"
#include "Door.h"
#include "Settings.h"
#include "Logger.h"
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

		n->log->log(1, "%s loading menu %s on node %d", n->get_user().get_username().c_str(), filename.c_str(), n->getnodenum());

		auto _gfile = data["menu"]["gfile"].as_string();

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
		n->cls();
		if (gfile != "") {
			n->send_gfile(gfile);
		}

		std::stringstream ss;

		ss.str("");

		ss << "|08[";

		int longest_hotkey = 0;

		for (size_t i = 0; i < items.size(); i++) {
			ss << "|15" << items[i].hotkey;
			if (items[i].hotkey.size() > longest_hotkey) {
				longest_hotkey = items[i].hotkey.size();
			}
			if (i < items.size() - 1) {
				ss << "|08,";
			}
		}

		ss << "|08]";

		n->print_f("\r\n|14Command %s|08: ", ss.str().c_str());
		std::string cmd = n->get_string(longest_hotkey, false);

		for (size_t i = 0; i < items.size(); i++) {
			if (strcasecmp(cmd.c_str(), items[i].hotkey.c_str()) == 0) {
				if (strcasecmp(items[i].command.c_str(), "goodbye") == 0) {
					return true;
				} else if (strcasecmp(items[i].command.c_str(), "prevmenu") == 0) {
					return false;
				} else if (strcasecmp(items[i].command.c_str(), "submenu") == 0) {
					Menu m(n);
					if (m.load(n->get_config()->menu_path() + "/" + items[i].data + ".toml")) {
						if (m.run() == true) {
							return true;
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "listconfs") == 0) {
					n->log->log(1, "%s listing conferences on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					int newconf = MsgConf::list(n, n->get_user().get_sec_level());
					int count = 1;
					for (size_t mc = 0; mc < n->get_config()->msgconfs.size();mc++) {
						if (n->get_config()->msgconfs.at(mc).get_sec_level() > n->get_user().get_sec_level()) continue;
						if (count == newconf) {
							n->get_user().set_attribute("cur_msg_conf", std::to_string(mc));
							n->get_user().set_attribute("cur_msg_area", "-1");
							for (size_t ma = 0; ma < n->get_config()->msgconfs.at(mc).areas.size(); ma++) {
								if (n->get_config()->msgconfs.at(mc).areas.at(ma).get_r_sec_level() <= n->get_user().get_sec_level()) {
									n->get_user().set_attribute("cur_msg_area", std::to_string(ma));
									break;
								}
							}
							break;
						}
						count++;
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "listareas") == 0) {
					n->log->log(1, "%s listing areas on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					int msgconf = stoi(n->get_user().get_attribute("cur_msg_conf", "-1"));
					if (msgconf == -1) {
						n->print_f("|14Select a message conference first!|07");
					} else {
						int newarea = n->get_config()->msgconfs.at(msgconf).list_areas(n, n->get_user().get_sec_level());
						int count = 1;
						for (size_t ma = 0; ma < n->get_config()->msgconfs.at(msgconf).areas.size(); ma++) {
							if (count == newarea) {
								n->get_user().set_attribute("cur_msg_area", std::to_string(ma));
								break;
							}
							count++;
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "listmsgs") == 0) {
					n->log->log(1, "%s listing messages on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->print_f("\r\n\r\n");
					int msgconf = stoi(n->get_user().get_attribute("cur_msg_conf", "-1"));
					if (msgconf == -1) {
						n->print_f("|14Select a message conference first!|07");
					}
					else {
						int msgarea = stoi(n->get_user().get_attribute("cur_msg_area", "-1"));
						if (msgarea == -1) {
							n->print_f("|14Select a message area first!|07");
						}
						else {
							n->print_f("|14Start at |15F|08=|14First|08, |15L|08=|14Last Read or |08[|151|08-|15%d|08]: |07", n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_total_msgs());
							std::string start = n->get_string(6, false);
							int msgno;
							if (tolower(start[0]) == 'f') {
								msgno = 1;
							}
							else if (tolower(start[0] == 'l')) {
								int lr = n->get_user().user_get_lastread(n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_file());
								if (lr == 0) {
									msgno = 1;
								}
								else {
									msgno = lr + 1;
								}
							}
							else {
								try {
									msgno = stoi(start);
									if (msgno == 0) msgno++;
								}
								catch (std::invalid_argument) {
									msgno = 1;
								}
								catch (std::out_of_range) {
									msgno = 1;
								}
							}
							msgno = n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).list_messages(msgno);
							if (msgno > 0 && msgno <= n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_total_msgs()) {
								n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).read_message(msgno);
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "postmsg") == 0) {
					n->log->log(1, "%s listing posting a message on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					int msgconf = stoi(n->get_user().get_attribute("cur_msg_conf", "-1"));
					if (msgconf == -1) {
						n->print_f("|14Select a message conference first!|07\r\n");
					}
					else {
						int msgarea = stoi(n->get_user().get_attribute("cur_msg_area", "-1"));
						if (msgarea == -1) {
							n->print_f("|14Select a message area first!|07\r\n");
						}
						else {
							if (n->get_user().get_sec_level() < n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_w_sec_level()) {
								n->print_f("|14Sorry, you do not have permission to post in this area!|07\r\n");
							}
							else {
								n->print_f("\r\n     To: ");
								std::string to = n->get_string(35, false);
								n->print_f("\r\nSubject: ");
								std::string subject = n->get_string(60, false);
								std::string netaddr;
								if (n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).is_netmail()) {
									n->print_f("\r\nAddress: ");
									netaddr = n->get_string(16, false);
								}
								else {
									netaddr = "";
								}

								if (to.size() == 0) {
									to = "All";
								}
								if (subject.size() == 0) {
									n->print_f("\r\n|14Aborted!\r\n");
								}
								else {
									n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).enter_message(to, subject, netaddr, 0, nullptr);
								}
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "mailscan") == 0) {
					n->log->log(1, "%s running mailscan on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->cls();
					MsgConf::scan(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "last10") == 0) {
					n->log->log(1, "%s listing last 10 callers on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->cls();
					CallLog::last10_callers(n);
					n->print_f("|14Press any key...|07");
					n->getch();
				}
				else if (strcasecmp(items[i].command.c_str(), "rundoor") == 0) {
					n->log->log(1, "%s running door %s on node %d", n->get_user().get_username().c_str(), items[i].data.c_str(), n->getnodenum());
					std::vector<std::string> arguments;
					arguments.push_back(std::to_string(n->getnodenum()));
#ifdef _MSC_VER
					arguments.push_back(std::to_string(n->get_socket()));
#endif
					Door::createDropfiles(n);
					Door::runExternal(n, items[i].data, arguments, false);
					n->clog->ran_door();
				}
				else if (strcasecmp(items[i].command.c_str(), "sysinfo") == 0) {
					n->log->log(1, "%s looking at system info on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->system_info();
					n->print_f("|14Press any key...|07");
					n->getch();
				}
				else if (strcasecmp(items[i].command.c_str(), "settings") == 0) {
					n->log->log(1, "%s modifying settings on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					Settings::do_settings(n);
				}
			}

		}
	}
}