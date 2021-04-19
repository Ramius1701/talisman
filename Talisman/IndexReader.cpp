#include <vector>
#include <cstring>
#include "Nodelist.h"
#include "Editor.h"
#include "Node.h"
#include "CallLog.h"
#include "IndexReader.h"
#include "../Common/Squish.h"

#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

struct area_details_t {
	MsgArea* ma;
	int lr;
	size_t total;
	size_t unread;
	size_t unread_personal;
	bool tagged;
	bool subbed;
	time_t last_post;
};

struct conf_details_t {
	MsgConf* mc;
	std::vector<struct area_details_t> area;
};

void IndexReader::run(Node* n) {
	int selected_conf = 0;
	int selected_area = 0;

	int start_conf = 0;
	int start_area = 0;

	int x = 0;

	while (true) {
		std::vector<struct conf_details_t> conf;

		for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
			if (n->get_config()->msgconfs.at(i).get_sec_level() > n->get_user().get_sec_level()) continue;

			struct conf_details_t newconf;

			newconf.mc = &n->get_config()->msgconfs.at(i);
			for (size_t j = 0; j < newconf.mc->areas.size(); j++) {
				if (newconf.mc->areas.at(j).get_r_sec_level() > n->get_user().get_sec_level()) continue;

				struct area_details_t newarea;

				newarea.ma = &newconf.mc->areas.at(j);
				newarea.lr = n->get_user().user_get_lastread(newarea.ma->get_file());
				newarea.total = newarea.ma->get_total_msgs();
				newarea.unread = newarea.total - newarea.lr;
				newarea.unread_personal = 0;
				newarea.tagged = false;
				newarea.last_post = 0;
				newarea.subbed = n->get_user().is_subscribed(newarea.ma->get_file());
				sq_msg_base_t* mb = SquishOpenMsgBase(newarea.ma->get_file().c_str());
				if (!mb) continue;
				if (mb->basehdr.num_msg > 0) {
					sq_msg_t* msg;
					msg = SquishReadMsg(mb, mb->basehdr.num_msg);
					struct tm localtm;

					localtm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980 - 1900;
					localtm.tm_mday = msg->xmsg.date_written.date & 31;
					localtm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
					localtm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
					localtm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
					localtm.tm_sec = msg->xmsg.date_written.time & 31;

					newarea.last_post = mktime(&localtm);
					SquishFreeMsg(msg);
				}
				SquishCloseMsgBase(mb);

				newconf.area.push_back(newarea);
			}

			conf.push_back(newconf);
		}

		n->cls();
		n->print_f("\x1b[1;1H%s     Message Area                      Total Unread Last Msg\x1b[K", n->get_config()->get_prompt_colour());
		n->print_f("\x1b[%d;1H%s   Up/Down - Select, ENTER - Read, SPACE - Tag, C - Clear Tagged Q - Quit\x1b[K", n->get_term_height() - 2, n->get_config()->get_prompt_colour());
		n->print_f("\x1b[%d;1H%s  P - Post,  With Tagged: S - Subscribe, U - Unsubscribe, R - Mark As Read\x1b[K", n->get_term_height() - 1, n->get_config()->get_prompt_colour());
		n->print_f("|16");
		while (true) {
			int cur_conf = start_conf;
			int cur_area = start_area;
			for (int i = 0; i < n->get_term_height() - 4; i++) {
				n->print_f("\x1b[%d;1H", i + 2);
				if (cur_area == 0) {
					n->print_f("|14     %s\x1b[K", conf.at(cur_conf).mc->get_name().c_str());
					i++;
					if (i == n->get_term_height() - 4) break;
					n->print_f("\x1b[%d;1H", i + 2);
				}
				struct tm ltm;
#ifdef _MSC_VER
				localtime_s(&ltm, &conf.at(cur_conf).area.at(cur_area).last_post);
#else
				localtime_r(&conf.at(cur_conf).area.at(cur_area).last_post, &ltm);
#endif

				if (conf.at(cur_conf).area.at(cur_area).unread > 0) {
					n->print_f("|10NEW |15");
				}
				else {
					n->print_f("|07    ");
				}

				if (selected_conf == cur_conf && selected_area == cur_area) {
					n->print_f("%s", n->get_config()->get_prompt_colour());
					x = i;
				}
				else {
					n->print_f("|16");
				}


				if (conf.at(cur_conf).area.at(cur_area).last_post == 0) {
					n->print_f(" %-32.32s %6d %6d No Messages %c %c\x1b[K", conf.at(cur_conf).area.at(cur_area).ma->get_name().c_str(), conf.at(cur_conf).area.at(cur_area).total, conf.at(cur_conf).area.at(cur_area).unread,
						conf.at(cur_conf).area.at(cur_area).subbed ? 'S' : ' ', conf.at(cur_conf).area.at(cur_area).tagged ? 'T' : ' ');
				}
				else {
					n->print_f(" %-32.32s %6d %6d %04d/%02d/%02d  %c %c\x1b[K", conf.at(cur_conf).area.at(cur_area).ma->get_name().c_str(), conf.at(cur_conf).area.at(cur_area).total, conf.at(cur_conf).area.at(cur_area).unread,
						ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday, conf.at(cur_conf).area.at(cur_area).subbed ? 'S' : ' ', conf.at(cur_conf).area.at(cur_area).tagged ? 'T' : ' ');
				}
				n->print_f("|16");
				cur_area++;
				
				if (cur_area >= conf.at(cur_conf).area.size()) {
					cur_conf++;
					i++;
					cur_area = 0;
					if (i == n->get_term_height() - 4) break;
					n->print_f("\x1b[%d;1H\x1b[K", i + 2);
				}

				if (cur_conf >= conf.size()) {
					break;
				}
			}

			char c = n->getch();

			if (c == '\r' || c == '\n') {
				if (conf.at(selected_conf).area.at(selected_area).unread > 0) {
					
					int msgno = conf.at(selected_conf).area.at(selected_area).lr + 1;
					while (true) {
						msgno = conf.at(selected_conf).area.at(selected_area).ma->list_messages(msgno);
						if (msgno > 0 && msgno <= conf.at(selected_conf).area.at(selected_area).ma->get_total_msgs()) {
							int last;
							conf.at(selected_conf).area.at(selected_area).ma->read_message(msgno, &last);
							msgno = last;
						}
						else {
							break;
						}
					}

					break;
				}
				else {
					int msgno = 1;
					while (true) {
						msgno = conf.at(selected_conf).area.at(selected_area).ma->list_messages(msgno);
						if (msgno > 0 && msgno <= conf.at(selected_conf).area.at(selected_area).ma->get_total_msgs()) {
							int last;
							conf.at(selected_conf).area.at(selected_area).ma->read_message(msgno, &last);
							msgno = last;
						}
						else {
							break;
						}
					}
					break;
				}
			}
			else if (c == ' ') {
				conf.at(selected_conf).area.at(selected_area).tagged = !conf.at(selected_conf).area.at(selected_area).tagged;
			}
			else if (c == 'S' || c == 's') {
				for (size_t i = 0; i < conf.size(); i++) {
					for (size_t j = 0; j < conf.at(i).area.size(); j++) {
						if (conf.at(i).area.at(j).tagged) {
							n->get_user().set_subscribed(conf.at(i).area.at(j).ma->get_file(), true);
							conf.at(i).area.at(j).subbed = true;
						}
					}
				}
			}
			else if (c == 'U' || c == 'u') {
				for (size_t i = 0; i < conf.size(); i++) {
					for (size_t j = 0; j < conf.at(i).area.size(); j++) {
						if (conf.at(i).area.at(j).tagged) {
							n->get_user().set_subscribed(conf.at(i).area.at(j).ma->get_file(), false);
							conf.at(i).area.at(j).subbed = false;
						}
					}
				}
			}
			else if (c == 'R' || c == 'r') {
				for (size_t i = 0; i < conf.size(); i++) {
					for (size_t j = 0; j < conf.at(i).area.size(); j++) {
						if (conf.at(i).area.at(j).tagged) {
							n->get_user().user_set_lastread(conf.at(i).area.at(j).ma->get_file(), conf.at(i).area.at(j).total);
							conf.at(i).area.at(j).lr = conf.at(i).area.at(j).total;
							conf.at(i).area.at(j).unread = 0;
						}
					}
				}
			}
			else if (c == 'C' || c == 'c') {
				for (size_t i = 0; i < conf.size(); i++) {
					for (size_t j = 0; j < conf.at(i).area.size(); j++) {
						conf.at(i).area.at(j).tagged = false;
					}
				}
			}

			else if (c == 'q' || c == 'Q') {
				return;
			}
			else if (c == 'p' || c == 'P') {
				n->cls();
				bool doabort = false;
				n->print_f("\r\n     To: ");
				std::string to = n->get_string(35, false);
				n->print_f("\r\nSubject: ");
				std::string subject = n->get_string(60, false);
				std::string netaddr;
				if (conf.at(selected_conf).area.at(selected_area).ma->is_netmail()) {
					n->print_f("\r\nAddress: ");
					netaddr = n->get_string(16, false);
					if (conf.at(selected_conf).area.at(selected_area).ma->get_wwivnode() == 0) {
						NETADDR* na = parse_fido_addr(netaddr.c_str());

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
						try {
							int nn = stoi(netaddr);
							if (nn <= 0 || nn > 0xffff) {
								doabort = true;
							}
							else {
								n->print_f("\r\n\r\n|14 Sending to.. |15@%d", nn);
							}
						}
						catch (std::out_of_range) {
							doabort = true;
						}
						catch (std::invalid_argument) {
							doabort = true;
						}
					}
					if (to.size() == 0 || strcasecmp(to.c_str(), "ALL") == 0) {
						doabort = true;
					}
				}
				else {
					if (to.size() == 0) {
						to = "All";
					}
					netaddr = "";
				}


				if (doabort || subject.size() == 0) {
					n->print_f("\r\n|14Aborted!\r\n");
				}
				else {
					std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, conf.at(selected_conf).area.at(selected_area).ma->get_name(), conf.at(selected_conf).area.at(selected_area).ma->is_netmail(), nullptr);
					if (nmsg.size() > 0) {
						if (n->get_user().get_attribute("signature_enabled", "false") == "true") {
							MsgArea::attach_sig(&nmsg, n->get_user().get_attribute("signature", ""));
						}
						if (conf.at(selected_conf).area.at(selected_area).ma->get_real_names()) {
							conf.at(selected_conf).area.at(selected_area).ma->save_message(to, n->get_user().get_attribute("fullname", n->get_user().get_username()), subject, nmsg, netaddr, 0);
						}
						else {
							conf.at(selected_conf).area.at(selected_area).ma->save_message(to, n->get_user().get_username(), subject, nmsg, netaddr, 0);

						}
						n->clog->post_msg();
					}
				}
				break;
			}
			else if (c == '\x1b') {
				c = n->getch();
				if (c == '[') {
					c = n->getch();
					if (c == 'A') {
						// up
						if (selected_area > 0) {
							selected_area--;
							x--;
						}
						else {
							if (selected_conf > 0) {
								selected_conf--;
								selected_area = conf.at(selected_conf).area.size() - 1;
								x -= 3;
							}
						}
					}
					else if (c == 'B') {
						// down
						if (selected_area < conf.at(selected_conf).area.size() - 1) {
							selected_area++;
							x++;
						}
						else {
							if (selected_conf < conf.size() - 1) {
								selected_conf++;
								selected_area = 0;
								x += 3;
							}
						}
					}
				}
			}

			// recalculate the start
			while (x < 0) {
				// scroll up
				if (start_area > 0) {
					start_area--;
					x++;
				}
				else {
					if (start_conf > 0) {
						start_conf--;
						start_area = conf.at(start_conf).area.size() - 1;
						x += 3;
					}
				}
			}

			while (x >= n->get_term_height() - 4) {
				// scroll down
				if (start_area < conf.at(start_conf).area.size() - 1) {
					start_area++;
					x--;
				}
				else {
					if (start_conf < conf.size() - 1) {
						start_conf++;
						start_area = 0;
						x -= 3;
					}
				}
			}
		}
	}
}