#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#include <Windows.h>
#else
#include <iconv.h>
#endif
#include "../Common/INIReader.h"
#include "../Common/toml.hpp"
#include "Archiver.h"
#include "Config.h"
#include "FileConf.h"
#include "Protocol.h"
#include "../Common/Logger.h"

Config::Config() { prompt_background_ansi = ""; }

Config::~Config() {
  for (MsgConf *c : msgconfs) {
    delete c;
  }
  for (FileConf *c : fileconfs) {
    delete c;
  }
}

std::string Config::convert_cp437(std::string input) {
  std::string output;

#ifdef _MSC_VER
  int wchars_num = MultiByteToWideChar( CP_UTF8 , 0 , input.c_str() , -1, NULL , 0 );
  wchar_t* wstr = new wchar_t[wchars_num];
  MultiByteToWideChar( CP_UTF8 , 0 , input.c_str() , -1, wstr , wchars_num );

  int chars_num = WideCharToMultiByte(437, 0, wstr, wchars_num, NULL, 0, NULL, NULL);

  char *str = new char[chars_num + 1];
  memset(str, 0, chars_num + 1);

  WideCharToMultiByte(437, 0, wstr, wchars_num, str, chars_num, NULL, NULL);

  output = std::string(str);

  delete[] str;
  delete[] wstr;
#else
  iconv_t ic;
  ic = iconv_open("CP437//TRANSLIT", "UTF-8");
  if (ic == (iconv_t)-1) {
    return input;
  }

  char *str = new char[input.size() + 1];
  
  char *inp = (char *)input.c_str();
  size_t isz = input.size();

  char *oup = str;
  size_t osz = input.size();

  memset(str, 0, osz + 1);
  
  std::stringstream ss;

  while (iconv(ic, &inp, &isz, &oup, &osz) == -1) {
    if (errno == E2BIG) {
      ss << str;
      memset(str, 0, input.size() + 1);
      osz = input.size();
      continue;
    } else {
      output = input;
      iconv_close(ic);
      delete[] str;
      return output;
    }
  }

  ss << str;
  output = ss.str();
  iconv_close(ic);

  delete[] str;

#endif
  return output;
}

std::string Config::convert_utf8(std::string input) {
  std::string output;

#ifdef _MSC_VER
  int wchars_num = MultiByteToWideChar(437, 0, input.c_str(), -1, NULL, 0);
  wchar_t *wstr = new wchar_t[wchars_num];
  MultiByteToWideChar(437, 0, input.c_str(), -1, wstr, wchars_num);

  int chars_num = WideCharToMultiByte(CP_UTF8, 0, wstr, wchars_num, NULL, 0, NULL, NULL);

  char *str = new char[chars_num + 1];
  memset(str, 0, chars_num + 1);

  WideCharToMultiByte(CP_UTF8, 0, wstr, wchars_num, str, chars_num, NULL, NULL);

  output = std::string(str);

  delete[] str;
  delete[] wstr;
#else
  iconv_t ic;
  ic = iconv_open("UTF-8", "CP437");
  if (ic == (iconv_t)-1) {
    return input;
  }

  char *str = new char[input.size() + 1];

  char *inp = (char *)input.c_str();
  size_t isz = input.size();

  char *oup = str;
  size_t osz = input.size();

  memset(str, 0, osz + 1);

  std::stringstream ss;

  while (iconv(ic, &inp, &isz, &oup, &osz) == -1) {
    if (errno == E2BIG) {
      ss << str;
      memset(str, 0, input.size() + 1);
      osz = input.size();
      continue;
    } else {
      output = input;
      iconv_close(ic);
      delete[] str;
      return output;
    }
  }

  ss << str;
  output = ss.str();
  iconv_close(ic);

  delete[] str;

#endif
  return output;
}

bool Config::load(Node *n, std::string filename, Logger **log) {
  INIReader inir(filename);

  if (inir.ParseError() != 0) {
    return false;
  }

  _gfilepath = inir.Get("Paths", "GFile Path", "gfiles");
  _datapath = inir.Get("Paths", "Data Path", "data");
  _menupath = inir.Get("Paths", "Menu Path", "menus");
  _mainmenu = inir.Get("Main", "Root Menu", "main");
  _qwk_id = inir.Get("Main", "Qwk ID", "TALISMAN");
  _location = convert_cp437(inir.Get("Main", "Location", "Somewhere, The World"));
  _msgpath = inir.Get("Paths", "Message Path", "msgs");
  _tmppath = inir.Get("Paths", "Temp Path", "temp");
  _scriptpath = inir.Get("Paths", "Script Path", "scripts");
  _opname = convert_cp437(inir.Get("Main", "Sysop Name", "Sysop"));
  _sysname = convert_cp437(inir.Get("Main", "System Name", "Talisman"));
  _netmailsem = inir.Get("Paths", "Netmail Semaphore", "netmail.sem");
  _echomailsem = inir.Get("Paths", "Echomail Semaphore", "echomail.sem");
  _externaleditor = inir.Get("Paths", "External Editor", "");
  _logpath = inir.Get("Paths", "Log Path", "logs");
  _bg_colour = inir.Get("Main", "Input Background", "red");
  _fg_colour = inir.Get("Main", "Input Foreground", "bright white");
  _max_nodes = inir.GetInteger("Main", "Max Nodes", 4);
  _new_user_sec_level = inir.GetInteger("Main", "New User Sec Level", 10);
  _new_user_feedback = inir.GetBoolean("Main", "New User Feedback", false);
  _hostname = inir.Get("Main", "Hostname", "localhost");
  _gopherport = inir.GetInteger("Main", "Gopher Port", -1);

  main_aka = parse_fido_addr(inir.Get("Main", "Main AKA", "0:0/0").c_str());

  (*log)->load(_logpath + "/talisman.log");

  if (_hostname == "localhost") {
    (*log)->log(LOG_DEBUG, "talisman.ini hostname (under main) not set or is localhost");
  }

  struct theme_t dtheme;

  dtheme.name = "Default Theme";
  dtheme.gfile_path = _gfilepath;
  dtheme.menu_path = _menupath;
  dtheme.req_ansi = false;

  themes.push_back(dtheme);

  selected_theme = 0;

  try {
    auto data = toml::parse_file(_datapath + "/msgconfs.toml");

    auto confitems = data.get_as<toml::array>("messageconf");

    for (size_t i = 0; i < confitems->size(); i++) {
      auto itemtable = confitems->get(i)->as_table();

      std::string myname;
      std::string myconfig;
      int mysec_level;
      std::string mytagline;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = convert_cp437(name->as_string()->value_or("Invalid Name"));
      } else {
        myname = "Unknown Name";
      }
      auto conf = itemtable->get("config");
      if (conf != nullptr) {
        myconfig = conf->as_string()->value_or("");
      } else {
        myconfig = "";
      }

      auto tagline = itemtable->get("tagline");
      if (tagline != nullptr) {
        mytagline = tagline->as_string()->value_or("");
      } else {
        mytagline = "";
      }

      auto sec_level = itemtable->get("sec_level");
      if (sec_level != nullptr) {
        mysec_level = sec_level->as_integer()->value_or(10);
      } else {
        mysec_level = 10;
      }

      MsgConf *c = new MsgConf(myname, mysec_level, mytagline);

      if (c->load(n, myconfig)) {
        msgconfs.push_back(c);
      }
    }
  } catch (toml::parse_error const& p) {
    (*log)->log(LOG_ERROR, "Error parsing %s/msgconfs.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
    (*log)->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }
  try {
    auto data2 = toml::parse_file(_datapath + "/seclevels.toml");

    auto secitems = data2.get_as<toml::array>("seclevel");

    for (size_t i = 0; i < secitems->size(); i++) {
      auto itemtable = secitems->get(i)->as_table();

      std::string myname;
      int mysec_level;
      int mytimeonline;
      int mytimeout;
      bool my_can_delete_msgs;
      bool my_can_delete_own_msgs;
      bool mybulk;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = convert_cp437(name->as_string()->value_or("Invalid Name"));
      } else {
        myname = "Unknown Name";
      }
      auto seclvl = itemtable->get("sec_level");
      if (seclvl != nullptr) {
        mysec_level = seclvl->as_integer()->value_or(0);
      } else {
        mysec_level = 0;
      }

      auto timeon = itemtable->get("mins_per_day");
      if (timeon != nullptr) {
        mytimeonline = timeon->as_integer()->value_or(0);
      } else {
        mytimeonline = 0;
      }
      auto timeout = itemtable->get("timeout_mins");
      if (timeout != nullptr) {
        mytimeout = timeout->as_integer()->value_or(0);
      } else {
        mytimeout = 0;
      }

      auto bulk = itemtable->get("bulk_msg_allowed");
      if (bulk != nullptr) {
        mybulk = bulk->as_boolean()->value_or(false);
      } else {
        mybulk = false;
      }

      auto can_delete_msgs = itemtable->get("can_delete_msgs");
      if (can_delete_msgs != nullptr) {
        my_can_delete_msgs = can_delete_msgs->as_boolean()->value_or(false);
      } else {
        my_can_delete_msgs = false;
      }

      auto can_delete_own_msgs = itemtable->get("can_delete_own_msgs");
      if (can_delete_own_msgs != nullptr) {
        my_can_delete_own_msgs = can_delete_own_msgs->as_boolean()->value_or(false);
      } else {
        my_can_delete_own_msgs = false;
      }

      if (mysec_level != 0) {
        struct sec_level_t slvl;
        slvl.level = mysec_level;
        slvl.name = myname;
        slvl.timeout = mytimeout;
        slvl.time_online = mytimeonline;
        slvl.bulk_msg_allowed = mybulk;
        slvl.can_delete_msgs = my_can_delete_msgs;
        slvl.can_delete_own_msgs = my_can_delete_own_msgs;
        seclevels.push_back(slvl);
      }
    }
  } catch (toml::parse_error const& p) {
    (*log)->log(LOG_ERROR, "Error parsing %s/seclevels.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
    (*log)->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }

  try {
    auto data2 = toml::parse_file(_datapath + "/loginitems.toml");

    auto loginitemst = data2.get_as<toml::array>("loginitem");

    for (size_t i = 0; i < loginitemst->size(); i++) {
      auto itemtable = loginitemst->get(i)->as_table();

      std::string mycommand;
      std::string mydata;
      bool myclearscreen;
      bool mypauseafter;
      int mysec_level;

      auto cmd = itemtable->get("command");
      if (cmd != nullptr) {
        mycommand = cmd->as_string()->value_or("");
      } else {
        mycommand = "";
      }

      auto data = itemtable->get("data");
      if (data != nullptr) {
        mydata = data->as_string()->value_or("");
      } else {
        mydata = "";
      }

      auto clearscreen = itemtable->get("clear_screen");
      if (clearscreen != nullptr) {
        myclearscreen = clearscreen->as_boolean()->value_or(false);
      } else {
        myclearscreen = false;
      }

      auto pauseafter = itemtable->get("pause_after");
      if (pauseafter != nullptr) {
        mypauseafter = pauseafter->as_boolean()->value_or(false);
      } else {
        mypauseafter = false;
      }

      auto seclevel = itemtable->get("sec_level");
      if (seclevel != nullptr) {
        mysec_level = seclevel->as_integer()->value_or(0);
      } else {
        mysec_level = 0;
      }

      if (mycommand != "") {
        struct login_item_t litm;
        litm.command = mycommand;
        litm.data = mydata;
        litm.clearscreen = myclearscreen;
        litm.pauseafter = mypauseafter;
        litm.seclevel = mysec_level;

        loginitems.push_back(litm);
      }
    }
  } catch (toml::parse_error const &p) {
    (*log)->log(LOG_ERROR, "Error parsing %s/loginitems.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
    (*log)->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }
  try {
    auto data3 = toml::parse_file(_datapath + "/protocols.toml");

    auto protitems = data3.get_as<toml::array>("protocol");

    for (size_t i = 0; i < protitems->size(); i++) {
      auto itemtable = protitems->get(i)->as_table();

      std::string myname;
      std::string myul_cmd;
      std::string mydl_cmd;
      std::string myssh_ul_cmd;
      std::string myssh_dl_cmd;
      bool mybatch;
      bool myprompt;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = convert_cp437(name->as_string()->value_or("Invalid Name"));
      } else {
        myname = "Unknown";
      }
      auto ul_cmd = itemtable->get("upload_command");
      if (ul_cmd != nullptr) {
        myul_cmd = ul_cmd->as_string()->value_or("");
      } else {
        myul_cmd = "";
      }
      auto dl_cmd = itemtable->get("download_command");
      if (dl_cmd != nullptr) {
        mydl_cmd = dl_cmd->as_string()->value_or("");
      } else {
        mydl_cmd = "";
      }
      auto ssh_ul_cmd = itemtable->get("ssh_upload_command");
      if (ssh_ul_cmd != nullptr) {
        myssh_ul_cmd = ssh_ul_cmd->as_string()->value_or(myul_cmd);
      } else {
        myssh_ul_cmd = myul_cmd;
      }
      auto ssh_dl_cmd = itemtable->get("ssh_download_command");
      if (ssh_dl_cmd != nullptr) {
        myssh_dl_cmd = ssh_dl_cmd->as_string()->value_or(mydl_cmd);
      } else {
        myssh_dl_cmd = mydl_cmd;
      }
      auto batch = itemtable->get("batch");
      if (batch != nullptr) {
        mybatch = batch->as_boolean()->value_or(false);
      } else {
        mybatch = false;
      }
      auto prompt = itemtable->get("prompt");
      if (prompt != nullptr) {
        myprompt = prompt->as_boolean()->value_or(true);
      } else {
        myprompt = true;
      }

      Protocol *p = new Protocol(myname, mydl_cmd, myssh_dl_cmd, myul_cmd, myssh_ul_cmd, mybatch, myprompt);
      protocols.push_back(p);
    }
  } catch (toml::parse_error const& p) {
    (*log)->log(LOG_ERROR, "Error parsing %s/protocols.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
    (*log)->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }
  try {
    auto data3 = toml::parse_file(_datapath + "/archivers.toml");

    auto arcitems = data3.get_as<toml::array>("archiver");

    for (size_t i = 0; i < arcitems->size(); i++) {
      auto itemtable = arcitems->get(i)->as_table();

      std::string myname;
      std::string myext;
      std::string myunarc;
      std::string myarc;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = convert_cp437(name->as_string()->value_or("Invalid Name"));
      } else {
        myname = "Unknown";
      }

      auto ext = itemtable->get("extension");
      if (ext != nullptr) {
        myext = ext->as_string()->value_or("");
      } else {
        myext = "";
      }

      auto unarc = itemtable->get("unarc");
      if (unarc != nullptr) {
        myunarc = unarc->as_string()->value_or("");
      } else {
        myunarc = "";
      }
      auto arc = itemtable->get("arc");
      if (arc != nullptr) {
        myarc = arc->as_string()->value_or("");
      } else {
        myarc = "";
      }
      Archiver *a = new Archiver(myname, myext, myunarc, myarc);
      archivers.push_back(a);
    }
  } catch (toml::parse_error const& p) {
    (*log)->log(LOG_ERROR, "Error parsing %s/archivers.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
    (*log)->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }
  try {
    auto data = toml::parse_file(_datapath + "/fileconfs.toml");

    auto confitems = data.get_as<toml::array>("fileconf");

    for (size_t i = 0; i < confitems->size(); i++) {
      auto itemtable = confitems->get(i)->as_table();

      std::string myname;
      std::string myconfig;
      int mysec_level;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = convert_cp437(name->as_string()->value_or("Invalid Name"));
      } else {
        myname = "Unknown Name";
      }
      auto conf = itemtable->get("config");
      if (conf != nullptr) {
        myconfig = conf->as_string()->value_or("");
      } else {
        myconfig = "";
      }

      auto sec_level = itemtable->get("sec_level");
      if (sec_level != nullptr) {
        mysec_level = sec_level->as_integer()->value_or(10);
      } else {
        mysec_level = 10;
      }

      FileConf *f = new FileConf(myname, myconfig, mysec_level);

      if (f->load(n)) {
        fileconfs.push_back(f);
      }
    }
  } catch (toml::parse_error const &p) {
    (*log)->log(LOG_ERROR, "Error parsing %s/fileconfs.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
    (*log)->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }

  if (std::filesystem::exists(std::filesystem::path(_datapath + "/themes.toml"))) {

    try {
      auto data = toml::parse_file(_datapath + "/themes.toml");

      auto themeitems = data.get_as<toml::array>("theme");

      for (size_t i = 0; i < themeitems->size(); i++) {
        auto itemtable = themeitems->get(i)->as_table();

        std::string myname;
        std::string mygfiles;
        std::string mymenus;
        bool myansi;

        auto name = itemtable->get("name");
        if (name != nullptr) {
          myname = convert_cp437(name->as_string()->value_or("Theme " + std::to_string(i + 1)));
        } else {
          myname = "Theme " + std::to_string(i + 1);
        }

        auto gfiles = itemtable->get("gfile_path");
        if (gfiles != nullptr) {
          mygfiles = gfiles->as_string()->value_or(_gfilepath);
        } else {
          mygfiles = _gfilepath;
        }

        auto menus = itemtable->get("menu_path");
        if (menus != nullptr) {
          mymenus = menus->as_string()->value_or(_menupath);
        } else {
          mymenus = _menupath;
        }

        auto ansi = itemtable->get("req_ansi");
        if (ansi != nullptr) {
          myansi = ansi->as_boolean()->value_or(false);
        } else {
          myansi = false;
        }

        struct theme_t theme;

        theme.name = myname;
        theme.gfile_path = mygfiles;
        theme.menu_path = mymenus;
        theme.req_ansi = myansi;
        themes.push_back(theme);
      }
    } catch (toml::parse_error const& p) {
      (*log)->log(LOG_ERROR, "Error parsing %s/themes.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
      (*log)->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
      return false;
    }
  }
  return true;
}

struct sec_level_t *Config::get_sec_level_info(int seclvl) {
  for (size_t i = 0; i < seclevels.size(); i++) {
    if (seclevels.at(i).level == seclvl) {
      return &seclevels.at(i);
    }
  }
  return NULL;
}

Protocol *Config::select_protocol(Node *n) {
  n->print_f("|14Available Protocols\r\n");
  n->print_f("|08----------------------------------------\r\n");
  for (size_t i = 0; i < protocols.size(); i++) {
    n->print_f("|15%2d|08. |14%s\r\n", i + 1, protocols.at(i)->get_name().c_str());
  }
  n->print_f("|15 Q|08. |14Quit\r\n");
  n->print_f("|08----------------------------------------\r\n");
  std::string res = n->get_string(2, false);
  if (res.size() > 0) {
    if (tolower(res.at(0)) == 'q') {
      return nullptr;
    }
    try {
      size_t prot = (size_t)stoi(res);
      if (prot > 0 && prot <= protocols.size()) {
        return protocols.at(prot - 1);
      }
    } catch (std::invalid_argument const&) {

    } catch (std::out_of_range const&) {
    }
  }
  return nullptr;
}

int Config::select_theme(Node *n, bool apply) {
  n->print_f("|14Available Themes\r\n");
  n->print_f("|08----------------------------------------\r\n");
  for (size_t i = 0; i < themes.size(); i++) {
    n->print_f("|15%2d|08. |14%s %s\r\n", i + 1, themes.at(i).name.c_str(), (themes.at(i).req_ansi ? "(Req. ANSI)" : ""));
  }
  n->print_f("|15 Q|08. |14Quit\r\n");
  n->print_f("|08----------------------------------------\r\n");
  std::string res = n->get_string(2, false);
  if (res.size() > 0) {
    if (tolower(res.at(0)) == 'q') {
      return -1;
    }
    try {
      size_t theme = (size_t)stoi(res);
      if (theme > 0 && theme <= themes.size()) {
        n->get_user().set_attribute("theme", std::to_string(theme - 1));

        if (apply) {
          selected_theme = theme - 1;
        }
        return theme - 1;
      }
    } catch (std::invalid_argument const&) {

    } catch (std::out_of_range const&) {
    }
  }
  return -1;
}

int Config::select_archiver(Node *n) {
  n->print_f("|14Available Archivers\r\n");
  n->print_f("|08----------------------------------------\r\n");
  for (size_t i = 0; i < archivers.size(); i++) {
    n->print_f("|15%2d|08. |14%s\r\n", i + 1, archivers.at(i)->name.c_str());
  }
  n->print_f("|15 Q|08. |14Quit\r\n");
  n->print_f("|08----------------------------------------\r\n");
  std::string res = n->get_string(2, false);
  if (res.size() > 0) {
    if (tolower(res.at(0)) == 'q') {
      return -1;
    }
    try {
      size_t arc = (size_t)stoi(res);
      if (arc > 0 && arc <= archivers.size()) {
        return arc - 1;
      }
    } catch (std::invalid_argument const&) {

    } catch (std::out_of_range const&) {
    }
  }
  return -1;
}

const char *Config::get_prompt_colour() {
  std::stringstream ss;

  ss << "\x1b[";

  if (prompt_background_ansi != "")
    return prompt_background_ansi.c_str();

  if (strcasecmp(_fg_colour.c_str(), "black") == 0) {
    ss << "0;30;";
  }
  if (strcasecmp(_fg_colour.c_str(), "red") == 0) {
    ss << "0;31;";
  }
  if (strcasecmp(_fg_colour.c_str(), "green") == 0) {
    ss << "0;32;";
  }
  if (strcasecmp(_fg_colour.c_str(), "brown") == 0) {
    ss << "0;33;";
  }
  if (strcasecmp(_fg_colour.c_str(), "blue") == 0) {
    ss << "0;34;";
  }
  if (strcasecmp(_fg_colour.c_str(), "magenta") == 0) {
    ss << "0;35;";
  }
  if (strcasecmp(_fg_colour.c_str(), "cyan") == 0) {
    ss << "0;36;";
  }
  if (strcasecmp(_fg_colour.c_str(), "white") == 0) {
    ss << "0;37;";
  }

  if (strcasecmp(_fg_colour.c_str(), "bright black") == 0) {
    ss << "1;30;";
  }
  if (strcasecmp(_fg_colour.c_str(), "bright red") == 0) {
    ss << "1;31;";
  }
  if (strcasecmp(_fg_colour.c_str(), "bright green") == 0) {
    ss << "1;32;";
  }
  if (strcasecmp(_fg_colour.c_str(), "bright brown") == 0) {
    ss << "1;33;";
  }
  if (strcasecmp(_fg_colour.c_str(), "bright blue") == 0) {
    ss << "1;34;";
  }
  if (strcasecmp(_fg_colour.c_str(), "bright magenta") == 0) {
    ss << "1;35;";
  }
  if (strcasecmp(_fg_colour.c_str(), "bright cyan") == 0) {
    ss << "1;36;";
  }
  if (strcasecmp(_fg_colour.c_str(), "bright white") == 0) {
    ss << "1;37;";
  }

  if (strcasecmp(_bg_colour.c_str(), "black") == 0) {
    ss << "40m";
  }
  if (strcasecmp(_bg_colour.c_str(), "red") == 0) {
    ss << "41m";
  }
  if (strcasecmp(_bg_colour.c_str(), "green") == 0) {
    ss << "42m";
  }
  if (strcasecmp(_bg_colour.c_str(), "brown") == 0) {
    ss << "43m";
  }
  if (strcasecmp(_bg_colour.c_str(), "blue") == 0) {
    ss << "44m";
  }
  if (strcasecmp(_bg_colour.c_str(), "magenta") == 0) {
    ss << "45m";
  }
  if (strcasecmp(_bg_colour.c_str(), "cyan") == 0) {
    ss << "46m";
  }
  if (strcasecmp(_bg_colour.c_str(), "white") == 0) {
    ss << "47m";
  }

  prompt_background_ansi = ss.str();

  return prompt_background_ansi.c_str();
}
