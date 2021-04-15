#include <iostream>
#include <filesystem>
#include "Tosser.h"
#include "Config.h"
#include "../Common/INIReader.h"
#include "../Common/wwivnet.h"
#include "../Common/Squish.h"
#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

bool Tosser::open_user_database(sqlite3** db)
{
	static const char* create_users_sql = "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";

	int rc;
	char* err_msg = NULL;
	std::string filepath = _datapath + "/users.sqlite3";
	if (sqlite3_open(filepath.c_str(), db) != SQLITE_OK) {
		//std::cerr << "Unable to open database: users.db" << std::endl;
		return false;
	}
	sqlite3_busy_timeout(*db, 5000);

	rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create user table: " << err_msg << std::endl;
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}

	return true;
}

bool Tosser::import_email(int to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent) {
	// lookup user num -> username
	sqlite3* db;

	sqlite3_stmt* stmt;
	static const char* sql = "SELECT username FROM users WHERE id = ?";
	if (!open_user_database(&db)) {
		std::cerr << "Failed to open user database" << std::endl;
		return false;
	}
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
		std::cerr << "Failed to prepare statement" << std::endl;
		return false;
	}

	std::string username;

	sqlite3_bind_int(stmt, 1, to);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		username = std::string((const char*)sqlite3_column_text(stmt, 0));
		sqlite3_finalize(stmt);
		sqlite3_close(db);

		return import_email(username, from, fromsys, subject, msg, network, sent);
	}

	std::cerr << "Unknown user (user num " << to << ")" << std::endl;

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return false;
}

bool Tosser::import_email(std::string to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent) {
	sq_msg_base_t *mb;
	sq_msg_t newmsg;

	memset(&newmsg, 0, sizeof(sq_msg_t));

	std::stringstream ss;
	std::stringstream cs;
	bool ctrlline = false;

	for (size_t line = 0; line < msg.size(); line++) {
		for (size_t ch = 0; ch < msg.at(line).size(); ch++) {
			if (ch < msg.at(line).size() - 1 && msg.at(line).at(ch) == 0x4 && msg.at(line).at(ch + 1) == '0') {
				ctrlline = true;
				cs << 0x01;
				continue;
			}
			if (ctrlline) {
				cs << msg.at(line).at(ch);
			}
			else {
				ss << msg.at(line).at(ch);
			}
		}
		if (ctrlline) {
			ctrlline = false;
		}
		else {
			ss << '\r';
		}
	}

	newmsg.ctrl_len = cs.str().size();
	newmsg.ctrl = (char*)malloc(newmsg.ctrl_len + 1);
	if (!newmsg.ctrl) {
		return false;
	}

	strncpy(newmsg.ctrl, cs.str().c_str(), newmsg.ctrl_len);

	newmsg.msg_len = ss.str().size();
	newmsg.msg = (char*)malloc(newmsg.msg_len + 1);
	if (!newmsg.msg) {
		free(newmsg.ctrl);
		return false;
	}
	strncpy(newmsg.msg, ss.str().c_str(), newmsg.msg_len);

	newmsg.xmsg.attr = MSGUID | MSGPRIVATE;
	newmsg.xmsg.dest.zone = 20000;
	newmsg.xmsg.dest.net = 20000;
	newmsg.xmsg.dest.node = config.networks.at(network).mynode;
	newmsg.xmsg.dest.point = 0;

	newmsg.xmsg.orig.zone = 20000;
	newmsg.xmsg.orig.net = 20000;
	newmsg.xmsg.orig.node = fromsys;
	newmsg.xmsg.orig.point = 0;

	int hashloc = from.rfind('#');
	if (hashloc != std::string::npos) {
		strncpy(newmsg.xmsg.from, from.substr(0, hashloc - 1).c_str(), 35);
	}
	else {
		strncpy(newmsg.xmsg.from, from.c_str(), 35);
	}
	strncpy(newmsg.xmsg.to, to.c_str(), 35);
	strncpy(newmsg.xmsg.subject, subject.c_str(), 71);

	std::tm at;

	time_t now = time(NULL);
#ifdef _MSC_VER
	localtime_s(&at, &now);
#else
	localtime_r(&now, &at);
#endif
	newmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
	newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
	newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

	newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
	newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
	newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

#ifdef _MSC_VER
	localtime_s(&at, &sent);
#else
	localtime_r(&sent, &at);
#endif
	newmsg.xmsg.date_written.date |= (((sq_word)at.tm_mday) & 31);
	newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
	newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

	newmsg.xmsg.date_written.time |= (((sq_word)at.tm_sec) & 31);
	newmsg.xmsg.date_written.time |= (((sq_word)at.tm_min) & 63) << 5;
	newmsg.xmsg.date_written.time |= (((sq_word)at.tm_hour) & 31) << 11;

	mb = SquishOpenMsgBase(std::string(_msgpath + "/" + config.networks.at(network).emailbase).c_str());
	if (!mb) {
		free(newmsg.msg);
		free(newmsg.ctrl);
		return false;
	}
	if (!SquishLockMsgBase(mb)) {
		free(newmsg.msg);
		free(newmsg.ctrl);

		return false;
	}
	SquishWriteMsg(mb, &newmsg);

	SquishUnlockMsgBase(mb);
	SquishCloseMsgBase(mb);
	free(newmsg.msg);
	free(newmsg.ctrl);

	return true;
}

bool Tosser::import_message(std::string subtype, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent) {
	for (size_t i = 0; i < config.areas.size(); i++) {
		if (strcasecmp(config.areas.at(i).subtype.c_str(), subtype.c_str()) == 0 && strcasecmp(config.areas.at(i).netname.c_str(), config.networks.at(network).name.c_str()) == 0) {
			sq_msg_base_t* mb;
			sq_msg_t newmsg;

			memset(&newmsg, 0, sizeof(sq_msg_t));
			std::stringstream ss;
			std::stringstream cs;
			bool ctrlline = false;

			for (size_t line = 0; line < msg.size(); line++) {
				for (size_t ch = 0; ch < msg.at(line).size(); ch++) {
					if (ch < msg.at(line).size() - 1 && msg.at(line).at(ch) == 0x4 && msg.at(line).at(ch + 1) == '0') {
						ctrlline = true;
						cs << 0x01;
						continue;
					}
					if (ctrlline) {
						cs << msg.at(line).at(ch);
					}
					else {
						ss << msg.at(line).at(ch);
					}
				}
				if (ctrlline) {
					ctrlline = false;
				}
				else {
					ss << '\r';
				}
			}

			newmsg.ctrl_len = cs.str().size();
			newmsg.ctrl = (char*)malloc(newmsg.ctrl_len + 1);
			if (!newmsg.ctrl) {
				return false;
			}

			strncpy(newmsg.ctrl, cs.str().c_str(), newmsg.ctrl_len);

			newmsg.msg_len = ss.str().size();
			newmsg.msg = (char*)malloc(newmsg.msg_len + 1);
			if (!newmsg.msg) {
				free(newmsg.ctrl);
				return false;
			}
			strncpy(newmsg.msg, ss.str().c_str(), newmsg.msg_len);

			newmsg.xmsg.attr = MSGUID;

			newmsg.xmsg.orig.zone = 20000;
			newmsg.xmsg.orig.net = 20000;
			newmsg.xmsg.orig.node = fromsys;
			newmsg.xmsg.orig.point = 0;

			int hashloc = from.rfind('#');
			if (hashloc != std::string::npos) {
				strncpy(newmsg.xmsg.from, from.substr(0, hashloc - 1).c_str(), 35);
			}
			else {
				strncpy(newmsg.xmsg.from, from.c_str(), 35);
			}
			strncpy(newmsg.xmsg.to, "ALL", 35);
			strncpy(newmsg.xmsg.subject, subject.c_str(), 71);

			std::tm at;

			time_t now = time(NULL);
#ifdef _MSC_VER
			localtime_s(&at, &now);
#else
			localtime_r(&now, &at);
#endif
			newmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
			newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
			newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

			newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
			newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
			newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

#ifdef _MSC_VER
			localtime_s(&at, &sent);
#else
			localtime_r(&sent, &at);
#endif
			newmsg.xmsg.date_written.date |= (((sq_word)at.tm_mday) & 31);
			newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
			newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

			newmsg.xmsg.date_written.time |= (((sq_word)at.tm_sec) & 31);
			newmsg.xmsg.date_written.time |= (((sq_word)at.tm_min) & 63) << 5;
			newmsg.xmsg.date_written.time |= (((sq_word)at.tm_hour) & 31) << 11;

			mb = SquishOpenMsgBase(std::string(_msgpath + "/" + config.areas.at(i).basefile).c_str());
			if (!mb) {
				free(newmsg.msg);
				free(newmsg.ctrl);
				return false;
			}
			if (!SquishLockMsgBase(mb)) {
				free(newmsg.msg);
				free(newmsg.ctrl);

				return false;
			}
			SquishWriteMsg(mb, &newmsg);

			SquishUnlockMsgBase(mb);
			SquishCloseMsgBase(mb);
			free(newmsg.msg);
			free(newmsg.ctrl);

			return true;
		}
	}

	return false;
}

void Tosser::run() {
	INIReader inir("talisman.ini");

	if (inir.ParseError()) {
		std::cerr << "Failed to parse talisman.ini" << std::endl;
		return;
	}

	_datapath = inir.Get("Paths", "Data Path", "data");
	_msgpath = inir.Get("Paths", "Message Path", "msgs");
	_logpath = inir.Get("Paths", "Log Path", "logs");
	_tmppath = inir.Get("Paths", "Temp Path", "temp");
	
	if (!config.load(_datapath)) {
		std::cerr << "Failed to parse falcon.toml" << std::endl;
		return;
	}

	for (size_t i = 0; i < config.networks.size(); i++) {
		std::filesystem::path fspath = config.inbound() + "/s" + std::to_string(config.networks.at(i).mynode) + ".net";
	
		if (std::filesystem::exists(fspath)) {
			// toss file for network.
			FILE* fptr = fopen(fspath.u8string().c_str(), "rb");
			if (!fptr) {
				std::cerr << "Unable to load " << fspath.u8string() << std::endl;
				continue;
			}

			while (!feof(fptr)) {
				struct net_header_rec msgrec;
				std::vector<uint16_t> nlist;
				if (fread(&msgrec, sizeof(struct net_header_rec), 1, fptr) != 1) {
					break;
				}
				bool sr = false;
				for (uint16_t j = 0; j < msgrec.list_len; j++) {
					uint16_t n;
					if (fread(&n, sizeof(uint16_t), 1, fptr) != 1) {
						std::cerr << "Short read on 2 " << fspath.u8string() << std::endl;
						sr = true;
						break;
					}
					nlist.push_back(n);
				}

				if (sr) break;
				std::stringstream ss;
				std::vector<std::string> msg;
				char lastc = 'x';
				for (size_t j = 0; j < msgrec.length; j++) {
					char c;
					if (fread(&c, sizeof(char), 1, fptr) != 1) {
						std::cerr << "Short read on 3 " << fspath.u8string() << std::endl;
						sr = true;
						break;
					}
					if (c == '\r' || (c == '\n' && lastc != '\r')) {
						msg.push_back(ss.str());
						ss.str("");
					}
					else if (c != '\n' && c != 0x3 && c != 0x1 && c != 0x1a && c != 0x4) {
						// remove heart codes
						if (lastc == 0x3) { 
							if (!config.striphearts()) {
								switch (c) {
								case '0':
									ss << "|16|07";
									break;
								case '1':
									ss << "|16|11";
									break;
								case '2':
									ss << "|16|14";
									break;
								case '3':
									ss << "|16|13";
									break;
								case '4':
									ss << "|17|15";
									break;
								case '5':
									ss << "|16|10";
									break;
								case '6':
									ss << "|16|12";
									break;
								case '7':
									ss << "|16|09";
									break;
								case '8':
									ss << "|16|05";
									break;
								case '9':
									ss << "|16|03";
									break;
								}
							}
						}
						else if (lastc == 0x4) {
							if (c == '0') {
								ss << "\x4";
								ss << c;
							}
						}
						else {
							ss << c;
						}
					}
					lastc = c;
				}

				if (ss.str().size() > 0) {
					msg.push_back(ss.str());
					ss.str("");
				}

				if (sr) break;

				switch (msgrec.main_type) {
				case 1:
					break;
				case 2: // email to num type
					if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {
						if (msgrec.tosys == 0) {
							bool found = false;
							for (size_t k = 0; k < nlist.size(); k++) {
								if (nlist.at(k) == config.networks.at(i).mynode) {
									found = true;
									break;
								}
							}
							if (!found) {
								break;
							}
						}
						std::string subj;
						std::string sender;
						std::string datestr;
						std::stringstream ss;

						if (msg.size() == 0) break;

						for (size_t h = 0; h < msg.at(0).size(); h++) {
							if (msg.at(0).at(h) == '\0') {
								subj = ss.str();
								ss.str("");
							}
							else {
								ss << msg.at(0).at(h);
							}
						}

						sender = ss.str();

						datestr = msg.at(1);

						msg.erase(msg.begin(), msg.begin() + 1);

						import_email(msgrec.touser, sender, msgrec.fromsys, subj, msg, i, msgrec.daten);
						
					}
					break;
				case 7: // email to name type
					

					if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {
						
						if (msgrec.tosys == 0) {
							bool found = false;
							for (size_t k = 0; k < nlist.size(); k++) {
								if (nlist.at(k) == config.networks.at(i).mynode) {
									found = true;
									break;
								}
							}
							if (!found) {
								break;
							}
						}
						std::string subj;
						std::string sender;
						std::string toname;
						std::string datestr;
						std::stringstream ss;
						bool gottoname = false;

						if (msg.size() == 0) break;

						for (size_t h = 0; h < msg.at(0).size(); h++) {
							if (msg.at(0).at(h) == '\0') {
								if (!gottoname) {
									toname = ss.str();
									gottoname = true;
								}
								else {
									subj = ss.str();
								}
								ss.str("");
							}
							else {
								ss << msg.at(0).at(h);
							}
						}

						sender = ss.str();

						datestr = msg.at(1);

						msg.erase(msg.begin(), msg.begin() + 1);

						import_email(toname, sender, msgrec.fromsys, subj, msg, i, msgrec.daten);

					}
					break;
				case 18:
				{
					std::string subtype;
					uint8_t status;
					std::stringstream ss;
					std::string subject;
					std::string sender;
					bool gotsubtype = false;
					if (msg.size() == 0) break;

					if (msgrec.minor_type != 0) {
						gotsubtype = true;
						subtype = std::to_string(msgrec.minor_type);
					}

					for (size_t h = 0; h < msg.at(0).size(); h++) {
						if (msg.at(0).at(h) == '\0') {
							if (!gotsubtype) {
								subtype = ss.str();
								gotsubtype = true;
							}
							else {
								status = (uint8_t)ss.str().at(0);
								subject = ss.str().substr(1);
							}
							ss.str("");
						}
						else {
							ss << msg.at(0).at(h);
						}
					}
					sender = ss.str();

					msg.erase(msg.begin(), msg.begin() + 1);
					std::string stat_msg;

					switch (status) {
					case 0:
						stat_msg = "|10SUCCESS - You have been successfully added to the area.|07";
						break;
					case 1:
						stat_msg = "|12FAILED - I (" + std::to_string(msgrec.fromsys) + ") am not the host!|07";
						break;
					case 3:
						stat_msg = "|12FAILED - Not allowed to add subscribers automatically.|07";
						break;
					case 4:
						stat_msg = "|12FAILED - You are already subscribed!|07";
						break;
					}

					msg.insert(msg.begin(), stat_msg);

					import_email(1, sender, msgrec.fromsys, subject, msg, i, msgrec.daten);

				}
				break;
				case 19:
				{
					std::string subtype;
					uint8_t status;
					std::stringstream ss;
					std::string subject;
					std::string sender;
					bool gotsubtype = false;
					if (msg.size() == 0) break;

					if (msgrec.minor_type != 0) {
						gotsubtype = true;
						subtype = std::to_string(msgrec.minor_type);
					}

					for (size_t h = 0; h < msg.at(0).size(); h++) {
						if (msg.at(0).at(h) == '\0') {
							if (!gotsubtype) {
								subtype = ss.str();
								gotsubtype = true;
							}
							else {
								status = (uint8_t)ss.str().at(0);
								subject = ss.str().substr(1);
							}
							ss.str("");
						}
						else {
							ss << msg.at(0).at(h);
						}
					}
					sender = ss.str();

					msg.erase(msg.begin(), msg.begin() + 1);
					std::string stat_msg;

					switch (status) {
					case 0:
						stat_msg = "|10SUCCESS - You have been successfully removed from the area.|07";
						break;
					case 1:
						stat_msg = "|12FAILED - I (" + std::to_string(msgrec.fromsys) + ") am not the host!|07";
						break;
					case 3:
						stat_msg = "|12FAILED - Not allowed to add subscribers automatically.|07";
						break;
					case 2:
						stat_msg = "|12FAILED - You are not subscribed!|07";
						break;
					}

					msg.insert(msg.begin(), stat_msg);

					import_email(1, sender, msgrec.fromsys, subject, msg, i, msgrec.daten);
				}
				break;
				case 26: // main type post
				{
					std::string subtype;
					std::string subject;
					std::string sender;
					std::stringstream ss;
					bool gotsubtype = false;
					if (msg.size() == 0) break;

					if (msgrec.minor_type != 0) {
						gotsubtype = true;
						subtype = std::to_string(msgrec.minor_type);
					}

					for (size_t h = 0; h < msg.at(0).size(); h++) {
						if (msg.at(0).at(h) == '\0') {
							if (!gotsubtype) {
								subtype = ss.str();
								gotsubtype = true;
							}
							else {
								subject = ss.str();
							}
							ss.str("");
						}
						else {
							ss << msg.at(0).at(h);
						}
					}
					sender = ss.str();

					msg.erase(msg.begin(), msg.begin() + 1);

					import_message(subtype, sender, msgrec.fromsys, subject, msg, i, msgrec.daten);
				}

					break;
				}
			}

			fclose(fptr);
			try {
				std::cerr << fspath << std::endl;
				std::filesystem::remove(fspath);
			}
			catch (std::exception) {
				std::cerr << "failed to remove file" << std::endl;
			}
		}
	}
}
