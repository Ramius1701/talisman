#pragma once

#include <sqlite3.h>
#include <string>

class User {
public:
  static std::string hash_sha256(std::string pass, std::string salt);
  static bool update_password(std::string data_path, std::string username, std::string newpass);
  static bool set_attribute(std::string datapath, std::string username, std::string attrib, std::string value);
  static bool clear_lastread(std::string datapath, std::string msgbase, std::string username);
  static bool delete_user(std::string datapath, std::string username);

private:
  static bool open_database(std::string filename, sqlite3 **db);
  static bool open_email_database(std::string filename, sqlite3 **db);
  static bool open_gopher_database(std::string filename, sqlite3 **db);
  static int get_uid(std::string datapath, std::string username);
};
