#pragma once

#include "FileArea.h"

class Node;

class FileConf {
public:
  FileConf(std::string name, std::string config_file, int sec_level) {
    this->name = name;
    this->config_file = config_file;
    this->sec_level = sec_level;
  }

  ~FileConf();

  bool load(Node *n);
  int get_sec_level() { return sec_level; }
  std::string get_name() { return name; }
  int list_areas(Node *n, int sec);
  int list_areas_old(Node *n, int sec);
  int list_areas_fsr(Node *n, int sec);
  static int list(Node *n, int sec);
  static int list_old(Node *n, int sec);
  static int list_fsr(Node *n, int sec);
  std::vector<FileArea *> areas;

private:
  std::string name;
  std::string config_file;
  int sec_level;
};
