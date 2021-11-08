#ifdef _MSC_VER
#define strcasecmp _stricmp
#include <Windows.h>
#endif
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include "../Common/toml.hpp"
#include "Bulletins.h"
#include "Config.h"
#include "Node.h"

void Bulletins::display(Node *n) {
  size_t hotkeylen = 0;
  struct tm ftm;
  struct stat s;
  std::stringstream ss;
  bool found;
  int total = 0;

  if (!isloaded) {
    return;
  }

  for (size_t i = 0; i < bullets.size(); i++) {
    if (bullets.at(i).hotkey.size() > hotkeylen) {
      hotkeylen = bullets.at(i).hotkey.size();
    }
  }

  while (true) {
    n->cls();
    n->send_gfile("bulletins");
    ss.str("");
    ss << "\r\n|14View Bulletin |08[";

    for (size_t i = 0; i < bullets.size(); i++) {
      found = false;
      if (n->hasANSI) {
        std::filesystem::path b(n->get_config()->gfile_path() + "/" + bullets.at(i).file + ".ans");
        if (stat(b.u8string().c_str(), &s) == 0) {
          found = true;
        }
      }
      if (!found) {
        std::filesystem::path b(n->get_config()->gfile_path() + "/" + bullets.at(i).file + ".asc");
        if (stat(b.u8string().c_str(), &s) == 0) {
          found = true;
        }
      }

      if (found) {
        total++;
        time_t t = s.st_mtime;
        time_t now = time(NULL);
        struct tm nowtm;

#ifdef _MSC_VER
        localtime_s(&ftm, &t);
        localtime_s(&nowtm, &now);
#else
        localtime_r(&t, &ftm);
        localtime_r(&now, &nowtm);
#endif
        if (ftm.tm_year == nowtm.tm_year && ftm.tm_yday == nowtm.tm_yday) {
          n->print_f(" |15%*s |14%-48.48s |08Updated: |10Today\r\n", hotkeylen, bullets.at(i).hotkey.c_str(), bullets.at(i).name.c_str());
        } else if (ftm.tm_year == nowtm.tm_year && ftm.tm_yday == nowtm.tm_yday - 1) {
          n->print_f(" |15%*s |14%-48.48s |08Updated: |10Yesterday\r\n", hotkeylen, bullets.at(i).hotkey.c_str(), bullets.at(i).name.c_str());
        } else {
          n->print_f(" |15%*s |14%-48.48s |08Updated: |10%04d-%02d-%02d %02d:%02d\r\n", hotkeylen, bullets.at(i).hotkey.c_str(), bullets.at(i).name.c_str(),
                     ftm.tm_year + 1900, ftm.tm_mon + 1, ftm.tm_mday, ftm.tm_hour, ftm.tm_min);
        }
        ss << "|15" << bullets.at(i).hotkey;

        if (i < bullets.size() - 1) {
          ss << "|08,";
        }
      }
    }

    if (total == 0) {
      return;
    }

    ss << "|08] |15ENTER|08=|14Quit: |07";

    n->print_f("%s", ss.str().c_str());
    std::string cmd = n->get_string(hotkeylen, false);

    if (cmd.size() == 0) {
      return;
    }
    bool disp = false;
    for (size_t i = 0; i < bullets.size(); i++) {
      if (strcasecmp(cmd.c_str(), bullets.at(i).hotkey.c_str()) == 0) {
        n->cls();
        n->send_gfile(bullets.at(i).file, true);
        n->pause();
        disp = true;
        break;
      }
    }

    if (!disp) {
      return;
    }
  }
}

bool Bulletins::load(Node *n) {
  Config *c = n->get_config();
  try {
    auto data = toml::parse_file(c->data_path() + "/bulletins.toml");
    auto bullitems = data.get_as<toml::array>("bulletin");

    for (size_t i = 0; i < bullitems->size(); i++) {
      auto itemtable = bullitems->get(i)->as_table();

      std::string myname;
      std::string myfile;
      std::string myhotkey;
      int myseclevel;
      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or("Invalid Name");
      } else {
        myname = "Unknown Name";
      }
      auto file = itemtable->get("file");
      if (file != nullptr) {
        myfile = file->as_string()->value_or("");
      } else {
        myfile = "";
      }
      auto hotkey = itemtable->get("hotkey");
      if (hotkey != nullptr) {
        myhotkey = hotkey->as_string()->value_or("");
      } else {
        myhotkey = "";
      }
      auto seclevel = itemtable->get("sec_level");
      if (seclevel != nullptr) {
        myseclevel = seclevel->as_integer()->value_or(10);
      } else {
        myseclevel = 10;
      }

      if (myfile != "" && myhotkey != "") {
        struct bulletin_t b;

        b.name = myname;
        b.file = myfile;
        b.hotkey = myhotkey;
        b.seclevel = myseclevel;

        bullets.push_back(b);
      }
    }
    isloaded = true;
    return true;
  } catch (toml::parse_error const&) {
    return false;
  }
}
