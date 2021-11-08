#pragma once

#include "MsgArea.h"
#include <string>
#include <vector>

class Config;
class Node;

class MsgConf {
public:
  MsgConf(std::string name, int sec_level, std::string mytagline);
  bool load(Node *n, std::string filename);
  int list_areas(Node *n, int sec);
  int list_areas_old(Node *n, int sec);
  int list_areas_fsr(Node *n, int sec);

  static int list(Node *n, int sec);
  static int list_old(Node *n, int sec);
  static int list_fsr(Node *n, int sec);
  static void scan(Node *n);
  std::string get_name() { return name; }

  int get_sec_level() { return sec_level; }
  std::vector<MsgArea> areas;

private:
  bool isloaded;
  std::string name;
  int sec_level;
  std::string tagline;
  int wwivnode;
};
