#pragma once
#include "../Common/Squish.h"
#include <filesystem>
#include <string>
#include <vector>

class Archiver;
class Logger;

struct address_conf_t {
  NETADDR *aka;
};

struct route_conf_t {
  NETADDR *aka;
  std::string route;
  std::string flavour;
};

struct link_conf_t {
  NETADDR *aka;
  NETADDR *ouraka;
  std::string flavour;
  std::string filebox;
  std::string archiver;
  std::string packetpwd;
  std::string ticpwd;
  std::string areafixpwd;
  std::string allowedgroups;
  std::filesystem::path packetpath;
  FILE *fptr;
};

struct netmail_area_conf_t {
  NETADDR *aka;
  std::string file;
};

struct area_conf_t {
  NETADDR *aka;
  std::string file;
  std::string areatag;
  char group;
  std::string thook;
  std::string shook;
  std::vector<struct link_conf_t *> links;
};

struct flink_conf_t {
  struct link_conf_t *link;
  bool forward_allowed;
  bool process_allowed;
};

struct farea_conf_t {
  NETADDR *aka;
  std::string database;
  std::string directory;
  std::string areatag;
  std::string hook;
  char group;
  std::vector<struct flink_conf_t> links;
};

class Config {
public:
  ~Config();
  bool load(std::string filename, Logger *log);
  bool load_archivers(std::string datapath, Logger *log);
  std::vector<Archiver *> archivers;
  std::vector<struct address_conf_t> addresses;
  std::vector<struct link_conf_t> links;
  std::vector<struct area_conf_t> areas;
  std::vector<struct route_conf_t> routes;
  std::vector<struct netmail_area_conf_t> netmailareas;
  std::vector<struct farea_conf_t> fileareas;
  static void sort_addr(std::vector<NETADDR *> *links);
  std::string packetdir() { return __packetdir; }

  std::string outbound() { return __outbound; }

  std::string inbound() { return __inbound; }
  std::string protinbound() { return __protinbound; }
  std::string dupebase() { return __dupebase; }

  bool bundlename_ts() { return __bundlename_ts; }

private:
  std::string __inbound;
  std::string __protinbound;
  std::string __outbound;
  std::string __packetdir;
  std::string __dupebase;
  bool __bundlename_ts;
};
