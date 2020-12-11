#ifdef _MSC_VER
#include <Windows.h>
#include <Shlwapi.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#include <fnmatch.h>
#endif
#include <fstream>
#include <filesystem>
#include <ctime>
#include <sstream>
#include "INIReader.h"
#include "Config.h"
#include "TicProc.h"
#include "Logger.h"
#include "Dupe.h"


bool TicProc::check_crc(const char * filename, uint32_t crc_chk) {
	uint32_t crc;
	
	if (!Dupe::crc32file(filename, &crc)) {
		return false;
	}

	return (crc == crc_chk);
}

bool TicProc::add_file_to_area(struct ticfile_t* tic, std::filesystem::path srcfile, std::filesystem::path destfile, std::string database) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	time_t now = time(NULL);

	static const char* sql = "INSERT INTO files (filename, filesize, dlcount, uldate, ulname, descr) VALUES(?, ?, 0, ?, ?, ?)";
	static const char* dsql = "DELETE FROM files WHERE filename LIKE ?";
	if (!open_database(database, &db)) {
		return false;
	}
	
	if (tic->replaces != "") {
		// remove file that this file replaces
		std::filesystem::path dir = destfile.parent_path();
		for (auto& p : std::filesystem::directory_iterator(dir)) {
#ifdef _MSC_VER
			if (PathMatchSpecA(p.path().filename().u8string().c_str(), tic->replaces.c_str())) {
#else
			if (fnmatch(tic->replaces.c_str(), p.path().filename().u8string().c_str(), FNM_CASEFOLD)) {
#endif
				// remove from database
				if (sqlite3_prepare_v2(db, dsql, strlen(dsql), &stmt, NULL) != SQLITE_OK) {
					sqlite3_close(db);
					return false;
				}

				std::string filen = p.path().u8string();

				sqlite3_bind_text(stmt, 1, filen.c_str(), -1, NULL);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
				// remove file
				std::filesystem::remove(p.path());
				break;
			}
		}
	}

	if (!std::filesystem::copy_file(srcfile, destfile)) {
		sqlite3_close(db);
		return false;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}

	std::string fname = std::filesystem::absolute(destfile).u8string();
	std::string ulname = "Tic Processor";
	std::string descr;
	sqlite3_bind_text(stmt, 1, fname.c_str(), -1, NULL);
	sqlite3_bind_int64(stmt, 2, std::filesystem::file_size(destfile));
	sqlite3_bind_int64(stmt, 3, now);
	sqlite3_bind_text(stmt, 4, ulname.c_str(), -1, NULL);

	if (tic->desc.size() > 0) {
		std::stringstream ss;
		for (size_t i = 0; i < tic->desc.size(); i++) {
			ss << tic->desc.at(i) << "\n";
		}

		descr = ss.str();
		
		sqlite3_bind_text(stmt, 5, descr.c_str(), -1, NULL);
	}
	else {
		descr = tic->shortdesc;
		sqlite3_bind_text(stmt, 5, descr.c_str(), -1, NULL);
	}

	sqlite3_step(stmt);
	sqlite3_close(db);

	return true;
}

bool TicProc::open_database(std::string filename, sqlite3** db)
{
	static const char* create_sql = "CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, filesize INTEGER, dlcount INTEGER, uldate INTEGER, ulname TEXT, descr TEXT);";
	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
		//std::cerr << "Unable to open database: users.db" << std::endl;
		return false;
	}
	sqlite3_busy_timeout(*db, 5000);

	rc = sqlite3_exec(*db, create_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create file table: " << err_msg << std::endl;
		free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}

bool TicProc::run() {
	INIReader inir("talisman.ini");
	Config c;
	unsigned long pid;
	static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	if (inir.ParseError()) {
		return false;
	}

	_datapath = inir.Get("Paths", "Data Path", "data");
	_logpath = inir.Get("Paths", "Log Path", "logs");
	_tmppath = inir.Get("Paths", "Temp Path", "temp");

	Logger log;

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

	std::filesystem::path temppth(_tmppath + "/postie-" + std::to_string(pid));
	std::filesystem::create_directories(temppth);

	std::vector<std::filesystem::path> removelist;

	// foreach tic file
	for (auto& p : std::filesystem::directory_iterator(c.protinbound())) {
		std::filesystem::path filepth = p.path();

		if (strcasecmp(filepth.extension().u8string().c_str(), ".tic") == 0) {
			//    process tic file
			struct ticfile_t tic;
			tic.crc = 0;
			std::ifstream ifs(filepth);
			std::string line;
			while (std::getline(ifs, line))
			{
				if (line[line.size() - 1] == '\r') {
					line = line.substr(0, line.size() - 1);
				}
				if (strncasecmp(line.c_str(), "area ", 5) == 0) {
					tic.area = line.substr(5);
				}
				if (strncasecmp(line.c_str(), "file ", 5) == 0) {
					tic.file = line.substr(5);
				}
				if (strncasecmp(line.c_str(), "lfile ", 6) == 0) {
					tic.lname = line.substr(6);
				}
				if (strncasecmp(line.c_str(), "fullname ", 9) == 0) {
					tic.lname = line.substr(9);
				}
				if (strncasecmp(line.c_str(), "desc ", 5) == 0) {
					tic.shortdesc = line.substr(5);
				}
				if (strncasecmp(line.c_str(), "ldesc ", 6) == 0) {
					tic.desc.push_back(line.substr(6));
				}
				if (strncasecmp(line.c_str(), "replaces ", 9) == 0) {
					tic.replaces = line.substr(9);
				}
				if (strncasecmp(line.c_str(), "crc ", 4) == 0) {
					tic.crc = strtoul(line.substr(4).c_str(), NULL, 16);
				}
				if (strncasecmp(line.c_str(), "pw ", 3) == 0) {
					tic.password = line.substr(3);
				}
				if (strncasecmp(line.c_str(), "from ", 5) == 0) {
					tic.from = line.substr(5);
				}
				if (strncasecmp(line.c_str(), "seenby ", 7) == 0) {
					NETADDR* addr = parse_fido_addr(line.substr(7).c_str());
					if (addr != NULL) {
						tic.seenbys.push_back(addr);
					}
				}
				if (strncasecmp(line.c_str(), "path ", 5) == 0) {
					tic.path.push_back(line.substr(5));
				}
			}

			ifs.close();

			//    check crc of file
			fprintf(stderr, "%s\r\n", std::string(c.protinbound() + "/" + tic.file).c_str());
			if (!check_crc(std::string(c.protinbound() + "/" + tic.file).c_str(), tic.crc)) {
				log.log(LOG_ERROR, "%s failed CRC check!", tic.file.c_str());
				for (NETADDR* addr : tic.seenbys) {
					free(addr);
				}
				continue;
			}
			// check tick password
			NETADDR* ticfrom = parse_fido_addr(tic.from.c_str());
			if (ticfrom == NULL) {
				log.log(LOG_ERROR, "%s contains no from field!", filepth.u8string().c_str());
				for (NETADDR* addr : tic.seenbys) {
					free(addr);
				}

				continue;
			}

			bool passok = false;

			for (size_t i = 0; i < c.links.size(); i++) {
				if (c.links.at(i).aka->zone == ticfrom->zone && c.links.at(i).aka->net == ticfrom->net && c.links.at(i).aka->node == ticfrom->node && c.links.at(i).aka->point == ticfrom->point) {
					if (c.links.at(i).ticpwd == tic.password) {
						passok = true;
					}
					break;
				}
			}

			free(ticfrom);

			if (!passok) {
				log.log(LOG_ERROR, "%s contains an invalid password!", filepth.u8string().c_str());
				for (NETADDR* addr : tic.seenbys) {
					free(addr);
				}

				continue;
			}

			struct farea_conf_t* filearea = nullptr;

			//    add file to area
			for (size_t i = 0; i < c.fileareas.size(); i++) {
				if (strcasecmp(c.fileareas.at(i).areatag.c_str(), tic.area.c_str()) == 0) {
					filearea = &c.fileareas.at(i);
					break;
				}
			}

			if (filearea == nullptr) {
				log.log(LOG_ERROR, "%s for unconfigured area %s", filepth.u8string().c_str(), tic.area.c_str());
				for (NETADDR* addr : tic.seenbys) {
					free(addr);
				}
				continue;
			}
			std::filesystem::path fsrc(c.protinbound() + "/" + tic.file);
			std::filesystem::path fdest(filearea->directory);

			if (tic.lname != "") {
				fdest.append(tic.lname);
			}
			else {
				fdest.append(tic.file);
			}

			if (!add_file_to_area(&tic, fsrc, fdest, _datapath + "/" + filearea->database + ".sqlite3")) {
				log.log(LOG_ERROR, "%s failed to add to area...", filepth.u8string().c_str());
				for (NETADDR* addr : tic.seenbys) {
					free(addr);
				}
				continue;
			}
			std::vector<struct link_conf_t*> downlinks;

			//    forward any files to downlinks
			for (size_t l = 0; l < filearea->links.size(); l++) {
				bool inseenby = false;
				for (size_t s = 0; s < tic.seenbys.size(); s++) {
					if (tic.seenbys.at(s)->zone == filearea->links.at(l)->aka->zone && tic.seenbys.at(s)->net == filearea->links.at(l)->aka->net && tic.seenbys.at(s)->node == filearea->links.at(l)->aka->node && tic.seenbys.at(s)->point == filearea->links.at(l)->aka->point) {
						inseenby = true;
						break;
					}
				}
				if (!inseenby) {
					downlinks.push_back(filearea->links.at(l));
				}
			}

			for (size_t l = 0; l < downlinks.size(); l++) {
				std::ifstream ifs(filepth);
				std::string line;
				std::vector<std::string> ticlines;
				while (std::getline(ifs, line))
				{
					ticlines.push_back(line);
				}

				ifs.close();

				// create tic file for link
				std::filesystem::path newtic(temppth);
				newtic.append(filepth.filename().u8string());

				FILE* fptr = fopen(newtic.u8string().c_str(), "wb");

				if (fptr) {
					for (size_t ln = 0; ln < ticlines.size(); ln++) {
						if (strncasecmp(ticlines.at(ln).c_str(), "Pw ", 3) == 0) {
							fprintf(fptr, "Pw %s\r\n", downlinks.at(l)->ticpwd.c_str());
						}
						else if (strncasecmp(ticlines.at(ln).c_str(), "To ", 3) == 0) {
							fprintf(fptr, "To %d:%d/%d.%d\r\n", downlinks.at(l)->aka->zone, downlinks.at(l)->aka->net, downlinks.at(l)->aka->node, downlinks.at(l)->aka->point);
						}
						else if (strncasecmp(ticlines.at(ln).c_str(), "From ", 5) == 0) {
							fprintf(fptr, "From %d:%d/%d.%d\r\n", downlinks.at(l)->ouraka->zone, downlinks.at(l)->ouraka->net, downlinks.at(l)->ouraka->node, downlinks.at(l)->ouraka->point);
						}
						else if (strncasecmp(ticlines.at(ln).c_str(), "Seenby ", 7) != 0 && strncasecmp(ticlines.at(ln).c_str(), "Path ", 5) != 0) {
							fprintf(fptr, "%s\r\n", ticlines.at(ln).c_str());
						}
					}

					// print path
					for (size_t pt = 0; pt < tic.path.size(); pt++) {
						fprintf(fptr, "Path %s\r\n", tic.path.at(pt).c_str());
					}
					time_t now = time(NULL);
					struct tm ltime;
#ifdef _MSC_VER
					gmtime_s(&ltime, &now);
#else
					gmtime_r(&now, &ltime);
#endif

					fprintf(fptr, "Path %d:%d/%d.%d %ull %s %s %02d:%02d:%02d %d UTC\r\n", downlinks.at(l)->ouraka->zone, downlinks.at(l)->ouraka->net, downlinks.at(l)->ouraka->node, downlinks.at(l)->ouraka->point, now, days[ltime.tm_wday], months[ltime.tm_mon], ltime.tm_hour, ltime.tm_min, ltime.tm_sec, ltime.tm_year + 1900);
					
					// print seenbys
					for (size_t sb = 0; sb < tic.seenbys.size(); sb++) {
						fprintf(fptr, "Seenby %d:%d/%d.%d\r\n", tic.seenbys.at(sb)->zone, tic.seenbys.at(sb)->net, tic.seenbys.at(sb)->node, tic.seenbys.at(sb)->point);
					}
					for (size_t sb = 0; sb < downlinks.size(); sb++) {
						fprintf(fptr, "Seenby %d:%d/%d.%d\r\n", downlinks.at(sb)->aka->zone, downlinks.at(sb)->aka->net, downlinks.at(sb)->aka->node, downlinks.at(sb)->aka->point);
					}
					fclose(fptr);

					// copy new tic file, and actual file to outbox

					std::filesystem::path desttic(downlinks.at(l)->filebox);
					desttic.append(filepth.filename().u8string());

					std::filesystem::path destfile(downlinks.at(l)->filebox);
					destfile.append(tic.file);

					std::filesystem::copy(newtic, desttic);
					std::filesystem::copy(fsrc, destfile);

					std::filesystem::remove(newtic);
				}
			}
			removelist.push_back(fsrc);
			removelist.push_back(filepth);
			for (NETADDR* addr : tic.seenbys) {
				free(addr);
			}
		}
	}

	for (size_t r = 0; r < removelist.size(); r++) {
		std::filesystem::remove(removelist.at(r));
	}

	std::filesystem::remove_all(temppth);
	return true;
}
