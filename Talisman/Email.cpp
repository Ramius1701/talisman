#include <string>
#include <iostream>
#include <sstream>
#include <cstring>
#include <sqlite3.h>
#include "Node.h"
#include "Logger.h"
#include "Email.h"
#include "Editor.h"

bool Email::open_database(std::string filename, sqlite3** db) {
	const char* create_users_sql = "CREATE TABLE IF NOT EXISTS email(id INTEGER PRIMARY KEY, sender TEXT COLLATE NOCASE, recipient TEXT COLLATE NOCASE, subject TEXT, body TEXT, date INTEGER, seen INTEGER)";

	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
		std::cerr << "Unable to open database: " << filename << std::endl;
		return false;
	}
	sqlite3_busy_timeout(*db, 5000);

	rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		std::cerr << "Unable to create email table: " << err_msg << std::endl;
		free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}

bool Email::save_message(Node *n, std::string to, std::string from, std::string subject, std::vector<std::string> msg)
{
	sqlite3* db;
	sqlite3_stmt* stmt;
	static const char sql[] = "INSERT INTO email (sender, recipient, subject, body, date, seen) VALUES(?, ?, ?, ?, ?, 0)";


	if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
		n->log->log(LOG_ERROR, "Unable to open email sqlite database");
		return false;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		n->log->log(LOG_ERROR, "Unable to prepare save_message (email) sql");
		sqlite3_close(db);
		return false;
	}

	std::stringstream ss;

	for (size_t i = 0; i < msg.size(); i++) {
		ss << msg.at(i) << "\r";
	}

	std::string msgstr = ss.str();

	time_t now = time(NULL);

	sqlite3_bind_text(stmt, 1, from.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 2, to.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 3, subject.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 4, msgstr.c_str(), -1, NULL);
	sqlite3_bind_int64(stmt, 5, now);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return true;
}

void Email::list_email(Node* n) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	std::vector<Email> emails;
	static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	static const char sql[] = "SELECT id, sender, subject, body, date, seen FROM email WHERE recipient = ?";

	if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
		n->log->log(LOG_ERROR, "Unable to open email sqlite database");
		return;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
		sqlite3_close(db);
		return;
	}
	std::string uname = n->get_user().get_username();
	sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		Email e;
		std::string body;
		std::stringstream ss;
		
		e.id = sqlite3_column_int(stmt, 0);
		e.sender = std::string((const char *)sqlite3_column_text(stmt, 1));
		e.subject = std::string((const char*)sqlite3_column_text(stmt, 2));
		body = std::string((const char*)sqlite3_column_text(stmt, 3));
		e.date = sqlite3_column_int64(stmt, 4);
		e.seen = (sqlite3_column_int(stmt, 5) == 1);

		for (size_t i = 0; i < body.size(); i++) {
			if (body[i] == '\r') {
				e.msg.push_back(ss.str());
				ss.str("");
			}
			else {
				ss << body[i];
			}
		}

		emails.push_back(e);
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	int lines = 1;
	n->cls();
	n->print_f("|09 Msg#    Subject                          From             Date            |07\r\n");
	for (size_t i = 0; i < emails.size(); i++) {
		struct tm time_tm;
#ifdef _MSC_VER
		localtime_s(&time_tm, &emails.at(i).date);
#else
		localtime_r(&emails.at(i).date, &time_tm);
#endif

		if (emails.at(i).seen) {
			n->print_f("|08[|15%6d|08] |14%-32.32s |13%-16.16s |11%02d:%02d %s %02d\r\n", i + 1, emails.at(i).subject.c_str(), emails.at(i).sender.c_str(), time_tm.tm_hour, time_tm.tm_min, months[time_tm.tm_mon], time_tm.tm_mday);
		}
		else {
			n->print_f("|08[|15%6d|08]|12*|14%-32.32s |13%-16.16s |11%02d:%02d %s %02d\r\n", i + 1, emails.at(i).subject.c_str(), emails.at(i).sender.c_str(), time_tm.tm_hour, time_tm.tm_min, months[time_tm.tm_mon], time_tm.tm_mday);
		}
		
		lines++;
		if (lines == 23) {
			n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", 1, emails.size());

			std::string res = n->get_string(6, false);

			if (res.size() == 0) {
				lines = 1;
				n->cls();
				n->print_f("|09 Msg#    Subject                          From             Date            |07\r\n");
				continue;
			}
			else if (tolower(res[0]) == 'q') {
				return;
			}
			else {
				int emailno;
				try {
					emailno = std::stoi(res) - 1;
					while (emailno >= 0 && emailno < emails.size()) {
						int ret = view_email(n, emails.at(emailno));
						if (ret == 0) {
							break;
						}
						else {
							emailno += ret;
						}
					}
					return;
				}
				catch (std::invalid_argument) {
					return;
				}
				catch (std::out_of_range) {
					return;
				}
			}
		}
	}

	n->print_f("|14Select |08[|15%d|08-|15%d|08], |15ENTER|08=|14Quit |07", 1, emails.size());
	std::string res = n->get_string(6, false);

	if (res.size() == 0) {
		return;
	}
	else {
		int emailno;
		try {
			emailno = std::stoi(res) - 1;
			// view email
			if (emailno >= 0 && emailno < emails.size()) {
				view_email(n, emails.at(emailno));
			}
			return;
		}
		catch (std::invalid_argument) {
			return;
		}
		catch (std::out_of_range) {
			return;
		}
	}
}

int Email::view_email(Node* n, Email e) {
	struct tm time_tm;
	int lines = 0;
	sqlite3* db;
	sqlite3_stmt* stmt;
	static const char sql[] = "UPDATE email SET seen=1 WHERE id=?";

	if (!e.seen) {
		if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
			n->log->log(LOG_ERROR, "Unable to open email sqlite database");
			return 0;
		}

		if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
			n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
			sqlite3_close(db);
			return 0;
		}

		sqlite3_bind_int(stmt, 1, e.id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		sqlite3_close(db);
	}
#ifdef _MSC_VER
	localtime_s(&time_tm, &e.date);
#else
	localtime_r(&emails.at(i).date, &time_tm);
#endif

	n->cls();
	n->print_f("|14Subject: |15%-65.65s\r\n", e.subject.c_str());
	n->print_f("|14   From: |15%-41.41s\r\n", e.sender.c_str());
	n->print_f("|14   Date: |15%04d-%02d-%02d %02d:%02d\r\n", time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min);
	n->print_f("|08------------------------------------------------------------------------------\r\n");
	lines = 4;

	for (size_t i = 0; i < e.msg.size(); i++) {
		if (e.msg.at(i).find('>') < 5) {
			n->print_f("|10%s\r\n", e.msg.at(i).c_str());
		}
		else {
			n->print_f("|15%s\r\n", e.msg.at(i).c_str());
		}
		lines++;
		if (lines == 23) {
			n->print_f("|14Continue (Y/N) : |07");
			if (tolower(n->getche()) == 'n') {
				n->print_f("\r\n");
				break;
			}
			n->print_f("\r\n");
			lines = 0;
		}
	}

	n->print_f("\r\n");
	n->print_f("|15R|08=|14Reply|08, |15P|08=|14Prev|08, |15N|08=|14Next|08, |15Q|08=|14Quit |08: |07");
	std::string res = n->get_string(1, false);
	if (res.size() == 0) {
		return 1;
	}
	else {
		switch (tolower(res[0])) {
		case 'r':
		{
			std::vector<std::string> quotemsg;

			for (size_t i = 0; i < e.msg.size(); i++) {
				if (e.msg.at(i).size() > 75) {
					std::vector<std::string> newline = MsgArea::word_wrap(e.msg.at(i), 75);
					for (size_t y = 0; y < newline.size(); y++) {
						quotemsg.push_back(" > " + newline.at(y));
					}
				}
				else {
					quotemsg.push_back(" > " + e.msg.at(i));
				}
			}

			std::vector<std::string> newmsg = Editor::enter_message(n, e.sender, e.subject, true, &quotemsg);
			if (newmsg.size() > 0) {
				Email::save_message(n, e.sender, n->get_user().get_username(), e.subject, newmsg);
			}

		}
			return 0;
		case 'p':
			return -1;
		case 'n':
			return 1;
		case 'q':
			return 0;
		}
	}
	return 0;
}