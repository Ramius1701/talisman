#include <sqlite3.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstring>
#include "FileArea.h"
#include "Node.h"

bool FileArea::open_database(std::string filename, sqlite3** db)
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

void FileArea::inc_download_count(Node* n, std::string filename) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	int dlcount = 0;
	static const char sql[] = "SELECT dlcount FROM files WHERE filename = ?";
	static const char sql2[] = "UPDATE files SET dlcount = ? WHERE filename = ?";
	if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
		return;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_text(stmt, 1, filename.c_str(), -1, NULL);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		dlcount = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
		if (sqlite3_prepare_v2(db, sql2, strlen(sql2), &stmt, NULL) != SQLITE_OK) {
			sqlite3_close(db);
			return;
		}
		dlcount++;

		sqlite3_bind_int(stmt, 1, dlcount);
		sqlite3_bind_text(stmt, 2, filename.c_str(), -1, NULL);
		sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

int FileArea::get_total_files(Node* n) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	int ret = 0;
	static const char sql[] = "SELECT COUNT(*) FROM files";

	if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
		return 0;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return 0;
	}
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		ret = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return ret;
}

struct file_list_t {
	std::string filename;
	size_t filesize;
	int dlcount;
	time_t uldate;
	std::string ulname;
	std::vector<std::string> desc;
};

void FileArea::list_files(Node* n) {
	int lines = 0;
	sqlite3* db;
	sqlite3_stmt* stmt;
	std::vector<file_list_t> filelist;
	static const char units[] = " kmgt";

	static const char sql[] = "SELET filename, filesize, dlcount, uldate, ulname, descr FROM files ORDER BY uldate DESC";

	if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
		return;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		struct file_list_t f;

		f.filename = std::string((const char*)sqlite3_column_text(stmt, 0));
		f.filesize = sqlite3_column_int64(stmt, 1);
		f.dlcount = sqlite3_column_int(stmt, 2);
		f.uldate = sqlite3_column_int64(stmt, 3);
		f.ulname = std::string((const char*)sqlite3_column_text(stmt, 4));
		std::string descr((const char*)sqlite3_column_text(stmt, 5));
		std::stringstream ss;

		for (size_t i = 0; i < descr.size(); i++) {
			if (descr.at(i) == '\n') {
				f.desc.push_back(ss.str());
				ss.str("");
			}
			else {
				ss << descr.at(i);
			}
		}
		filelist.push_back(f);
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	for (size_t i = 0; i < filelist.size(); i++) {
		int unit;
		for (unit = 0; unit < 5; unit++) {
			if (filelist.at(i).filesize >= 1024) {
				filelist.at(i).filesize /= 1024;
			}
			else {
				break;
			}
		}
		std::filesystem::path p(filelist.at(i).filename);

		if (filelist.at(i).desc.size() > 0) {
			n->print_f("|14%4d. |15%-16.16s |13%5d%c |12%4d |07%s\r\n", i + 1, p.filename().u8string().c_str(), filelist.at(i).filesize, units[unit], filelist.at(i).dlcount, filelist.at(i).desc.at(0).c_str());
			lines++;
			for (size_t z = 1; z < filelist.at(i).desc.size(); z++) {
				if (lines == 23) {
					n->print_f("|08[|151|08-|15%d|08] |14Tag File, |15Q|08=|14Quit|08, |15ENTER|08=|14Continue", filelist.size());
					std::string res = n->get_string(5, false);
					if (res.size() > 0) {
						if (tolower(res.at(0) == 'q')) {
							return;
						}
						int ftag;
						try {
							ftag = stoi(res);
						}
						catch (std::invalid_argument) {
							ftag = 0;
						}
						catch (std::out_of_range) {
							ftag = 0;
						}
						if (ftag > 0 && ftag <= filelist.size()) {
							n->tag_file(filelist.at(ftag - 1).filename, this);
						}
					}
					n->print_f("\r\n");
					lines = 0;
				}

				n->print_f("                                   |07%s\r\n", filelist.at(i).desc.at(z).c_str());
				lines++;
			}
		}
		else {
			n->print_f("|14%4d. |15%-16.16s |13%5d%c |12%4d |07No Description\r\n", i + 1, p.filename().u8string().c_str(), filelist.at(i).filesize, units[unit], filelist.at(i).dlcount);
			lines++;
		}
		if (lines == 23) {
			n->print_f("|08[|151|08-|15%d|08] |14Tag File, |15Q|08=|14Quit|08, |15ENTER|08=|14Continue", filelist.size());
			std::string res = n->get_string(5, false);
			if (res.size() > 0) {
				if (tolower(res.at(0) == 'q')) {
					return;
				}
				int ftag;
				try {
					ftag = stoi(res);
				}
				catch (std::invalid_argument) {
					ftag = 0;
				}
				catch (std::out_of_range) {
					ftag = 0;
				}
				if (ftag > 0 && ftag <= filelist.size()) {
					n->tag_file(filelist.at(ftag - 1).filename, this);
				}
			}
			n->print_f("\r\n");
			lines = 0;

		}
	}
	if (lines > 0) {
		n->print_f("|08[|151|08-|15%d|08] |14Tag File, |15ENTER|08=|14Continue", filelist.size());
		std::string res = n->get_string(5, false);
		if (res.size() > 0) {
			int ftag;
			try {
				ftag = stoi(res);
			}
			catch (std::invalid_argument) {
				ftag = 0;
			}
			catch (std::out_of_range) {
				ftag = 0;
			}
			if (ftag > 0 && ftag <= filelist.size()) {
				n->tag_file(filelist.at(ftag - 1).filename, this);
			}
		}
		n->print_f("\r\n");
	}
}