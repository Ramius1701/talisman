#pragma once
#include "Config.h"
#include <sqlite3.h>
#include <vector>

class Tosser {
public:
  void run();
  bool import_email(Logger *log, int to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent);
  bool import_email(Logger *log, std::string to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent);
  bool import_message(Logger *log, std::string subtype, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network,
                      time_t sent);
  bool open_user_database(Logger *log, sqlite3 **db);
  static std::vector<uint16_t> get_subscribers(std::string dpath, std::string subtype, std::string network);
  
private:
  std::string strip_hearts(std::string line);
  int check_if_subscriber(std::string subtype, std::string network, int system);
  bool add_subscriber(std::string subtype, std::string network, int system);
  bool remove_subscriber(std::string subtype, std::string network, int system);
  int calc_length(std::vector<std::string> msg);

  Config config;
  std::string _datapath;
  std::string _logpath;
  std::string _msgpath;
  std::string _tmppath;
};
