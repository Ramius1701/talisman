#pragma once

#include <string>
#include <filesystem>
#include <sqlite3.h>

class Request
{
public:
	void dorequest(int socket, std::string request);
private:
	std::string hostname;
	int port;
	std::string data_path;
	void dodirlist(int socket, std::filesystem::path base, std::filesystem::path dir);
	bool compare_ext(std::filesystem::path path, std::string ext);
	bool opendatabase(std::string dbpath, sqlite3** db);
	bool open_user_database(std::string filename, sqlite3** db);
	std::string last_post(sqlite3 *db, int uid);
	void dolistusers(int socket, std::string dbpath);
	void dolistposts(int socket, std::string db_path, int uid);
	void doshowpost(int socket, std::string db_path, int uid, int id);
};

