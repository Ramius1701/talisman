#pragma once
#include <string>
#include <vector>

#include "../Common/Squish.h"
#include "FileConf.h"
#include "MsgConf.h"

class Node;
class Protocol;
class Archiver;
class Logger;

struct sec_level_t {
  std::string name;
  int level;
  int time_online;
  int timeout;
  int can_delete_msgs;
  int can_delete_own_msgs;
  bool bulk_msg_allowed;
};

struct login_item_t {
  std::string command;
  std::string data;
  bool clearscreen;
  bool pauseafter;
  int seclevel;
};

struct theme_t {
  std::string name;
  std::string gfile_path;
  std::string menu_path;
  bool req_ansi;
};

class Config {
public:
  Config();
  ~Config();
  bool load(Node *n, std::string filename, Logger **log);

  struct sec_level_t *get_sec_level_info(int seclvl);

  int max_nodes() { return _max_nodes; }

  std::string gfile_path() { return themes.at(selected_theme).gfile_path; }
  std::string data_path() { return _datapath; }
  std::string menu_path() { return themes.at(selected_theme).menu_path; }
  std::string msg_path() { return _msgpath; }
  std::string tmp_path() { return _tmppath; }
  std::string sys_name() { return _sysname; }
  std::string script_path() { return _scriptpath; }
  std::string op_name() { return _opname; }

  std::string main_menu() { return _mainmenu; }

  std::string netmail_sem() { return _netmailsem; }

  std::string echomail_sem() { return _echomailsem; }

  std::string external_editor() { return _externaleditor; }

  std::string get_logpath() { return _logpath; }

  std::string qwk_id() { return _qwk_id; }

  std::string get_location() { return _location; }

  std::string get_hostname() { return _hostname; }

  int get_gopher_port() { return _gopherport; }

  std::vector<struct login_item_t> *get_login_items() { return &loginitems; }

  int new_user_sec_level() { return _new_user_sec_level; }

  bool new_user_feedback() { return _new_user_feedback; }

  const char *get_prompt_colour();

  std::vector<MsgConf *> msgconfs;
  std::vector<FileConf *> fileconfs;
  std::vector<Archiver *> archivers;
  Protocol *select_protocol(Node *n);
  int select_archiver(Node *n);
  int select_theme(Node *n, bool apply);
  std::string selected_theme_name() { return themes.at(selected_theme).name; }
  bool theme_is_valid(int t) {
    if (t >= (int)themes.size()) {
      return false;
    }
    return true;
  }

  void set_theme(int t) {
    if (theme_is_valid(t)) {
      selected_theme = t;
    }
  }

  bool theme_needs_ansi(int t) { return themes.at(t).req_ansi; }
  NETADDR *main_aka;
  static std::string convert_cp437(std::string in);
  static std::string convert_utf8(std::string in);
  static int convert_utf8(const char *input, int len, char **output);
  bool windows_echo;
private:
  int selected_theme;
  int _max_nodes;
  std::string _bg_colour;
  std::string _fg_colour;
  std::string _mainmenu;
  std::string _menupath;
  std::string _gfilepath;
  std::string _datapath;
  std::string _msgpath;
  std::string _tmppath;
  std::string _sysname;
  std::string _opname;
  std::string _netmailsem;
  std::string _echomailsem;
  std::string _externaleditor;
  std::string _logpath;
  std::string _scriptpath;
  std::string _location;
  std::string _qwk_id;
  std::string _hostname;
  bool _new_user_feedback;
  std::vector<struct sec_level_t> seclevels;
  std::vector<Protocol *> protocols;
  std::vector<struct login_item_t> loginitems;
  std::string prompt_background_ansi;
  std::vector<struct theme_t> themes;
  int _new_user_sec_level;
  int _gopherport;
};
