#pragma once

#include "../Common/Logger.h"
#include "../Common/Squish.h"
#include <filesystem>

class Config;

class Tosser {
public:
  bool run(bool protinbound);
  NETADDR *get_echomail_addr(std::string ctrlbody, std::string msgbody);

private:
  unsigned long pid;
  Logger log;

  void filefix(Config *c, sq_msg_t *msg);
  void areafix(Config *c, sq_msg_t *msg);
  bool update(std::string tag, std::string links, bool filearea);
  std::string get_msgid(std::string ctrlbody);
  std::filesystem::path tempdir;
  std::string _datapath;
  std::string _logpath;
  std::string _msgpath;
  std::string _tmppath;
};
