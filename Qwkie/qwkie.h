#pragma once

#include <string>
#include <vector>

class Logger;

struct area_t {
  std::string msgarea;
  int qwkbaseno;
};

struct network_t {
  std::string name;
  std::string qwkid;
  std::string myqwkid;
  std::string ftpserver;
  std::string archiver;
  int port;
  std::string password;
  std::string tagline;
  std::string override_out;
  std::string override_in;
  std::vector<struct area_t> areas;
};

class Archiver;

class Qwkie {
public:
  bool scan(int net);
  bool scan(std::string network);
  bool scanall();
  bool toss(int net);
  bool toss(std::string network);
  bool tossall();
  bool poll(int net);
  bool poll(std::string network);
  bool pollall();
  bool loadConfig(std::string datapath, std::string msgpath, std::string temppath, Logger *log);
  ~Qwkie();

private:
  bool load_archivers(Logger *log);
  std::string msgpath;
  std::string datapath;
  std::string temppath;
  Logger *log;
  std::vector<struct network_t> networks;
  std::vector<Archiver *> archivers;
};
