#pragma once

#include <string>
#include <sqlite3.h>
#include "Config.h"

class User
{
public:
	User();
	void set_config(Config c);
	bool update_password(std::string password);
	bool check_password(std::string password);
	bool load_user(std::string username, std::string password);
	bool inst_user(std::string username, std::string password, std::string firstname, std::string lastname, std::string location, std::string email);
	void set_attribute(std::string attrib, std::string value);
	bool is_subscribed(std::string msgbase);
	void set_subscribed(std::string msgbase, bool value);

	std::string get_username() {
		return username;
	}

	int get_sec_level();
	int get_uid() {
		return uid;
	}
	std::string get_attribute(std::string attrib, std::string def);

	static std::string user_exists(Config *c, std::string usern);
	static bool open_database(std::string filename, sqlite3 **db);
	static bool username_allowed(Config config, std::string username);
	static bool check_fullname(Config c, std::string fullname);
	static void user_list(Node* n);
	static std::string get_attribute_s(Config *c, int id, std::string attrib, std::string def);
	int user_get_lastread(std::string msgbase);
	void user_set_lastread(std::string msgbase, int mid);
private:
	int sec_level;
	int uid;
	Config c;
	std::string hash_sha256(std::string pass, std::string salt);
	std::string username;
};

