#pragma once

#include <string>
#include <sqlite3.h>
#include "Config.h"

class User
{
public:
	User();
	void set_config(Config c);
	bool load_user(std::string username, std::string password);
	bool inst_user(std::string username, std::string password, std::string firstname, std::string lastname, std::string location, std::string email);
	void set_attribute(std::string attrib, std::string value);
	std::string get_username() {
		return username;
	}

	int get_sec_level();
	std::string get_attribute(std::string attrib, std::string def);

	static bool open_database(std::string filename, sqlite3 **db);
	static bool username_allowed(Config config, std::string username);
	static bool check_fullname(Config c, std::string fullname);
private:
	int sec_level;
	int uid;
	Config c;
	std::string hash_sha256(std::string pass, std::string salt);
	std::string username;
};

