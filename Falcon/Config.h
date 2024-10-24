#pragma once
#include <string>
#include <vector>

class Logger;

struct network_t {
  std::string name;
  std::string outbox;
  std::string emailbase;
  int mynode;
  int upnode;
};

struct area_t {
  std::string netname;
  std::string basefile;
  std::string subtype;
  bool manual_subsciption;
  int mynode;
  int hostnode;
};

class Config {
public:
  bool load(std::string datapath, Logger *log);
  std::vector<struct network_t> networks;
  std::vector<struct area_t> areas;

  std::string inbound() { return __inbound; }
  bool striphearts() { return _striphearts; }

private:
  bool _striphearts;
  std::string __inbound;
};
