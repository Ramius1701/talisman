#include <cstring>
#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>

#include "GenDefs.h"
#include "MsgArea.h"
#include "Squish.h"
#include "Node.h"
#include "CallLog.h"
#include "Door.h"
#include "Editor.h"
#include "Qwk.h"
#include "Nodelist.h"

MsgArea::MsgArea(Node *n, std::string name, std::string filename, int r, int w, std::string oaddr, bool netmail, std::string tagline, int qwk, bool rn)
{
	this->name = name;
	this->file = filename;
	this->read_sec_level = r;
	this->write_sec_level = w;
	this->n = n;
	this->orig_addr = oaddr;
	this->_is_netmail = netmail;
	this->tagline = tagline;
	this->qwk_base_no = qwk;
	this->real_names = rn;
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

bool MsgArea::save_message(std::string to, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to)
{
	return save_message(to, subject, text, netaddr, inreply_to, 0);
}

bool MsgArea::save_message(std::string to, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to, time_t date)
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

	n->clog->post_msg();

	if (date == 0) {
		thetime = time(NULL);
	}
	else {
		thetime = date;
	}

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
		originline << "\r--- Talisman v" << VERSION_MAJOR << "." << VERSION_MINOR << "-" << VERSION_STR << " (" << n->operating_system() << ")\r * Origin: ";
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

	if (bias < 0) {
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
			newmsg.xmsg.orig.zone = orig->zone;
			newmsg.xmsg.orig.net = orig->net;
			newmsg.xmsg.orig.node = orig->node;
			newmsg.xmsg.orig.point = orig->point;
			free(orig);
		}
		else {
			newmsg.xmsg.orig.zone = 0;
			newmsg.xmsg.orig.net = 0;
			newmsg.xmsg.orig.node = 0;
			newmsg.xmsg.orig.point = 0;
		}
	}
	else {
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
	if (real_names) {
		strncpy(newmsg.xmsg.from, n->get_user().get_attribute("fullname", n->get_user().get_username()).c_str(), 36);
	}
	else {
		strncpy(newmsg.xmsg.from, n->get_user().get_username().c_str(), 36);
	}
	strncpy(newmsg.xmsg.to, to.c_str(), 36);

	newmsg.xmsg.replyto = repmsgid;
	newmsg.xmsg.attr = MSGLOCAL | MSGUID;
	
	if (netaddr != "") {
		newmsg.xmsg.attr |= MSGPRIVATE;
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

	if (netaddr != "") {
		do_semaphore(n->get_config()->netmail_sem());
	}
	else if (orig_addr != "") {
		do_semaphore(n->get_config()->echomail_sem());
	}

	return (ret == 1);
}

void MsgArea::do_semaphore(std::string sem)
{
	time_t thetime = time(NULL);

	FILE* fptr = fopen(sem.c_str(), "w");
	if (fptr) {
		fwrite(&thetime, sizeof(time_t), 1, fptr);
		fclose(fptr);
	}
}

std::vector<std::string> MsgArea::strip_ansi(const char* msg, int len) {
	std::stringstream output;
	std::vector<std::string> ansi_msg = demangle_ansi(msg, len);
	std::vector<std::string> new_msg;

	for (size_t i = 0; i < ansi_msg.size(); i++) {
		for (size_t j = 0; j < ansi_msg.at(i).size(); j++) {
			if (ansi_msg.at(i).at(j) == '\x1b') {
				while (j < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", ansi_msg.at(i).at(j)) == NULL) j++;
			}
			else if (ansi_msg.at(i).at(j) != '\n' && ansi_msg.at(i).at(j) != '\r') {
				output << ansi_msg.at(i).at(j);
			}

		}
		new_msg.push_back(output.str());
		output.str("");
	}

	return new_msg;
}

struct character_t {
	char c;
	int fg_color;
	int bg_color;
	bool bold;
};

std::vector<std::string> MsgArea::demangle_ansi(const char* msg, int len) {
	std::vector<std::string> new_msg;
	int lines = 0;
	int line_at = 0;
	int col_at = 0;
	int params[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	int param_count = 0;
	int fg_color = 7;
	int bg_color = 0;
	bool bold = false;
	int save_col = 0;
	int save_row = 0;


	for (size_t i = 0; i < len; i++) {
		if (msg[i] == '\r' || (i >= 1 && msg[i] == '\n' && msg[i - 1] != '\r')) {
			line_at++;
			if (line_at > lines) {
				lines = line_at;
			}
			col_at = 0;
		}
		else if (msg[i] == '\x1b') {
			i++;
			if (msg[i] != '[') {
				i--;
				continue;
			}
			else {
				param_count = 0;
				while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", msg[i]) == NULL) {
					if (msg[i] == ';') {
						param_count++;
					}
					else if (msg[i] >= '0' && msg[i] <= '9') {
						if (param_count == 0) {
							param_count = 1;
							for (int j = 0; j < 9; j++) {
								params[j] = 0;
							}
						}
						params[param_count - 1] = params[param_count - 1] * 10 + (msg[i] - '0');
					}
					i++;
				}
				switch (msg[i]) {
				case 'A':
					if (param_count > 0) {
						line_at -= params[0];
					}
					else {
						line_at--;
					}
					if (line_at < 0) line_at = 0;
					break;
				case 'B':
					if (param_count > 0) {
						line_at += params[0];
					}
					else {
						line_at++;
					}
					if (line_at > lines) {
						lines = line_at;
					}
					break;
				case 'C':
					if (param_count > 0) {
						col_at += params[0];
					}
					else {
						col_at++;
					}
					if (col_at > n->term_width) {
						col_at = n->term_width;
					}
					break;
				case 'D':
					if (param_count > 0) {
						col_at -= params[0];
					}
					else {
						col_at--;
					}
					if (col_at < 0) col_at = 0;
					break;
				case 'H':
				case 'f':
					if (param_count > 1) {
						params[0]--;
						params[1]--;
					}
					line_at = params[0];
					col_at = params[1];

					if (line_at > lines) {
						lines = line_at;
					}
					if (col_at > n->term_width) {
						col_at = n->term_width;
					}
					if (line_at < 0) line_at = 0;
					if (col_at < 0) col_at = 0;
					break;
				case 'u':
					col_at = save_col;
					line_at = save_row;
					break;
				case 's':
					save_col = col_at;
					save_row = line_at;
					break;
				case 'm':
					break;
				default:

					break;
				}
				
			}
		}
		else if (msg[i] != '\n') {
			col_at++;
			
			if (col_at >= n->term_width) {
				col_at = 0;
				line_at++;
				if (line_at > lines) {
					lines = line_at;
				}
			}
		}
	}

	struct character_t ** fakescreen = (struct character_t **)malloc(sizeof(struct character_t *) * (lines + 1));
	
	if (!fakescreen) {
		return new_msg;
	}


	for (int i = 0; i <= lines; i++) {
		fakescreen[i] = (struct character_t *)malloc(sizeof(struct character_t) * (n->term_width + 1));
		if (!fakescreen[i]) return new_msg;
		for (int x = 0; x <= n->term_width; x++) {
			fakescreen[i][x].c = ' ';
			fakescreen[i][x].fg_color = 7;
			fakescreen[i][x].bg_color = 0;
		}
	}
	line_at = 0;
	col_at = 0;
	save_row = 0;
	save_col = 0;
	for (size_t i = 0; i < len; i++) {
		if (msg[i] == '\r' || (i >= 1 && msg[i] == '\n' && msg[i - 1] != '\r')) {
			line_at++;
			col_at = 0;
		}
		else if (msg[i] == '\x1b') {
			i++;
			if (msg[i] != '[') {
				i--;
				continue;
			}
			else {
				param_count = 0;
				while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", msg[i]) == NULL) {
					if (msg[i] == ';') {
						param_count++;
					}
					else if (msg[i] >= '0' && msg[i] <= '9') {
						if (param_count == 0) {
							param_count = 1;
							for (int j = 0; j < 9; j++) {
								params[j] = 0;
							}
						}
						params[param_count - 1] = params[param_count - 1] * 10 + (msg[i] - '0');
					}
					i++;
				}
				switch (msg[i]) {
				case 'A':
					if (param_count > 0) {
						line_at -= params[0];
					}
					else {
						line_at--;
					}
					if (line_at < 0) line_at = 0;
					break;
				case 'B':
					if (param_count > 0) {
						line_at += params[0];
					}
					else {
						line_at++;
					}
					break;
				case 'C':
					if (param_count > 0) {
						col_at += params[0];
					}
					else {
						col_at++;
					}
					if (col_at > n->term_width) {
						col_at = n->term_width;
					}
					break;
				case 'D':
					if (param_count > 0) {
						col_at -= params[0];
					}
					else {
						col_at--;
					}
					if (col_at < 0) col_at = 0;
					break;
				case 'H':
				case 'f':
					if (param_count > 1) {
						params[0]--;
						params[1]--;
					}
					line_at = params[0];
					col_at = params[1];
					if (line_at < 0) line_at = 0;
					if (col_at < 0) col_at = 0;
					if (col_at > n->term_width) col_at = n->term_width;
					break;
				case 'm':
					for (int z = 0; z < param_count; z++) {
						if (params[z] == 0) {
							bold = false;
							fg_color = 7;
							bg_color = 0;
						}
						else if (params[z] == 1) {
							bold = true;
						}
						else if (params[z] == 2) {
							bold = false;
						}

						else if (params[z] >= 30 && params[z] <= 37) {
							fg_color = params[z] - 30;
						}
						else if (params[z] >= 40 && params[z] <= 47) {
							bg_color = params[z] - 40;
						}
					}
					break;
				case 'u':
					col_at = save_col;
					line_at = save_row;
					break;
				case 's':
					save_col = col_at;
					save_row = line_at;
					break;
				}
			}
		}
		else if (msg[i] != '\n') {
			fakescreen[line_at][col_at].c = msg[i];
			fakescreen[line_at][col_at].bold = bold;
			fakescreen[line_at][col_at].fg_color = fg_color;
			fakescreen[line_at][col_at].bg_color = bg_color;
			col_at++;
			if (col_at >= n->term_width) {
				line_at++;
				col_at = 0;
			}
		}
	}

	for (int i = 0; i < lines; i++) {
		for (int j = n->term_width - 1; j >= 0; j--) {
			if (fakescreen[i][j].c == ' ') {
				fakescreen[i][j].c = '\0';
			}
			else {
				break;
			}
		}
	}

	std::stringstream ss;

	fg_color = 7;
	bg_color = 0;
	bold = false;
	bool got_tearline = false;
	for (int i = 0; i < lines; i++) {
		ss.str("");
		int j;

		if ((fakescreen[i][0].c == '-' &&
			fakescreen[i][1].c == '-' &&
			fakescreen[i][2].c == '-') && (fakescreen[i][3].c == 0 || fakescreen[i][3].c == ' ')) {
			got_tearline = true;
		}

		if (!got_tearline) {
			if (fakescreen[i][0].c != '\001') {
				if (bold) {
					ss << "\x1b[1m";
				}
				else {
					ss << "\x1b[0m";
				}

				ss << "\x1b[" << std::to_string(fg_color + 30) << "m";
				ss << "\x1b[" << std::to_string(bg_color + 40) << "m";
			}
		}
		for (j = 0; j < n->term_width; j++) {
			if (fakescreen[i][j].c == '\0') {
				break;
			}
			if (!got_tearline) {
				if (fakescreen[i][j].bold != bold) {
					bold = fakescreen[i][j].bold;
					if (bold) {
						ss << "\x1b[1m";
					}
					else {
						ss << "\x1b[0m";
					}
				}

				if (fakescreen[i][j].fg_color != fg_color) {
					fg_color = fakescreen[i][j].fg_color;
					ss << "\x1b[" << std::to_string(fg_color + 30) << "m";
				}
				if (fakescreen[i][j].bg_color != bg_color) {
					bg_color = fakescreen[i][j].bg_color;
					ss << "\x1b[" << std::to_string(bg_color + 40) << "m";
				}
			}
			ss << fakescreen[i][j].c;
		}
		if (j < n->term_width) {
			if (!got_tearline) {
				ss << "\r\n";
			}
		}
		new_msg.push_back(ss.str());
	}

	for (int i = 0; i <= lines; i++) {
		free(fakescreen[i]);
	}
	free(fakescreen);

	return new_msg;
}
 
struct line_t {
	std::string line;
	int type;
};
void MsgArea::read_message(int start) {
	read_message(start, false, false, true);
}
bool MsgArea::read_message(int start, bool search, bool unread, bool set_last_read) {
	sq_msg_base_t* mb;
	int lr = 0; // TODO set last read
	mb = SquishOpenMsgBase(file.c_str());
	if (!mb) {
		n->print_f("|14Unable to open message base!|07\r\n");
		return false;
	}
	if (start > mb->basehdr.num_msg) {
		n->print_f("|14Empty message base!|07\r\n");
		SquishCloseMsgBase(mb);
		return false;
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
			if (search || unread) {
				return true;
			}
			return false;
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

		if (set_last_read) {
			if (n->get_user().user_get_lastread(file) < msg_to_read) {
				n->get_user().user_set_lastread(file, msg_to_read);
			}
		}

		std::stringstream ss;
		std::vector<struct line_t> linesv;
		ss.str("");
		for (int i = 0; i < msg->ctrl_len; i++) {
			if (msg->ctrl[i] == '\x01' && ss.str().size() > 0) {
				if (ss.str().size() > n->term_width -1) {
					int type = 2;
					std::vector<std::string> newvec = word_wrap("\x01" + ss.str(), n->term_width - 1);

					for (size_t z = 0; z < newvec.size(); z++) {
						struct line_t nline;
						nline.line = newvec.at(z);
						nline.type = type;
						linesv.push_back(nline);
					}
				}
				else if (ss.str().size() > 0) {
					int type = 2;
					struct line_t nline;
					nline.line = "\x01" + ss.str();
					nline.type = type;
					linesv.push_back(nline);
				}
				ss.str("");
			}
			else if (msg->ctrl[i] != '\x01') {
				ss << msg->ctrl[i];
			}
		}
		if (ss.str().size() > 0) {
			int type = 2;
			struct line_t nline;
			nline.line = "\x01" + ss.str();
			nline.type = type;
			linesv.push_back(nline);
		}
		ss.str("");

		bool ansimsg = false;

		for (int i = 0; i < msg->msg_len; i++) {
			if (msg->msg[i] == '\x1b') {
				ansimsg = true;
				break;
			}
		}

		bool got_tearline = false;

		if (ansimsg && n->hasANSI) {
			std::vector<std::string> new_msg = demangle_ansi(msg->msg, msg->msg_len);
			for (size_t i = 0; i < new_msg.size(); i++) {
				int type = 0;
				if (!got_tearline) {
					if (new_msg.at(i).find('>') < 5) {
						type = 1;
					}
					else if (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01') {
						type = 2;
					}
					else if (new_msg.at(i) == "---" || new_msg.at(i).find("--- ") == 0) {
						got_tearline = true;
						type = 3;
					}
				}
				else {
					if (new_msg.at(i).find("SEEN-BY: ") == 0 || (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01')) {
						type = 2;
					}
					else {
						type = 3;
					}
				}

				struct line_t nline;
				nline.line = new_msg.at(i);
				nline.type = type;
				linesv.push_back(nline);
			}
		}
		else {
			std::vector<std::string> new_msg;
			if (ansimsg) {
				new_msg = strip_ansi(msg->msg, msg->msg_len);
			}
			else {
				std::stringstream ss;
				for (size_t i = 0; i < msg->msg_len; i++) {
					if (msg->msg[i] == '\r') {
						new_msg.push_back(ss.str());
						ss.str("");
					}

					else {
						ss << msg->msg[i];
					}
				}
				new_msg.push_back(ss.str());
			}
			for (size_t i = 0; i < new_msg.size(); i++) {
				if (new_msg.at(i).size() > n->term_width - 1) {
					int type = 0;
					if (!got_tearline) {
						if (new_msg.at(i).find('>') < 5) {
							type = 1;
						}
						else if (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01') {
							type = 2;
						}
						else if (new_msg.at(i) == "---" || new_msg.at(i).find("--- ") == 0) {
							got_tearline = true;
							type = 3;
						}
						else {
							type = 0;
						}
					}
					else {
						if (new_msg.at(i).find("SEEN-BY: ") == 0 || (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01')) {
							type = 2;
						}
						else {
							type = 3;
						}
					}

					std::vector<std::string> newvec = word_wrap(new_msg.at(i), n->term_width - 1);

					for (size_t z = 0; z < newvec.size(); z++) {
						struct line_t nline;
						nline.line = newvec.at(z);
						nline.type = type;
						linesv.push_back(nline);
					}
				}
				else {
					int type = 0;
					if (!got_tearline) {
						if (new_msg.at(i).find('>') < 5) {
							type = 1;
						}
						else if (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01') {
							type = 2;
						}
						else if (new_msg.at(i) == "---" || new_msg.at(i).find("--- ") == 0) {
							got_tearline = true;
							type = 3;
						}
						else {
							type = 0;
						}
					}
					else {
						if (new_msg.at(i).find("SEEN-BY: ") == 0 || (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01')) {
							type = 2;
						}
						else {
							type = 3;
						}
					}
					struct line_t nline;
					nline.line = new_msg.at(i);
					nline.type = type;
					linesv.push_back(nline);
				}
			}
		}
		quotebuffer.clear();
		ss.str("");

		if (n->get_user().get_attribute("viewkludges", "false") == "true") {
			for (int i = 0; i < msg->ctrl_len; i++) {
				if (msg->ctrl[i] == '\x01' && ss.str().size() > 0) {
					if (ss.str().size() > 69) {
						std::vector<std::string> newvec = word_wrap("@" + ss.str(), 70);

						for (size_t z = 0; z < newvec.size(); z++) {
							quotebuffer.push_back(" > " + newvec.at(z));
						}
					}
					else {
						quotebuffer.push_back(" > @" + ss.str());
					}
					ss.str("");
				}
				else if (msg->ctrl[i] != '\x01') {
					ss << msg->ctrl[i];
				}
			}
			if (ss.str().size() > 0) {
				quotebuffer.push_back(" > @" + ss.str());
			}
		}
		
		std::vector<std::string> q_msg = strip_ansi(msg->msg, msg->msg_len);

		for (size_t i = 0; i < q_msg.size(); i++) {
			if (q_msg.at(i).size() > 0 && n->get_user().get_attribute("viewkludges", "false") == "false" && (q_msg.at(i).at(0) == '\x01' || q_msg.at(i).find("SEEN-BY: ") == 0)) {
				continue;
			}
			else {
				if (q_msg.at(i).size() > 0 && q_msg.at(i).at(0) == '\x01') {
					q_msg.at(i).at(0) = '@';
				}
				if (q_msg.at(i).size() > 72) {
					std::vector<std::string> newvec = word_wrap(q_msg.at(i), 70);

					for (size_t z = 0; z < newvec.size(); z++) {
						std::stringstream ss2;
						ss2 << " > " << newvec.at(z);
						quotebuffer.push_back(ss2.str());
					}
				}
				else {
					std::stringstream ss2;
					ss2 << " > " << q_msg.at(i);
					quotebuffer.push_back(ss2.str());
				}
			}
		}

		n->cls();
		n->print_f("|14   Area: |15%-46.46s\r\n", name.c_str());
		n->print_f("|14Subject: |15%-65.65s\r\n", msg->xmsg.subject);
		
		if (msg->xmsg.orig.zone == 0 && msg->xmsg.orig.net == 0 && msg->xmsg.orig.node == 0) {
			n->print_f("|14   From: |15%-41.41s\r\n", msg->xmsg.from);
			n->print_f("|14     To: |15%-36.36s\r\n", msg->xmsg.to);
		}
		else {
			n->print_f("|14   From: |15%-32.32s |14Addr: |15%d:%d/%d.%d\r\n", msg->xmsg.from, msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point);

			std::string node = Nodelist::lookup_bbsname(n, std::to_string(msg->xmsg.orig.zone) + ":" + std::to_string(msg->xmsg.orig.net) + "/" + std::to_string(msg->xmsg.orig.node));

			if (msg->xmsg.orig.point == 0) {
				n->print_f("|14     To: |15%-32.32s |14Host: |15%-30.30s\r\n", msg->xmsg.to, node.c_str());
			}
			else {
				n->print_f("|14     To: |15%-32.32s |14Host: |15A Point System\r\n", msg->xmsg.to);
			}
		}
		n->print_f("|14   Date: |15%04d-%02d-%02d %02d:%02d                 |14Msg#: |15%6d of %6d\r\n", ((msg->xmsg.date_written.date >> 9) & 127) + 1980, (msg->xmsg.date_written.date >> 5) & 15, msg->xmsg.date_written.date & 31, (msg->xmsg.date_written.time >> 11) & 31, (msg->xmsg.date_written.time >> 5) & 63, msg_to_read, total_msgs);
		n->print_f("|08------------------------------------------------------------------------------\r\n");
		lines = 6;
		for (size_t lno = 0; lno < linesv.size(); lno++) {
			if (linesv.at(lno).type == 0) {
				if (ansimsg) {
					n->print_f("%s", linesv.at(lno).line.c_str());
				}
				else {
					n->print_f("|07%s\r\n", linesv.at(lno).line.c_str());
				}
				lines++;
			}
			else if (linesv.at(lno).type == 1) {
				n->print_f("|10%s\r\n", linesv.at(lno).line.c_str());
				lines++;
			}
			else if (linesv.at(lno).type == 2) {
				if (n->get_user().get_attribute("viewkludges", "false") == "true") {
					if (linesv.at(lno).line[0] == '\x01') {
						n->print_f("|08@%s\r\n", linesv.at(lno).line.substr(1).c_str());
					}
					else {
						n->print_f("|08%s\r\n", linesv.at(lno).line.c_str());
					}
					
					lines++;
				}
			}
			else if (linesv.at(lno).type == 3) {
				n->print_f("|13%s\r\n", linesv.at(lno).line.c_str());
				lines++;
			}
			if (lines == n->term_height -2) {
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
		if (search) {
			n->print_f("|15R|08=|14Reply|08, |15A|08=|14Again|08, |15P|08=|14Prev|08, |15N|08=|14Next|08, |15C|08=|14Continue Search|08, |15Q|08=|14Quit |08: |07");
			
		}
		else if (unread) {
			n->print_f("|15R|08=|14Reply|08, |15A|08=|14Again|08, |15P|08=|14Prev|08, |15N|08=|14Next|08, |15C|08=|14Continue to Next Area|08, |15Q|08=|14Quit |08: |07");
		}
		else {
			n->print_f("|15R|08=|14Reply|08, |15A|08=|14Again|08, |15P|08=|14Prev|08, |15N|08=|14Next|08, |15Q|08=|14Quit |08: |07");
		}
		std::string res = n->get_string(1, false);
		if (res.size() == 0) {
			if (search) {
				SquishCloseMsgBase(mb);
				return true;
			}
			else {
				direction = 1;
				msg_to_read++;
			}
		}
		else {
			switch (tolower(res[0])) {
			case 'r':
				if (_is_netmail) {
					std::stringstream netaddr;
					netaddr << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;

					n->print_f("\r\n     To: ");
					std::string to = n->get_string(35, false, false, std::string(msg->xmsg.from));
					n->print_f("\r\nSubject: ");
					std::string subject = n->get_string(60, false, false, std::string(msg->xmsg.subject));
					n->print_f("\r\nAddress: ");
					std::string nnetaddr = n->get_string(16, false, false, netaddr.str());

					bool doabort = false;

					if (to.size() == 0) {
						to = "All";
					}

					NETADDR* na = parse_fido_addr(nnetaddr.c_str());
					if (!na) {
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

					if (subject.size() > 0 && !doabort) {
						std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, name, true, &quotebuffer);
						if (nmsg.size() > 0) {
							save_message(to, subject, nmsg, nnetaddr, msg->xmsg.umsgid);
						}
					}
				}
				else {
					n->print_f("\r\n     To: ");
					std::string to = n->get_string(35, false, false, std::string(msg->xmsg.from));
					n->print_f("\r\nSubject: ");
					std::string subject = n->get_string(60, false, false, std::string(msg->xmsg.subject));

					if (to.size() == 0) {
						to = "All";
					}

					if (subject.size() > 0) {
						std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, name, false, &quotebuffer);
						if (nmsg.size() > 0) {
							save_message(to, subject, nmsg, "", msg->xmsg.umsgid);
						}
					}
				}
				break;
			case 'a':
				direction = 1;
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
				return false;
			case 'c':
				if (search || unread) {
					SquishCloseMsgBase(mb);
					return true;
				}
			}
		}
	}
}

int MsgArea::list_messages(int start) {
	sq_msg_base_t* mb;
	int lr = n->get_user().user_get_lastread(file); 
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
	int lines = 1;
	n->cls();
	n->print_f("|09 Msg#    Subject                          From             To              |07\r\n");
	for (size_t i = start; i <= mb->basehdr.num_msg; i++) {
		sq_msg_t* msg = SquishReadMsg(mb, i);
		if (msg->xmsg.attr & MSGPRIVATE && strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 && strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
			SquishFreeMsg(msg);
			continue;
		}
		else {
			if (i <= lr) {
				n->print_f("|08[|15%6d|08] |14%-32.32s |13%-16.16s |11%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
			}
			else {
				n->print_f("|08[|15%6d|08]|12*|14%-32.32s |13%-16.16s |11%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
			}
		}
		lines++;
		if (lines == n->term_height - 2) {
			n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", start, mb->basehdr.num_msg);

			std::string res = n->get_string(6, false);

			if (res.size() == 0) {
				lines = 1;
				n->cls();
				n->print_f("|09 Msg#    Subject                          From             To              |07\r\n");
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

void MsgArea::update_lr(time_t date) {
	sq_msg_base_t* mb;
	bool foundmsg = false;

	mb = SquishOpenMsgBase(file.c_str());
	if (!mb) {
		return;
	}

	size_t totmsgs = mb->basehdr.num_msg;

	for (size_t i = 1; i <= totmsgs; i++) {
		sq_msg_t* msg = SquishReadMsg(mb, i);
		struct tm msg_tm;
		memset(&msg_tm, 0, sizeof(struct tm));
		msg_tm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 80;
		msg_tm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
		msg_tm.tm_mday = msg->xmsg.date_written.date & 31;
		msg_tm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
		msg_tm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
		SquishFreeMsg(msg);
		time_t msgtime = mktime(&msg_tm);

		if (msgtime > date) {
			n->get_user().user_set_lastread(file, i - 1);
			SquishCloseMsgBase(mb);
			return;
		}
	}
	n->get_user().user_set_lastread(file, totmsgs);
	SquishCloseMsgBase(mb);
	return;
}

bool MsgArea::search(std::vector<std::string> keywords, int type, bool newonly) {
	sq_msg_base_t* mb;
	bool foundmsg = false;
	mb = SquishOpenMsgBase(file.c_str());
	if (!mb) {
		return true;
	}

	size_t i;

	if (newonly) {
		i = n->get_user().user_get_lastread(file) + 1;
	}
	else {
		i = 1;
	}

	n->print_f("|14Scanning |15%s\r\n|07", name.c_str());

	for (size_t i = 1; i <= mb->basehdr.num_msg; i++) {
		foundmsg = false;
		sq_msg_t* msg = SquishReadMsg(mb, i);

		if (msg->xmsg.attr & MSGPRIVATE && strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 && strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
			SquishFreeMsg(msg);
			continue;
		}

		switch (type) {
		case MSGSEARCH_BODY:
		{
			
			char* body = (char*)malloc(msg->msg_len + 1);
			if (!body) {
				SquishFreeMsg(msg);
				SquishCloseMsgBase(mb);
				return true;
			}
			memcpy(body, msg->msg, msg->msg_len);
			body[msg->msg_len] = '0';
			std::string bodystr(body);
			for (size_t k = 0; k < keywords.size(); k++) {
				if (bodystr.find(" " + keywords.at(k) + " ") != std::string::npos) {
					foundmsg = true;
					break;
				}
			}
		}
		break;
		case MSGSEARCH_SUBJ:
		{
			for (size_t k = 0; k < keywords.size(); k++) {
				if (strncasecmp(msg->xmsg.subject, keywords.at(k).c_str(), 72) == 0) {
					foundmsg = true;
					break;
				}
			}
		}
		break;
		case MSGSEARCH_USER:
		{
			for (size_t k = 0; k < keywords.size(); k++) {
				if (strncasecmp(msg->xmsg.from, keywords.at(k).c_str(), 36) == 0 || strncasecmp(msg->xmsg.to, keywords.at(k).c_str(), 36) == 0) {
					foundmsg = true;
					break;
				}
			}
			
		}
		break;
		}
		SquishFreeMsg(msg);
		if (foundmsg) {
			if (read_message(i, true, false, true) == false) {
				SquishCloseMsgBase(mb);
				return false;
			}
		}
	}

	SquishCloseMsgBase(mb);
	return true;
}

static int ieee_to_msbin(float* src4, float* dest4) {
	unsigned char* ieee = (unsigned char*)src4;
	unsigned char* msbin = (unsigned char*)dest4;
	unsigned char sign = 0x00;
	unsigned char msbin_exp = 0x00;
	int i;
	/* See _fmsbintoieee() for details of formats   */
	sign = ieee[3] & 0x80;
	msbin_exp |= ieee[3] << 1;
	msbin_exp |= ieee[2] >> 7;
	/* An ieee exponent of 0xfe overflows in MBF    */
	if (msbin_exp == 0xfe) return 1;
	msbin_exp += 2; /* actually, -127 + 128 + 1 */
	for (i = 0; i < 4; i++) msbin[i] = 0;
	msbin[3] = msbin_exp;
	msbin[2] |= sign;
	msbin[2] |= ieee[2] & 0x7f;
	msbin[1] = ieee[1];
	msbin[0] = ieee[0];
	return 0;
}

int MsgArea::qwk_scan(Node* n, FILE* msgs_dat_fptr, FILE* pers_ndx_fptr, FILE* conf_ndx_fptr, int tot, int confno, unsigned int* last_msg_packed) {
	int lastread = n->get_user().user_get_lastread(file);
	char buffer[256];

	sq_msg_base_t* mb = SquishOpenMsgBase(file.c_str());
	if (!mb) {
		return 0;
	}

	for (size_t msgno = lastread + 1; msgno <= mb->basehdr.num_msg; msgno++) {
		sq_msg_t* msg = SquishReadMsg(mb, msgno);

		if (msg == NULL) {
			continue;
		}

		if (msg->xmsg.attr & MSGPRIVATE && strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 && strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
			SquishFreeMsg(msg);
			continue;
		}

		std::string subject(msg->xmsg.subject);
		std::string sender(msg->xmsg.from);
		std::string recipient(msg->xmsg.to);
		int hour = (msg->xmsg.date_written.time >> 11) & 31;
		int minute = (msg->xmsg.date_written.time >> 5) & 63;

		int day = msg->xmsg.date_written.date & 31;
		int month = (msg->xmsg.date_written.date >> 5) & 15;
		int year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980;

		std::stringstream msgss;
		for (size_t i = 0; i < msg->msg_len; i++) {
			if (msg->msg[i] == '\r') {
				if (i < msg->msg_len - 1) {
					if (msg->msg[i] == '\001') {
						i++;
						while (i < msg->msg_len && msg->msg[i] != '\r') {
							i++;
						}
						continue;
					}
				}
				else if (i < msg->msg_len - 9) {
					if (msg->msg[i] == 'S' && msg->msg[i + 1] == 'E' && msg->msg[i + 2] == 'E' && msg->msg[i + 3] == 'N' &&
						msg->msg[i + 4] == '-' && msg->msg[i + 5] == 'B' && msg->msg[i + 6] == 'Y' && msg->msg[i + 7] == ':' && msg->msg[i + 8] == ' ') {
						while (i < msg->msg_len && msg->msg[i] != '\r') {
							i++;
						}
						continue;
					}
				}
			}
			msgss << msg->msg[i];
		}
		unsigned int msgid = msg->xmsg.umsgid;
		SquishFreeMsg(msg);
		std::string msgbody(msgss.str());
		std::stringstream extra;
		struct QwkHeader q;

		uint32_t ndx = ftell(msgs_dat_fptr);
		float fndx;
		float mndx;
		uint8_t zero = 0;

		fndx = (float)ndx;
		ieee_to_msbin(&fndx, &mndx);

		q.Msgstat = ' ';
		snprintf(buffer, 7, "%d", msgid);

		memset(q.Msgnum, ' ', 7);
		memcpy(q.Msgnum, buffer, strlen(buffer));

		snprintf(buffer, sizeof buffer, "%02d-%02d-%02d", month, day, year - 2000);
		memcpy(q.Msgdate, buffer, 8);

		snprintf(buffer, sizeof buffer, "%02d:%02d", hour, minute);
		memcpy(q.Msgtime, buffer, 5);

		memset(q.Msgpass, ' ', 12);
		memset(q.Msgrply, ' ', 8);

		memset(q.MsgSubj, ' ', 25);

		memset(q.MsgTo, ' ', 25);

		std::stringstream mbody2;
		mbody2.str("");
		for (int i = 0; i < msgbody.length(); i++) {
			if (msgbody.at(i) != '\n') {
				mbody2 << msgbody.at(i);
			}
		}

		msgbody = mbody2.str();

		extra.str("");

		if (recipient.length() > 25) {
			extra << "To: " << recipient << "\r";
			memcpy(q.MsgTo, recipient.c_str(), 25);
		}
		else {
			memcpy(q.MsgTo, recipient.c_str(), recipient.length());
		}

		memset(q.MsgFrom, ' ', 25);
		if (sender.length() > 25) {
			extra << "From: " << sender << "\r";
			memcpy(q.MsgFrom, sender.c_str(), 25);
		}
		else {
			memcpy(q.MsgFrom, sender.c_str(), sender.length());
		}

		if (subject.length() > 25) {
			extra << "Subject: " << subject << "\r";
			memcpy(q.MsgSubj, subject.c_str(), 25);
		}
		else {
			memcpy(q.MsgSubj, subject.c_str(), subject.length());
		}

		if (extra.str().length() > 0) {
			extra << "\r" << msgbody;
			msgbody = extra.str();
		}



		int len = msgbody.length() / 128;

		if (len * 128 < msgbody.length()) {
			len++;
		}

		int lenbytes = len * 128;

		char* msgbuf = (char*)malloc(lenbytes);

		if (!lenbytes) {
			SquishCloseMsgBase(mb);
			return tot;
		}

		memset(msgbuf, ' ', lenbytes);

		for (int i = 0; i < msgbody.length(); i++) {
			if (msgbody.c_str()[i] == '\r') {
				msgbuf[i] = '\xe3';
			}
			else {
				msgbuf[i] = msgbody.c_str()[i];
			}
		}


		snprintf(buffer, 7, "%d", len + 1);
		memset(q.Msgrecs, ' ', 6);
		memcpy(q.Msgrecs, buffer, strlen(buffer));

		q.Msglive = 0xE1;
		q.Msgarealo = qwk_base_no & 0xff;
		q.Msgareahi = (qwk_base_no >> 8) & 0xff;

		q.Msgoffhi = (ndx >> 8) & 0xff;
		q.Msgofflo = ndx & 0xff;

		q.Msgtagp = ' ';

		fwrite(&mndx, 4, 1, conf_ndx_fptr);
		fwrite(&zero, 1, 1, conf_ndx_fptr);

		if (strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 && strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
			fwrite(&mndx, 4, 1, pers_ndx_fptr);
			fwrite(&zero, 1, 1, pers_ndx_fptr);
		}

		fwrite(&q, sizeof(struct QwkHeader), 1, msgs_dat_fptr);
		fwrite(msgbuf, lenbytes, 1, msgs_dat_fptr);
		*last_msg_packed = msgid;
		tot++;
	}

	SquishCloseMsgBase(mb);

	return tot;
}