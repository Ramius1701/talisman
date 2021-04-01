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
#include <sys/stat.h>
#include "Archiver.h"
#include "Files.h"
#include "../Common/toml.hpp"


std::vector<struct file_area_t> Files::load_areas(std::string datapath, int sec_level) {
	std::vector<struct file_area_t> ret;
	
	try {
		auto data1 = toml::parse_file(datapath + "/fileconfs.toml");
		auto confitems = data1.get_as<toml::array>("fileconf");

		for (size_t i = 0; i < confitems->size(); i++) {
			auto itemtable = confitems->get(i)->as_table();

			std::string myconfname;
			std::string myfbdatafile;

			auto confname = itemtable->get("name");
			if (confname != nullptr) {
				myconfname = confname->as_string()->value_or("Invalid Name");
			}
			else {
				myconfname = "Unknown Name";
			}

			auto conf = itemtable->get("config");
			if (conf != nullptr) {
				myfbdatafile = conf->as_string()->value_or("");
			}
			else {
				myfbdatafile = "";
			}

			if (myfbdatafile != "") {
				try {
					auto data2 = toml::parse_file(datapath + "/" + myfbdatafile + ".toml");
					auto areaitems = data2.get_as<toml::array>("filearea");
					for (size_t d = 0; d < areaitems->size(); d++) {
						auto itemtable2 = areaitems->get(d)->as_table();
						std::string myareaname;
						std::string mydatabase;
						int my_d_sec_level;

						auto areaname = itemtable2->get("name");
						if (areaname != nullptr) {
							myareaname = areaname->as_string()->value_or("Invalid Name");
						}
						else {
							myareaname = "Unknown Name";
						}
						auto database = itemtable2->get("database");
						if (database != nullptr) {
							mydatabase = database->as_string()->value_or("");
						}
						else {
							mydatabase = "";
						}

						auto d_sec_level = itemtable2->get("download_sec_level");
						if (d_sec_level != nullptr) {
							my_d_sec_level = d_sec_level->as_integer()->value_or(10);
						}
						else {
							my_d_sec_level = 10;
						}

						if (my_d_sec_level <= sec_level) {
							struct file_area_t newfa;

							newfa.confname = myconfname;
							newfa.areaname = myareaname;
							newfa.database = mydatabase;

							ret.push_back(newfa);
						}
					}
				}
				catch (toml::parse_error) {
					return std::vector<struct file_area_t>();
				}
			}
		}
	}
	catch (toml::parse_error) {
		return std::vector<struct file_area_t>();
	}

	return ret;
}

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
			Archiver* a = new Archiver(myname, myext, myunarc, myarc, 0, NULL, 0);
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
	struct stat s;
	static const char sql[] = "SELECT filesize FROM files WHERE filename = ?";
	static const char sql2[] = "DELETE FROM files WHERE filename = ?";
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
		// check if the file is the same size
		if (stat(fp.c_str(), &s) == 0) {
			if (s.st_size != sqlite3_column_int64(stmt, 0)) {
				sqlite3_finalize(stmt);
				if (sqlite3_prepare_v2(db, sql2, strlen(sql2), &stmt, NULL) != SQLITE_OK) {
					sqlite3_close(db);
					std::cerr << "Error preparing statement" << std::endl;
					return true;
				}
				sqlite3_bind_text(stmt, 1, fp.c_str(), -1, NULL);
				sqlite3_step(stmt);
				ret = false;
			}
			else {
				ret = true;
			}
		}
		else {
			ret = true;
		}
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
		flist.push_back("FILE_ID.DIZ");

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
				else {
					d = p;
					d.append("FILE_ID.DIZ");
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
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}

int Files::trim(std::string dbname) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	int ret = 0;
	std::vector<int> totrim;
	struct stat s;

	static const char sql[] = "SELECT id, filename FROM files";
	static const char dsql[] = "DELETE FROM files WHERE id=?";

	if (!open_database(dbname, &db)) {
		return ret;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return ret;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		if (stat((const char*)sqlite3_column_text(stmt, 1), &s) != 0) {
			totrim.push_back(sqlite3_column_int(stmt, 0));
		}
	}
	sqlite3_finalize(stmt);
	for (size_t i = 0; i < totrim.size(); i++) {
		if (sqlite3_prepare_v2(db, dsql, strlen(dsql), &stmt, NULL) != SQLITE_OK) {
			sqlite3_close(db);
			return ret;
		}
		sqlite3_bind_int(stmt, 1, totrim.at(i));

		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		ret++;
	}
	sqlite3_close(db);

	return ret;
}

bool Files::move_file(std::string datapath, std::string srcfile, std::string destfile, std::string srcdb, std::string destdb) {
	std::filesystem::path srcp(srcfile);
	std::filesystem::path destp(destfile);

	if (!std::filesystem::is_regular_file(srcp)) {
		std::cout << srcfile << " is not a regular file!" << std::endl;

		return false;
	}

	if (!std::filesystem::is_directory(destp)) {
		std::cout << destfile << " is not a directory!" << std::endl;
		return false;
	}

	destp.append(srcp.filename().u8string());

	sqlite3* sdb;
	sqlite3* ddb;

	sqlite3_stmt* stmt;
	static const char* sql = "SELECT filename, filesize, dlcount, uldate, ulname, descr, id FROM files WHERE filename LIKE ?";
	static const char* sql2 = "DELETE FROM files WHERE id = ?";
	static const char* sql3 = "INSERT INTO files (filename, filesize, dlcount, uldate, ulname, descr) VALUES(?, ?, ?, ?, ?, ?)";

	if (!open_database(srcdb, &sdb)) {
		std::cerr << "Unable to open source database " << srcdb << std::endl;
		return false;
	}
	if (!open_database(destdb, &ddb)) {
		std::cerr << "Unable to open destination database " << destdb << std::endl;
		sqlite3_close(sdb);
		return false;
	}

	if (sqlite3_prepare_v2(sdb, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(sdb);
		sqlite3_close(ddb);
		return false;
	}
	std::string srcend = std::string("%" + srcp.filename().u8string());

	sqlite3_bind_text(stmt, 1, srcend.c_str(), -1, NULL);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		std::filesystem::path fp = std::string((const char *)sqlite3_column_text(stmt, 0));
		if (fp.filename() == srcp.filename()) {
			int fsize = sqlite3_column_int(stmt, 1);
			int dlcount = sqlite3_column_int(stmt, 2);
			time_t uldate = sqlite3_column_int64(stmt, 3);
			std::string ulname((const char*)sqlite3_column_text(stmt, 4));
			std::string desc((const char*)sqlite3_column_text(stmt, 5));
			int id = sqlite3_column_int(stmt, 6);
			sqlite3_finalize(stmt);
			if (sqlite3_prepare_v2(sdb, sql2, strlen(sql2), &stmt, NULL) != SQLITE_OK) {
				sqlite3_close(sdb);
				sqlite3_close(ddb);
				return false;
			}

			sqlite3_bind_int(stmt, 1, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);

			std::filesystem::copy_file(srcp, destp);
			std::filesystem::remove(srcp);
			sqlite3_close(sdb);

			if (sqlite3_prepare_v2(ddb, sql3, strlen(sql3), &stmt, NULL) != SQLITE_OK) {
				sqlite3_close(ddb);
				return false;
			}

			std::string filefullp = std::filesystem::absolute(destp).u8string();

			sqlite3_bind_text(stmt, 1, filefullp.c_str(), -1, NULL);
			sqlite3_bind_int(stmt, 2, fsize);
			sqlite3_bind_int(stmt, 3, dlcount);
			sqlite3_bind_int64(stmt, 4, uldate);
			sqlite3_bind_text(stmt, 5, ulname.c_str(), -1, NULL);
			sqlite3_bind_text(stmt, 6, desc.c_str(), -1, NULL);

			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			sqlite3_close(ddb);
			return true;
		}
	}
	sqlite3_finalize(stmt);
	sqlite3_close(sdb);
	return false;
}

int Files::all_files(std::string datapath, int sec_level, time_t date, std::string output) {
	int tot_files = 0;
	time_t now;
	struct tm now_tm;

	if (date == 0) {
		now = time(NULL);
	}
	else {
		now = date;
	}

#ifdef _MSC_VER
	localtime_s(&now_tm, &now);
#else
	localtime_r(&now, &now_tm);
#endif

	FILE* fptr = fopen(output.c_str(), "w");

	std::vector<struct file_area_t> fareas = load_areas(datapath, sec_level);


	if (date == 0) {
		fprintf(fptr, "------------------------------------------------------------------------------\n");
		fprintf(fptr, " All files list as of %4d-%2d-%2d\n", now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday);
		fprintf(fptr, "------------------------------------------------------------------------------\n");
	}
	else {
		fprintf(fptr, "------------------------------------------------------------------------------\n");
		fprintf(fptr, " New files list since %4d-%2d-%2d\n", now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday);
		fprintf(fptr, "------------------------------------------------------------------------------\n");
	}

	for (size_t i = 0; i < fareas.size(); i++) {
		sqlite3* db;
		sqlite3_stmt* stmt;

		static const char sql[] = "SELECT filename, descr FROM files WHERE uldate > ? ORDER BY uldate DESC";

		if (!open_database(datapath + "/" + fareas.at(i).database + ".sqlite3", &db)) {
			continue;
		}
		fprintf(fptr, "------------------------------------------------------------------------------\n");
		fprintf(fptr, " %s -> %s\n", fareas.at(i).confname.c_str(), fareas.at(i).areaname.c_str());
		fprintf(fptr, "------------------------------------------------------------------------------\n");

		if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
			sqlite3_close(db);
			continue;
		}
		sqlite3_bind_int64(stmt, 1, date);

		while (sqlite3_step(stmt) == SQLITE_ROW) {
			std::filesystem::path file(std::string((const char*)sqlite3_column_text(stmt, 0)));
			std::string descr((const char*)sqlite3_column_text(stmt, 1));
			std::stringstream ss;
			std::vector<std::string> desc;

			for (size_t i = 0; i < descr.size(); i++) {
				if (descr.at(i) == '\n') {
					desc.push_back(ss.str());
					ss.str("");
				}
				else if (descr.at(i) != '\r') {
					ss << descr.at(i);
				}
			}
			if (ss.str().size() > 0) {
				desc.push_back(ss.str());
			}

			if (desc.size() > 0) {
				fprintf(fptr, "\n%-24.24s %-54.54s\n", file.filename().u8string().c_str(), desc.at(0).c_str());

				for (size_t j = 1; j < desc.size(); j++) {
					fprintf(fptr, "                         %-54.54s\n", desc.at(j).c_str());
				}
			}
			else {
				fprintf(fptr, "\n%-24.24s No Description\n", file.filename().u8string().c_str());
			}
			tot_files++;
		}

		sqlite3_finalize(stmt);
		sqlite3_close(db);
	}
	return tot_files;
}