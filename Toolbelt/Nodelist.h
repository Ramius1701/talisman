#pragma once

#include <string>
#include <sqlite3.h>

class Nodelist {
public:
  static bool parse(std::string domain, std::string file, std::string database);

private:
  static bool open_database(std::string filename, sqlite3 **db);
  static std::string replace_underscores(std::string in);
};
