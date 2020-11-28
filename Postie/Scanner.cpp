#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif
#include <filesystem>
#include <sstream>
#include "Scanner.h"
#include "Logger.h"
#include "INIReader.h"
#include "Config.h"
#include "PacketStructs.h"
#include "GenDefs.h"
#include "Archiver.h"

struct seenby_t {
	uint16_t net;
	uint16_t node;
};



std::string add_cr_to_kludges(sq_msg_t* msg) {
	std::stringstream ss;

	for (size_t i = 0; i < msg->ctrl_len; i++) {
		if (i > 0 && msg->ctrl[i] == '\001') {
			ss << "\r";
		}
		ss << msg->ctrl[i];
	}
	ss << "\r";

	return ss.str();
}

struct {
	bool operator()(struct seenby_t a, struct seenby_t b) const
	{
		if (a.net < b.net) {
			return true;
		}
		else {
			if (a.net > b.net) {
				return false;
			}
			else if (a.node < b.node) {
				return true;
			}
			else if (a.node > b.node) {
				return false;
			}
		}
		return true;
	}
} compare;

void sort_seenby(std::vector<struct seenby_t> *seenbys)
{
	if (seenbys->size() > 1) {
		std::sort(seenbys->begin(), seenbys->end(), compare);
	}
}

std::string remove_tid(std::string ctrlbuf) {
	int pos = ctrlbuf.find("\001TID: ");
	if (pos == std::string::npos) {
		return ctrlbuf;
	}
	
	int endoftid = ctrlbuf.substr(pos).find("\r");
	if (endoftid != std::string::npos) {
		return ctrlbuf.substr(0, pos) + ctrlbuf.substr(endoftid + 1);
		
	}
	else {
		return ctrlbuf.substr(0, pos);
	}
}

bool check_seenby(std::vector<struct seenby_t>* seenbys, NETADDR* node) {
	for (size_t i = 0; i < seenbys->size(); i++) {
		if (seenbys->at(i).net == node->net && seenbys->at(i).node == node->node) {
			return true;
		}
	}

	return false;
}

void add_seenby(std::vector<struct seenby_t>* seenbys, NETADDR* node) {
	if (!check_seenby(seenbys, node)) {
		struct seenby_t newseenby;

		newseenby.net = node->net;
		newseenby.node = node->node;

		seenbys->push_back(newseenby);
	}
}

std::string remove_seenby_path(std::string msgbuf) {
	std::stringstream ss(msgbuf);
	std::stringstream ss2;
	std::string buff;
	bool gotorigin = false;
	while (getline(ss, buff, '\r')) {
		if (buff.substr(0, 11) == " * Origin: ") {
			gotorigin = true;
			ss2 << buff << '\r';
			continue;
		}
		if (gotorigin) {
			if (buff.find("SEEN-BY: ") == 0) {
				continue;
			}
			if (buff.find("\001PATH: ") == 0) {
				continue;
			}
		}
		ss2 << buff << '\r';
	}

	return ss2.str();
}

std::vector<struct seenby_t> parse_path(std::string msgbuf) {
	std::vector<struct seenby_t> seenbys;

	std::stringstream ss(msgbuf);
	std::string buff;
	bool gotorigin = false;
	while (getline(ss, buff, '\r')) {
		if (buff.substr(0, 11) == " * Origin: ") {
			gotorigin = true;
			continue;
		}
		if (gotorigin) {
			if (buff.find("\001PATH: ") == 0) {
				std::stringstream ss2(buff);
				std::string sb;
				uint16_t last_net = 0;
				while (getline(ss2, sb, ' ')) {
					if (sb.find(":")) {
						std::string sb2 = sb.substr(sb.find(":") + 1);
						sb = sb2;
					}
					if (sb.find("/") != std::string::npos) {
						try {
							last_net = stoi(sb.substr(0, sb.find("/")));
							struct seenby_t nsb;
							nsb.net = last_net;
							nsb.node = stoi(sb.substr(sb.find("/") + 1));
							seenbys.push_back(nsb);
						}
						catch (std::invalid_argument) {

						}
						catch (std::out_of_range) {

						}
					}
					else {
						struct seenby_t nsb;
						nsb.net = last_net;
						try {
							nsb.node = stoi(sb);
							seenbys.push_back(nsb);
						}
						catch (std::invalid_argument) {

						}
						catch (std::out_of_range) {

						}

					}
				}
			}
		}
	}

	return seenbys;
}

std::vector<struct seenby_t> parse_seenbys(std::string msgbuf) {
	std::vector<struct seenby_t> seenbys;

	std::stringstream ss(msgbuf);
	std::string buff;
	bool gotorigin = false;
	while (getline(ss, buff, '\r')) {
		if (buff.substr(0, 11) == " * Origin: ") {
			gotorigin = true;
			continue;
		}
		if (gotorigin) {
			if (buff.find("SEEN-BY: ") == 0) {
				std::stringstream ss2(buff);
				std::string sb;
				uint16_t last_net = 0;
				while (getline(ss2, sb, ' ')) {
					if (sb.find(":")) {
						sb = sb.substr(sb.find(":") + 1);
					}
					if (sb.find("/") != std::string::npos) {
						try {
							last_net = stoi(sb.substr(0, sb.find("/")));
							struct seenby_t nsb;
							nsb.net = last_net;
							nsb.node = stoi(sb.substr(sb.find("/") + 1));
							seenbys.push_back(nsb);
						}
						catch (std::invalid_argument) {

						}
						catch (std::out_of_range) {

						}
					}
					else {
						struct seenby_t nsb;
						nsb.net = last_net;
						try {
							nsb.node = stoi(sb);
							seenbys.push_back(nsb);
						}
						catch (std::invalid_argument) {

						}
						catch (std::out_of_range) {

						}
					}
				}
			}
		}
	}

	return seenbys;
}

bool Scanner::matchroute(std::string route, NETADDR* aka) {
	int stage = 0;
	bool match = true;
	int number = 0;
	bool wildcard = false;

	for (size_t i = 0; i < route.size(); i++) {
		if (route[i] == ':') {
			if (wildcard == false) {
				if (number != aka->zone) {
					match = false;
					break;
				}
				number = 0;
				wildcard = false;
			}
			stage = 1;
			continue;
		} 
		if (route[i] == '/') {
			if (wildcard == false) {
				if (number != aka->net) {
					match = false;
					break;
				}
				number = 0;
				wildcard = false;
			}
			stage = 2;
			continue;
		}
		if (route[i] == '.') {
			if (wildcard == false) {
				if (number != aka->node) {
					match = false;
					break;
				}
				number = 0;
				wildcard = false;
			}
			stage = 3;
			continue;
		}

		if (route[i] != '*') {
			number = number * 10 + (route[i] - '0');
		}
		else {
			wildcard = true;
		}
	}

	if (stage == 3) {
		if (wildcard == false) {
			if (number != aka->point) {
				match = false;
			}
		}
	}

	return match;
}

void Scanner::initialize_packet(struct link_conf_t *link, std::string working_path, NETADDR *pktorig) {
	time_t thetime = time(NULL);
	struct tm thetimetm;
	struct packet_t phdr;
#ifdef _MSC_VER
	localtime_s(&thetimetm, &thetime);
#else
	localtime_r(&thetime, &thetimetm);
#endif
	char packetname[9];

	sprintf(packetname, "%02d%02d%02d%02d", thetimetm.tm_mday, thetimetm.tm_hour, thetimetm.tm_min, thetimetm.tm_sec);

	link->packetpath = working_path;
	link->packetpath.append(std::to_string(link->aka->zone) + "." + std::to_string(link->aka->net) + "." + std::to_string(link->aka->node) + "." + std::to_string(link->aka->point));
	if (std::filesystem::exists(link->packetpath)) {
		std::filesystem::remove_all(link->packetpath);
	}
	std::filesystem::create_directories(link->packetpath);

	link->packetpath.append(std::string(packetname) + ".pkt");
	link->fptr = fopen(link->packetpath.u8string().c_str(), "wb");
	memset(&phdr, 0, sizeof(struct packet_t));

	phdr.orignode = pktorig->node;
	phdr.destnode = link->aka->node;
	phdr.year = thetimetm.tm_year + 1900;
	phdr.month = thetimetm.tm_mon;
	phdr.day = thetimetm.tm_mday;
	phdr.hour = thetimetm.tm_hour;
	phdr.minute = thetimetm.tm_min;
	phdr.second = thetimetm.tm_sec;
	phdr.baud = 0;
	phdr.version = 2;
	if (pktorig->point != 0) {
		phdr.origNet = 0xffff;
		phdr.auxNet = pktorig->net;
	}
	else {
		phdr.origNet = pktorig->net;
		phdr.auxNet = 0;
	}
	phdr.destNet = link->aka->net;
	phdr.prodCode = 0xfe;
	phdr.prodVersionMajor = 1;

	for (int j = 0; j < 8 && j < link->packetpwd.size(); j++) {
		phdr.password[j] = link->packetpwd.at(j);
	}
	phdr.origZone = pktorig->zone;
	phdr.destZone = link->aka->zone;
	phdr.capWord = 0x0001;
	phdr.capValid = ((phdr.capWord & 0xff) << 8) | ((phdr.capWord >> 8) & 0xff);
	phdr.origZone2 = pktorig->zone;
	phdr.destZone2 = link->aka->zone;
	phdr.origPoint = pktorig->point;
	phdr.destPoint = link->aka->point;
	phdr.prodData = 0x45545350;

	fwrite(&phdr, sizeof(struct packet_t), 1, link->fptr);

}

void Scanner::write_netmail_to_pkt(struct link_conf_t* link, sq_msg_t* msg, bool local) {
	static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	std::stringstream ss;
	for (size_t m = 0; m < msg->msg_len; m++) {
		ss << msg->msg[m];
	}
	std::string msgbody = ss.str();

	// write message
	struct packed_message_t pmhdr;
	memset(&pmhdr, 0, sizeof(struct packed_message_t));
	pmhdr.orig_net = link->ouraka->net;
	pmhdr.orig_node = link->ouraka->node;

	pmhdr.dest_net = link->aka->net;
	pmhdr.dest_node = link->aka->node;

	pmhdr.cost = 0;

	if (local) {
		pmhdr.attribute = PKT_MSGLOCAL | PKT_MSGPRIVATE;
	}
	else {
		pmhdr.attribute = PKT_MSGPRIVATE;
	}
	if (strcasecmp(link->flavour.c_str(), "crash") == 0) {
		pmhdr.attribute |= PKT_MSGCRASH;
	}

	pmhdr.message_type = 2;

	fwrite(&pmhdr, sizeof(struct packed_message_t), 1, link->fptr);

	char buffer[256];

	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.2d %s %.2d  %02d:%02d:%02d", msg->xmsg.date_written.date & 31, months[((msg->xmsg.date_written.date >> 5) & 15) - 1], ((msg->xmsg.date_written.date >> 9) & 127) + 1980 - 2000, (msg->xmsg.date_written.time >> 11) & 31, (msg->xmsg.date_written.time >> 5) & 63, msg->xmsg.date_written.time & 31);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	// tousername 36 bytes
	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.35s", msg->xmsg.to);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	// fromusername 36 bytes
	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.35s", msg->xmsg.from);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	// subject 72 bytes
	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.71s", msg->xmsg.subject);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	std::string kludges = remove_tid(add_cr_to_kludges(msg));
	fwrite(kludges.c_str(), kludges.size(), 1, link->fptr);
	fprintf(link->fptr, "\001TID: Postie/%d.%d\r", VERSION_MAJOR, VERSION_MINOR);

	fwrite(msgbody.c_str(), msgbody.size(), 1, link->fptr);

	// add via line
	time_t now = time(NULL);
	struct tm thetime;
#ifdef _MSC_VER
	gmtime_s(&thetime, &now);
#else
	gmtime_r(&now, &thetime);
#endif
	snprintf(buffer, sizeof buffer, "\001Via %d:%d/%d.%d @%04d%02d%02d.%02d%02d%02d.UTC Postie/%d.%d\r", link->ouraka->zone, link->ouraka->net, link->ouraka->node, link->ouraka->point, thetime.tm_year + 1900, thetime.tm_mon + 1, thetime.tm_mday, thetime.tm_hour, thetime.tm_min, thetime.tm_sec, VERSION_MAJOR, VERSION_MINOR);

	fwrite(buffer, strlen(buffer), 1, link->fptr);

	char null = '\0';

	fwrite(&null, 1, 1, link->fptr);
}

void Scanner::write_msg_to_pkt(struct area_conf_t* area, struct link_conf_t* link, sq_msg_t* msg, bool local) {
	static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	std::stringstream ss;
	for (size_t m = 0; m < msg->msg_len; m++) {
		ss << msg->msg[m];
	}

	std::vector<struct seenby_t> seenbys = parse_seenbys(ss.str());
	std::vector<struct seenby_t> path = parse_path(ss.str());
	if (check_seenby(&seenbys, link->aka)) {
		return;
	}

	// trim off seenbys & path
	std::string msgbody = remove_seenby_path(ss.str());
	// write message
	struct packed_message_t pmhdr;
	memset(&pmhdr, 0, sizeof(struct packed_message_t));
	pmhdr.orig_net = link->ouraka->net;
	pmhdr.orig_node = link->ouraka->node;

	pmhdr.dest_net = link->aka->net;
	pmhdr.dest_node = link->aka->node;

	pmhdr.cost = 0;
	if (local) {
		pmhdr.attribute = PKT_MSGLOCAL;
	}
	else {
		pmhdr.attribute = 0;
	}

	if (strcasecmp(link->flavour.c_str(), "crash") == 0) {
		pmhdr.attribute |= PKT_MSGCRASH;
	}

	pmhdr.message_type = 2;

	fwrite(&pmhdr, sizeof(struct packed_message_t), 1, link->fptr);

	char buffer[73];

	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.2d %s %.2d  %02d:%02d:%02d", msg->xmsg.date_written.date & 31, months[((msg->xmsg.date_written.date >> 5) & 15) - 1], ((msg->xmsg.date_written.date >> 9) & 127) + 1980 - 2000, (msg->xmsg.date_written.time >> 11) & 31, (msg->xmsg.date_written.time >> 5) & 63, msg->xmsg.date_written.time & 31);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	// tousername 36 bytes
	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.35s", msg->xmsg.to);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	// fromusername 36 bytes
	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.35s", msg->xmsg.from);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	// subject 72 bytes
	memset(buffer, 0, sizeof buffer);
	snprintf(buffer, sizeof buffer, "%.71s", msg->xmsg.subject);
	fwrite(buffer, strlen(buffer) + 1, 1, link->fptr);

	fprintf(link->fptr, "AREA:%s\r", area->areatag.c_str());
	std::string kludges = remove_tid(add_cr_to_kludges(msg));
	fwrite(kludges.c_str(), kludges.size(), 1, link->fptr);
	fprintf(link->fptr, "\001TID: Postie/%d.%d\r", VERSION_MAJOR, VERSION_MINOR);



	fwrite(msgbody.c_str(), msgbody.size(), 1, link->fptr);

	// add seenby
	if (area->aka->point != 0) {
		add_seenby(&seenbys, area->aka);
	}

	for (size_t lid2 = 0; lid2 < area->links.size(); lid2++) {
		if (area->links.at(lid2)->aka->point != 0) {
			add_seenby(&seenbys, area->links.at(lid2)->aka);
		}
	}
	// - sort seenby
	sort_seenby(&seenbys);
	// append seenby to file
	std::stringstream sbline;
	bool first = true;
	for (size_t sb = 0; sb < seenbys.size(); ) {
		if (first == true) {
			sbline.str("");
			sbline << "SEEN-BY: " << seenbys.at(sb).net << "/" << seenbys.at(sb).node;
			first = false;
			sb++;
			continue;
		}
		std::string addr2d;
		if (seenbys.at(sb - 1).net != seenbys.at(sb).net) {
			addr2d = std::to_string(seenbys.at(sb).net) + "/" + std::to_string(seenbys.at(sb).node);
		}
		else {
			addr2d = std::to_string(seenbys.at(sb).node);
		}

		if (sbline.str().size() + addr2d.size() + 2 >= 79) {
			sbline << "\r";
			first = true;
			fwrite(sbline.str().c_str(), sbline.str().size(), 1, link->fptr);
			continue;
		}
		sbline << " " << addr2d;
		sb++;
	}

	if (sbline.str().size() > 0) {
		sbline << "\r";
		fwrite(sbline.str().c_str(), sbline.str().size(), 1, link->fptr);
	}

	// add path
	if (link->ouraka->point == 0) {
		struct seenby_t ourpth;

		ourpth.net = link->ouraka->net;
		ourpth.node = link->ouraka->node;

		path.push_back(ourpth);
	}

	first = true;
	for (size_t sb = 0; sb < path.size(); ) {
		if (first == true) {
			sbline.str("");
			sbline << "\001PATH: " << path.at(sb).net << "/" << path.at(sb).node;
			first = false;
			sb++;
			continue;
		}
		std::string addr2d;
		if (path.at(sb - 1).net != path.at(sb).net) {
			addr2d = std::to_string(path.at(sb).net) + "/" + std::to_string(path.at(sb).node);
		}
		else {
			addr2d = std::to_string(path.at(sb).node);
		}

		if (sbline.str().size() + addr2d.size() + 2 >= 79) {
			sbline << "\r";
			first = true;
			fwrite(sbline.str().c_str(), sbline.str().size(), 1, link->fptr);
			continue;
		}
		sbline << " " << addr2d;
		sb++;
	}

	if (sbline.str().size() > 0) {
		sbline << "\r";
		fwrite(sbline.str().c_str(), sbline.str().size(), 1, link->fptr);
	}
	char null = '\0';

	fwrite(&null, 1, 1, link->fptr);
}

bool Scanner::run() {
	INIReader inir("talisman.ini");
	Config c;
	static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	if (inir.ParseError()) {
		return false;
	}
	unsigned long pid;
	FILE* pfptr = NULL;
	std::filesystem::path packet;

#ifdef _MSC_VER
	pid = GetCurrentProcessId();
#else
	pid = getpid();
#endif

	_datapath = inir.Get("Paths", "Data Path", "data");
	_msgpath = inir.Get("Paths", "Message Path", "msgs");
	_logpath = inir.Get("Paths", "Log Path", "logs");
	_tmppath = inir.Get("Paths", "Temp Path", "temp");

	Logger log;

	log.load(_logpath + "/postie.log");

	c.load(_datapath);
	c.load_archivers(_datapath);

	for (size_t i = 0; i < c.areas.size(); i++) {
		sq_msg_base_t* mb;

		mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.areas.at(i).file).c_str());

		if (!mb) {
			log.log(LOG_ERROR, "Unable to open message base %s", std::string(_msgpath + "/" + c.areas.at(i).file).c_str());
			continue;
		}
		for (size_t mid = 1; mid <= mb->basehdr.num_msg; mid++) {
			sq_msg_t* msg = SquishReadMsg(mb, mid);
			if (!msg) {
				log.log(LOG_ERROR, "Error Reading Message %d from %s", mid, std::string(_msgpath + "/" + c.areas.at(i).file).c_str());
				break;
			}

			if (msg->xmsg.attr & MSGLOCAL && !(msg->xmsg.attr & MSGSENT)) {
				// export message.
				for (size_t lid = 0; lid < c.areas.at(i).links.size(); lid++) {
					std::stringstream ss;
					for (size_t m = 0; m < msg->msg_len; m++) {
						ss << msg->msg[m];
					}

					std::vector<struct seenby_t> seenbys = parse_seenbys(ss.str());
					std::vector<struct seenby_t> path = parse_path(ss.str());
					if (check_seenby(&seenbys, c.areas.at(i).links.at(lid)->aka)) {
						continue;
					}

					// trim off seenbys & path
					std::string msgbody = remove_seenby_path(ss.str());

					if (c.areas.at(i).links.at(lid)->fptr == NULL) {
						initialize_packet(c.areas.at(i).links.at(lid), std::string(_tmppath + "/postie-" + std::to_string(pid)), c.areas.at(i).links.at(lid)->ouraka);
					}
					// write message
					write_msg_to_pkt(&c.areas.at(i), c.areas.at(i).links.at(lid), msg, true);
				}

				msg->xmsg.attr |= MSGSENT;
				SquishLockMsgBase(mb);
				SquishUpdateHdr(mb, msg);
				SquishUnlockMsgBase(mb);
			}
			SquishFreeMsg(msg);
		}

	}

	for (size_t i = 0; i < c.netmailareas.size(); i++) {
		sq_msg_base_t* mb;

		mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.netmailareas.at(i).file).c_str());

		if (!mb) {
			log.log(LOG_ERROR, "Unable to open message base %s", std::string(_msgpath + "/" + c.netmailareas.at(i).file).c_str());
			continue;
		}
		for (size_t mid = 1; mid <= mb->basehdr.num_msg; mid++) {
			sq_msg_t* msg = SquishReadMsg(mb, mid);
			if (!msg) {
				log.log(LOG_ERROR, "Error Reading Message %d from %s", mid, std::string(_msgpath + "/" + c.netmailareas.at(i).file).c_str());
				break;
			}

			if (msg->xmsg.attr & MSGLOCAL && !(msg->xmsg.attr & MSGSENT)) {
				// export message.
				// find route
				for (size_t r = 0; r < c.routes.size(); r++) {
					if (Scanner::matchroute(c.routes.at(r).route, &msg->xmsg.dest)) {
						// found route

						// find link relating to route
						for (size_t l = 0; l < c.links.size(); l++) {
							if (c.links.at(l).aka->zone == c.routes.at(r).aka->zone && c.links.at(l).aka->node == c.routes.at(r).aka->node && c.links.at(l).aka->net == c.routes.at(r).aka->net && c.links.at(l).aka->point == c.routes.at(r).aka->point) {
								if (c.links.at(l).fptr == NULL) {
									Scanner::initialize_packet(&c.links.at(l), std::string(_tmppath + "/postie-" + std::to_string(pid)), &msg->xmsg.orig);
								}
								Scanner::write_netmail_to_pkt(&c.links.at(l), msg, true);
								break;
							}
						}
						break;
					}
				}

				msg->xmsg.attr |= MSGSENT;
				SquishLockMsgBase(mb);
				SquishUpdateHdr(mb, msg);
				SquishUnlockMsgBase(mb);
			}
			SquishFreeMsg(msg);
		}

	}

	for (size_t fil = 0; fil < c.links.size(); fil++) {
		if (c.links.at(fil).fptr != NULL) {
			char null[2];
			memset(null, 0, 2);

			fwrite(null, 2, 1, c.links.at(fil).fptr);

			fclose(c.links.at(fil).fptr);

			// create bundle
			std::string bundlename = get_bundle_name(&c.links.at(fil), c.packetdir());
			if (bundlename == "") {
				log.log(LOG_ERROR, "Unable to get bundle name");
				continue;
			}

			for (size_t arc = 0; arc < c.archivers.size(); arc++) {
				if (strcasecmp(c.archivers.at(arc)->name.c_str(), c.links.at(fil).archiver.c_str()) == 0) {
					std::vector<std::string> files;

					files.push_back(c.links.at(fil).packetpath.u8string());

					c.archivers.at(arc)->compress(bundlename, files);
					append_flo_file(&c.links.at(fil), &c, bundlename, "ref");
					break;
				}
			}
		}
	}

	std::filesystem::path temppath(_tmppath + "/postie-" + std::to_string(pid));

	std::filesystem::remove_all(temppath);

	return true;
}

bool Scanner::append_flo_file(struct link_conf_t *link, Config *c, std::string bundlefname, std::string flowtype) {
	std::stringstream flowfname;
	char buffer[13];

	flowfname.str("");

	if (link->aka->point != 0) {
		snprintf(buffer, sizeof buffer, "%08x", link->aka->point);
		flowfname << buffer;
	}
	else {
		snprintf(buffer, sizeof buffer, "%04x%04x", link->aka->net, link->aka->node);
		flowfname << buffer;
	}

	if (strcasecmp(flowtype.c_str(), "mail") == 0) {
		if (strcasecmp(link->flavour.c_str(), "crash") == 0) {
			flowfname << ".c";
		}
		else if (strcasecmp(link->flavour.c_str(), "hold") == 0) {
			flowfname << ".h";
		}
		else if (strcasecmp(link->flavour.c_str(), "normal") == 0) {
			flowfname << ".o";
		}

		flowfname << "ut";
	}
	else if (strcasecmp(flowtype.c_str(), "ref") == 0) {
		if (strcasecmp(link->flavour.c_str(), "crash") == 0) {
			flowfname << ".c";
		}
		else if (strcasecmp(link->flavour.c_str(), "hold") == 0) {
			flowfname << ".h";
		}
		else if (strcasecmp(link->flavour.c_str(), "normal") == 0) {
			flowfname << ".f";
		}

		flowfname << "lo";
	}
	else {
		return false;
	}

	std::filesystem::path fpath;

	if (c->addresses.at(0).aka->zone == link->ouraka->zone) {
		fpath = c->outbound();
	}
	else {
		
		sprintf(buffer, ".%03x", link->ouraka->zone);


		fpath = std::string(c->outbound() + buffer);
	}

	if (link->aka->point != 0) {
		snprintf(buffer, sizeof buffer, "%04x%04x.pnt", link->aka->net, link->aka->node);
		fpath.append(buffer);
	}

	std::filesystem::create_directories(fpath);

	fpath.append(flowfname.str());

	FILE* fptr;

	fptr = fopen(fpath.u8string().c_str(), "a");

	fprintf(fptr, "^%s\n", bundlefname.c_str());

	fclose(fptr);
	return true;
}

std::string Scanner::get_bundle_name(struct link_conf_t *link, std::string packetpath) {
	time_t ttime;
	struct tm thetm;
	static const char* days[] = { "su", "mo", "tu", "we", "th", "fr", "sa" };
	static const char ext[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	std::stringstream ss;
	std::filesystem::path finalpath;
	bool found = false;

	char buffer[9];

	if (link->aka->point != 0) {
		snprintf(buffer, sizeof buffer, "0000p%03x", link->aka->point);
	}
	else {
		snprintf(buffer, sizeof buffer, "%04x%04x", abs(link->ouraka->net - link->aka->net), abs(link->ouraka->node - link->aka->node));
	}

	ttime = time(NULL);

#ifdef _MSC_VER
	localtime_s(&thetm, &ttime);
#else 
	localtime_r(&ttime, &thetm);
#endif
	ss << buffer << "." << days[thetm.tm_wday];

	for (int i = 0; i < 37; i++) {
		ss << ext[i];
		finalpath = packetpath;
		finalpath.append(ss.str());
		if (!std::filesystem::exists(finalpath)) {
			found = true;
			break;
		}
		ss.seekp(-1, ss.cur);
	}
	if (found) {
		return finalpath.u8string();
	}
	return "";
}