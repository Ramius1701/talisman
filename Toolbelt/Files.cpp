#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <sstream>
#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif
#include "Archiver.h"
#include "Files.h"
#include "toml.hpp"

bool Files::load_archivers(std::string datapath)
{
	try {
		auto data3 = toml::parse_file(datapath + "/archivers.toml");

		auto arcitems = data3.get_as<toml::array>("archiver");

		for (size_t i = 0; i < arcitems->size(); i++) {
			auto itemtable = arcitems->get(i)->as_table();

			std::string myname;
			std::string myext;
			std::string myunarc;
			std::string myarc;

			auto name = itemtable->get("name");
			if (name != nullptr) {
				myname = name->as_string()->value_or("Invalid Name");
			}
			else {
				myname = "Unknown";
			}

			auto ext = itemtable->get("extension");
			if (ext != nullptr) {
				myext = ext->as_string()->value_or("");
			}
			else {
				myext = "";
			}

			auto unarc = itemtable->get("unarc");
			if (unarc != nullptr) {
				myunarc = unarc->as_string()->value_or("");
			}
			else {
				myunarc = "";
			}
			auto arc = itemtable->get("arc");
			if (arc != nullptr) {
				myarc = arc->as_string()->value_or("");
			}
			else {
				myarc = "";
			}
			Archiver* a = new Archiver(myname, myext, myunarc, myarc);
			archivers.push_back(a);
		}
	}
	catch (toml::parse_error) {
		std::cerr << "Error parsing " << datapath << "/archivers.toml" << std::endl;
		return false;
	}
	return true;
}

bool Files::file_exists(std::string filename, std::string database) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	bool ret;
	static const char sql[] = "SELECT filename FROM files WHERE filename = ?";
	std::filesystem::path p(filename);

	if (!open_database(database, &db)) {
		std::cerr << "Error opening file database : " << database << std::endl;

		return true;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		std::cerr << "Error preparing statement" << std::endl;
		return true;
	}
	std::string fp = std::filesystem::absolute(p).u8string();
	sqlite3_bind_text(stmt, 1, fp.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		ret = true;
	}
	else {
		ret = false;
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return ret;
}

bool Files::insert_file(std::string database, std::string filename, std::vector<std::string> descr, std::string uploader) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	std::stringstream desc;
	std::string ddesc;
	struct stat s;
	time_t now = time(NULL);
	bool ret = false;

	for (size_t i = 0; i < descr.size(); i++) {
		desc << descr.at(i) << "\n";
	}
	ddesc = desc.str();
	if (stat(filename.c_str(), &s) != 0) {
		return false;
	}

	static const char* sql = "INSERT INTO files (filename, filesize, dlcount, uldate, ulname, descr) VALUES(?,?,0,?,?,?)";

	if (!open_database(database, &db)) {
		return false;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}
	sqlite3_bind_text(stmt, 1, filename.c_str(), -1, NULL);
	sqlite3_bind_int64(stmt, 2, s.st_size);
	sqlite3_bind_int64(stmt, 3, now);
	sqlite3_bind_text(stmt, 4, uploader.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 5, ddesc.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_DONE) {
		ret = true;
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return ret;
}


bool Files::add_file(std::string temppath, std::string dbname, std::string filename, std::string uploader)
{
	unsigned long pid;
#ifdef _MSC_VER
	pid = GetCurrentProcessId();
#else
	pid = getpid();
#endif

	if (!file_exists(filename, dbname)) {
		std::filesystem::path p(temppath);
		p.append("toolbelt-" + std::to_string(pid));
		std::filesystem::remove_all(p);
		std::filesystem::create_directories(p);
		std::vector<std::string> flist;
		std::vector<std::string> descr;
		flist.push_back("file_id.diz");

		for (size_t i = 0; i < archivers.size(); i++) {
			std::filesystem::path f(filename);
			if (strcasecmp(archivers.at(i)->extension.c_str(), f.extension().u8string().c_str()) == 0) {
				archivers.at(i)->extract(f.u8string(), flist, p.u8string());
				std::filesystem::path d(p);
				d.append("file_id.diz");

				if (std::filesystem::exists(d)) {
					std::ifstream infile(d.u8string());
					std::string line;
					while (std::getline(infile, line))
					{
						descr.push_back(line);
					}
					break;
				}
			}
		}

		std::filesystem::remove_all(p);

		if (descr.size() == 0) {
			descr.push_back("No Description.");
		}
		return insert_file(dbname, filename, descr, uploader);
	}
	return false;
}

bool Files::open_database(std::string filename, sqlite3** db)
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