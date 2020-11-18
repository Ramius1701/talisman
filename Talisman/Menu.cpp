#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#endif
#include <fstream>
#include <sstream>
#include <iterator>
#include <cstring>
#include "GenDefs.h"
#include "Node.h"
#include "Menu.h"
#include "CallLog.h"
#include "Door.h"
#include "Settings.h"
#include "Logger.h"
#include "Editor.h"
#include "Email.h"
#include "Bulletins.h"
#include "toml.hpp"
#include "Protocol.h"
#include "Config.h"
#include "Script.h"
#include "Archiver.h"
#include "Qwk.h"
#include "Nodelist.h"
#include "Squish.h"

Menu::Menu(Node *n)
{
	isloaded = false;
	this->n = n;
}

bool Menu::load(std::string filename)
{
	try {
		auto data = toml::parse_file(filename);

		n->log->log(LOG_INFO, "%s loading menu %s on node %d", n->get_user().get_username().c_str(), filename.c_str(), n->getnodenum());

		auto _gfile = data["menu"]["gfile"].as_string();

		if (_gfile == nullptr) {
			gfile = "";
		}
		else {
			gfile = _gfile->value_or("");
		}

		auto _prompt = data["menu"]["prompt"].as_string();

		if (_prompt == nullptr) {
			prompt = "";
		}
		else {
			prompt = _prompt->value_or("");
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
	std::filesystem::path nmsgp(n->get_config()->tmp_path());

	nmsgp.append(std::to_string(n->getnodenum()));
	nmsgp.append("node.msg");

	

	while (true) {
		if (std::filesystem::exists(nmsgp)) {
			n->cls();
			std::ifstream file(nmsgp);
			std::string str;
			while (std::getline(file, str))
			{
				n->print_f("%s\r\n", str.c_str());
			}
			file.close();
			std::filesystem::remove(nmsgp);

			n->pause();
		}

		n->update_node_use("Browsing Menus");

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

		if (prompt == "") {
			n->print_f("\r\n|14Command %s|08: ", ss.str().c_str());
		}
		else {
			n->print_f("\r\n%s ", prompt.c_str());
		}

		std::string cmd = n->get_string(longest_hotkey, false);

		n->print_f("\r\n");

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
					n->update_node_use("Listing Mail Conferences");
					n->log->log(LOG_INFO, "%s listing conferences on node %d", n->get_user().get_username().c_str(), n->getnodenum());
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
					n->update_node_use("Listing Mail Areas");
					n->log->log(LOG_INFO, "%s listing areas on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					int msgconf = stoi(n->get_user().get_attribute("cur_msg_conf", "-1"));
					if (msgconf == -1) {
						n->print_f("|14Select a message conference first!|07");
					} else {
						int newarea = n->get_config()->msgconfs.at(msgconf).list_areas(n, n->get_user().get_sec_level());
						int count = 1;
						for (size_t ma = 0; ma < n->get_config()->msgconfs.at(msgconf).areas.size(); ma++) {
							if (n->get_config()->msgconfs.at(msgconf).areas.at(ma).get_r_sec_level() <= n->get_user().get_sec_level()) {
								if (count == newarea) {
									n->get_user().set_attribute("cur_msg_area", std::to_string(ma));
									break;
								}
								count++;
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "listmsgs") == 0) {
					n->update_node_use("Listing Messages");
					n->log->log(LOG_INFO, "%s listing messages on node %d", n->get_user().get_username().c_str(), n->getnodenum());
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
				n->update_node_use("Posting a Message");
					n->log->log(LOG_INFO, "%s posting a message on node %d", n->get_user().get_username().c_str(), n->getnodenum());
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
								bool doabort = false;
								n->print_f("\r\n     To: ");
								std::string to = n->get_string(35, false);
								n->print_f("\r\nSubject: ");
								std::string subject = n->get_string(60, false);
								std::string netaddr;
								if (n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).is_netmail()) {
									n->print_f("\r\nAddress: ");
									netaddr = n->get_string(16, false);
									NETADDR *na = parse_fido_addr(netaddr.c_str());

									if (na == NULL) {
										doabort = true;
									}
									else {
										if (na->point == 0) {
											n->print_f("\r\n\r\n|14 Sending to.. |15%d:%d/%d.%d (%s)", na->zone, na->net, na->node, na->point, Nodelist::lookup_bbsname(n, std::to_string(na->zone) + ":" + std::to_string(na->net) + "/" + std::to_string(na->node)).c_str());
										}
										else {
											n->print_f("\r\n\r\n|14 Sending to.. |15%d:%d/%d.%d (A Point System)", na->zone, na->net, na->node, na->point);
										}
										free(na);
									}
								}
								else {
									netaddr = "";
								}

								if (to.size() == 0) {
									to = "All";
								}
								if (doabort || subject.size() == 0) {
									n->print_f("\r\n|14Aborted!\r\n");
								}
								else {
									std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_name(), n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).is_netmail(), nullptr);
									if (nmsg.size() > 0) {
										n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).save_message(to, subject, nmsg, netaddr, 0);
									}
								}
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "mailscan") == 0) {
					n->update_node_use("Running a Mail Scan");
					n->log->log(LOG_INFO, "%s running mailscan on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->cls();
					MsgConf::scan(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "last10") == 0) {
					n->update_node_use("Listing Last 10 Callers");
					n->log->log(LOG_INFO, "%s listing last 10 callers on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->cls();
					CallLog::last10_callers(n);
					n->pause();
				}
				else if (strcasecmp(items[i].command.c_str(), "rundoor") == 0) {
					n->update_node_use("Running a Door");
					n->log->log(LOG_INFO, "%s running door %s on node %d", n->get_user().get_username().c_str(), items[i].data.c_str(), n->getnodenum());
					std::vector<std::string> arguments;
					arguments.push_back(std::to_string(n->getnodenum()));
#ifdef _MSC_VER
					arguments.push_back(std::to_string(n->get_socket()));
#endif
					Door::createDropfiles(n);
					if (!Door::runExternal(n, items[i].data, arguments, false)) {
#ifdef _MSC_VER
						closesocket(n->get_socket());
#else
						close(n->get_socket());
#endif
						n->disconnected();
					}

					n->clog->ran_door();
				}
				else if (strcasecmp(items[i].command.c_str(), "sysinfo") == 0) {
					n->update_node_use("Looking at System Info");
					n->log->log(LOG_INFO, "%s looking at system info on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->system_info();
					n->pause();
				}
				else if (strcasecmp(items[i].command.c_str(), "settings") == 0) {
					n->update_node_use("Modifying User Settings");
					n->log->log(LOG_INFO, "%s modifying settings on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					Settings::do_settings(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "postemail") == 0) {
					n->update_node_use("Posting an EMail");
					n->log->log(LOG_INFO, "%s posting an email on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->print_f("\r\n     To: ");
					std::string to = n->get_string(30, false);

					to = User::user_exists(n->get_config(), to);

					if (to.size() == 0) {
						n->print_f("|12No such user!|07\r\n");
					}
					else {
						n->print_f("\r\n|14Sending mail to |15%s\r\n", to.c_str());
						n->print_f("\r\nSubject: ");
						std::string subject = n->get_string(60, false);

						std::vector<std::string> newemail = Editor::enter_message(n, to, subject, "E-Mail", true, nullptr);
						if (newemail.size() > 0) {
							Email::save_message(n, to, n->get_user().get_username(), subject, newemail);
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "listemail") == 0) {
					n->update_node_use("Listing EMail");
					n->log->log(LOG_INFO, "%s listing email on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					Email::list_email(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "feedback") == 0) {
					n->update_node_use("Sending Feedback");
					n->log->log(LOG_INFO, "%s sending feedback on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					std::string to = User::user_exists(n->get_config(), n->get_config()->op_name());

					if (to.size() == 0) {
						n->print_f("|12No such user!|07\r\n");
					}
					else {
						n->print_f("\r\n|14Sending mail to |15%s\r\n", to.c_str());
						std::vector<std::string> newemail = Editor::enter_message(n, to, "Feedback", "E-Mail", true, nullptr);
						if (newemail.size() > 0) {
							Email::save_message(n, to, n->get_user().get_username(), "Feedback", newemail);
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "listusers") == 0) {
					n->update_node_use("Listing Users");
					n->log->log(LOG_INFO, "%s listing users on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->cls();
					User::user_list(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "bulletins") == 0) {
					n->update_node_use("Browsing the Bulletins");
					n->log->log(LOG_INFO, "%s viewing bulletins on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->bulletins->display(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "fileconfs") == 0) {
					n->update_node_use("Listing File Conferences");
					n->log->log(LOG_INFO, "%s listing file conferences on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					int newconf = FileConf::list(n, n->get_user().get_sec_level());
					int count = 1;
					for (size_t fc = 0; fc < n->get_config()->fileconfs.size(); fc++) {
						if (n->get_config()->fileconfs.at(fc).get_sec_level() > n->get_user().get_sec_level()) continue;
						if (count == newconf) {
							n->get_user().set_attribute("cur_file_conf", std::to_string(fc));
							n->get_user().set_attribute("cur_file_area", "-1");
							for (size_t fa = 0; fa < n->get_config()->fileconfs.at(fc).areas.size(); fa++) {
								if (n->get_config()->fileconfs.at(fc).areas.at(fa).get_d_sec_level() <= n->get_user().get_sec_level()) {
									n->get_user().set_attribute("cur_file_area", std::to_string(fa));
									break;
								}
							}
							break;
						}
						count++;
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "fileareas") == 0) {
					n->update_node_use("Listing File Areas");
					n->log->log(LOG_INFO, "%s listing fileareas on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					int fileconf = stoi(n->get_user().get_attribute("cur_file_conf", "-1"));
					if (fileconf == -1) {
						n->print_f("|14Select a file conference first!|07");
					}
					else {
						int newarea = n->get_config()->fileconfs.at(fileconf).list_areas(n, n->get_user().get_sec_level());
						int count = 1;
						for (size_t fa = 0; fa < n->get_config()->fileconfs.at(fileconf).areas.size(); fa++) {
							if (n->get_config()->fileconfs.at(fileconf).areas.at(fa).get_d_sec_level() <= n->get_user().get_sec_level()) {
								if (count == newarea) {
									n->get_user().set_attribute("cur_file_area", std::to_string(fa));
									break;
								}
								count++;
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "listfiles") == 0) {
					n->update_node_use("Listing Files");
					n->log->log(LOG_INFO, "%s listing messages on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->print_f("\r\n\r\n");
					int fileconf = stoi(n->get_user().get_attribute("cur_file_conf", "-1"));
					if (fileconf == -1) {
						n->print_f("|14Select a file conference first!|07");
					}
					else {
						int filearea = stoi(n->get_user().get_attribute("cur_file_area", "-1"));
						if (filearea == -1) {
							n->print_f("|14Select a file area first!|07");
						}
						else {
							n->get_config()->fileconfs.at(fileconf).areas.at(filearea).list_files(n);
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "download") == 0) {
					if (n->tagged_files.size() > 0) {
						n->update_node_use("Downloading Files");
						n->log->log(LOG_INFO, "%s downloading files on node %d", n->get_user().get_username().c_str(), n->getnodenum());
						Protocol *p = n->get_config()->select_protocol(n);
						if (p != nullptr) {
							std::vector<std::filesystem::path> files;

							for (size_t i = 0; i < n->tagged_files.size(); i++) {
								std::filesystem::path pt(n->tagged_files.at(i).filename);
								files.push_back(pt);
							}
							p->download(n, n->get_socket(), &files);
							for (size_t i = 0; i < n->tagged_files.size(); i++) {
								n->tagged_files.at(i).fa->inc_download_count(n, n->tagged_files.at(i).filename);
							}
							n->tagged_files.clear();
							n->pause();
						}
					}
					else {
						n->print_f("|12You have no files tagged!|07\r\n");
						n->pause();
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "cleartagged") == 0) {
					n->tagged_files.clear();
					n->print_f("|14Tagged files cleared!|07\r\n");
				}
				else if (strcasecmp(items[i].command.c_str(), "upload") == 0) {
					n->update_node_use("Uploading Files");
					n->log->log(LOG_INFO, "%s uploading files on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->print_f("\r\n\r\n");
					int fileconf = stoi(n->get_user().get_attribute("cur_file_conf", "-1"));
					if (fileconf == -1) {
						n->print_f("|14Select a file conference first!|07");
					}
					else {
						int filearea = stoi(n->get_user().get_attribute("cur_file_area", "-1"));
						if (filearea == -1) {
							n->print_f("|14Select a file area first!|07");
						}
						else {
							if (n->get_config()->fileconfs.at(fileconf).areas.at(filearea).get_u_sec_level() > n->get_user().get_sec_level()) {
								n->print_f("|12You do not have permission to upload into this area!|07\r\n");
							}
							else {
								if (n->get_config()->fileconfs.at(fileconf).areas.at(filearea).upload_file(n)) {
									n->pause();
								}
								else {
									n->print_f("|12Upload failed!|07\r\n");
									n->pause();
								}
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "runscript") == 0) {
					n->update_node_use("Running a Script");
					n->log->log(LOG_INFO, "%s running script %s on node %d", n->get_user().get_username().c_str(), items[i].data.c_str(), n->getnodenum());
					std::stringstream ss;

					ss << n->get_config()->script_path() << "/" << items[i].data << ".lua";
					Script::exec(n, ss.str());
				}
				else if (strcasecmp(items[i].command.c_str(), "nlbrowse") == 0) {
					n->update_node_use("Browsing the Nodelist");
					n->log->log(LOG_INFO, "%s browsing nodelists on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					Nodelist::browse_nodelist(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "msgreadnew") == 0) {
					n->update_node_use("Reading New Messages");
					n->log->log(LOG_INFO, "%s reading all new messages on node %d", n->get_user().get_username().c_str(), n->getnodenum());

					bool subonly = false;
					n->print_f("\r\n|14Read only subscribed areas? (Y/N) : |07");
					subonly = (tolower(n->getch()) == 'y');

					bool done = false;
					for (size_t msgconf = 0; msgconf < n->get_config()->msgconfs.size(); msgconf++) {
						if (n->get_config()->msgconfs.at(msgconf).get_sec_level() <= n->get_user().get_sec_level()) {
							n->print_f("\r\n|14Searching conference |15%s|14...\r\n", n->get_config()->msgconfs.at(msgconf).get_name().c_str());
							for (size_t msgarea = 0; msgarea < n->get_config()->msgconfs.at(msgconf).areas.size(); msgarea++) {
								if (n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_r_sec_level() <= n->get_user().get_sec_level()) {
									if (!subonly || n->get_user().is_subscribed(n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_file())) {
										int last_read = n->get_user().user_get_lastread(n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_file());
										if (last_read < n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).get_total_msgs()) {
											done = !n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).read_message(last_read + 1, false, true, true);
										}
										if (done) {
											break;
										}
									}
								}
							}
							if (done) {
								break;
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "msgupdatelr") == 0) {
					n->update_node_use("Updating Message Pointers");
					n->log->log(LOG_INFO, "%s updating last read messages on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->cls();
					n->print_f("|14Update last lead pointers on...\r\n|15T|08=|14This Area|08, |15C|08=|14This Conference|08, |15A|08=|14All Conferences|08, |15ENTER|08=|14Cancel |08: |07");
					std::string res = n->get_string(1, false);
					time_t now = time(NULL);
					struct tm now_tm;
					struct tm lr_tm;
#ifdef _MSC_VER
					localtime_s(&now_tm, &now);
#else
					localtime_r(&now, &now_tm);
#endif

					if (res.size() > 0 && (tolower(res[0]) == 't' || tolower(res[0]) == 'c' || tolower(res[0]) == 'a')) {
						try {
							memset(&lr_tm, 0, sizeof(struct tm));
							n->print_f("\r\n  Year: ");
							lr_tm.tm_year = std::stoi(n->get_string(4, false, false, std::to_string(now_tm.tm_year + 1900))) - 1900;
							n->print_f("\r\n Month: ");
							lr_tm.tm_mon = std::stoi(n->get_string(2, false, false, std::to_string(now_tm.tm_mon + 1))) - 1;
							n->print_f("\r\n   Day: ");
							lr_tm.tm_mday = std::stoi(n->get_string(2, false, false, std::to_string(now_tm.tm_mday)));
							n->print_f("\r\n  Hour: ");
							lr_tm.tm_hour = std::stoi(n->get_string(2, false, false, std::to_string(now_tm.tm_hour)));
							n->print_f("\r\nMinute: ");
							lr_tm.tm_min = std::stoi(n->get_string(2, false, false, std::to_string(now_tm.tm_min)));
							lr_tm.tm_sec = 0;

							time_t lrtime = mktime(&lr_tm);

							if (tolower(res[0]) == 't') {
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
										n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).update_lr(lrtime);
									}
								}
							}
							else if (tolower(res[0]) == 'c') {
								int msgconf = stoi(n->get_user().get_attribute("cur_msg_conf", "-1"));
								if (msgconf == -1) {
									n->print_f("|14Select a message conference first!|07\r\n");
								}
								else {
									for (size_t i = 0; i < n->get_config()->msgconfs.at(msgconf).areas.size(); i++) {
										n->get_config()->msgconfs.at(msgconf).areas.at(i).update_lr(lrtime);
									}
								}
							}
							else if (tolower(res[0]) == 'a') {
								for (size_t j = 0; j < n->get_config()->msgconfs.size(); j++) {
									for (size_t i = 0; i < n->get_config()->msgconfs.at(j).areas.size(); i++) {
										n->get_config()->msgconfs.at(j).areas.at(i).update_lr(lrtime);
									}
								}
							}
						}
						catch (std::invalid_argument) {
							n->print_f("\r\n|12Invalid Argument!\r\n");
						}
						catch (std::out_of_range) {
							n->print_f("\r\n|12Out of Range!\r\n");
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "newfiles") == 0) {
					n->update_node_use("Scanning for New Files");
					for (size_t fileconf = 0; fileconf < n->get_config()->fileconfs.size(); fileconf++) {
						if (n->get_config()->fileconfs.at(fileconf).get_sec_level() > n->get_user().get_sec_level()) continue;
						n->print_f("|14Scanning conference: |15%s|14...|07\r\n", n->get_config()->fileconfs.at(fileconf).get_name().c_str());
						for (size_t filearea = 0; filearea < n->get_config()->fileconfs.at(fileconf).areas.size(); filearea++) {
							if (n->get_config()->fileconfs.at(fileconf).areas.at(filearea).get_d_sec_level() > n->get_user().get_sec_level()) continue;
							n->print_f("|14... Scanning area: |15%s|14...|07\r\n", n->get_config()->fileconfs.at(fileconf).areas.at(filearea).get_name().c_str());
							n->get_config()->fileconfs.at(fileconf).areas.at(filearea).list_files(n, n->get_last_on());
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "filesearch") == 0) {
					n->update_node_use("Performing a File Search");
					n->log->log(LOG_INFO, "%s performing a file search on node %d", n->get_user().get_username().c_str(), n->getnodenum());
					n->cls();
					n->print_f("|14Enter |15SPACE |14seperated keywords: |07");

					std::string keywordstr = n->get_string(45, false);
					std::stringstream ss(keywordstr);
					std::istream_iterator<std::string> begin(ss);
					std::istream_iterator<std::string> end;
					std::vector<std::string> keywords(begin, end);

					n->print_f("\r\n|14Search |15T|08=|14This Conference|08, |15A|08=|14All Conferences|08, |15ENTER|08=|14Cancel |08: |07");
					std::string res = n->get_string(1, false);
					if (res.size() > 0) {
						if (tolower(res[0]) == 'a') {
							for (size_t fileconf = 0; fileconf < n->get_config()->fileconfs.size(); fileconf++) {
								if (n->get_config()->fileconfs.at(fileconf).get_sec_level() > n->get_user().get_sec_level()) continue;
								n->print_f("\r\n|14Searching File Conference: |15%s|14...\r\n", n->get_config()->fileconfs.at(fileconf).get_name().c_str());
								for (size_t filearea = 0; filearea < n->get_config()->fileconfs.at(fileconf).areas.size(); filearea++) {
									if (n->get_config()->fileconfs.at(fileconf).areas.at(filearea).get_d_sec_level() > n->get_user().get_sec_level()) continue;
									n->print_f("\r\n|14Searching File Area: |15%s|14...\r\n", n->get_config()->fileconfs.at(fileconf).areas.at(filearea).get_name().c_str());
									n->get_config()->fileconfs.at(fileconf).areas.at(filearea).list_files(n, 0, &keywords);
								}
								if (fileconf < n->get_config()->fileconfs.size() - 1) {
									n->print_f("\r\n|14Continue Search (Y/N) : ");
									if (tolower(n->getch()) == 'n') break;
								}
							}
						}
						else if (tolower(res[0]) == 't') {
							int fileconf = stoi(n->get_user().get_attribute("cur_file_conf", "-1"));
							if (fileconf == -1) {
								n->print_f("\r\n|12Select a file conference first!|07\r\n");
							}
							else {
								for (size_t filearea = 0; filearea < n->get_config()->fileconfs.at(fileconf).areas.size(); filearea++) {
									if (n->get_config()->fileconfs.at(fileconf).areas.at(filearea).get_d_sec_level() > n->get_user().get_sec_level()) continue;
									n->print_f("\r\n|14Searching File Area: |15%s|14...\r\n", n->get_config()->fileconfs.at(fileconf).areas.at(filearea).get_name().c_str());
									n->get_config()->fileconfs.at(fileconf).areas.at(filearea).list_files(n, 0, &keywords);
								}
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "msgsearch") == 0) {
					n->update_node_use("Performing a Message Search");
					n->log->log(LOG_INFO, "%s performing a message search on node %d", n->get_user().get_username().c_str(), n->getnodenum());

					n->cls();

					n->print_f("|14Enter |15SPACE |14seperated keywords: |07");

					std::string keywordstr = n->get_string(45, false);

					std::stringstream ss(keywordstr);
					std::istream_iterator<std::string> begin(ss);
					std::istream_iterator<std::string> end;
					std::vector<std::string> keywords(begin, end);

					n->print_f("\r\n|14Search |15T|08=|14This Conference|08, |15A|08=|14All Conferences|08, |15ENTER|08=|14Cancel |08: |07");
					std::string res = n->get_string(1, false);

					bool allconfs = true;
					int stype = MSGSEARCH_BODY;
					bool done = false;
					if (res.size() > 0) {
						if (tolower(res[0]) == 'a') {
							allconfs = true;
						}
						else if (tolower(res[0]) == 't') {
							allconfs = false;
						}
						n->print_f("\r\n|14Search |15B|08=|14Body, |15S|08=|14Subject|08, |15U|08=|14User|08, |15ENTER|08=|14Cancel |08 : |07");
						std::string res2 = n->get_string(1, false);

						if (res2.size() > 0) {
							if (tolower(res2[0]) == 'b') {
								stype = MSGSEARCH_BODY;
							}
							else if (tolower(res2[0]) == 's') {
								stype = MSGSEARCH_SUBJ;
							}
							else if (tolower(res2[0]) == 'u') {
								stype = MSGSEARCH_USER;
							}

							if (allconfs) {
								for (size_t msgconf = 0; msgconf < n->get_config()->msgconfs.size(); msgconf++) {
									n->print_f("|14Searching conference |15%s|14...\r\n", n->get_config()->msgconfs.at(msgconf).get_name().c_str());
									for (size_t msgarea = 0; msgarea < n->get_config()->msgconfs.at(msgconf).areas.size(); msgarea++) {
										if (!n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).search(keywords, stype, false)) {
											done = true;
											break;
										}
									}
									if (done) {
										break;
									}
								}
								n->print_f("\r\n");
								n->pause();
							}
							else {
								int msgconf = stoi(n->get_user().get_attribute("cur_msg_conf", "-1"));

								if (msgconf == -1) {
									n->print_f("|14Select a message conference first!|07\r\n");
								}
								else {
									for (size_t msgarea = 0; msgarea < n->get_config()->msgconfs.at(msgconf).areas.size(); msgarea++) {
										if (!n->get_config()->msgconfs.at(msgconf).areas.at(msgarea).search(keywords, stype, false)) {
											break;
										}
									}
									n->print_f("\r\n");
									n->pause();
								}
							}
						}
					}
				}
				else if (strcasecmp(items[i].command.c_str(), "msgsubareas") == 0) {
					n->update_node_use("Managing Message Area Subscriptions");
					n->log->log(LOG_INFO, "%s managing msgbase subscriptions on node %d", n->get_user().get_username().c_str(), n->getnodenum());

					while (true) {
						int confcounter = 0;
						int lines = 3;
						std::string res = "";
						n->cls();
						n->print_f("|14Message Conferences|07\r\n\r\n");
						for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
							if (n->get_config()->msgconfs.at(i).get_sec_level() <= n->get_user().get_sec_level()) {
								n->print_f("|15%3d|08. |14%s\r\n", confcounter + 1, n->get_config()->msgconfs.at(i).get_name().c_str());
								if (lines >= n->term_height -2) {
									n->print_f("|14Continue (Y/N) : ");
									if (tolower(n->getch()) == 'n') {
										break;
									}
									lines = 0;
									n->print_f("\r\n");
								}
								lines++;
								confcounter++;
							}
						}

						n->print_f("\r\n|08[|151|08,|15%d|08] |14Select Conference|08, |15Q|08=|14Quit : ", confcounter);

						res = n->get_string(6, false);
						if (res.size() > 0) {
							if (tolower(res[0]) == 'q') {
								break;
							}
							else {
								int selconf = -1;
								try {
									selconf = stoi(res) - 1;
								}
								catch (std::invalid_argument) {
								}
								catch (std::out_of_range) {
								}

								if (selconf >= 0 && selconf < confcounter) {
									confcounter = 0;
									int actualconf = -1;
									for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
										if (n->get_config()->msgconfs.at(i).get_sec_level() <= n->get_user().get_sec_level()) {
											if (selconf == confcounter) {
												actualconf = (int)i;
												break;
											}
											confcounter++;
										}
									}
									if (actualconf >= 0 && actualconf < n->get_config()->msgconfs.size()) {
										while (true) {
											n->cls();
											n->print_f("|14Message Areas|07\r\n\r\n");
											int areacounter = 0;
											lines = 3;
											for (size_t i = 0; i < n->get_config()->msgconfs.at(actualconf).areas.size(); i++) {
												if (n->get_config()->msgconfs.at(actualconf).areas.at(i).get_r_sec_level() <= n->get_user().get_sec_level()) {
													n->print_f("|15%3d|08. |14%-48.48s |08[|14%s|08]\r\n", areacounter + 1, n->get_config()->msgconfs.at(actualconf).areas.at(i).get_name().c_str(), n->get_user().is_subscribed(n->get_config()->msgconfs.at(actualconf).areas.at(i).get_file()) ? "ON" : "OFF");
													if (lines >= n->term_height - 2) {
														n->print_f("|14Continue (Y/N) : ");
														if (tolower(n->getch()) == 'n') {
															break;
														}
														lines = 0;
														n->print_f("\r\n");
													}
													lines++;
													areacounter++;
												}
											}
											n->print_f("\r\n|08[|151|08,|15%d|08] |14Select Area|08, |15Q|08=|14Quit : ", areacounter);
											res = n->get_string(6, false);
											if (res.size() > 0) {
												if (tolower(res[0]) == 'q') {
													break;
												}
												else {
													int selarea = -1;
													try {
														selarea = stoi(res) - 1;
													}
													catch (std::invalid_argument) {
													}
													catch (std::out_of_range) {
													}
													if (selarea >= 0 && selarea < areacounter) {
														areacounter = 0;
														int actualarea = -1;
														for (size_t i = 0; i < n->get_config()->msgconfs.at(actualconf).areas.size(); i++) {
															if (n->get_config()->msgconfs.at(actualconf).areas.at(i).get_r_sec_level() <= n->get_user().get_sec_level()) {
																if (selarea == areacounter) {
																	actualarea = (int)i;
																	break;
																}
																areacounter++;
															}
														}

														if (actualarea >= 0 && actualarea < n->get_config()->msgconfs.at(actualconf).areas.size()) {
															n->get_user().set_subscribed(n->get_config()->msgconfs.at(actualconf).areas.at(actualarea).get_file(), !n->get_user().is_subscribed(n->get_config()->msgconfs.at(actualconf).areas.at(actualarea).get_file()));
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				} 

				else if (strcasecmp(items[i].command.c_str(), "qwkdown") == 0) {
					n->update_node_use("Downloading a QWK Packet");
					qwk_down(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "qwkup") == 0) {
					n->update_node_use("Uploading a QWK Packet");
					qwk_up(n);
				}
				else if (strcasecmp(items[i].command.c_str(), "nodemsg") == 0) {
					n->update_node_use("Node Messaging");
					n->cls();
					n->display_nodes();

					n->print_f("|14Message Node |08[|151|08-|15%d|08], |15ENTER|08=|14Quit |08: ", n->get_config()->max_nodes());
					std::string res = n->get_string(3, false);
					if (res.size() > 0) {
						try {
							n->print_f("\r\n");
							int nn = stoi(res);
							if (nn < 1 || nn > n->get_config()->max_nodes()) {
								n->print_f("|12Invalid Node!|07");
							}
							else {
								n->print_f("\r\nYour Message: ");
								std::string msg = n->get_string(65, false);
								if (msg.size() > 0) {
									std::filesystem::path nmsgp(n->get_config()->tmp_path());
									nmsgp.append(std::to_string(nn));
									std::filesystem::create_directories(nmsgp);
									nmsgp.append("node.msg");

									

									FILE* fptr = fopen(nmsgp.u8string().c_str(), "a");
									if (fptr) {
										fprintf(fptr, "|14Message from |15%s |14on Node %d|08:|07\r\n\r\n%s\r\n\r\n", n->get_user().get_username().c_str(), n->getnodenum(), msg.c_str());
										fclose(fptr);
										n->print_f("\r\n|10Sent!|07\r\n");
									}
									else {
										n->print_f("\r\n|12Failed!|07\r\n");
									}
								}
								else {
									n->print_f("\r\n|12Aborted!|07\r\n");
								}
							}
						}
						catch (std::invalid_argument) {
							n->print_f("\r\n|12Invalid Node!|07\r\n");
						}
						catch (std::out_of_range) {
							n->print_f("\r\n|12Invalid Node!|07\r\n");
						}
						n->pause();
					}
				}
			}
		}
	}
}

void Menu::qwk_down(Node* n) {
	static const char* chdr = "Produced by Qmail...Copyright (c) 1987 by Sparkware.  All Rights Reserved";

	int tot_areas = 0;
	int tot_msgs = 0;
	FILE* msgs_dat_fptr;
	FILE* pers_ndx_fptr;
	FILE* conf_ndx_fptr;
	char bufferfname[13];
	char buffer[128];
	std::vector<std::string> flist;
	std::vector<unsigned int> last_read_ptrs;
	std::filesystem::path fpath(n->get_config()->tmp_path());
	n->cls();
	
	fpath.append(std::to_string(n->getnodenum()));

	fpath.append("qwk");


	if (std::filesystem::exists(fpath)) {
		std::filesystem::remove_all(fpath);
	}
	std::filesystem::create_directories(fpath);

	std::filesystem::path msgs_dat(fpath);
	msgs_dat.append("MESSAGES.DAT");

	msgs_dat_fptr = fopen(msgs_dat.u8string().c_str(), "wb");
	flist.push_back(msgs_dat.u8string());

	memset(buffer, ' ', 128);
	memcpy(buffer, chdr, strlen(chdr));
	fwrite(buffer, 128, 1, msgs_dat_fptr);

	std::filesystem::path pers_ndx(fpath);
	pers_ndx.append("PERSONAL.NDX");

	pers_ndx_fptr = fopen(pers_ndx.u8string().c_str(), "wb");
	flist.push_back(pers_ndx.u8string());

	n->print_f("\r\n\r\n|14Searching |15Email|14...\r\n");
	std::filesystem::path conf_ndx(fpath);
	conf_ndx.append("0000.NDX");
	conf_ndx_fptr = fopen(conf_ndx.u8string().c_str(), "wb");
	flist.push_back(conf_ndx.u8string());
	tot_msgs = Email::qwk_scan(n, msgs_dat_fptr, pers_ndx_fptr, conf_ndx_fptr, tot_msgs);
	if (!tot_msgs) {
		n->print_f("|14... |12None\r\n");
	}
	else {
		n->print_f("|14... |10%d Messages\r\n", tot_msgs);
	}
	for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
		if (n->get_config()->msgconfs.at(i).get_sec_level() <= n->get_user().get_sec_level()) {
			n->print_f("\r\n\r\n|14Searching |15%s|14...\r\n", n->get_config()->msgconfs.at(i).get_name().c_str());
			for (size_t j = 0; j < n->get_config()->msgconfs.at(i).areas.size(); j++) {
				if (n->get_config()->msgconfs.at(i).areas.at(j).get_r_sec_level() <= n->get_user().get_sec_level() && n->get_config()->msgconfs.at(i).areas.at(j).get_qwk_id() != 0 && n->get_user().is_subscribed(n->get_config()->msgconfs.at(i).areas.at(j).get_file()) && !n->get_config()->msgconfs.at(i).areas.at(j).is_netmail()) {
					conf_ndx = fpath;
					snprintf(bufferfname, sizeof bufferfname, "%04d.NDX", n->get_config()->msgconfs.at(i).areas.at(j).get_qwk_id());
					conf_ndx.append(bufferfname);
					conf_ndx_fptr = fopen(conf_ndx.u8string().c_str(), "wb");
					flist.push_back(conf_ndx.u8string());
					unsigned int last_msg_packed = 0;
					int last_tot = tot_msgs;
					tot_msgs = n->get_config()->msgconfs.at(i).areas.at(j).qwk_scan(n, msgs_dat_fptr, pers_ndx_fptr, conf_ndx_fptr, tot_msgs, i, &last_msg_packed);
					if (last_tot == tot_msgs) {
						n->print_f("|14... |15%s |14... |12None\r\n", n->get_config()->msgconfs.at(i).areas.at(j).get_name().c_str());
					}
					else {
						n->print_f("|14... |15%s |14... |10%d Messages\r\n", n->get_config()->msgconfs.at(i).areas.at(j).get_name().c_str(), tot_msgs - last_tot);
					}
					last_read_ptrs.push_back(last_msg_packed);
					fclose(conf_ndx_fptr);
					tot_areas++;
				}
			}
		}
	}
	fclose(msgs_dat_fptr);
	fclose(pers_ndx_fptr);

	if (tot_msgs > 0) {
		std::filesystem::path door_id(fpath);
		door_id.append("DOOR.ID");
		flist.push_back(door_id.u8string());
		FILE* fptr = fopen(door_id.u8string().c_str(), "wb");
		if (!fptr) {
			// error
			return;
		}

		fprintf(fptr, "DOOR = TALISMAN\r\n");
		fprintf(fptr, "VERSION = %d.%d-%s\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);
		fprintf(fptr, "SYSTEM = Talisman BBS %d.%d-%s\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);
		fprintf(fptr, "MIXEDCASE = YES\r\n");

		fclose(fptr);

		std::filesystem::path ctrl_dat(fpath);
		ctrl_dat.append("CONTROL.DAT");
		flist.push_back(ctrl_dat.string());
		fptr = fopen(ctrl_dat.string().c_str(), "w");
		if (!fptr) {
			// error
			return;
		}

		fprintf(fptr, "%s\r\n", n->get_config()->sys_name().c_str());
		fprintf(fptr, "%s\r\n", n->get_config()->get_location().c_str());
		fprintf(fptr, "000-000-0000\r\n");
		fprintf(fptr, "%s\r\n", n->get_config()->op_name().c_str());
		fprintf(fptr, "99999,%s\r\n", n->get_config()->qwk_id().c_str());
		time_t thetime = time(NULL);
		struct tm timetm;
#ifdef _MSC_VER
		localtime_s(&timetm, &thetime);
#else
		localtime_r(&thetime, &timetm);
#endif
		fprintf(fptr, "%02d-%02d-%04d,%02d:%02d:%02d\r\n", timetm.tm_mon + 1, timetm.tm_mday, timetm.tm_year + 1900, timetm.tm_hour, timetm.tm_min, timetm.tm_sec);

		fprintf(fptr, "%s\r\n", n->get_user().get_username().c_str());

		fprintf(fptr, "\r\n");
		fprintf(fptr, "0\r\n");
		fprintf(fptr, "%d\r\n", tot_msgs);
		fprintf(fptr, "%d\r\n", tot_areas);
		fprintf(fptr, "0\r\n");
		fprintf(fptr, "Email\r\n");
		for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
			if (n->get_config()->msgconfs.at(i).get_sec_level() > n->get_user().get_sec_level()) continue;

			for (size_t j = 0; j < n->get_config()->msgconfs.at(i).areas.size(); j++) {
				if (n->get_config()->msgconfs.at(i).areas.at(j).get_r_sec_level() <= n->get_user().get_sec_level() && n->get_config()->msgconfs.at(i).areas.at(j).get_qwk_id() != 0 && n->get_user().is_subscribed(n->get_config()->msgconfs.at(i).areas.at(j).get_file()) && !n->get_config()->msgconfs.at(i).areas.at(j).is_netmail()) {
					fprintf(fptr, "%d\r\n", n->get_config()->msgconfs.at(i).areas.at(j).get_qwk_id());
					fprintf(fptr, "%s\r\n", n->get_config()->msgconfs.at(i).areas.at(j).get_name().c_str());
				}
			}
		}

		fclose(fptr);
		std::stringstream ss;

		ss.str("");

		std::filesystem::path qwk_file(fpath);
		qwk_file.append(n->get_config()->qwk_id() + ".QWK");

		int arc = stoi(n->get_user().get_attribute("archiver", "-1"));

		if (arc == -1) arc = 0;

		if (arc < 0 || arc >= n->get_config()->archivers.size()) {
			n->print_f("|12Invalid Archiver!|07\r\n\r\n");
			return;
		}

		n->get_config()->archivers.at(arc)->compress(qwk_file.u8string(), flist);

		std::vector<std::filesystem::path> sendlist;

		sendlist.push_back(qwk_file);

		Protocol* p = n->get_config()->select_protocol(n);

		if (p == nullptr) {
			return;
		}

		p->download(n, n->get_socket(), &sendlist);

		// Update pointers
		while (1) {
			n->print_f("\r\n|14Update last read pointers? (Y/N) : ");
			int h = 0;
			char c = n->getch();
			if (tolower(c) == 'y') {
				Email::set_all_seen(n);
				for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
					if (n->get_config()->msgconfs.at(i).get_sec_level() > n->get_user().get_sec_level()) continue;
					for (size_t j = 0; j < n->get_config()->msgconfs.at(i).areas.size(); j++) {
						if (n->get_config()->msgconfs.at(i).areas.at(j).get_r_sec_level() <= n->get_user().get_sec_level() && n->get_config()->msgconfs.at(i).areas.at(j).get_qwk_id() != 0 && n->get_user().is_subscribed(n->get_config()->msgconfs.at(i).areas.at(j).get_file()) && !n->get_config()->msgconfs.at(i).areas.at(j).is_netmail()) {
							if (last_read_ptrs.at(h) != 0) {
								n->get_user().user_set_lastread(n->get_config()->msgconfs.at(i).areas.at(j).get_file(), last_read_ptrs.at(h));
							}
							h++;
						}
					}
				}
				return;
			}
			else if (tolower(c) == 'n') {
				return;
			}
		}

	}
	else {
		n->print_f("|12No new messages!\r\n");
		n->pause();
	}
}

static int safe_atoi(const char* str, int len) {
	int ret = 0;

	for (int i = 0; i < len; i++) {
		if (str[i] < '0' || str[i] > '9') {
			break;
		}
		ret = ret * 10 + (str[i] - '0');
	}
	return ret;
}

void Menu::qwk_up(Node *n) {
	n->cls();
	std::filesystem::path fpath;
	fpath.append(n->get_config()->tmp_path());
	fpath.append(std::to_string(n->getnodenum()));

	fpath.append("qwk");
	if (std::filesystem::exists(fpath)) {
		std::filesystem::remove_all(fpath);
	}
	std::filesystem::create_directories(fpath);

	Protocol *p = n->get_config()->select_protocol(n);

	if (p == nullptr) {
		return;
	}

	p->upload(n, n->get_socket(), fpath.u8string());

	std::filesystem::path qwkfile(fpath);
	std::stringstream ss;

	ss.str("");
	for (int i = 0; i < strlen(n->get_config()->qwk_id().c_str()); i++) {
		ss << (char)tolower(n->get_config()->qwk_id().at(i));
	}

	qwkfile.append(ss.str() + ".rep");
	if (!std::filesystem::exists(qwkfile)) {
		qwkfile = fpath;
		qwkfile.append(n->get_config()->qwk_id() + ".REP");
		if (!std::filesystem::exists(qwkfile)) {
			n->print_f("|12Could not find %s.REP\r\n|07", n->get_config()->qwk_id().c_str());
			n->pause();
			return;
		}
	}


	ss.str("");

	int arc = stoi(n->get_user().get_attribute("archiver", "-1"));

	if (arc == -1) {
		arc = n->get_config()->select_archiver(n);
	}

	if (arc < 0 || arc >= n->get_config()->archivers.size()) {
		n->print_f("|12Invalid Archiver!|07\r\n\r\n");
		n->pause();
		return;
	}
	std::vector<std::string> flist;

	ss << n->get_config()->qwk_id() << ".MSG";

	flist.push_back(ss.str());

	ss.str("");

	for (int i = 0; i < strlen(n->get_config()->qwk_id().c_str()); i++) {
		ss << (char)tolower(n->get_config()->qwk_id().at(i));
	}
	ss << ".msg";

	flist.push_back(ss.str());

	n->get_config()->archivers.at(arc)->extract(qwkfile.u8string(), flist, fpath.u8string());

	qwkfile = fpath;
	qwkfile.append(n->get_config()->qwk_id() + ".MSG");
	if (!std::filesystem::exists(qwkfile)) {
		qwkfile = fpath;
		qwkfile.append(ss.str());
		if (!std::filesystem::exists(qwkfile)) {
			n->print_f("|14Could not find %s.MSG|07\r\n", n->get_config()->qwk_id().c_str());
			n->pause();
			return;
		}
	}

	FILE* msgsfptr = fopen(qwkfile.string().c_str(), "rb");

	if (!msgsfptr) {
		n->print_f("|12Could not open %s.MSG|07\r\n", n->get_config()->qwk_id().c_str());
		n->pause();
		return;
	}

	struct QwkHeader qhdr;

	if (fread(&qhdr, sizeof(struct QwkHeader), 1, msgsfptr) != 1) {
		n->print_f("|12Short read on %s.MSG|07\r\n", n->get_config()->qwk_id().c_str());
		fclose(msgsfptr);
		n->pause();
		return;
	}

	if (strncasecmp((char*)&qhdr, n->get_config()->qwk_id().c_str(), n->get_config()->qwk_id().size()) != 0) {
		n->print_f("|12QWK Packet not for this system..|07\r\n");
		fclose(msgsfptr);
		n->pause();
		return;
	}

	while (!feof(msgsfptr)) {
		if (fread(&qhdr, sizeof(struct QwkHeader), 1, msgsfptr) != 1) {
			fclose(msgsfptr);
			return;
		}
		int msgrecs = safe_atoi((const char*)qhdr.Msgrecs, 6);
		char* msgcontent = (char*)malloc((msgrecs - 1) * 128 + 1);
		if (!msgcontent) {
			n->print_f("|12Error allocating memory|07\r\n");
			fclose(msgsfptr);
			n->pause();
			return;
		}

		memset(msgcontent, 0, (msgrecs - 1) * 128 + 1);
		if (fread(msgcontent, sizeof(struct QwkHeader), msgrecs - 1, msgsfptr) != msgrecs - 1) {
			n->print_f("|12Short read on %s.MSG|07\r\n", n->get_config()->qwk_id().c_str());
			fclose(msgsfptr);
			n->pause();
			return;
		}

		for (int i = (msgrecs - 1) * 128; i >= 0; i--) {
			if (msgcontent[i] == ' ') {
				msgcontent[i] = '\0';
			}
			else {
				break;
			}
		}
		for (int i = 0; i < strlen(msgcontent); i++) {
			if (msgcontent[i] == '\xe3') {
				msgcontent[i] = '\r';
			}
		}
		int msgbase = (qhdr.Msgareahi << 8) | qhdr.Msgarealo;
		bool found = false;
		size_t mb;
		size_t mc;
		if (msgbase == 0) {
			found = true;
		}
		else {

			for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
				if (n->get_config()->msgconfs.at(i).get_sec_level() <= n->get_user().get_sec_level()) {
					for (size_t j = 0; j < n->get_config()->msgconfs.at(i).areas.size(); j++) {
						if (n->get_config()->msgconfs.at(i).areas.at(j).get_r_sec_level() <= n->get_user().get_sec_level() && n->get_config()->msgconfs.at(i).areas.at(j).get_qwk_id() == msgbase) {
							mb = j;
							mc = i;
							found = true;
							break;
						}
					}
					if (found == true) {
						break;
					}
				}
			}
		}
		if (found == true) {
			std::string subject;
			std::string to;
			std::string from;
			time_t date;
			int inreplyto;

			std::stringstream ss;
			std::stringstream msgbody;

			msgbody.str("");

			bool gotkludge = false;
			int i = 0;
			std::vector<std::string> text;
			while (true) {
				ss.str("");
				for (; i < strlen(msgcontent); i++) {
					if (msgcontent[i] == '\r') {
						i++;
						break;
					}
					ss << msgcontent[i];
				}

				if (strcasecmp(ss.str().substr(0, 8).c_str(), "subject:") == 0) {
					gotkludge = true;
					int j;
					for (j = 8; j < ss.str().length(); j++) {
						if (ss.str().at(j) != ' ') break;
					}
					subject = ss.str().substr(j);
				}
				else if (strcasecmp(ss.str().substr(0, 3).c_str(), "to:") == 0) {
					gotkludge = true;
					int j;
					for (j = 3; j < ss.str().length(); j++) {
						if (ss.str().at(j) != ' ') break;
					}
					to = ss.str().substr(j);
				}
				else if (strcasecmp(ss.str().substr(0, 5).c_str(), "from:") == 0) {
					gotkludge = true;
					int j;
					for (j = 5; j < ss.str().length(); j++) {
						if (ss.str().at(j) != ' ') break;
					}
					from = ss.str().substr(j);
				}
				else {
					if (gotkludge) {
						msgbody << &msgcontent[i];
					}
					else {
						msgbody << msgcontent;
					}
					break;
				}
			}

			free(msgcontent);

			ss.str("");
			for (size_t i = 0; i < msgbody.str().size(); i++) {
				if (msgbody.str().at(i) == '\r') {
					text.push_back(ss.str());
					ss.str("");
					continue;
				}
				ss << msgbody.str().at(i);
			}

			if (ss.str().size() > 0) {
				text.push_back(ss.str());
			}

			if (subject.length() == 0) {
				subject.append((const char*)qhdr.MsgSubj, 25);
				for (i = subject.length() - 1; i >= 0; i--) {
					if (subject.at(i) == ' ') {
						subject.pop_back();
					}
					else {
						break;
					}
				}
			}
			if (to.length() == 0) {
				to.append((const char*)qhdr.MsgTo, 25);
				for (i = to.length() - 1; i >= 0; i--) {
					if (to.at(i) == ' ') {
						to.pop_back();
					}
					else {
						break;
					}
				}
			}
			if (from.length() == 0) {
				from.append((const char*)qhdr.MsgFrom, 25);
				for (i = from.length() - 1; i >= 0; i--) {
					if (from.at(i) == ' ') {
						from.pop_back();
					}
					else {
						break;
					}
				}
			}
			struct tm thedate;
			memset(&thedate, 0, sizeof(struct tm));

			thedate.tm_mday = (qhdr.Msgdate[3] - '0') * 10 + (qhdr.Msgdate[4] - '0');
			thedate.tm_mon = ((qhdr.Msgdate[0] - '0') * 10 + (qhdr.Msgdate[1] - '0')) - 1;
			int year = (qhdr.Msgdate[6] - '0') * 10 + (qhdr.Msgdate[7] - '0');
			if (year < 80) {
				year += 100;
			}
			thedate.tm_year = year;

			thedate.tm_hour = (qhdr.Msgtime[0] - '0') * 10 + (qhdr.Msgtime[1] - '0');
			thedate.tm_min = (qhdr.Msgtime[3] - '0') * 10 + (qhdr.Msgtime[4] - '0');

			date = mktime(&thedate);
			inreplyto = safe_atoi((const char*)qhdr.Msgrply, 8);
			if (msgbase == 0) {
				std::string sanatized_to = User::user_exists(n->get_config(), to);
				if (sanatized_to != "") {
					Email::save_message(n, sanatized_to, n->get_user().get_username(), subject, text);
					n->print_f("|10Posted email to \"|15%s\"!\r\n", sanatized_to.c_str());
				}
				else {
					n->print_f("|14Failed to post email to \"%s\"!|07\r\n", to.c_str());
				}
			}
			else {
				if (n->get_config()->msgconfs.at(mc).areas.at(mb).get_w_sec_level() <= n->get_user().get_sec_level()) {
					if (!n->get_config()->msgconfs.at(mc).areas.at(mb).save_message(to, subject, text, "", inreplyto, date)) {
						n->print_f("|14Failed to post message in %s -> %s!\r\n\r\n", n->get_config()->msgconfs.at(mc).get_name().c_str(), n->get_config()->msgconfs.at(mc).areas.at(mb).get_name().c_str());
					}
					else {
						n->print_f("|10Posted message in |15%s |10-> |15%s|10!|07\r\n\r\n", n->get_config()->msgconfs.at(mc).get_name().c_str(), n->get_config()->msgconfs.at(mc).areas.at(mb).get_name().c_str());
						n->clog->post_msg();
					}
				}
				else {
					n->print_f("|14Failed to post message in %s -> %s!\r\n\r\n", n->get_config()->msgconfs.at(mc).get_name().c_str(), n->get_config()->msgconfs.at(mc).areas.at(mb).get_name().c_str());
				}
			}
		}
		else {
			n->print_f("|14Unknown message base |15%d|07\r\n", msgbase);
		}
	}

	fclose(msgsfptr);
	n->pause();
}