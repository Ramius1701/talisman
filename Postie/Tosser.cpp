#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "INIReader.h"
#include "Config.h"
#include "Tosser.h"
#include "Logger.h"
#include "Archiver.h"
#include "PacketStructs.h"
#include "Scanner.h"


bool Tosser::run() {
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

	Logger log;

	log.load(_logpath + "/postie.log");

	c.load(_datapath);
	c.load_archivers(_datapath);

#ifdef _MSC_VER
	pid = GetCurrentProcessId();
#else
	pid = getpid();
#endif

	static const char* fileext1 = "SMTWFsmtwf";
	static const char* fileext2 = "UOEHRAuoehra";
	static const char* fileext3 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

	// toss each file one at a time
	std::filesystem::path inbound(c.protinbound());
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
			// read packet header
			struct packet_t phdr;
			struct packed_message_t pmsg;

			fread(&phdr, sizeof(struct packet_t), 1, fptr);

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

			while (fread(&pmsg, sizeof(struct packed_message_t), 1, fptr) == 1) {
				char ch;
				std::stringstream datestr;

				datestr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
					datestr << ch;
				}

				std::stringstream tostr;

				tostr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
					tostr << ch;
				}
				std::stringstream fromstr;

				fromstr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
					fromstr << ch;
				}
				std::stringstream subjstr;

				subjstr.str("");
				while ((ch = fgetc(fptr)) != '\0') {
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
				// check if dupe
				if (areatag != "") {
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
						continue;
					}
					memcpy(sqmsg.msg, msgstr.str().c_str(), msgstr.str().size());
					sqmsg.msg_len = msgstr.str().size();

					sqmsg.xmsg.orig.zone = phdr.origZone;
					if (phdr.origNet == 0xffff) {
						sqmsg.xmsg.orig.net = phdr.auxNet;
						sqmsg.xmsg.orig.point = phdr.origPoint;
					}
					else {
						sqmsg.xmsg.orig.net = phdr.origNet;
						sqmsg.xmsg.orig.point = 0;
					}
					sqmsg.xmsg.orig.node = phdr.orignode;

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

					for (size_t a = 0; a < c.areas.size(); a++) {
						if (areatag == c.areas.at(a).areatag) {
							// process message
							// TODO: check if dupe
							// send to downlinks
							for (size_t l = 0; l < c.areas.at(a).links.size(); l++) {
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
								if (c.areas.at(a).links.at(l)->fptr == NULL) {
									Scanner::initialize_packet(c.areas.at(a).links.at(l), tempdir.u8string(), &pktorig);
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

							msgprocessed = true;
							break;
						}
					}
					if (!msgprocessed) {
						log.log(LOG_ERROR, "Message for area %s not processed!", areatag.c_str());
					}
				}
				else {
					// it's a netmail...
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
								if (kludge.str().find("\001INTL ") == 0) {
									size_t addrsize = kludge.str().substr(6).find(" ");

									if (intldest != NULL) {
										free(intldest); // incase there is more than one intl line :O
									}

									std::string intl = kludge.str().substr(1);
									std::string intld = kludge.str().substr(6, addrsize);
									std::string intlo = kludge.str().substr(6 + addrsize + 1);

									intldest = parse_fido_addr(intld.c_str());
									intlorig = parse_fido_addr(intlo.c_str());

									log.log(LOG_INFO, "Found intl line \"%s\" -> dest \"%s\"  orig \"%s\"", intl.c_str(), intld.c_str(), intlo.c_str());
								}
								else if (kludge.str().find("\001TOPT ") == 0) {
									try {
										intlpoint = stoi(kludge.str().substr(6));
									}
									catch (std::invalid_argument) {

									}
									catch (std::out_of_range) {

									}
								}
								else if (kludge.str().find("\001FMPT ") == 0) {
									try {
										intlfpoint = stoi(kludge.str().substr(6));
									}
									catch (std::invalid_argument) {

									}
									catch (std::out_of_range) {

									}
								}
							}
							kludge.str("");
						}
						kludge << ctrlstr.str().at(z);
					}

					if (kludge.str().size() > 0) {
						if (kludge.str().find("\001INTL ") == 0) {
							size_t addrsize = kludge.str().substr(6).find(" ");

							if (intldest != NULL) {
								free(intldest); // incase there is more than one intl line :O
							}

							std::string intl = kludge.str().substr(1);
							std::string intld = kludge.str().substr(6, addrsize);
							std::string intlo = kludge.str().substr(6 + addrsize + 1);

							intldest = parse_fido_addr(intld.c_str());
							intlorig = parse_fido_addr(intlo.c_str());

							log.log(LOG_INFO, "Found intl line \"%s\" -> dest \"%s\"  orig \"%s\"", intl.c_str(), intld.c_str(), intlo.c_str());
						}
						else if (kludge.str().find("\001TOPT ") == 0) {
							try {
								intlpoint = stoi(kludge.str().substr(6));
							}
							catch (std::invalid_argument) {

							}
							catch (std::out_of_range) {

							}
						}
						else if (kludge.str().find("\001FMPT ") == 0) {
							try {
								intlfpoint = stoi(kludge.str().substr(6));
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
						sq_msg_base_t* mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.netmailareas.at(nmarea).file).c_str());

						if (mb != NULL) {
							SquishLockMsgBase(mb);
							SquishWriteMsg(mb, &sqmsg);
							SquishUnlockMsgBase(mb);
							SquishCloseMsgBase(mb);
						}

					}
					else {
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
											Scanner::initialize_packet(&c.links.at(l), tempdir.u8string(), &pktorig);
										}
										Scanner::write_netmail_to_pkt(&c.links.at(l), &sqmsg, false);
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
			std::string bundlename = Scanner::get_bundle_name(&c.links.at(lid), c.packetdir());
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