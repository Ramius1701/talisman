#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#include "Archiver.h"
#include "CallLog.h"
#include "FileArea.h"
#include "Node.h"
#include "Protocol.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <algorithm>
#include <string>
#include <sys/stat.h>
#include <vector>

bool FileArea::open_database(std::string filename, sqlite3 **db) {
  static const char *create_sql = "CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, filesize INTEGER, dlcount INTEGER, uldate INTEGER, "
                                  "ulname TEXT, descr TEXT COLLATE NOCASE);";
  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
    // std::cerr << "Unable to open database: users.db" << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create file table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

bool FileArea::delete_file(Node *n, std::string filename) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char *sql = "DELETE FROM files WHERE filename = ?";

  if (std::filesystem::exists(filename)) {
    std::filesystem::remove(filename);
  }
  if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, NULL);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return true;
}

bool FileArea::insert_file(Node *n, std::string filename, std::vector<std::string> descr) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  std::stringstream desc;
  std::string ddesc;
  std::string uname;
  struct stat s;
  time_t now = time(NULL);
  bool ret = false;

  for (size_t i = 0; i < descr.size(); i++) {
    desc << descr.at(i) << "\n";
  }
  ddesc = desc.str();
  uname = n->get_user().get_username();
  if (stat(filename.c_str(), &s) != 0) {
    return false;
  }

  static const char *sql = "INSERT INTO files (filename, filesize, dlcount, uldate, ulname, descr) VALUES(?,?,0,?,?,?)";

  if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }
  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, NULL);
  sqlite3_bind_int64(stmt, 2, s.st_size);
  sqlite3_bind_int64(stmt, 3, now);
  sqlite3_bind_text(stmt, 4, uname.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 5, ddesc.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_DONE) {
    ret = true;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ret;
}

void FileArea::inc_download_count(Node *n, std::string filename) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int dlcount = 0;
  static const char sql[] = "SELECT dlcount FROM files WHERE filename = ?";
  static const char sql2[] = "UPDATE files SET dlcount = ? WHERE filename = ?";
  if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
    return;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, NULL);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    dlcount = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    if (sqlite3_prepare_v2(db, sql2, strlen(sql2), &stmt, NULL) != SQLITE_OK) {
      sqlite3_close(db);
      return;
    }
    dlcount++;

    sqlite3_bind_int(stmt, 1, dlcount);
    sqlite3_bind_text(stmt, 2, filename.c_str(), -1, NULL);
    sqlite3_step(stmt);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

int FileArea::get_total_files(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int ret = 0;
  static const char sql[] = "SELECT COUNT(*) FROM files";

  if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ret = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ret;
}

bool FileArea::file_exists(Node *n, std::string filename) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  bool ret;
#ifdef _MSC_VER
  static const char sql[] = "SELECT filename FROM files WHERE filename = ? COLLATE NOCASE";
#else
  static const char sql[] = "SELECT filename FROM files WHERE filename = ?";
#endif
  std::filesystem::path p(file_path);
  p.append(filename);
  std::string fullpath = p.u8string();

  if (std::filesystem::exists(fullpath)) {
    return true;
  }

  if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
    std::cerr << "Error opening file database" << std::endl;

    return true;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    std::cerr << "Error preparing statement" << std::endl;
    return true;
  }

  sqlite3_bind_text(stmt, 1, fullpath.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ret = true;
  } else {
    ret = false;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ret;
}

bool FileArea::list_files(Node *n) { return list_files(n, 0, nullptr, false); }

bool FileArea::list_files(Node *n, time_t date) { return list_files(n, date, nullptr, false); }

bool FileArea::list_files(Node *n, time_t date, std::vector<std::string> *keywords) { return list_files(n, date, keywords, false); }

bool FileArea::list_files(Node *n, time_t date, std::vector<std::string> *keywords, bool cancel) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  std::vector<file_list_t> filelist;

  struct stat s;
  static const char sql[] = "SELECT filename, filesize, dlcount, uldate, ulname, descr FROM files ORDER BY uldate DESC";
  static const char sql2[] = "SELECT filename, filesize, dlcount, uldate, ulname, descr FROM files WHERE uldate > ? ORDER BY uldate DESC";
  std::stringstream sql3;

  if (!open_database(n->get_config()->data_path() + "/" + database + ".sqlite3", &db)) {
    return false;
  }

  if (date > 0) {
    if (sqlite3_prepare_v2(db, sql2, strlen(sql2), &stmt, NULL) != SQLITE_OK) {
      sqlite3_close(db);
      return false;
    }
    sqlite3_bind_int64(stmt, 1, date);
  } else {
    if (keywords != nullptr) {
      sql3 << "SELECT filename, filesize, dlcount, uldate, ulname, descr FROM files WHERE";
      sql3 << " descr LIKE ?";
      for (size_t i = 1; i < keywords->size(); i++) {
        sql3 << " OR descr LIKE ?";
      }
      sql3 << " ORDER BY uldate DESC";
      std::string ssql3 = sql3.str();
      if (sqlite3_prepare_v2(db, ssql3.c_str(), ssql3.size(), &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
      }
      for (size_t i = 0; i < keywords->size(); i++) {
        std::string kw = "%" + keywords->at(i) + "%";
        sqlite3_bind_text(stmt, i + 1, kw.c_str(), -1, NULL);
      }
    } else if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
      sqlite3_close(db);
      return false;
    }
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    struct file_list_t f;

    f.filename = std::string((const char *)sqlite3_column_text(stmt, 0));
    f.filesize = sqlite3_column_int(stmt, 1);
    f.dlcount = sqlite3_column_int(stmt, 2);
    f.uldate = sqlite3_column_int64(stmt, 3);
    f.ulname = std::string((const char *)sqlite3_column_text(stmt, 4));

    if (stat(f.filename.c_str(), &s) != 0 || s.st_size == 0) {
      f.missing = true;
    } else {
      f.missing = false;
    }

    std::string descr((const char *)sqlite3_column_text(stmt, 5));
    std::stringstream ss;

    for (size_t i = 0; i < descr.size(); i++) {
      if (descr.at(i) == '\n') {
        f.desc.push_back(ss.str());
        ss.str("");
      } else if (descr.at(i) == '\x1b') {
        i++;
        while (std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz").find(descr.at(i)) == std::string::npos) i++;
      } else if (descr.at(i) != '\r') {
        ss << descr.at(i);
      }
    }
    if (ss.str().size() > 0) {
      f.desc.push_back(ss.str());
    }
    filelist.push_back(f);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  bool fsr = (n->get_user().get_attribute("fullscreenreader", "true") == "true" && n->hasANSI);
  if (fsr) {
    return do_list_fsr(n, &filelist, cancel);
  } else {
    return do_list(n, &filelist, cancel);
  }
}

bool sort_by_alpha(struct file_list_t f1, struct file_list_t f2)
{
    std::filesystem::path fpath1 = std::filesystem::path(f1.filename);
    std::filesystem::path fpath2 = std::filesystem::path(f2.filename);

    return (strcasecmp(fpath1.u8string().c_str(), fpath2.u8string().c_str()) <= 0);
}

bool sort_by_date(struct file_list_t f1, struct file_list_t f2)
{
    return (f1.uldate < f2.uldate);
}

bool FileArea::do_list_fsr(Node *n, std::vector<struct file_list_t> *filelist, bool cancel) {
  int unit;
  bool tagged = false;
  int start = 0;
  int selected = 0;
  // bool redraw = true;
  bool sortby = true;
  static const char units[] = " KMGT";

  if (filelist->size() == 0)
    return false;

  n->cls();
  n->print_f("\x1b[1;1H");
  n->print_f("%s", n->get_config()->get_prompt_colour());
  n->print_f("File Area: %s", name.c_str());
  n->print_f("\x1b[K");

  n->print_f("\x1b[%d;1H", n->get_term_height() - 1);
  n->print_f("%s", n->get_config()->get_prompt_colour());
  if (cancel) {
    n->print_f("Up/Down: Scroll, SPACE to tag, S to Sort, Q to quit, X to cancel scan");
  } else {
    n->print_f("Up/Down: Scroll, SPACE to tag, S to Sort, Q to quit.");
  }
  n->print_f("\x1b[K");

  n->print_f("\x1b[0;37;40m");

  while (true) {

    for (size_t i = start; i < start + n->get_term_height() - 5; i++) {
      if (i < filelist->size()) {
        int fsz = filelist->at(i).filesize;
        for (unit = 0; unit < 5; unit++) {
          if (fsz >= 1024) {
            fsz /= 1024;
          } else {
            break;
          }
        }

        tagged = false;

        for (size_t j = 0; j < n->tagged_files.size(); j++) {
          if (n->tagged_files.at(j).filename == filelist->at(i).filename) {
            tagged = true;
            break;
          }
        }

        n->print_f("\x1b[%d;%dH", i - start + 3, 2);
        if ((int)i == selected) {
          n->print_f("%s", n->get_config()->get_prompt_colour());
        } else {
          n->print_f("|16|07");
        }

        std::filesystem::path p(filelist->at(i).filename);

        if (filelist->at(i).missing) {
          n->print_f(" %-20.20s MISSING", p.filename().u8string().c_str());
        } else {
          if (tagged) {
            n->print_f("*%-20.20s %5d%c ", p.filename().u8string().c_str(), fsz, units[unit]);
          } else {
            n->print_f(" %-20.20s %5d%c ", p.filename().u8string().c_str(), fsz, units[unit]);
          }
        }
        n->print_f("\x1b[0;37;40m");
      } else {
        n->print_f("\x1b[%d;%dH", i - start + 3, 2);
        n->print_f("                            ");
      }
    }

    n->print_f("\x1b[3;34H");
    n->print_f("|14Uploaded By|08: |15%-16.16s", filelist->at(selected).ulname.c_str());
    n->print_f("\x1b[3;64H");

    struct tm f_tm;

#ifdef _MSC_VER
    localtime_s(&f_tm, &filelist->at(selected).uldate);
#else
    localtime_r(&filelist->at(selected).uldate, &f_tm);
#endif
    n->print_f("|14On|08: |15%04d/%02d/%02d", f_tm.tm_year + 1900, f_tm.tm_mon + 1, f_tm.tm_mday);
    n->print_f("\x1b[4;35H");
    n->print_f("|14Downloaded|08: |15%d times.", filelist->at(selected).dlcount);

    for (size_t i = 0; i < n->get_term_height() - 8; i++) {
      n->print_f("\x1b[%d;34H", i + 6);
      if (i < filelist->at(selected).desc.size()) {
        n->print_f("%-44.44s\x1b[K", filelist->at(selected).desc.at(i).c_str());
      } else {
        n->print_f("\x1b[K");
      }
    }

    char c = n->getch();
    if (c == '\x1b') {
      c = n->getch();
      if (c == '[') {
        c = n->getch();
        if (c == 'A') { // up
          if (selected > 0) {
            selected--;
            if (selected < start) {
              start = selected - (n->get_term_height() - 6);
              if (start < 0)
                start = 0;
            }
          }
        } else if (c == 'B') {
          if (selected < (int)filelist->size() - 1) {
            selected++;
            if (selected > start + ((int)n->get_term_height() - 6)) {
              start = selected;
            }
          }
        }
      }
    } else if (c == ' ') {
      // tag file
      if (!filelist->at(selected).missing && dl_sec_level <= n->get_user().get_sec_level()) {
        tagged = false;
        for (size_t j = 0; j < n->tagged_files.size(); j++) {
          if (n->tagged_files.at(j).filename == filelist->at(selected).filename) {
            n->tagged_files.erase(n->tagged_files.begin() + j);
            tagged = true;
            break;
          }
        }
        if (tagged == false) {
          n->tag_file(filelist->at(selected).filename, this);
        }
      }
    } else if (c == 'd' || c == 'D') {
      if (del_sec_level != -1 && del_sec_level <= n->get_user().get_sec_level()) {
        delete_file(n, filelist->at(selected).filename);
      }
    } else if (c == 'Q' || c == 'q') {
      return false;
    } else if (c == 'S' || c == 's') {
      if (sortby) {
        // sort by alpha
        std::sort(filelist->begin(), filelist->end(), sort_by_alpha);
      } else {
        // sort by uldate
        std::sort(filelist->begin(), filelist->end(), sort_by_date);
      }
      selected = 0;
      start = 0;
      sortby = !sortby;
    } else if (cancel && (c == 'X' || c == 'x')) {
      return true;
    }
  }
}

bool FileArea::do_list(Node *n, std::vector<struct file_list_t> *filelist, bool cancel) {
  static const char units[] = " KMGT";
  int lines = 0;
  for (size_t i = 0; i < filelist->size(); i++) {
    int unit;
    bool tagged = false;
    for (unit = 0; unit < 5; unit++) {
      if (filelist->at(i).filesize >= 1024) {
        filelist->at(i).filesize /= 1024;
      } else {
        break;
      }
    }
    std::filesystem::path p(filelist->at(i).filename);

    for (size_t j = 0; j < n->tagged_files.size(); j++) {
      if (n->tagged_files.at(j).filename == filelist->at(i).filename) {
        tagged = true;
        break;
      }
    }

    if (filelist->at(i).desc.size() > 0) {
      if (tagged) {
        n->print_f("|14%4d.|10*|15%-16.16s |13%5d%cb |11%4d |07%s\r\n", i + 1, p.filename().u8string().c_str(), filelist->at(i).filesize, units[unit],
                   filelist->at(i).dlcount, filelist->at(i).desc.at(0).substr(0, n->get_term_width() - 37).c_str());
      } else {
        if (filelist->at(i).missing) {
          n->print_f("|14%4d. |15%-16.16s |12MISSING |11%4d |07%s\r\n", i + 1, p.filename().u8string().c_str(), filelist->at(i).dlcount,
                     filelist->at(i).desc.at(0).substr(0, n->get_term_width() - 37).c_str());
        } else {
          n->print_f("|14%4d. |15%-16.16s |13%5d%cb |11%4d |07%s\r\n", i + 1, p.filename().u8string().c_str(), filelist->at(i).filesize, units[unit],
                     filelist->at(i).dlcount, filelist->at(i).desc.at(0).substr(0, n->get_term_width() - 37).c_str());
        }
      }
      lines++;
      for (size_t z = 1; z < filelist->at(i).desc.size(); z++) {
        if (lines == (int)n->get_term_height() - 2) {
          if (cancel) {
            n->print_f("|08[|151|08-|15%d|08] |14Tag File, |15Q|08=|14Quit|08, |15X|08=|14Cancel Scan|08, |15ENTER|08=|14Continue: ", filelist->size());
          } else {
            n->print_f("|08[|151|08-|15%d|08] |14Tag File, |15Q|08=|14Quit|08, |15ENTER|08=|14Continue: ", filelist->size());
          }
          std::string res = n->get_string(5, false);
          if (res.size() > 0) {
            if (tolower(res.at(0) == 'q')) {
              return false;
            } else if (cancel && tolower(res.at(0)) == 'x') {
              return true;
            }
            size_t ftag;
            try {
              ftag = (size_t)stoi(res);
            } catch (std::invalid_argument const &) {
              ftag = 0;
            } catch (std::out_of_range const &) {
              ftag = 0;
            }
            if (ftag > 0 && ftag <= filelist->size()) {
              if (!filelist->at(ftag - 1).missing && dl_sec_level <= n->get_user().get_sec_level()) {
                n->tag_file(filelist->at(ftag - 1).filename, this);
              }
            }
          }
          n->print_f("\r\n");
          lines = 0;
        }

        n->print_f("                                    |07%s\r\n", filelist->at(i).desc.at(z).substr(0, n->get_term_width() - 37).c_str());
        lines++;
      }
    } else {
      n->print_f("|14%4d. |15%-16.16s |13%5d%cb |12%4d |07No Description\r\n", i + 1, p.filename().u8string().c_str(), filelist->at(i).filesize, units[unit],
                 filelist->at(i).dlcount);
      lines++;
    }
    if (lines == (int)n->get_term_height() - 2) {
      if (cancel) {
        n->print_f("|08[|151|08-|15%d|08] |14Tag File, |15Q|08=|14Quit|08, |15X|08=|14Cancel Scan|08, |15ENTER|08=|14Continue: ", filelist->size());
      } else {
        n->print_f("|08[|151|08-|15%d|08] |14Tag File, |15Q|08=|14Quit|08, |15ENTER|08=|14Continue: ", filelist->size());
      }
      std::string res = n->get_string(5, false);
      if (res.size() > 0) {
        if (tolower(res.at(0) == 'q')) {
          return false;
        } else if (cancel && (tolower(res.at(0)) == 'x')) {
          return true;
        }
        size_t ftag;
        try {
          ftag = (size_t)stoi(res);
        } catch (std::invalid_argument const &) {
          ftag = 0;
        } catch (std::out_of_range const &) {
          ftag = 0;
        }
        if (ftag > 0 && ftag <= filelist->size()) {
          if (!filelist->at(ftag - 1).missing && dl_sec_level <= n->get_user().get_sec_level()) {
            n->tag_file(filelist->at(ftag - 1).filename, this);
          }
        }
      }
      n->print_f("\r\n");
      lines = 0;
    }
  }
  if (lines > 0) {
    if (cancel) {
      n->print_f("|08[|151|08-|15%d|08] |14Tag File|08, |15X|08=|14Cancel Scan|08, |15ENTER|08=|14Quit: ", filelist->size());
    } else {
      n->print_f("|08[|151|08-|15%d|08] |14Tag File|08, |15ENTER|08=|14Quit: ", filelist->size());
    }
    std::string res = n->get_string(5, false);
    if (cancel && tolower(res.at(0)) == 'x') {
      return true;
    }
    if (res.size() > 0) {
      size_t ftag;
      try {
        ftag = (size_t)stoi(res);
      } catch (std::invalid_argument const &) {
        ftag = 0;
      } catch (std::out_of_range const &) {
        ftag = 0;
      }
      if (ftag > 0 && ftag <= filelist->size()) {
        if (!filelist->at(ftag - 1).missing && dl_sec_level <= n->get_user().get_sec_level()) {
          n->tag_file(filelist->at(ftag - 1).filename, this);
        }
      }
    }
    n->print_f("\r\n");
  }
  return false;
}

bool FileArea::upload_file(Node *n) {
  std::filesystem::path p1(n->get_config()->tmp_path() + "/" + std::to_string(n->getnodenum()) + "/upload/");
  std::vector<std::string> file_id;
  std::vector<std::string> descr;
  std::filesystem::path p = std::filesystem::absolute(p1);

  bool ret = false;

  file_id.push_back("file_id.diz");

  if (std::filesystem::exists(p)) {
    std::filesystem::remove_all(p);
  }
  std::filesystem::create_directories(p);

  Protocol *pr = n->get_config()->select_protocol(n);

  if (pr != nullptr) {
    pr->upload(n, n->get_socket(), p.u8string());

    for (auto &f : std::filesystem::directory_iterator(p)) {
      n->print_f("\r\n\r\n|10Found File: |15%s\r\n", f.path().filename().u8string().c_str());

      if (file_exists(n, f.path().filename().u8string())) {
        n->print_f("|12Duplicate File|07\r\n");
        continue;
      }

      std::filesystem::path t(n->get_config()->tmp_path() + "/" + std::to_string(n->getnodenum()) + "/extract/");
      if (std::filesystem::exists(t)) {
        std::filesystem::remove_all(t);
      }
      std::filesystem::create_directories(t);

      Archiver::extract(n, f.path().u8string(), file_id, t.u8string());
      for (auto &d : std::filesystem::directory_iterator(t)) {
        if (strcasecmp(d.path().filename().u8string().c_str(), "file_id.diz") == 0) {
          // found description;
          std::ifstream infile(d.path().u8string());
          std::string line;
          while (std::getline(infile, line)) {
            descr.push_back(line);
          }
          break;
        }
      }
      std::filesystem::remove_all(t);

      if (descr.size() > 0) {
        n->print_f("|10Found Description!\r\n|07");
      } else {
        n->print_f("|14Please enter a description... (5 Lines MAX, Blank Line Ends)|07\r\n");
        for (int i = 0; i < 5; i++) {
          n->print_f("\r\n|14%d: ", i + 1);
          std::string line = n->get_string(32, false);
          if (line.size() == 0) {
            break;
          }
          descr.push_back(line);
        }
      }

      // copy file to directory
      std::filesystem::path np(file_path);
      np.append(f.path().filename().u8string());

      if (!std::filesystem::copy_file(f.path(), np)) {
        n->print_f("|12Copy file failed!|07\r\n");
        continue;
      }
      // add to database
      std::filesystem::path newp(std::filesystem::absolute(std::filesystem::path(file_path)));
      newp.append(f.path().filename().u8string());
      if (!insert_file(n, newp.u8string(), descr)) {
        n->print_f("|12Failed to add to the database!|07\r\n");
      } else {
        n->clog->up_bytes((uint32_t)std::filesystem::file_size(newp));
        n->get_user().inc_attrib("uploads");
        n->print_f("|10Thankyou for your upload!|07\r\n");
        ret = true;
      }
    }
  }
  std::filesystem::remove_all(p);
  return ret;
}
