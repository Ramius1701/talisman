#include <cstring>
#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#endif
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>

#include "GenDefs.h"
#include "MsgArea.h"
#include "Squish.h"
#include "Node.h"

MsgArea::MsgArea(Node *n, std::string name, std::string filename, int r, int w, std::string oaddr, bool netmail, std::string tagline)
{
	this->name = name;
	this->file = filename;
	this->read_sec_level = r;
	this->write_sec_level = w;
	this->n = n;
	this->orig_addr = oaddr;
	this->is_netmail = netmail;
	this->tagline = tagline;
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
			if (last_space == line_start) {
				strvec.push_back(str.substr(line_start, i - line_start));
				line_start = i;
				last_space = i;
			}
			else {
				strvec.push_back(str.substr(line_start, last_space - line_start));
				line_start = last_space + 1;
				i = line_start;
				last_space = line_start;
			}
		}
	}

	if (line_start < str.size()) {
		strvec.push_back(str.substr(line_start));
	}
	return strvec;
}

void MsgArea::enter_message(std::string to, std::string subject, std::vector<std::string> *quotebuffer)
{
	std::vector<std::string> lines;
	bool done = false;
	std::string cur_line;

	n->print_f("\r\n|08---------------------------------------------------------------------");
	n->print_f("\r\n|14 Commands on a new line: /? for HELP /S to SAVE, /A to ABORT");
	n->print_f("\r\n|08---------------------------------------------------------------------");
	while (!done) {
		n->print_f("\r\n|08[|15%.4d|08]: |07", lines.size());
		cur_line = n->get_string(70, false, true);
		if (cur_line == "/S" || cur_line == "/s") {
			if (lines.size() > 0) {
				// save message
				if (!save_message(to, subject, lines, "", 0)) {
					n->print_f("\r\n|14Failed to save message!!|07\r\n");
				}
				return;
			}
			else {
				return;
			}
		}
		else if (cur_line == "/A" || cur_line == "/a") {
			return;
		}
		else if ((cur_line == "/Q" || cur_line == "/q") && quotebuffer != nullptr) {
			n->print_f("\r\n\r\n");
			int qlinec = 0;
			for (int i = 0; i < quotebuffer->size(); i++) {
				n->print_f("[%.4d]: %s\r\n", i, quotebuffer->at(i).c_str());
				qlinec++;
				if (qlinec == 23) {
					n->print_f("|14Continue (Y/N) : |07");
					if (tolower(n->getche()) == 'n') {
						break;
					}
					qlinec = 0;
				}
			}

			try {
				n->print_f("\r\n|15Quote From Line: |07");
				int qfrom = std::stoi(n->get_string(5, false));
				n->print_f("\r\n  |15Quote To Line: |07");
				int qto = std::stoi(n->get_string(5, false));
				if (!(qfrom > qto || qfrom < 0 || qto >= quotebuffer->size())) {
					for (int i = qfrom; i <= qto; i++) {
						lines.push_back(quotebuffer->at(i));
					}
				}
			}
			catch (std::out_of_range&) {
				n->print_f("\r\n|14Value out of range!|07\r\n");
			}
			catch (std::invalid_argument&) {
				n->print_f("\r\n|14Invalid Argument!|07\r\n");
			}
		}
		else if ((cur_line == "/D" || cur_line == "/d") && lines.size() > 0) {
			try {
				n->print_f("\r\n|14Delete From Line: |07");
				int dfrom = std::stoi(n->get_string(5, false));
				n->print_f("\r\n  |14Delete To Line: |07");
				int dto = std::stoi(n->get_string(5, false));
				if (!(dfrom > dto || dfrom < 0 || dto >= lines.size())) {
					for (int i = dto; i >= dfrom; i--) {
						lines.erase(lines.begin() + i);
					}
				}
			}
			catch (std::out_of_range&) {
				n->print_f("\r\n|14Value out of range!|07\r\n");
			}
			catch (std::invalid_argument&) {
				n->print_f("\r\n|14Invalid Argument!|07\r\n");
			}
		}
		else if (cur_line == "/L" || cur_line == "/l") {
			for (int i = 0; i < lines.size(); i++) {
				n->print_f("\r\n|08[|14%.4d|08]: |07%s", i, lines.at(i).c_str());
			}
			n->print_f("\r\n");
		}
		else if (cur_line == "/?") {
			n->print_f("\r\n|12>>>> |15HELP |12<<<<|07\r\n");
			n->print_f("/S Save Message\r\n");
			n->print_f("/A Abort Message\r\n");
			n->print_f("/Q Quote Message\r\n");
			n->print_f("/L List Message\r\n");
			n->print_f("/D Delete Lines\r\n\00170");
		}
		else {
			lines.push_back(cur_line);
		}
	}
}

bool MsgArea::save_message(std::string to, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to)
{
	sq_msg_base_t* mb = SquishOpenMsgBase(file.c_str());
	char replyidbuffer[256];
	char charsbuffer[] = "\001CHRS: CP437 2";
	char tzutcbuffer[256];
	char toptbuffer[256];
	char fmptbuffer[256];
	char intlbuffer[256];
	char msgidbuffer[256];
	char* msg;
	FILE* fptr;
	uint32_t msgid;
	time_t thetime;

	int at;
	struct tm lt;
	const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int ret;
	uint32_t repmsgid = 0;
	sq_msg_t* rep_msg = NULL;
	std::stringstream ss;

	thetime = time(NULL);

	for (size_t x = 0; x < text.size(); x++) {
		ss << text.at(x) << "\r";
	}

	msg = (char*)malloc(ss.str().size() + 1);
	if (!msg) {
		return false;
	}

	strcpy(msg, ss.str().c_str());

	std::stringstream originline;

	if (orig_addr != "") {
		originline << "\r--- Talisman v" << VERSION_MAJOR << "." << VERSION_MINOR << "-" << VERSION_STR << "\r * Origin: ";
		if (tagline != "") {
			originline << tagline << " (" << orig_addr << ")\r";
		}
		else {
			originline << "A Mysterious BBS (" << orig_addr << ")\r";
		}
	}

#ifdef _MSC_VER
	TIME_ZONE_INFORMATION tz;
	GetTimeZoneInformation(&tz);
	int bias = tz.Bias;

#else
	time_t gmt, rawtime = time(NULL);
	struct tm* ptm;

	struct tm gbuf;
	ptm = gmtime_r(&rawtime, &gbuf);
	// Request that mktime() looksup dst in timezone database
	ptm->tm_isdst = -1;
	gmt = mktime(ptm);

	int bias = (int)difftime(rawtime, gmt);
	bias /= 60;
#endif

	if (bias > 0) {
		snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: -%02d%02d", abs(bias / 60), abs(bias % 60));
	}
	else {
		snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: %02d%02d", abs(bias / 60), abs(bias % 60));
	}
	memset(replyidbuffer, 0, 256);

	if (inreply_to > 0) {
		rep_msg = SquishReadMsg(mb, inreply_to);
		if (rep_msg != NULL && rep_msg->xmsg.attr & MSGUID) {
			repmsgid = rep_msg->xmsg.umsgid;
			for (int i = 0; i < rep_msg->ctrl_len - 8; i++) {
				if (strncmp(&rep_msg->ctrl[i], "\x01MSGID: ", 8) == 0) {
					int h = 8;
					snprintf(replyidbuffer, sizeof replyidbuffer, "\001REPLY: ");
					for (int j = i + 8; j < rep_msg->ctrl_len && rep_msg->ctrl[j] != '\x01'; j++) {
						replyidbuffer[h++] = rep_msg->ctrl[j];
					}
					break;
				}
			}
		}
	}

	memset(msgidbuffer, 0, 256);
	if (orig_addr != "") {
		fptr = fopen(std::string(n->get_config()->data_path() + "/msgserial.dat").c_str(), "rb");

		if (!fptr) {
			msgid = (uint32_t)thetime;
		}
		else {
			fread(&msgid, sizeof(uint32_t), 1, fptr);
			fclose(fptr);

			if (thetime > msgid) {
				msgid = (uint32_t)thetime;
			}
			else {
				msgid++;
			}
		}

		fptr = fopen(std::string(n->get_config()->data_path() + "/msgserial.dat").c_str(), "wb");
		if (fptr) {
			fwrite(&msgid, sizeof(uint32_t), 1, fptr);
			fclose(fptr);
		}
		snprintf(msgidbuffer, sizeof msgidbuffer, "\x01MSGID: %s %08X", orig_addr.c_str(), msgid);
	}
	sq_msg_t newmsg;

	memset(&newmsg, 0, sizeof(newmsg));

	// are we a netmail
	newmsg.ctrl_len = strlen(msgidbuffer) + strlen(tzutcbuffer) + strlen(replyidbuffer) + strlen(charsbuffer);

	if (orig_addr != "") {
		NETADDR* orig = parse_fido_addr(orig_addr.c_str());
		if (orig != NULL) {
			fprintf(stderr, "%d:%d/%d.%d from %s\n", orig->zone, orig->net, orig->node, orig->point, orig_addr.c_str());
			newmsg.xmsg.orig.zone = orig->zone;
			newmsg.xmsg.orig.net = orig->net;
			newmsg.xmsg.orig.node = orig->node;
			newmsg.xmsg.orig.point = orig->point;
			free(orig);
		}
		else {
			fprintf(stderr, "Failed to parse \"%s\"\r\n", orig_addr.c_str());
			newmsg.xmsg.orig.zone = 0;
			newmsg.xmsg.orig.net = 0;
			newmsg.xmsg.orig.node = 0;
			newmsg.xmsg.orig.point = 0;
		}
	}
	else {
		fprintf(stderr, "Orig_addr =  \"%s\"\r\n", orig_addr.c_str());
		newmsg.xmsg.orig.zone = 0;
		newmsg.xmsg.orig.net = 0;
		newmsg.xmsg.orig.node = 0;
		newmsg.xmsg.orig.point = 0;
	}

	if (netaddr != "") {
		NETADDR* dest = parse_fido_addr(netaddr.c_str());
		if (dest != NULL) {
			newmsg.xmsg.dest.zone = dest->zone;
			newmsg.xmsg.dest.net = dest->net;
			newmsg.xmsg.dest.node = dest->node;
			newmsg.xmsg.dest.point = dest->point;
			free(dest);
			snprintf(intlbuffer, sizeof intlbuffer, "\x01INTL %d:%d/%d %d:%d/%d", newmsg.xmsg.dest.zone, newmsg.xmsg.dest.net, newmsg.xmsg.dest.node, newmsg.xmsg.orig.zone, newmsg.xmsg.orig.net, newmsg.xmsg.orig.node);
			newmsg.ctrl_len += strlen(intlbuffer);
			if (newmsg.xmsg.dest.point > 0) {
				snprintf(toptbuffer, sizeof toptbuffer, "\x01TOPT %d", newmsg.xmsg.dest.point);
				newmsg.ctrl_len += strlen(toptbuffer);
			}
			if (newmsg.xmsg.orig.point > 0) {
				snprintf(fmptbuffer, sizeof fmptbuffer, "\001FMPT %d", newmsg.xmsg.orig.point);
				newmsg.ctrl_len += strlen(fmptbuffer);
			}
		}
		else {
			newmsg.xmsg.dest.zone = 0;
			newmsg.xmsg.dest.net = 0;
			newmsg.xmsg.dest.node = 0;
			newmsg.xmsg.dest.point = 0;
		}
	}

	newmsg.ctrl = (char*)malloc(newmsg.ctrl_len);
	if (!newmsg.ctrl) {
		free(msg);
		return false;
	}
	at = 0;
	memcpy(newmsg.ctrl, tzutcbuffer, strlen(tzutcbuffer));
	at += strlen(tzutcbuffer);
	memcpy(&newmsg.ctrl[at], charsbuffer, strlen(charsbuffer));
	at += strlen(charsbuffer);
	if (orig_addr != "") {
		memcpy(&newmsg.ctrl[at], msgidbuffer, strlen(msgidbuffer));
		at += strlen(msgidbuffer);
	}
	if (inreply_to > 0) {
		memcpy(&newmsg.ctrl[at], replyidbuffer, strlen(replyidbuffer));
		at += strlen(replyidbuffer);
	}
	if (newmsg.xmsg.dest.zone != 0) {
		if (newmsg.xmsg.dest.point != 0) {
			memcpy(&newmsg.ctrl[at], toptbuffer, strlen(toptbuffer));
			at += strlen(toptbuffer);
		}
		if (newmsg.xmsg.orig.point != 0) {
			memcpy(&newmsg.ctrl[at], fmptbuffer, strlen(fmptbuffer));
			at += strlen(fmptbuffer);
		}
		memcpy(&newmsg.ctrl[at], intlbuffer, strlen(intlbuffer));
		at += strlen(intlbuffer);
	}
	if (orig_addr != "") {
		newmsg.msg_len = strlen(msg) + originline.str().size();
		newmsg.msg = (char*)malloc(strlen(msg) + originline.str().size());
		if (!newmsg.msg) {
			free(newmsg.ctrl);
			free(msg);
			return false;
		}
		memcpy(newmsg.msg, msg, strlen(msg));
		memcpy(&newmsg.msg[strlen(msg)], originline.str().c_str(), originline.str().size());
	}
	else {
		newmsg.msg_len = strlen(msg);
		newmsg.msg = (char*)malloc(strlen(msg));
		if (!newmsg.msg) {
			free(newmsg.ctrl);
			free(msg);
			return false;
		}
		memcpy(newmsg.msg, msg, strlen(msg));
	}

	strncpy(newmsg.xmsg.subject, subject.c_str(), 72);
	strncpy(newmsg.xmsg.from, n->get_user().get_username().c_str(), 36);
	strncpy(newmsg.xmsg.to, to.c_str(), 36);

	newmsg.xmsg.replyto = repmsgid;
	newmsg.xmsg.attr = MSGLOCAL | MSGUID;
	
	if (netaddr != "") {
		newmsg.xmsg.attr |= MSGPRIVATE;
	}
	if (newmsg.xmsg.dest.zone != 0) {
		newmsg.xmsg.attr |= MSGCRASH;
	}
#if _MSC_VER
	localtime_s(&lt, &thetime);
#else
	localtime_r(&thetime, &lt);
#endif
	newmsg.xmsg.date_written.date |= (((sq_word)lt.tm_mday) & 31);
	newmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_mon + 1)) & 15) << 5;
	newmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_year - 80)) & 127) << 9;

	newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_sec) & 31);
	newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_min) & 63) << 5;
	newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_hour) & 31) << 11;

	newmsg.xmsg.date_arrived.date = newmsg.xmsg.date_written.date;
	newmsg.xmsg.date_arrived.time = newmsg.xmsg.date_written.time;



	snprintf(newmsg.xmsg.__ftsc_date, 20, "%02d %s %02d  %02d:%02d:%02d", lt.tm_mday, months[lt.tm_mon], lt.tm_year - 100, lt.tm_hour, lt.tm_min, lt.tm_sec);

	SquishLockMsgBase(mb);
	ret = SquishWriteMsg(mb, &newmsg);
	if (rep_msg != NULL) {
		for (int i = 0; i < 9; i++) {
			if (rep_msg->xmsg.replies[i] == 0) {
				rep_msg->xmsg.replies[i] = newmsg.xmsg.umsgid;
				SquishUpdateHdr(mb, rep_msg);
				break;
			}
		}
		SquishFreeMsg(rep_msg);
	}
	SquishUnlockMsgBase(mb);
	SquishCloseMsgBase(mb);

	free(msg);
	free(newmsg.msg);
	free(newmsg.ctrl);

	return (ret == 1);
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

	std::vector<std::string> quotebuffer;

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
		quotebuffer.clear();
		for (int i = 0; i < msg->msg_len; i++) {
			if (msg->msg[i] == '\r') {
				if (ss.str().size() > 75) {
					std::vector<std::string> newvec = word_wrap(ss.str(), 75);

					for (size_t z = 0; z < newvec.size(); z++) {
						std::stringstream ss2;
						ss2 << " > " << newvec.at(z);
						quotebuffer.push_back(ss2.str());
					}
				}
				else {
					std::stringstream ss2;
					ss2 << " > " << ss.str();
					quotebuffer.push_back(ss2.str());
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
		n->print_f("|14   Date: |15%04d-%02d-%02d %02d:%02d\r\n", ((msg->xmsg.date_written.date >> 9) & 127) + 1980, (msg->xmsg.date_written.date >> 5) & 15, msg->xmsg.date_written.date & 31, (msg->xmsg.date_written.time >> 11) & 31, (msg->xmsg.date_written.time >> 5) & 63);
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
		n->print_f("|15R|08=|14Reply|08, |15P|08=|14Prev|08, |15N|08=|14Next|08, |15Q|08=|14Quit |08: |07");
		std::string res = n->get_string(1, false);
		if (res.size() == 0) {
			direction = 1;
			msg_to_read++;
		}
		else {
			switch (tolower(res[0])) {
			case 'r':
				enter_message(std::string(msg->xmsg.from), std::string(msg->xmsg.subject), &quotebuffer);
				break;
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
	n->print_f("|09 Msg#    Subject                          From             To              |07\r\n");
	for (size_t i = start; i <= mb->basehdr.num_msg; i++) {
		sq_msg_t* msg = SquishReadMsg(mb, i);
		if (msg->xmsg.attr & MSGPRIVATE && strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 && strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
			SquishFreeMsg(msg);
			continue;
		}
		else {
			if (i > lr) {
				n->print_f("|08[|15%6d|08] |14%-32.32s |13%-16.16s |11%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
			}
			else {
				n->print_f("|08[|15%6d|08]|12*|14%-32.32s |13%-16.16s |11%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
			}
		}
		if (lines == 23) {
			n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", start, mb->basehdr.num_msg);

			std::string res = n->get_string(6, false);

			if (res.size() == 0) {
				lines = 1;
				n->cls();
				n->print_f("|09[|14Msg#  |09] |14Subject                          |14From             |14To              |07\r\n");
				continue;
			}
			else if (tolower(res[0]) == 'q') {
				SquishCloseMsgBase(mb);
				return 0;
			}
			else {
				try {
					SquishCloseMsgBase(mb);
					return std::stoi(res);
				}
				catch (std::invalid_argument) {
					SquishCloseMsgBase(mb);
					return 0;
				}
				catch (std::out_of_range) {
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
		catch (std::out_of_range) {
			return 0;
		}
	}
}