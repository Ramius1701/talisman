#include <sqlite3.h>
#include <string>
#include <cstring>
#include <sstream>
#include "Editor.h"
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

struct article_t {
	int id;
	time_t datestamp;
	std::string subject;
	bool draft;
};

void Phlog::set_draft(Node* n, int id, bool draft) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char sql[] = "UPDATE phlog SET draft = ? WHERE uid = ? AND id = ?";

	if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
		n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		n->log->log(LOG_ERROR, "Unable to prepare set_draft (gopher) sql");
		sqlite3_close(db);
		return;
	}
	int uid = n->get_user().get_uid();
	int draftstatus = (draft ? 1 : 0);

	sqlite3_bind_int(stmt, 1, draftstatus);
	sqlite3_bind_int(stmt, 2, uid);
	sqlite3_bind_int(stmt, 3, id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void Phlog::edit_article(Node* n, int id) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char sql[] = "SELECT subject, body FROM phlog WHERE uid = ? AND id = ?";
	static const char sql2[] = "UPDATE phlog SET body = ?, datestamp = ? WHERE uid = ? AND id = ?";
	if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
		n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		n->log->log(LOG_ERROR, "Unable to prepare edit_article (gopher) sql");
		sqlite3_close(db);
		return;
	}
	int uid = n->get_user().get_uid();
	sqlite3_bind_int(stmt, 1, uid);
	sqlite3_bind_int(stmt, 2, id);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		std::string subject = std::string((const char*)sqlite3_column_text(stmt, 0));
		std::string b = std::string((const char*)sqlite3_column_text(stmt, 1));
		sqlite3_finalize(stmt);

		std::vector<std::string> body;
		std::stringstream ss;
		for (size_t i = 0; i < b.size(); i++) {
			if (b.at(i) == '\r') {
				body.push_back(ss.str());
				ss.str("");
			}
			else {
				ss << b.at(i);
			}
		}

		body.push_back(ss.str());

		std::vector<std::string> newbody = Editor::enter_message(n, "My Phlog", subject, "Gopher", false, nullptr, &body);
		if (newbody.size() > 0) {
			if (sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL) != SQLITE_OK) {
				n->log->log(LOG_ERROR, "Unable to prepare edit_article (gopher) sql");
				sqlite3_close(db);
				return;
			}

			std::stringstream ss2;

			for (size_t i = 0; i < newbody.size(); i++) {
				ss2 << newbody.at(i) << "\r";
			}
			time_t now = time(NULL);

			std::string b = ss2.str();

			sqlite3_bind_text(stmt, 1, b.c_str(), -1, NULL);
			sqlite3_bind_int64(stmt, 2, now);
			sqlite3_bind_int(stmt, 3, uid);
			sqlite3_bind_int(stmt, 4, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			sqlite3_close(db);
			return;
		}
		else {
			sqlite3_close(db);
			return;
		}
	} 

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void Phlog::delete_article(Node* n, int id) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char sql[] = "DELETE FROM phlog WHERE uid = ? AND id = ?";

	if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
		n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		n->log->log(LOG_ERROR, "Unable to prepare delete_article (gopher) sql");
		sqlite3_close(db);
		return;
	}
	int uid = n->get_user().get_uid();
	sqlite3_bind_int(stmt, 1, uid);
	sqlite3_bind_int(stmt, 2, id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void Phlog::list_articles(Node* n) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	struct tm a_tm;
	std::vector<struct article_t> articles;

	static const char sql[] = "SELECT id,subject,datestamp,draft FROM phlog WHERE uid = ? ORDER BY datestamp DESC";

	if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
		n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		n->log->log(LOG_ERROR, "Unable to prepare list_articles (gopher) sql");
		sqlite3_close(db);
		return;
	}
	int uid = n->get_user().get_uid();
	sqlite3_bind_int(stmt, 1, uid);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		struct article_t a;
		a.id = sqlite3_column_int(stmt, 0);
		a.subject = std::string((const char*)sqlite3_column_text(stmt, 1));
		a.datestamp = sqlite3_column_int64(stmt, 2);
		a.draft = (sqlite3_column_int(stmt, 3) == 1 ? true : false);
		articles.push_back(a);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	int lines = 1;
	n->cls();
	n->print_f("|09Post#    Subject                          Date                 Draft       |07\r\n");
	for (size_t i = 0; i < articles.size(); i++) {
		
#ifdef _MSC_VER
		localtime_s(&a_tm, &articles.at(i).datestamp);
#else
		localtime_r(&articles.at(i).datestamp, &a_tm);
#endif

		n->print_f("|08[|15%6d|08] |14%-32.32s |13%04d-%02d-%02d %02d:%02d     %s\r\n", i + 1, articles.at(i).subject.c_str(), a_tm.tm_year + 1900, a_tm.tm_mon + 1, a_tm.tm_mday, a_tm.tm_hour, a_tm.tm_min, (articles.at(i).draft ? "|08DRAFT" : "|11PUBLISHED"));
		lines++;
		if (lines == n->get_term_height() - 2) {
			n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", 1, articles.size());

			std::string res = n->get_string(6, false);

			if (res.size() == 0) {
				lines = 1;
				n->cls();
				n->print_f("|09Post#    Subject                          Date                 Draft       |07\r\n");
				continue;
			}
			else if (tolower(res[0]) == 'q') {
				return;
			}
			else {
				size_t choice = 0;
				try {
					choice = std::stoi(res);
				}
				catch (std::invalid_argument) {
					
				}
				catch (std::out_of_range) {
					
				}

				if (choice <= 0 || choice > articles.size()) {
					return;
				}

				n->cls();
#ifdef _MSC_VER
				localtime_s(&a_tm, &articles.at(choice - 1).datestamp);
#else
				localtime_r(&articles.at(choice - 1).datestamp, &a_tm);
#endif
				n->print_f("|14    Article: |07%s\r\n", articles.at(choice - 1).subject.c_str());
				n->print_f("|14Last Edited: |07%04d-%02d-%02d %02d:%02d\r\n", a_tm.tm_year + 1900, a_tm.tm_mon + 1, a_tm.tm_mday, a_tm.tm_hour, a_tm.tm_min);
				n->print_f("|14     Status: %s\r\n", (articles.at(choice - 1).draft ? "|08DRAFT" : "|11PUBLISHED"));
				n->print_f("\r\n|14Do you want to |15E|08=|14Edit|08, |15D|08=|14Delete|08, |15T|08=|14Toggle Draft|08, |15Q|08=|14Quit");
				char c = tolower(n->getch());
				switch (c) {
				case 'e':
					break;
				case 'd':
					delete_article(n, articles.at(choice - 1).id);
					return;
				case 't':
					set_draft(n, articles.at(choice - 1).id, !articles.at(choice - 1).draft);
					return;
				default:
					return;
				}

			}
		}
	}
	n->print_f("|14Select |08[|15%d|08-|15%d|08] |15ENTER|08=|14Quit |07", 1, articles.size());

	std::string res = n->get_string(6, false);

	if (res.size() == 0) {
		return;
	}
	else if (tolower(res[0]) == 'q') {
		return;
	}
	else {
		size_t choice = 0;
		try {
			choice = std::stoi(res);
		}
		catch (std::invalid_argument) {

		}
		catch (std::out_of_range) {

		}

		if (choice <= 0 || choice > articles.size()) {
			return;
		}

		n->cls();
#ifdef _MSC_VER
		localtime_s(&a_tm, &articles.at(choice - 1).datestamp);
#else
		localtime_r(&articles.at(choice - 1).datestamp, &a_tm);
#endif
		n->print_f("|14    Article: |07%s\r\n", articles.at(choice - 1).subject.c_str());
		n->print_f("|14Last Edited: |07%04d-%02d-%02d %02d:%02d\r\n", a_tm.tm_year + 1900, a_tm.tm_mon + 1, a_tm.tm_mday, a_tm.tm_hour, a_tm.tm_min);
		n->print_f("|14     Status: %s\r\n", (articles.at(choice - 1).draft ? "|08DRAFT" : "|11PUBLISHED"));
		n->print_f("\r\n|14Do you want to |15E|08=|14Edit|08, |15D|08=|14Delete|08, |15T|08=|14Toggle Draft|08, |15Q|08=|14Quit");
		char c = tolower(n->getch());
		switch (c) {
		case 'e':
			edit_article(n, articles.at(choice - 1).id);
			return;
		case 'd':
			delete_article(n, articles.at(choice - 1).id);
			return;
		case 't':
			set_draft(n, articles.at(choice - 1).id, !articles.at(choice - 1).draft);
			return;
		default:
			return;
		}

	}

}

bool Phlog::save_article(Node* n, std::string subject, std::vector<std::string> msg)
{
	sqlite3* db;
	sqlite3_stmt* stmt;
	static const char sql[] = "INSERT INTO phlog (uid, author, subject, datestamp, body, draft) VALUES(?, ?, ?, ?, ?, 1)";


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