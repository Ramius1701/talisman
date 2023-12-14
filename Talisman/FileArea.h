#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
class Node;

struct file_list_t {
  std::string filename;
  size_t filesize = 0;
  int dlcount = 0;
  time_t uldate = 0;
  bool missing = false;
  std::string ulname;
  std::vector<std::string> desc;
  int order;
};

class FileArea {
public:
  FileArea(std::string name, std::string file_path, std::string database, int dl_sec_level, int ul_sec_level, int vis_sec_level, int del_sec_level) {
    this->name = name;
    this->file_path = file_path;
    this->database = database;
    this->dl_sec_level = dl_sec_level;
    this->ul_sec_level = ul_sec_level;
    this->vis_sec_level = vis_sec_level;
    this->del_sec_level = del_sec_level;
  }
  int get_d_sec_level() { return dl_sec_level; }
  int get_u_sec_level() { return ul_sec_level; }
  int get_v_sec_level() { return vis_sec_level; }

  std::string get_name() { return name; }

  int get_total_files(Node *n);
  bool list_files(Node *n);
  bool list_files(Node *n, time_t date);
  bool list_files(Node *n, time_t date, std::vector<std::string> *keywords);
  bool list_files(Node *n, time_t date, std::vector<std::string> *keywords, bool cancel);

  void inc_download_count(Node *n, std::string filename);
  bool upload_file(Node *n);
  bool file_exists(Node *n, std::string filename);
  bool insert_file(Node *n, std::string filename, std::vector<std::string> descr);
  bool delete_file(Node *n, std::string filename);

private:
  bool do_list(Node *n, std::vector<struct file_list_t> *filelist, bool cancel);
  bool do_list_fsr(Node *n, std::vector<struct file_list_t> *filelist, bool cancel);
  bool open_database(std::string filename, sqlite3 **db);
  std::string name;
  std::string file_path;
  std::string database;
  int dl_sec_level;
  int vis_sec_level;
  int ul_sec_level;
  int del_sec_level;
};
