#include <vector>
#include "Node.h"
#include "IndexReader.h"
#include "../Common/Squish.h"

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
					/*
					for (int k = (newarea.lr > 0 ? newarea.lr : 1); k <= mb->basehdr.num_msg; k++) {
						msg = SquishReadMsg(mb, k);
						if (msg == NULL) continue;
						if (newarea.ma->is_to_me(n, msg)) {
							newarea.unread_personal++;
						}
						SquishFreeMsg(msg);
					}
					*/
					msg = SquishReadMsg(mb, mb->basehdr.num_msg);
					struct tm localtm;

					localtm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980 - 1900;
					localtm.tm_mday = msg->xmsg.date_written.date & 31;
					localtm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
					localtm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
					localtm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
					localtm.tm_sec = msg->xmsg.date_written.time & 31;

					newarea.last_post = mktime(&localtm);
				}
				SquishCloseMsgBase(mb);

				newconf.area.push_back(newarea);
			}

			conf.push_back(newconf);
		}

		n->cls();

		n->print_f("\x1b[1;1H%s     Message Area                      Total Unread Last Msg\x1b[K", n->get_config()->get_prompt_colour());
		n->print_f("\x1b[%d;1H%s   Up/Down - Select, ENTER - Read, SPACE - Tag, C - Clear Tagged Q - Quit\x1b[K", n->get_term_height() - 2, n->get_config()->get_prompt_colour());
		n->print_f("\x1b[%d;1H%s       With Tagged: S - Subscribe, U - Unsubscribe, R - Mark As Read\x1b[K", n->get_term_height() - 1, n->get_config()->get_prompt_colour());
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