#include <cstring>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#include <fstream>
#include <sstream>
#include <string>
#include "MsgArea.h"
#include "Squish.h"
#include "Node.h"

MsgArea::MsgArea(Node *n, std::string name, std::string filename, int r, int w)
{
	this->name = name;
	this->file = filename;
	this->read_sec_level = r;
	this->write_sec_level = w;
	this->n = n;
}

int MsgArea::get_total_msgs()
{
	sq_msg_base_t* mb;
	unsigned int tot = 0;
	mb = SquishOpenMsgBase(file.c_str());
	if (!mb) {
		return 0;
	}
	tot = mb->basehdr.num_msg;
	SquishCloseMsgBase(mb);
	return tot;
}

std::vector<std::string> MsgArea::word_wrap(std::string str, int len) {
	std::vector<std::string> strvec;
	size_t line_start = 0;
	size_t last_space = 0;

	for (size_t i = 0; i < str.size(); i++) {
		if (str[i] == ' ') {
			last_space = i;
		}
		if (i - line_start == len) {
			strvec.push_back(str.substr(line_start, last_space - line_start));
			line_start = last_space + 1;
			i = line_start;
		}
	}

	if (line_start < str.size()) {
		strvec.push_back(str.substr(line_start));
	}
	return strvec;
}

struct line_t {
	std::string line;
	int type;
};

void MsgArea::read_message(int start) {
	sq_msg_base_t* mb;
	int lr = 0; // TODO set last read
	mb = SquishOpenMsgBase(file.c_str());
	if (!mb) {
		n->print_f("|14Unable to open message base!|07\r\n");
		return;
	}
	if (start > mb->basehdr.num_msg) {
		n->print_f("|14Empty message base!|07\r\n");
		SquishCloseMsgBase(mb);
		return;
	}
	int total_msgs = mb->basehdr.num_msg;
	int msg_to_read = start;
	int direction = 1;
	int lines;
	while (true) {
		sq_msg_t* msg = SquishReadMsg(mb, msg_to_read);
		if (msg == NULL) {
			SquishCloseMsgBase(mb);
			return;
		}
		if (msg->xmsg.attr & MSGPRIVATE && strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 && strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
			if (direction == 1) {
				msg_to_read++;
			}
			else {
				msg_to_read--;
			}
			continue;
		}

		std::stringstream ss;
		std::vector<struct line_t> linesv;
		ss.str("");
		for (int i = 0; i < msg->msg_len; i++) {
			if (msg->msg[i] == '\r') {
				if (ss.str().size() > 79) {
					int type = 0;
					if (ss.str().find('>') < 5) {
						type = 1;
					}
					else if (ss.str().at(0) == '\x01') {
						type = 2;
					}
					
					std::vector<std::string> newvec = word_wrap(ss.str(), 79);
					
					for (size_t z = 0; z < newvec.size(); z++) {
						struct line_t nline;
						nline.line = newvec.at(z);
						nline.type = type;
						linesv.push_back(nline);
					}
				}
				else {
					int type = 0;
					if (ss.str().find('>') < 5) {
						type = 1;
					}
					else if (ss.str().size() > 0 && (ss.str().at(0) == '\x01' || ss.str().find("SEEN-BY: ") == 0)) {
						type = 2;
					}
					struct line_t nline;
					nline.line = ss.str();
					nline.type = type;
					linesv.push_back(nline);
				}
				ss.str("");
			}
			else {
				ss << msg->msg[i];
			}
		}

		n->cls();
		n->print_f("|14   Area: |15%-46.46s |14Msg#: |15%6d of %6d\r\n", name.c_str(), msg_to_read, total_msgs);
		n->print_f("|14Subject: |15%-65.65s\r\n", msg->xmsg.subject);
		n->print_f("|14   From: |15%-41.41s |14From Addr: |15%d:%d/%d.%d\r\n", msg->xmsg.from, msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point);
		n->print_f("|14     To: |15%-36.36s\r\n", msg->xmsg.to);
		n->print_f("|14   Date: |15 %04d-%02d-%02d %02d:%02d\r\n", ((msg->xmsg.date_written.date >> 9) & 127) + 1980, (msg->xmsg.date_written.date >> 5) & 15, msg->xmsg.date_written.date & 31, (msg->xmsg.date_written.time >> 11) & 31, (msg->xmsg.date_written.time >> 5) & 63);
		n->print_f("|08------------------------------------------------------------------------------\r\n");
		lines = 6;
		for (size_t lno = 0; lno < linesv.size(); lno++) {
			if (linesv.at(lno).type == 0) {
				n->print_f("|07%s\r\n", linesv.at(lno).line.c_str());
				lines++;
			}
			else if (linesv.at(lno).type == 1) {
				n->print_f("|10%s\r\n", linesv.at(lno).line.c_str());
				lines++;
			}
			else if (linesv.at(lno).type == 2) {
				if (linesv.at(lno).line[0] == '\x01') {
					n->print_f("|08@%s\r\n", linesv.at(lno).line.substr(1).c_str());
				}
				else {
					n->print_f("|08%s\r\n", linesv.at(lno).line.c_str());
				}
				lines++;
			}

			if (lines == 23) {
				n->print_f("|14Continue (Y/N) : |07");
				if (tolower(n->getche()) == 'n') {
					n->print_f("\r\n");
					break;
				}
				n->print_f("\r\n");
				lines = 0;
			}
		}
		n->print_f("\r\n");
		n->print_f("|14P=Prev, N=Next, Q=Quit : ");
		std::string res = n->get_string(1, false);
		switch (tolower(res[0])) {
		case 'n':
			direction = 1;
			msg_to_read++;
			break;
		case 'p':
			direction = 0;
			msg_to_read--;
			break;
		case 'q':
			SquishCloseMsgBase(mb);
			return;
		}
	}
}

int MsgArea::list_messages(int start) {
	sq_msg_base_t* mb;
	int lr = 0; // TODO set last read
	mb = SquishOpenMsgBase(file.c_str());
	if (!mb) {
		n->print_f("|14Unable to open message base!|07\r\n");
		return 0;
	}
	if (start > mb->basehdr.num_msg) {
		n->print_f("|14Empty message base!|07\r\n");
		SquishCloseMsgBase(mb);
		return 0;
	}
	int lines = 0;
	n->cls();
	for (size_t i = start; i <= mb->basehdr.num_msg; i++) {
		sq_msg_t* msg = SquishReadMsg(mb, i);
		if (msg->xmsg.attr & MSGPRIVATE && strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 && strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
			SquishFreeMsg(msg);
			continue;
		}
		else {
			if (i > lr) {
				n->print_f("|09[|14%6d|09] |15%-32.32s |13%-16.16s |12%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
			}
			else {
				n->print_f("|09[|14%6d|09]|12*|15%-32.32s |13%-16.16s |12%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
			}
		}
		if (lines == 23) {
			n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", start, mb->basehdr.num_msg);

			std::string res = n->get_string(6, false);

			if (res.size() == 0) {
				lines = 0;
				n->print_f("\r\n");
				continue;
			}
			else if (tolower(res[0]) == 'q') {
				SquishCloseMsgBase(mb);
				return 0;
			}
			else {
				try {
					return std::stoi(res);
				}
				catch (std::invalid_argument) {
					SquishCloseMsgBase(mb);
					return 0;
				}
			}
		}
	}

	n->print_f("|14Select |08[|15%d|08-|15%d|08], |15ENTER|08=|14Quit |07", start, mb->basehdr.num_msg);
	std::string res = n->get_string(6, false);
	
	SquishCloseMsgBase(mb);

	if (res.size() == 0) {
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