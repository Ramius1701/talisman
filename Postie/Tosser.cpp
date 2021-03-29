#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "../Common/INIReader.h"
#include "Config.h"
#include "Tosser.h"
#include "../Common/Logger.h"
#include "Archiver.h"
#include "PacketStructs.h"
#include "Scanner.h"
#include "Dupe.h"
#include "toml.hpp"
#include <fstream>


bool Tosser::update(std::string tag, std::string links) {
	auto config = toml::parse_file(_datapath + "/postie.toml");

	auto areaitems = config.get_as<toml::array>("area");
	if (areaitems != nullptr) {

		for (size_t k = 0; k < areaitems->size(); k++) {
			auto itemtable = areaitems->get(k)->as_table();

			auto areatag = itemtable->get("tag");

			std::string mytag;

			if (areatag != nullptr) {
				mytag = areatag->as_string()->value_or("");
			}
			else {
				mytag = "";
			}

			if (mytag == tag) {
				std::stringstream ss2;

				itemtable->insert_or_assign("links", links);
				std::fstream file(_datapath + "/postie.toml");
				file << config;
				file.close();

				return true;
			}
		}
	}

	return false;
}

void Tosser::areafix(Config *c, sq_msg_t* msg) {
	link_conf_t* link = NULL;

	for (size_t i = 0; i < c->links.size(); c++) {
		if (c->links.at(i).aka->zone == msg->xmsg.orig.zone &&
			c->links.at(i).aka->net == msg->xmsg.orig.net &&
			c->links.at(i).aka->node == msg->xmsg.orig.node &&
			c->links.at(i).aka->point == msg->xmsg.orig.point) {
			link = &c->links.at(i);
			break;
		}
	}

	if (link == NULL) {
		log.log(LOG_INFO, "Got areafix message from someone not defined in links! (%d:%d/%d.%d)", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point);
		return;
	}

	// first check password
	if (link->areafixpwd != "" && strcasecmp(link->areafixpwd.c_str(), msg->xmsg.subject) == 0) {
		// password is good.
		std::vector<std::string> smsg;
		log.log(LOG_INFO, "1");
		std::stringstream ss;
		for (size_t i = 0; i < msg->msg_len; i++) {
			if (msg->msg[i] == '\r') {
				smsg.push_back(ss.str());
				ss.str("");
			}
			else if (msg->msg[i] != '\n') {
				ss << msg->msg[i];
			}
 		}
		if (ss.str().size() > 0) {
			smsg.push_back(ss.str());
		}
		log.log(LOG_INFO, "2");
		for (std::string line : smsg) {
			if (line.at(0) == '-') {
				// remove area
				bool success = false;

				for (size_t i = 0; i < c->areas.size(); i++) {
					if (strcasecmp(c->areas.at(i).areatag.c_str(), line.substr(1).c_str()) == 0) {
						std::stringstream ss2;
						for (size_t j = 0; j < c->areas.at(i).links.size(); j++) {
							if (c->areas.at(i).links.at(j) == link) {
								success = true;
							}
							else {
								ss2 << c->areas.at(i).links.at(j)->aka->zone << ":" << c->areas.at(i).links.at(j)->aka->net << "/" << c->areas.at(i).links.at(j)->aka->node << "." << c->areas.at(i).links.at(j)->aka->point << ",";
							}
						}

						if (success == true) {
							if (ss2.str().size() > 1) {
								success = update(c->areas.at(i).areatag, ss2.str().substr(0, ss2.str().size() - 1));
							}
							else {
								success = update(c->areas.at(i).areatag, ss2.str());
							}
						}
						if (success) {
							log.log(LOG_INFO, "Successfully removed %d:%d/%d.%d from %s", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point, line.substr(1).c_str());
						}
						break;
					}
				}

			}
			else if (line.at(0) == '%') {

			}
			else {
				// add area
				bool success = false;
				log.log(LOG_INFO, "3");
				for (size_t i = 0; i < c->areas.size(); i++) {
					if (line.at(0) == '+') {
						line = line.substr(1);
					}
					if (strcasecmp(c->areas.at(i).areatag.c_str(), line.c_str()) == 0) {
						std::stringstream ss2;
						for (size_t j = 0; j < c->areas.at(i).links.size(); j++) {
							if (c->areas.at(i).links.at(j) == link) {
								success = false;
								break;
							}
							else {
								ss2 << c->areas.at(i).links.at(j)->aka->zone << ":" << c->areas.at(i).links.at(j)->aka->net << "/" << c->areas.at(i).links.at(j)->aka->node << "." << c->areas.at(i).links.at(j)->aka->point << ",";
							}
						}
						log.log(LOG_INFO, "4");
						if (ss2.str().size() > 1 && success == true) {
							ss2 << "," << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
							success = update(c->areas.at(i).areatag, ss2.str());
						}
						else {
							ss2 << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
							success = update(c->areas.at(i).areatag, ss2.str());
						}
						if (success) {
							log.log(LOG_INFO, "Successfully added %d:%d/%d.%d to %s", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point, line.substr(1).c_str());
						}
						break;
					}
				}
			}
		}
	}
	else {
		log.log(LOG_ERROR, "Incorrect password for %d:%d/%d.%d", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point);
	}
}

std::string Tosser::get_msgid(std::string ctrlbody) {
	std::stringstream kludge;

	for (size_t z = 0; z < ctrlbody.size(); z++) {
		if (ctrlbody.at(z) == '\001') {
			if (kludge.str().size() > 0) {
				if (kludge.str().find("MSGID: ") == 0) {
					int start = 7;

					std::string msgid = kludge.str().substr(start);

					return msgid;
				}
			}
			kludge.str("");
			continue;
		}
		kludge << ctrlbody.at(z);
	}

	return "";
}

NETADDR *Tosser::get_echomail_addr(std::string ctrlbody, std::string msgbody) {
	// first try getting address from origin line
	std::stringstream ss(msgbody);
	std::string line;
	NETADDR* ftnaddr = NULL;

	while (getline(ss, line, '\r')) {
		if (line.find(" * Origin: ") == 0) {
			// found origin line.

			int start = line.rfind("(") + 1;
			int size = line.substr(start).find(")");

			std::string addr = line.substr(start, size);
			if (ftnaddr != NULL) {
				free(ftnaddr);
			}

			ftnaddr = parse_fido_addr(addr.c_str());
		
		}
	}

	if (ftnaddr != NULL) {
		
		return ftnaddr;
	}

	// next try MSGID
	std::stringstream kludge;

	for (size_t z = 0; z < ctrlbody.size(); z++) {
		if (ctrlbody.at(z) == '\001') {
			if (kludge.str().size() > 0) {
				if (kludge.str().find("MSGID: ") == 0) {
					int start = 7;
					int size = kludge.str().substr(start).find(" ");

					std::string addr = kludge.str().substr(start, size - 1);
					ftnaddr = parse_fido_addr(addr.c_str());

					if (ftnaddr != NULL) {
						return ftnaddr;
					}
				}
			}
			kludge.str("");
			continue;
		}
		kludge << ctrlbody.at(z);
	}
	// next fail
	return NULL;
}

bool Tosser::run(bool protinbound) {
	INIReader inir("talisman.ini");
	Config c;
	unsigned long pid;

	if (inir.ParseError()) {
		return false;
	}

	_datapath = inir.Get("Paths", "Data Path", "data");
	_msgpath = inir.Get("Paths", "Message Path", "msgs");
	_logpath = inir.Get("Paths", "Log Path", "logs");
	_tmppath = inir.Get("Paths", "Temp Path", "temp");



	log.load(_logpath + "/postie.log");

	if (!c.load(_datapath)) {
		return false;
	}

	if (!c.load_archivers(_datapath)) {
		return false;
	}

#ifdef _MSC_VER
	pid = GetCurrentProcessId();
#else
	pid = getpid();
#endif

	static const char* fileext1 = "SMTWFsmtwf";
	static const char* fileext2 = "UOEHRAuoehra";
	static const char* fileext3 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

	// toss each file one at a time
	std::filesystem::path inbound((protinbound ? c.protinbound() : c.inbound()));
	std::filesystem::path tempdir(_tmppath + "/postie-" + std::to_string(pid));

	std::vector<std::filesystem::path> removelist;

	for (auto& p : std::filesystem::directory_iterator(inbound)) {
		std::filesystem::path packetpth = p.path();
		std::filesystem::remove_all(tempdir);
		std::filesystem::create_directories(tempdir);

		if (strcasecmp(packetpth.extension().u8string().c_str(), ".pkt") == 0) {
			std::filesystem::copy(packetpth, std::filesystem::path(tempdir.u8string() + "/" + packetpth.filename().u8string()));
			removelist.push_back(packetpth);
		}
		else {
			if (!strchr(fileext1, packetpth.extension().u8string().substr(1, 1).c_str()[0])) {
				continue;
			}

			if (!strchr(fileext2, packetpth.extension().u8string().substr(2, 1).c_str()[0])) {
				continue;
			}

			if (!strchr(fileext3, packetpth.extension().u8string().substr(3, 1).c_str()[0])) {
				continue;
			}

			removelist.push_back(packetpth);

			bool unarced = false;

			// unarchive packet
			for (size_t i = 0; i < c.archivers.size(); i++) {
				FILE* fptr = fopen(packetpth.u8string().c_str(), "rb");
				if (c.archivers.at(i)->offset >= 0) {
					fseek(fptr, c.archivers.at(i)->offset, SEEK_SET);
				}
				else {
					fseek(fptr, c.archivers.at(i)->offset, SEEK_END);
				}

				uint8_t byte;
				bool match = true;
				for (int z = 0; z < c.archivers.at(i)->bytelen; z++) {
					fread(&byte, 1, 1, fptr);

					if (byte != c.archivers.at(i)->bytes[z]) {
						match = false;
						break;
					}
				}
				fclose(fptr);
				if (match == false) continue;
				c.archivers.at(i)->extract(packetpth.u8string(), tempdir.u8string());
				unarced = true;
				break;
			}
			if (unarced == false) {
				log.log(LOG_ERROR, "Unable to find archiver for bundle!");
				continue;
			}
		}

		for (auto& pkt : std::filesystem::directory_iterator(tempdir)) {
			FILE* fptr = fopen(pkt.path().u8string().c_str(), "rb");

			if (!fptr) {
				log.log(LOG_ERROR, "Unable to open packet! %s", pkt.path().u8string().c_str());
				continue;
			}
			// read packet header
			struct packet_t phdr;
			struct packed_message_t pmsg;

			fread(&phdr, sizeof(struct packet_t), 1, fptr);
/*
			NETADDR pktorig;
			pktorig.zone = phdr.origZone;
			pktorig.node = phdr.orignode;
			if (phdr.origNet == 0xffff) {
				pktorig.net = phdr.auxNet;
				pktorig.point = phdr.origPoint;
			}
			else {
				pktorig.net = phdr.origNet;
				pktorig.point = 0;
			}
*/
			while (fread(&pmsg, sizeof(struct packed_message_t), 1, fptr) == 1) {
				char ch;
				std::stringstream datestr;

				datestr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
					if (feof(fptr)) break;
					datestr << ch;
				}

				std::stringstream tostr;

				tostr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
					if (feof(fptr)) break;
					tostr << ch;
				}
				std::stringstream fromstr;

				fromstr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
					if (feof(fptr)) break;
					fromstr << ch;
				}
				std::stringstream subjstr;

				subjstr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
					if (feof(fptr)) break;
					subjstr << ch;
				}

				std::stringstream areastr;
				std::stringstream bodystr;
				std::stringstream msgstr;
				std::stringstream ctrlstr;
				std::string line;
				std::string areatag;

				bool pastarea = false;
				bool pastorigin = false;
				bodystr.str("");

				while ((ch = fgetc(fptr)) != '\0') {
					if (feof(fptr)) break;
					// onto body
					bodystr << ch;
				}


				while (getline(bodystr, line, '\r')) {
					if (line.find("AREA:") == 0 && !pastarea) {
						areatag = line.substr(5);
						pastarea = true;
						continue;
					}
					else if (!pastarea) {
						areatag = "";
						pastarea = true;
					}
					if (line.find("--- ") == 0 || line == "---") {
						pastorigin = true;
					}
					if (line[0] == '\001' && !pastorigin) {
						ctrlstr << line;
					}
					else {
						msgstr << line << "\r";
					}
				}

				bool msgprocessed = false;

				if (areatag != "" && protinbound) {


					// it's an echomail
					sq_msg_t sqmsg;

					memset(&sqmsg, 0, sizeof(sq_msg_t));

					sqmsg.ctrl = (char*)malloc(ctrlstr.str().size());
					if (!sqmsg.ctrl) {
						continue;
					}
					memcpy(sqmsg.ctrl, ctrlstr.str().c_str(), ctrlstr.str().size());
					sqmsg.ctrl_len = ctrlstr.str().size();

					sqmsg.msg = (char*)malloc(msgstr.str().size());
					if (!sqmsg.msg) {
						free(sqmsg.ctrl);
						continue;
					}
					memcpy(sqmsg.msg, msgstr.str().c_str(), msgstr.str().size());
					sqmsg.msg_len = msgstr.str().size();

					NETADDR* emaddr = get_echomail_addr(ctrlstr.str(), msgstr.str());

					if (emaddr != NULL) {
						log.log(LOG_INFO, "Found echomail address %d:%d/%d.%d", emaddr->zone, emaddr->net, emaddr->node, emaddr->point);
						sqmsg.xmsg.orig.zone = emaddr->zone;
						sqmsg.xmsg.orig.node = emaddr->node;
						sqmsg.xmsg.orig.net = emaddr->net;
						sqmsg.xmsg.orig.point = emaddr->point;
						free(emaddr);
					}

					strncpy(sqmsg.xmsg.subject, subjstr.str().c_str(), 72);
					strncpy(sqmsg.xmsg.to, tostr.str().c_str(), 36);
					strncpy(sqmsg.xmsg.from, fromstr.str().c_str(), 36);

					std::tm lt;

					datestr >> std::get_time(&lt, "%d %b %y  %H:%M:%S");
					if (lt.tm_year < 68) {
						lt.tm_year += 100;
					}

					sqmsg.xmsg.date_written.date |= (((sq_word)lt.tm_mday) & 31);
					sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_mon + 1)) & 15) << 5;
					sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_year - 80)) & 127) << 9;

					sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_sec) & 31);
					sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_min) & 63) << 5;
					sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_hour) & 31) << 11;

					std::tm at;

					time_t now = time(NULL);
#ifdef _MSC_VER
					localtime_s(&at, &now);
#else
					localtime_r(&now, &at);
#endif

					sqmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
					sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
					sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

					sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
					sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
					sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

					strcpy(sqmsg.xmsg.__ftsc_date, datestr.str().c_str());

					sqmsg.xmsg.attr = MSGUID;

					// check if dupe
					std::string msgid = get_msgid(ctrlstr.str());

					if (msgid != "") {
						if (Dupe::is_dupe(_datapath + "/dupehist.dat", msgid)) {
							
							if (c.dupebase() == "") {
								log.log(LOG_INFO, "Found duplicate, discarding as no dupe base is configured.");
							}
							else {
								sq_msg_base_t* mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.dupebase()).c_str());

								if (mb != NULL) {
									SquishLockMsgBase(mb);
									SquishWriteMsg(mb, &sqmsg);
									SquishUnlockMsgBase(mb);
									SquishCloseMsgBase(mb);
									log.log(LOG_INFO, "Found duplicate, saved in dupe base.");
								}
								else {
									log.log(LOG_INFO, "Found duplicate, discarding because error occured opening the dupe base.");
								}
							}
							free(sqmsg.msg);
							free(sqmsg.ctrl);
							continue;
						}
					}

					for (size_t a = 0; a < c.areas.size(); a++) {
						if (strcasecmp(areatag.c_str(), c.areas.at(a).areatag.c_str()) == 0) {
							// process message
							// send to downlinks
							std::vector<struct seenby_t> seenbys = Scanner::parse_seenbys(msgstr.str());

							for (size_t l = 0; l < c.areas.at(a).links.size(); l++) {
								// if it's to a point
								if (c.areas.at(a).links.at(l)->aka->point != 0) {
									if (phdr.origNet == 0xffff) {
										if (c.areas.at(a).links.at(l)->aka->zone == phdr.origZone && c.areas.at(a).links.at(l)->aka->net == phdr.auxNet && c.areas.at(a).links.at(l)->aka->node == phdr.orignode && c.areas.at(a).links.at(l)->aka->point == phdr.origPoint) {
											continue;
										}
									}
									else {
										if (c.areas.at(a).links.at(l)->aka->zone == phdr.origZone && c.areas.at(a).links.at(l)->aka->net == phdr.origNet && c.areas.at(a).links.at(l)->aka->node == phdr.orignode && c.areas.at(a).links.at(l)->aka->point == phdr.origPoint) {
											continue;
										}
									}
								} else {
	
									// check seenbys
	
									if (Scanner::check_seenby(&seenbys, c.areas.at(a).links.at(l)->aka)) {
										continue;
									}
								}

								if (c.areas.at(a).links.at(l)->fptr == NULL) {
									Scanner::initialize_packet(c.areas.at(a).links.at(l), tempdir.u8string(), c.areas.at(a).links.at(l)->ouraka);
								}
								Scanner::write_msg_to_pkt(&c.areas.at(a), c.areas.at(a).links.at(l), &sqmsg, false);
							}
							// save to base

							sq_msg_base_t* mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.areas.at(a).file).c_str());

							if (mb != NULL) {
								SquishLockMsgBase(mb);
								SquishWriteMsg(mb, &sqmsg);
								SquishUnlockMsgBase(mb);
								SquishCloseMsgBase(mb);
							}
							else {
								log.log(LOG_ERROR, "Unable to open message base! : %s", std::string(_msgpath + "/" + c.areas.at(a).file).c_str());
							}

							msgprocessed = true;
							break;
						}
					}
					if (!msgprocessed) {
						log.log(LOG_ERROR, "Message for area %s not processed!", areatag.c_str());
					}
				}
				else if (areatag == "") {
					// it's a netmail...
					// delete empty mail
					if (bodystr.str().size() == 0) {
						log.log(LOG_INFO, "Got an empty netmail, discarding...");
						continue;
					}
					if (fromstr.str() == "ARCmail") {
						log.log(LOG_INFO, "Got a message from ARCmail, discarding...");
						continue;
					}
					// is it for us...
					// look for intl kludge & topt kludge
					std::stringstream kludge;

					NETADDR* intldest = NULL;
					NETADDR* intlorig = NULL;
					int intlpoint = 0;
					int intlfpoint = 0;

					log.log(LOG_INFO, "Got a netmail!");

					for (size_t z = 0; z < ctrlstr.str().size(); z++) {
						if (ctrlstr.str().at(z) == '\001') {
							if (kludge.str().size() > 0) {
								if (kludge.str().find("INTL ") == 0) {
									size_t addrsize = kludge.str().substr(5).find(" ");

									if (intldest != NULL) {
										free(intldest); // incase there is more than one intl line :O
									}

									std::string intl = kludge.str();
									std::string intld = kludge.str().substr(5, addrsize);
									std::string intlo = kludge.str().substr(5 + addrsize + 1);

									intldest = parse_fido_addr(intld.c_str());
									intlorig = parse_fido_addr(intlo.c_str());

									log.log(LOG_INFO, "Found intl line \"%s\" -> dest \"%s\"  orig \"%s\"", intl.c_str(), intld.c_str(), intlo.c_str());
								}
								else if (kludge.str().find("TOPT ") == 0) {
									try {
										intlpoint = stoi(kludge.str().substr(5));
									}
									catch (std::invalid_argument) {

									}
									catch (std::out_of_range) {

									}
								}
								else if (kludge.str().find("FMPT ") == 0) {
									try {
										intlfpoint = stoi(kludge.str().substr(5));
									}
									catch (std::invalid_argument) {

									}
									catch (std::out_of_range) {

									}
								}
							}
							kludge.str("");
							continue;
						}
						kludge << ctrlstr.str().at(z);
					}

					if (kludge.str().size() > 0) {
						if (kludge.str().find("INTL ") == 0) {
							size_t addrsize = kludge.str().substr(5).find(" ");

							if (intldest != NULL) {
								free(intldest); // incase there is more than one intl line :O
							}

							std::string intl = kludge.str();
							std::string intld = kludge.str().substr(5, addrsize);
							std::string intlo = kludge.str().substr(5 + addrsize + 1);

							intldest = parse_fido_addr(intld.c_str());
							intlorig = parse_fido_addr(intlo.c_str());

							log.log(LOG_INFO, "Found intl line \"%s\" -> dest \"%s\"  orig \"%s\"", intl.c_str(), intld.c_str(), intlo.c_str());
						}
						else if (kludge.str().find("TOPT ") == 0) {
							try {
								intlpoint = stoi(kludge.str().substr(5));
							}
							catch (std::invalid_argument) {

							}
							catch (std::out_of_range) {

							}
						}
						else if (kludge.str().find("FMPT ") == 0) {
							try {
								intlfpoint = stoi(kludge.str().substr(5));
							}
							catch (std::invalid_argument) {

							}
							catch (std::out_of_range) {

							}
						}
					}

					if (intldest != NULL) {
						intldest->point = intlpoint;
						if (intlorig != NULL) {
							intlorig->point = intlfpoint;
						}
					}
					else {
						// assume it's based on net / node
						intldest = (NETADDR*)malloc(sizeof(NETADDR));
						if (!intldest) {
							// panic!
							// oom?
							continue;
						}
						intldest->zone = phdr.destZone;
						intldest->net = pmsg.dest_net;
						intldest->node = pmsg.dest_node;
						intldest->point = intlpoint;
						if (intlorig == NULL) {
							intlorig = (NETADDR*)malloc(sizeof(NETADDR));
							if (!intlorig) {
								// panic!
								// oom?
								continue;
							}
							intlorig->zone = phdr.origZone;
							intlorig->net = pmsg.orig_net;
							intlorig->node = pmsg.orig_node;
							intlorig->point = intlfpoint;
						}
					}

					// if it is, import it
					// to me?
					bool found = false;
					size_t nmarea = 0;

					for (size_t a = 0; a < c.addresses.size(); a++) {
						if (intldest->zone == c.addresses.at(a).aka->zone && intldest->net == c.addresses.at(a).aka->net && intldest->node == c.addresses.at(a).aka->node && intldest->point == c.addresses.at(a).aka->point) {
							// it's for us!
							found = true;
							for (size_t ar = 0; ar < c.netmailareas.size(); ar++) {
								if (intldest->zone == c.netmailareas.at(ar).aka->zone && intldest->net == c.netmailareas.at(ar).aka->net && intldest->node == c.netmailareas.at(ar).aka->node && intldest->point == c.netmailareas.at(ar).aka->point) {
									// put it in this netmail area
									nmarea = ar;
									break;
								}
							}
							break;
						}
					}
					sq_msg_t sqmsg;

					memset(&sqmsg, 0, sizeof(sq_msg_t));

					sqmsg.ctrl = (char*)malloc(ctrlstr.str().size());
					if (!sqmsg.ctrl) {
						continue;
					}
					memcpy(sqmsg.ctrl, ctrlstr.str().c_str(), ctrlstr.str().size());
					sqmsg.ctrl_len = ctrlstr.str().size();

					sqmsg.msg = (char*)malloc(msgstr.str().size());
					if (!sqmsg.msg) {
						free(sqmsg.ctrl);
						continue;
					}
					memcpy(sqmsg.msg, msgstr.str().c_str(), msgstr.str().size());
					sqmsg.msg_len = msgstr.str().size();

					sqmsg.xmsg.orig.zone = intlorig->zone;
					sqmsg.xmsg.orig.net = intlorig->net;
					sqmsg.xmsg.orig.point = intlorig->point;
					sqmsg.xmsg.orig.node = intlorig->node;


					sqmsg.xmsg.dest.zone = intldest->zone;
					sqmsg.xmsg.dest.net = intldest->net;
					sqmsg.xmsg.dest.point = intldest->point;
					sqmsg.xmsg.dest.node = intldest->node;

					strncpy(sqmsg.xmsg.subject, subjstr.str().c_str(), 72);
					strncpy(sqmsg.xmsg.to, tostr.str().c_str(), 36);
					strncpy(sqmsg.xmsg.from, fromstr.str().c_str(), 36);

					std::tm lt;

					datestr >> std::get_time(&lt, "%d %b %y  %H:%M:%S");
					if (lt.tm_year < 68) {
						lt.tm_year += 100;
					}

					sqmsg.xmsg.date_written.date |= (((sq_word)lt.tm_mday) & 31);
					sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_mon + 1)) & 15) << 5;
					sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_year - 80)) & 127) << 9;

					sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_sec) & 31);
					sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_min) & 63) << 5;
					sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_hour) & 31) << 11;

					std::tm at;

					time_t now = time(NULL);
#ifdef _MSC_VER
					localtime_s(&at, &now);
#else
					localtime_r(&now, &at);
#endif

					sqmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
					sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
					sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

					sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
					sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
					sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

					strcpy(sqmsg.xmsg.__ftsc_date, datestr.str().c_str());

					sqmsg.xmsg.attr = MSGUID | MSGPRIVATE;

					if (found) {
						// TODO: it's to me, is it for areafix?
						if (strcasecmp(sqmsg.xmsg.to, "areafix") == 0 && protinbound) {
							log.log(LOG_INFO, "It's to areafix!");
							areafix(&c, &sqmsg);
						}
						else {

							sq_msg_base_t* mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.netmailareas.at(nmarea).file).c_str());

							if (mb != NULL) {
								SquishLockMsgBase(mb);
								SquishWriteMsg(mb, &sqmsg);
								SquishUnlockMsgBase(mb);
								SquishCloseMsgBase(mb);
							}
							else {
								log.log(LOG_ERROR, "Unable to open message base! %s", std::string(_msgpath + "/" + c.netmailareas.at(nmarea).file).c_str());
							}
						}

					}
					else if (protinbound) {
						// if not send it on
						log.log(LOG_INFO, "Netmail not to us... forwarding.");

						bool matchedroute = false;
						for (size_t r = 0; r < c.routes.size(); r++) {
							if (Scanner::matchroute(c.routes.at(r).route, intldest)) {
								// found route
								matchedroute = true;
								log.log(LOG_INFO, "Netmail not to us... matched route.");
								// find link relating to route
								for (size_t l = 0; l < c.links.size(); l++) {
									if (c.links.at(l).aka->zone == c.routes.at(r).aka->zone && c.links.at(l).aka->node == c.routes.at(r).aka->node && c.links.at(l).aka->net == c.routes.at(r).aka->net && c.links.at(l).aka->point == c.routes.at(r).aka->point) {
										if (c.links.at(l).fptr == NULL) {
											Scanner::initialize_packet(&c.links.at(l), tempdir.u8string(), c.links.at(1).ouraka);
										}
										Scanner::write_netmail_to_pkt(c.links.at(l).ouraka, c.links.at(l).aka, &sqmsg, false, c.links.at(l).fptr, c.links.at(l).flavour);
										log.log(LOG_INFO, "Netmail not to us... wrote packet.");
										break;
									}
								}
								break;
							}
						}

						if (!matchedroute) {
							log.log(LOG_ERROR, "Got netmail with no matching routes..");
						}
					}
					else {
						log.log(LOG_ERROR, "Got netmail not for me in unprotected inbound! Discarding...");
					}
					free(sqmsg.msg);
					free(sqmsg.ctrl);
				}
				else if (areatag != "" && !protinbound) {
					log.log(LOG_ERROR, "Got echomail in unprotected inbound! Discarding...");
				}
			}
			fclose(fptr);
		}
	}
	

	for (size_t lid = 0; lid < c.links.size(); lid++) {
		if (c.links.at(lid).fptr != NULL) {
			char null[2];
			memset(null, 0, 2);

			fwrite(null, 2, 1, c.links.at(lid).fptr);

			fclose(c.links.at(lid).fptr);
			c.links.at(lid).fptr = NULL;
			// create bundle
			std::string bundlename = c.packetdir() + "/" + Scanner::get_bundle_name(c.links.at(lid).ouraka, c.links.at(lid).aka, c.packetdir());
			if (bundlename == "") {
				log.log(LOG_ERROR, "Unable to get bundle name");
				continue;
			}

			for (size_t arc = 0; arc < c.archivers.size(); arc++) {
				if (strcasecmp(c.archivers.at(arc)->name.c_str(), c.links.at(lid).archiver.c_str()) == 0) {
					std::vector<std::string> files;

					files.push_back(c.links.at(lid).packetpath.u8string());

					c.archivers.at(arc)->compress(bundlename, files);
					Scanner::append_flo_file(&c.links.at(lid), &c, bundlename, "ref");
					break;
				}
			}
		}
	}
	std::filesystem::path temppath(_tmppath + "/postie-" + std::to_string(pid));

	std::filesystem::remove_all(temppath);

	for (size_t i = 0; i < removelist.size(); i++) {
		std::filesystem::remove(removelist.at(i));
	}

	return true;
}
