#include <sqlite3.h>
#include <string>
#include <cstring>
#include "Phlog.h"
#include "Node.h"
#include "../Common/Logger.h"

bool Phlog::open_database(std::string db_path, sqlite3** db) {
	static const char* create_gopher_sql = "CREATE TABLE IF NOT EXISTS phlog(id INTEGER PRIMARY KEY, uid INTEGER, author TEXT, subject TEXT, datestamp INTEGER, body TEXT, draft INTEGER)";

	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(db_path.c_str(), db) != SQLITE_OK) {
		//std::cerr << "Unable to open database: users.db" << std::endl;
		return false;
	}
	sqlite3_busy_timeout(*db, 5000);

	rc = sqlite3_exec(*db, create_gopher_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}

	return true;
}

bool Phlog::save_article(Node* n, std::string subject, std::vector<std::string> msg)
{
	sqlite3* db;
	sqlite3_stmt* stmt;
	static const char sql[] = "INSERT INTO phlog (uid, author, subject, datestamp, body, draft) VALUES(?, ?, ?, ?, ?, 0)";


	if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
		n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
		return false;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		n->log->log(LOG_ERROR, "Unable to prepare save_article (gopher) sql");
		sqlite3_close(db);
		return false;
	}

	std::stringstream ss;

	for (size_t i = 0; i < msg.size(); i++) {
		ss << msg.at(i) << "\r";
	}

	std::string msgstr = ss.str();

	time_t now = time(NULL);
	int uid = n->get_user().get_uid();
	std::string uname = n->get_user().get_username();

	sqlite3_bind_int(stmt, 1, uid);
	sqlite3_bind_text(stmt, 2, uname.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 3, subject.c_str(), -1, NULL);
	sqlite3_bind_int64(stmt, 4, now);
	sqlite3_bind_text(stmt, 5, msgstr.c_str(), -1, NULL);
	
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return true;
}