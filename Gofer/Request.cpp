#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#define strcasecmp stricmp
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <iostream>
#include <filesystem>
#include <optional>
#include <fstream>
#include "../Common/INIReader.h"
#include "Request.h"
#include "../Common/Logger.h"

std::optional<std::filesystem::path> MakeAbsolute(const std::filesystem::path& root, const std::filesystem::path& userPath)
{
	auto finalPath = (root / userPath).lexically_normal();

	auto [rootEnd, nothing] = std::mismatch(root.begin(), root.end(), finalPath.begin());

	if (rootEnd != root.end())
		return std::nullopt;

	return finalPath;
}

bool Request::open_user_database(std::string filename, sqlite3** db)
{
	static const char* create_users_sql = "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";
	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
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


bool Request::opendatabase(std::string db_path, sqlite3** db) {
	static const char* create_gopher_sql = "CREATE TABLE IF NOT EXISTS phlog(id INTEGER PRIMARY KEY, uid INTEGER, author TEXT, subject TEXT, datestamp INTEGER, body TEXT)";

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

bool Request::compare_ext(std::filesystem::path path, std::string ext) {
	if (strcasecmp(path.extension().u8string().c_str(), ext.c_str()) == 0) {
		return true;
	}
	return false;
}

void Request::dodirlist(int socket, std::filesystem::path base, std::filesystem::path dir) {
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		std::stringstream ss;
		const auto filenameStr = entry.path().filename().u8string();

		if (filenameStr == "gophermap") continue;

		try {
			if (entry.is_directory()) {
				ss << "1" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
			}
			else if (entry.is_regular_file()) {
				if (compare_ext(entry.path(), ".txt") || compare_ext(entry.path(), ".md") || compare_ext(entry.path(), ".markdown")) {
					ss << "0" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".gif")) {
					ss << "g" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".htm") || compare_ext(entry.path(), ".html")) {
					ss << "h" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".jpg") || compare_ext(entry.path(), ".jpeg") || compare_ext(entry.path(), ".png") ||
					compare_ext(entry.path(), ".bmp") || compare_ext(entry.path(), ".pcx") || compare_ext(entry.path(), ".ico") ||
					compare_ext(entry.path(), ".tif") || compare_ext(entry.path(), ".tiff") || compare_ext(entry.path(), ".svg") ||
					compare_ext(entry.path(), ".eps")) {
					ss << "I" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".mp3") || compare_ext(entry.path(), ".mp2") || compare_ext(entry.path(), ".wav")
					|| compare_ext(entry.path(), ".mid") || compare_ext(entry.path(), ".wma") || compare_ext(entry.path(), ".flac")
					|| compare_ext(entry.path(), ".mpc") || compare_ext(entry.path(), ".aiff") || compare_ext(entry.path(), ".aac")) {
					ss << "s" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".pdf")) {
					ss << "P" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else {
					ss << "9" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
			}
		}
		catch (std::exception) {
		}
		send(socket, ss.str().c_str(), ss.str().size(), 0);
	}
}

void Request::dolistposts(int socket, std::string db_path, int uid) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char* lusql = "SELECT username FROM users WHERE id = ?";
	static const char* lpsql = "SELECT id, subject, datestamp FROM phlog WHERE uid = ? ORDER BY datestamp DESC";

	std::stringstream ss;

	if (!open_user_database(db_path + "/users.sqlite3", &db)) {
		ss << "iUnable to open user database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}
	if (sqlite3_prepare_v2(db, lusql, -1, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		ss << "iUnable to open user database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}

	sqlite3_bind_int(stmt, 1, uid);

	if (sqlite3_step(stmt) != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		ss << "iNo such user!\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}
	ss << "i" << std::string((const char *)sqlite3_column_text(stmt, 0)) << "'s Phlog\tfake\t" << hostname << "\t" << port << "\r\n";
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	send(socket, ss.str().c_str(), ss.str().size(), 0);
	ss.str("");
	ss << "i--------------------------------------------------------------------\tfake\t" << hostname << "\t" << port << "\r\n";
	send(socket, ss.str().c_str(), ss.str().size(), 0);
	if (!opendatabase(db_path + "/gopher.sqlite3", &db)) {
		ss.str("");
		ss << "iUnable to open gopher database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}
	if (sqlite3_prepare_v2(db, lpsql, -1, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		ss.str("");
		ss << "iUnable to open gopher database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}
	sqlite3_bind_int(stmt, 1, uid);

	bool foundsome = false;

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		foundsome = true;
		ss.str("");
		struct tm post_tm;
		time_t thetime = sqlite3_column_int64(stmt, 2);
#ifdef _MSC_VER
		localtime_s(&post_tm, &thetime);
#else
		localtime_r(&thetime, &post_tm);
#endif
		ss << "0" << std::to_string(post_tm.tm_year + 1900) << "-" << std::to_string(post_tm.tm_mon + 1) << "-" << std::to_string(post_tm.tm_mday) << " " << std::to_string(post_tm.tm_hour) << ":" << std::to_string(post_tm.tm_min) << " - " << std::string((const char*)sqlite3_column_text(stmt, 1)) << "\tusers/" << uid << "/" << sqlite3_column_int(stmt, 0) << "\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	if (!foundsome) {
		ss.str("");
		ss << "iNo posts for user.\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
	}
}

void Request::doshowpost(int socket, std::string db_path, int uid, int id) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char* lpsql = "SELECT author, subject, datestamp, body FROM phlog WHERE id = ?";
	std::stringstream ss;

	if (!opendatabase(db_path + "/gopher.sqlite3", &db)) {
		ss.str("");
		ss << "iUnable to open gopher database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}

	if (sqlite3_prepare_v2(db, lpsql, -1, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		ss.str("");
		ss << "iUnable to open gopher database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}

	sqlite3_bind_int(stmt, 1, id);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		struct tm post_tm;
		time_t thetime = sqlite3_column_int64(stmt, 2);
#ifdef _MSC_VER
		localtime_s(&post_tm, &thetime);
#else
		localtime_r(&thetime, &post_tm);
#endif
		ss.str("");
		ss << " Title: " << std::string((const char*)sqlite3_column_text(stmt, 1)) << "\r\n";
		ss << "Author: " << std::string((const char*)sqlite3_column_text(stmt, 0)) << "\r\n";
		ss << "  Date: " << std::to_string(post_tm.tm_year + 1900) << "-" << std::to_string(post_tm.tm_mon + 1) << "-" << std::to_string(post_tm.tm_mday) << " " << std::to_string(post_tm.tm_hour) << ":" << std::to_string(post_tm.tm_min) << "\r\n";
		ss << "--------------------------------------------------------------------" << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);

		std::string body = std::string((const char*)sqlite3_column_text(stmt, 3));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		for (size_t i = 0; i < body.size(); i++) {
			if (body.at(i) != '\n') {
				send(socket, &body.at(i), 1, 0);
			}
			if (body.at(i) == '\r') {
				send(socket, "\n", 1, 0);
			}
		}
	}
	else {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		ss.str("");
		ss << "iNo such post!\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}
}

void Request::dolistusers(int socket, std::string db_path) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char* lusql = "SELECT id, username FROM users";

	std::stringstream ss;

	if (!open_user_database(db_path + "/users.sqlite3", &db)) {
		ss << "iUnable to open user database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	} 
	if (sqlite3_prepare_v2(db, lusql, -1, &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		ss << "iUnable to open user database\tfake\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
		return;
	}

	ss << "iUser Phlogs\tfake\t" << hostname << "\t" << port << "\r\n";
	send(socket, ss.str().c_str(), ss.str().size(), 0);
	ss.str("");
	ss << "i--------------------------------------------------------------------\tfake\t" << hostname << "\t" << port << "\r\n";
	send(socket, ss.str().c_str(), ss.str().size(), 0);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		ss.str("");
		ss << "1" << std::string((const char*)sqlite3_column_text(stmt, 1)) << "\tusers/" << sqlite3_column_int(stmt, 0) << "\t" << hostname << "\t" << port << "\r\n";
		send(socket, ss.str().c_str(), ss.str().size(), 0);
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

void Request::dorequest(int socket, std::string request) {
	INIReader inir("talisman.ini");

	Logger log;

	if (inir.ParseError() != 0) {
		std::cerr << "Unable to parse talisman.ini!" << std::endl;
		return;
	}

	hostname = inir.Get("main", "hostname", "localhost");
	port = inir.GetInteger("main", "gopher port", 7070);
	data_path = inir.Get("paths", "data path", "data");
	log.load(inir.Get("paths", "log path", "logs") + "/gofer.log");

	log.log(LOG_INFO, "Request: %s", request.c_str());

	if (request.size() > 0 && request.substr(0, 1) == "/") {
		request = request.substr(1);
	}

	if (request == "users" || request.find("users/") == 0) {
		std::istringstream iss(request);
		std::string token;
		std::vector<std::string> tokens;
		while (std::getline(iss, token, '/'))
			tokens.push_back(token);
		if (tokens.size() == 1) {
			dolistusers(socket, data_path);
		}
		if (tokens.size() == 2) {
			dolistposts(socket, data_path, strtol(tokens.at(1).c_str(), NULL, 10));
		}
		if (tokens.size() == 3) {
			doshowpost(socket, data_path, strtol(tokens.at(1).c_str(), NULL, 10), strtol(tokens.at(2).c_str(), NULL, 10));
		}
		return;
	}

	std::filesystem::path fpath = std::filesystem::absolute(inir.Get("paths", "gopher root", "gopher"));
	try {
		std::filesystem::path respath = MakeAbsolute(fpath, std::filesystem::path(request)).value();
		if (std::filesystem::exists(respath)) {
			if (std::filesystem::is_directory(respath)) {
				std::filesystem::path dir(respath);
				respath.append("gophermap");
				if (!std::filesystem::exists(respath)) {
					std::stringstream ss;

					ss << "iIndex of '" << std::filesystem::relative(respath.parent_path(), fpath).generic_u8string() << "'\tfake\t" << hostname << "\t" << port << "\r\n";
					send(socket, ss.str().c_str(), ss.str().size(), 0);

					dodirlist(socket, fpath, dir);
					return;
				}
			}

			std::ifstream file(respath);
			if (file.is_open()) {
				while (!file.eof()) {

					if (respath.filename().u8string() == "gophermap") {
						std::string line;
						std::getline(file, line);

						std::stringstream ss;
						if (line.size() > 0 && line.at(0) == '#') continue;
						
						if (line == "%FILES%") {
							dodirlist(socket, fpath, respath.parent_path());
							continue;
						}


						std::istringstream iss(line);
						std::string token;
						std::vector<std::string> tokens;
						while (std::getline(iss, token, '\t'))
							tokens.push_back(token);

						if (tokens.size() == 1) {
							ss << tokens.at(0) << "\t" << "fake" << "\t" << hostname << "\t" << port << "\r\n";
						}
						else if (tokens.size() > 1) {
							std::string respath2;
							if (tokens.at(1).size() > 0 && tokens.at(1).at(0) != '/') {
								respath2 = std::filesystem::relative(respath.parent_path(), fpath).generic_u8string() + "/" + tokens.at(1);
							}
							else {
								respath2 = tokens.at(1);
							}

							if (tokens.size() == 2) {
								ss << tokens.at(0) << "\t" << respath2 << "\t" << hostname << "\t" << port << "\r\n";
							}
							else if (tokens.size() == 3) {
								ss << tokens.at(0) << "\t" << respath2 << "\t" << tokens.at(2) << "\t" << port << "\r\n";
							}
							else {
								ss << tokens.at(0) << "\t" << respath2 << "\t" << tokens.at(2) << "\t" << tokens.at(3) << "\r\n";
							}
						}
						send(socket, ss.str().c_str(), ss.str().size(), 0);
					}
					else {
						char c;
						file.read(&c, 1);
						send(socket, &c, 1, 0);
					}
					
				}
				file.close();
				return;
			}
		}
	} catch (const std::bad_optional_access& e) {
		log.log(LOG_ERROR, "Bad Request!");
	}

	std::stringstream errormsg;

	errormsg << "3'" << request << "' Does not exist!\t" << hostname << "\t" << port << "\r\n";
	send(socket, errormsg.str().c_str(), errormsg.str().size(), 0);
}