#include <iostream>
#include <ctime>
#include <cstring>
#include "CallLog.h"
#include "Node.h"
#include "Config.h"

CallLog::CallLog(Config *c) {
	id = -1;
	bytesup = 0;
	bytesdown = 0;
	msgsposted = 0;
	doorsrun = 0;
	this->c = c;
}

bool CallLog::open_database(std::string filename, sqlite3** db) {
	const char* create_users_sql = "CREATE TABLE IF NOT EXISTS calllog(id INTEGER PRIMARY KEY, username TEXT, node INTEGER, timeon INTEGER, timeoff INTEGER, rundoor INTEGER, upload INTEGER, download INTEGER, msgpost INTEGER);";

	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
		std::cerr << "Unable to open database: call_log.db" << std::endl;
		return false;
	}
	sqlite3_busy_timeout(*db, 5000);

	rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		std::cerr << "Unable to create calllog table: " << err_msg << std::endl;
		free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}

void CallLog::log_on(std::string username, int node) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	time_t thetime = time(NULL);
	const char* sql = "INSERT INTO calllog (username, node, timeon, timeoff, rundoor, upload, download, msgpost) VALUES(?, ?, ?, 0, 0, 0, 0, 0)";

	if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
		return;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);
	sqlite3_bind_int(stmt, 2, node);
	sqlite3_bind_int(stmt, 3, thetime);
	sqlite3_step(stmt);
	id = (int)sqlite3_last_insert_rowid(db);

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void CallLog::log_off() {
	sqlite3* db;
	sqlite3_stmt* stmt;
	time_t thetime = time(NULL);
	const char* sql = "UPDATE calllog SET timeoff = ? WHERE id = ?";
	
	if (id == -1) {
		return;
	}
	if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_int(stmt, 1, thetime);
	sqlite3_bind_int(stmt, 2, id);

	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void CallLog::ran_door() {
	sqlite3* db;
	sqlite3_stmt* stmt;
	time_t thetime = time(NULL);
	const char* sql = "UPDATE calllog SET rundoor = ? WHERE id = ?";
	
	doorsrun++;

	if (id == -1) {
		return;
	}
	if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_int(stmt, 1, doorsrun);
	sqlite3_bind_int(stmt, 2, id);

	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void CallLog::up_bytes(int bytes) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	time_t thetime = time(NULL);
	const char* sql = "UPDATE calllog SET upload = ? WHERE id = ?";

	bytesup += bytes;

	if (id == -1) {
		return;
	}
	if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_int(stmt, 1, bytesup);
	sqlite3_bind_int(stmt, 2, id);

	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void CallLog::down_bytes(int bytes) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	time_t thetime = time(NULL);
	const char* sql = "UPDATE calllog SET download = ? WHERE id = ?";

	bytesdown += bytes;

	if (id == -1) {
		return;
	}
	if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_int(stmt, 1, bytesdown);
	sqlite3_bind_int(stmt, 2, id);

	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void CallLog::post_msg() {
	sqlite3* db;
	sqlite3_stmt* stmt;
	time_t thetime = time(NULL);
	const char* sql = "UPDATE calllog SET msgpost = ? WHERE id = ?";

	msgsposted++;

	if (id == -1) {
		return;
	}
	if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_int(stmt, 1, msgsposted);
	sqlite3_bind_int(stmt, 2, id);

	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void CallLog::last10_callers(Node* n) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	const char* sql = "SELECT id, username, node, timeon, timeoff, rundoor, upload, download, msgpost FROM (SELECT id, username, node, timeon, timeoff, rundoor, upload, download, msgpost FROM calllog ORDER by id DESC LIMIT 10) ORDER BY id ASC";

	if (!open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
		return;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}

	n->send_gfile("last10");

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int callno = sqlite3_column_int(stmt, 0);
		char* username = strdup((const char*)sqlite3_column_text(stmt, 1));
		int node = sqlite3_column_int(stmt, 2);
		time_t timeon = sqlite3_column_int(stmt, 3);
		time_t timeoff = sqlite3_column_int(stmt, 4);
		char rundoor = ' ';
		char upload = ' ';
		char download = ' ';
		char mgpost = ' ';
		struct tm tm_on;
		struct tm tm_off;
		if (sqlite3_column_int(stmt, 5) > 0) {
			rundoor = 'X';
		}

		if (sqlite3_column_int(stmt, 6) > 0) {
			upload = 'U';
		}

		if (sqlite3_column_int(stmt, 7) > 0) {
			download = 'D';
		}

		if (sqlite3_column_int(stmt, 8) > 0) {
			mgpost = 'M';
		}

		localtime_s(&tm_on, &timeon);
		
		if (timeoff == 0) {
			n->print_f(" |14%5d.   |13%2d   |15%-28.28s |10%02d:%02d |08- |12UNKWN     |09%c |11%c |13%c |10%c\r\n", callno, node, username, tm_on.tm_hour, tm_on.tm_min, rundoor, upload, download, mgpost);
		}
		else {
			localtime_s(&tm_off, &timeoff);
			n->print_f(" |14%5d.   |13%2d   |15%-28.28s |10%02d:%02d |08- |12%02d:%02d     |09%c |11%c |13%c |10%c\r\n", callno, node, username, tm_on.tm_hour, tm_on.tm_min, tm_off.tm_hour, tm_off.tm_min, rundoor, upload, download, mgpost);
		}
		free(username);
	}
	n->print_f("\r\n");
}
