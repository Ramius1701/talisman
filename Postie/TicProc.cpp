#ifdef _MSC_VER
#include <Shlwapi.h>
#include <Windows.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <fnmatch.h>
#include <unistd.h>
#endif
#include "../Common/INIReader.h"
#include "../Common/Logger.h"
#include "Archiver.h"
#include "Config.h"
#include "Dupe.h"
#include "GenDefs.h"
#include "TicProc.h"
#include <cinttypes>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

extern void sig_handler(int signal);

static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

bool TicProc::check_crc(const char *filename, uint32_t crc_chk) {
  uint32_t crc;

  if (!Dupe::crc32file(filename, &crc)) {
    return false;
  }

  return (crc == crc_chk);
}

bool TicProc::add_file_to_area(struct ticfile_t *tic, std::filesystem::path srcfile, std::filesystem::path destfile, std::string database) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  time_t now = time(NULL);

  static const char *sql = "INSERT INTO files (filename, filesize, dlcount, uldate, ulname, descr) VALUES(?, ?, 0, ?, ?, ?)";
  static const char *dsql = "DELETE FROM files WHERE filename LIKE ?";
  if (!open_database(database, &db)) {
    return false;
  }

  std::string filename;
  std::string pattern;

  if (tic->replaces != "") {
    // remove file that this file replaces
    std::filesystem::path dir = std::filesystem::absolute(destfile).parent_path();
    for (auto &p : std::filesystem::directory_iterator(dir)) {
      pattern = tic->replaces;
      filename = p.path().filename().u8string();
#ifdef _MSC_VER

      // fprintf(stderr, "%s v %s\n", filename.c_str(), pattern.c_str());

      if (PathMatchSpecA(filename.c_str(), pattern.c_str())) {
#else
      if (fnmatch(pattern.c_str(), filename.c_str(), FNM_CASEFOLD) == 0) {
#endif
        //  fprintf(stderr, "MATCH\n");
        // remove from database
        if (sqlite3_prepare_v2(db, dsql, strlen(dsql), &stmt, NULL) != SQLITE_OK) {
          sqlite3_close(db);
          return false;
        }

        std::string filen = p.path().u8string();

        sqlite3_bind_text(stmt, 1, filen.c_str(), -1, NULL);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        // remove file
        std::filesystem::remove(p.path());
        break;
      }
    }
  }
  if (std::filesystem::exists(destfile)) {
    std::filesystem::remove(destfile);
  }
  if (!std::filesystem::copy_file(srcfile, destfile)) {
    sqlite3_close(db);
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  std::string fname = std::filesystem::absolute(destfile).u8string();
  std::string ulname = "Tic Processor";
  std::string descr;
  sqlite3_bind_text(stmt, 1, fname.c_str(), -1, NULL);
  sqlite3_bind_int64(stmt, 2, std::filesystem::file_size(destfile));
  sqlite3_bind_int64(stmt, 3, now);
  sqlite3_bind_text(stmt, 4, ulname.c_str(), -1, NULL);

  if (tic->desc.size() > 0) {
    std::stringstream ss;
    for (size_t i = 0; i < tic->desc.size(); i++) {
      ss << tic->desc.at(i) << "\n";
    }

    descr = ss.str();

    sqlite3_bind_text(stmt, 5, descr.c_str(), -1, NULL);
  } else {
    descr = tic->shortdesc;
    sqlite3_bind_text(stmt, 5, descr.c_str(), -1, NULL);
  }

  sqlite3_step(stmt);
  sqlite3_close(db);

  return true;
}

bool TicProc::open_database(std::string filename, sqlite3 **db) {
  static const char *create_sql =
      "CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, filesize INTEGER, dlcount INTEGER, uldate INTEGER, ulname TEXT, descr TEXT);";
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

bool TicProc::hatch(const char *file, const char *area, const char *replace, const char *desc) {
  INIReader inir("talisman.ini");
  Config c;
  unsigned long pid;
  static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  if (inir.ParseError()) {
    return false;
  }

  _datapath = inir.Get("Paths", "Data Path", "data");
  _logpath = inir.Get("Paths", "Log Path", "logs");
  _tmppath = inir.Get("Paths", "Temp Path", "temp");

#ifdef _MSC_VER
  pid = GetCurrentProcessId();
#else
  pid = getpid();
#endif

  Logger log;

  log.load(_logpath + "/postie.log");

  std::filesystem::path pidfile(_datapath + "/postie.pid");
  int tries = 0;
  while (std::filesystem::exists(pidfile)) {
    if (tries == 10) {
      log.log(LOG_ERROR, "Timeout waiting for pid file...");
      return false;
    }
#ifdef _MSC_VER
    Sleep(1000);
#else
    sleep(1);
#endif
    tries++;
  }

  FILE *fptr = fopen(pidfile.u8string().c_str(), "wx");
  if (!fptr) {
    log.log(LOG_ERROR, "Failed to open pid file...");
    return false;
  }
  fprintf(fptr, "%lu\r\n", pid);
  fclose(fptr);

  log.log(LOG_DEBUG, "Starting TicHatch");

  if (!c.load(_datapath)) {
    std::filesystem::remove(pidfile);
    return false;
  }

  if (!c.load_archivers(_datapath)) {
    std::filesystem::remove(pidfile);
    return false;
  }

  std::filesystem::path temppth(_tmppath + "/postie-" + std::to_string(pid));
  std::filesystem::create_directories(temppth);

  std::filesystem::path extractpth(temppth.u8string() + "/extract");
  std::filesystem::create_directories(extractpth);

  std::filesystem::path tic(temppth);

  std::filesystem::path fpath(file);

  if (fpath.stem().u8string().size() > 8 || fpath.extension().u8string().size() > 4) {
    std::cerr << "Filename is not MS-DOS compatible!" << std::endl;
    std::filesystem::remove(pidfile);
    return false;
  }

  if (!std::filesystem::exists(fpath) || !std::filesystem::is_regular_file(fpath)) {
    std::cerr << "File does not exist, or is not a regular file!" << std::endl;
    std::filesystem::remove(pidfile);
    return false;
  }

  char buffer[9];
  time_t ttime = time(NULL);
  struct tm ttime_tm;

#ifdef _MSC_VER
  gmtime_s(&ttime_tm, &ttime);
#else
  gmtime_r(&ttime, &ttime_tm);
#endif

  snprintf(buffer, sizeof buffer, "%08" PRIx64, ttime & 0xffffffff);

  tic.append(std::string(buffer) + ".tic");

  uint32_t crc;

  if (!Dupe::crc32file(fpath.u8string().c_str(), &crc)) {
    std::filesystem::remove_all(temppth);
    std::filesystem::remove(pidfile);
    return false;
  }

  struct farea_conf_t *fa = NULL;

  for (size_t a = 0; a < c.fileareas.size(); a++) {
    if (strcasecmp(c.fileareas.at(a).areatag.c_str(), area) == 0) {
      fa = &c.fileareas.at(a);
      break;
    }
  }

  if (fa == NULL) {
    std::filesystem::remove_all(temppth);
    std::filesystem::remove(pidfile);
    return false;
  }

  bool unarced = false;

  for (size_t i = 0; i < c.archivers.size(); i++) {
    FILE *fptr = fopen(fpath.u8string().c_str(), "rb");
    if (c.archivers.at(i)->offset >= 0) {
      fseek(fptr, c.archivers.at(i)->offset, SEEK_SET);
    } else {
      fseek(fptr, c.archivers.at(i)->offset, SEEK_END);
    }

    uint8_t byte;
    bool match = true;
    for (int z = 0; z < c.archivers.at(i)->bytelen; z++) {
      fread(&byte, 1, 1, fptr);

      if (byte != c.archivers.at(i)->bytes[z]) {
        match = false;
        break;
      }
    }
    fclose(fptr);
    if (match == false)
      continue;

    c.archivers.at(i)->extract(fpath.u8string(), extractpth.u8string());
    unarced = true;
    break;
  }

  std::vector<std::string> descr;

  if (unarced) {
    for (auto &d : std::filesystem::directory_iterator(extractpth)) {
      if (strcasecmp(d.path().filename().u8string().c_str(), "file_id.diz") == 0) {
        // found description;
        std::ifstream infile(d.path().u8string());
        std::string line;
        while (std::getline(infile, line)) {
          rtrim(line);
          descr.push_back(line);
        }
        break;
      }
    }
  }

  for (size_t l = 0; l < fa->links.size(); l++) {
    FILE *fptr = fopen(tic.u8string().c_str(), "wb");

    if (!fptr) {
      continue;
    }

    fprintf(fptr, "Area %s\r\n", area);
    fprintf(fptr, "File %s\r\n", fpath.filename().u8string().c_str());

    fprintf(fptr, "Origin %d:%d/%d.%d\r\n", fa->aka->zone, fa->aka->net, fa->aka->node, fa->aka->point);

    fprintf(fptr, "From %d:%d/%d.%d\r\n", fa->aka->zone, fa->aka->net, fa->aka->node, fa->aka->point);

    fprintf(fptr, "To %d:%d/%d.%d\r\n", fa->links.at(l).link->aka->zone, fa->links.at(l).link->aka->net, fa->links.at(l).link->aka->node,
            fa->links.at(l).link->aka->point);

    fprintf(fptr, "Size %" PRIu64 "\r\n", std::filesystem::file_size(fpath));
    fprintf(fptr, "Desc %s\r\n", desc);

    for (size_t i = 0; i < descr.size(); i++) {
      fprintf(fptr, "Ldesc %s\r\n", descr.at(i).c_str());
    }

    fprintf(fptr, "Created by Postie %d.%d\r\n", VERSION_MAJOR, VERSION_MINOR);
    fprintf(fptr, "Replaces %s\r\n", replace);
    fprintf(fptr, "Crc %08X\r\n", crc);

    fprintf(fptr, "Pw %s\r\n", fa->links.at(l).link->ticpwd.c_str());

    std::vector<NETADDR *> addresses;

    for (size_t sb = 0; sb < fa->links.size(); sb++) {
      addresses.push_back(fa->links.at(sb).link->aka);
    }

    addresses.push_back(fa->aka);

    Config::sort_addr(&addresses);

    for (size_t sb = 0; sb < addresses.size(); sb++) {
      if (addresses.at(sb)->point > 0) {
        fprintf(fptr, "Seenby %d:%d/%d.%d\r\n", addresses.at(sb)->zone, addresses.at(sb)->net, addresses.at(sb)->node, addresses.at(sb)->point);
      } else {
        fprintf(fptr, "Seenby %d:%d/%d\r\n", addresses.at(sb)->zone, addresses.at(sb)->net, addresses.at(sb)->node);
      }
    }
    if (fa->links.at(l).link->ouraka->point > 0) {
      fprintf(fptr, "Path %d:%d/%d.%d %" PRId64 " %s %s %d %02d:%02d:%02d %04d UTC\r\n", fa->links.at(l).link->ouraka->zone, fa->links.at(l).link->ouraka->net,
              fa->links.at(l).link->ouraka->node, fa->links.at(l).link->ouraka->point, ttime, days[ttime_tm.tm_wday], months[ttime_tm.tm_mon], ttime_tm.tm_mday,
              ttime_tm.tm_hour, ttime_tm.tm_min, ttime_tm.tm_sec, ttime_tm.tm_year + 1900);
    } else {
      fprintf(fptr, "Path %d:%d/%d %" PRId64 " %s %s %d %02d:%02d:%02d %04d UTC\r\n", fa->links.at(l).link->ouraka->zone, fa->links.at(l).link->ouraka->net,
              fa->links.at(l).link->ouraka->node, ttime, days[ttime_tm.tm_wday], months[ttime_tm.tm_mon], ttime_tm.tm_mday, ttime_tm.tm_hour, ttime_tm.tm_min,
              ttime_tm.tm_sec, ttime_tm.tm_year + 1900);
    }
    fclose(fptr);

    std::filesystem::path targetfile(fa->links.at(l).link->filebox);
    targetfile.append(fpath.filename().u8string());

    std::filesystem::path targettic(fa->links.at(l).link->filebox);
    targettic.append(tic.filename().u8string());

    std::filesystem::copy_file(tic, targettic);
    std::filesystem::copy_file(fpath, targetfile);

    std::filesystem::remove(tic);
  }
  std::filesystem::remove_all(temppth);
  std::filesystem::remove(pidfile);
  return true;
}

bool TicProc::run() {
  INIReader inir("talisman.ini");
  Config c;
  unsigned long pid;
  static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  if (inir.ParseError()) {
    return false;
  }

  _datapath = inir.Get("Paths", "Data Path", "data");
  _logpath = inir.Get("Paths", "Log Path", "logs");
  _tmppath = inir.Get("Paths", "Temp Path", "temp");

#ifdef _MSC_VER
  pid = GetCurrentProcessId();
#else
  pid = getpid();
#endif

  Logger log;

  log.load(_logpath + "/postie.log");

  std::filesystem::path pidfile(_datapath + "/postie.pid");
  int tries = 0;
  while (std::filesystem::exists(pidfile)) {
    if (tries == 10) {
      log.log(LOG_ERROR, "Timeout waiting for pid file...");
      return false;
    }
#ifdef _MSC_VER
    Sleep(1000);
#else
    sleep(1);
#endif
    tries++;
  }

  FILE *fptr = fopen(pidfile.u8string().c_str(), "wx");
  if (!fptr) {
    log.log(LOG_ERROR, "Failed to open pid file...");
    return false;
  }
  fprintf(fptr, "%lu\r\n", pid);
  fclose(fptr);

  signal(SIGTERM, sig_handler);
#ifndef _MSC_VER
  signal(SIGHUP, sig_handler);
  signal(SIGINT, sig_handler);
#endif

  log.log(LOG_DEBUG, "Starting TicProc");

  if (!c.load(_datapath)) {
    std::filesystem::remove(pidfile);
    return false;
  }

  if (!c.load_archivers(_datapath)) {
    std::filesystem::remove(pidfile);
    return false;
  }

  std::filesystem::path temppth(_tmppath + "/postie-" + std::to_string(pid));
  std::filesystem::create_directories(temppth);

  std::vector<std::filesystem::path> removelist;

  // foreach tic file
  for (auto &p : std::filesystem::directory_iterator(c.protinbound())) {
    std::filesystem::path filepth = p.path();

    if (strcasecmp(filepth.extension().u8string().c_str(), ".tic") == 0) {
      //    process tic file
      struct ticfile_t tic;
      tic.crc = 0;
      std::ifstream ifs(filepth);
      std::string line;
      while (std::getline(ifs, line)) {
        if (line[line.size() - 1] == '\r') {
          line = line.substr(0, line.size() - 1);
        }
        if (strncasecmp(line.c_str(), "area ", 5) == 0) {
          tic.area = line.substr(5);
        }
        if (strncasecmp(line.c_str(), "file ", 5) == 0) {
          tic.file = line.substr(5);
        }
        if (strncasecmp(line.c_str(), "lfile ", 6) == 0) {
          tic.lname = line.substr(6);
        }
        if (strncasecmp(line.c_str(), "fullname ", 9) == 0) {
          tic.lname = line.substr(9);
        }
        if (strncasecmp(line.c_str(), "desc ", 5) == 0) {
          tic.shortdesc = line.substr(5);
        }
        if (strncasecmp(line.c_str(), "ldesc ", 6) == 0) {
          tic.desc.push_back(line.substr(6));
        }
        if (strncasecmp(line.c_str(), "replaces ", 9) == 0) {
          tic.replaces = line.substr(9);
        }
        if (strncasecmp(line.c_str(), "crc ", 4) == 0) {
          tic.crc = strtoul(line.substr(4).c_str(), NULL, 16);
        }
        if (strncasecmp(line.c_str(), "pw ", 3) == 0) {
          tic.password = line.substr(3);
        }
        if (strncasecmp(line.c_str(), "from ", 5) == 0) {
          tic.from = line.substr(5);
        }
        if (strncasecmp(line.c_str(), "seenby ", 7) == 0) {
          NETADDR *addr = parse_fido_addr(line.substr(7).c_str());
          if (addr != NULL) {
            tic.seenbys.push_back(addr);
          }
        }
        if (strncasecmp(line.c_str(), "path ", 5) == 0) {
          tic.path.push_back(line.substr(5));
        }
      }

      ifs.close();

      //    check crc of file
      // fprintf(stderr, "%s\r\n", std::string(c.protinbound() + "/" + tic.file).c_str());
      if (!check_crc(std::string(c.protinbound() + "/" + tic.file).c_str(), tic.crc)) {
        log.log(LOG_ERROR, "%s failed CRC check!", tic.file.c_str());
        for (NETADDR *addr : tic.seenbys) {
          free(addr);
        }
        continue;
      }
      // check tick password
      NETADDR *ticfrom = parse_fido_addr(tic.from.c_str());
      if (ticfrom == NULL) {
        log.log(LOG_ERROR, "%s contains no from field!", filepth.u8string().c_str());
        for (NETADDR *addr : tic.seenbys) {
          free(addr);
        }

        continue;
      }

      struct farea_conf_t *filearea = nullptr;

      //    add file to area
      for (size_t i = 0; i < c.fileareas.size(); i++) {
        if (strcasecmp(c.fileareas.at(i).areatag.c_str(), tic.area.c_str()) == 0) {
          filearea = &c.fileareas.at(i);
          break;
        }
      }

      if (filearea == nullptr) {
        log.log(LOG_ERROR, "%s for unconfigured area %s", filepth.u8string().c_str(), tic.area.c_str());
        for (NETADDR *addr : tic.seenbys) {
          free(addr);
        }
        continue;
      }

      bool passok = false;
      bool canforward = false;
      bool canprocess = false;

      for (size_t i = 0; i < filearea->links.size(); i++) {
        if (filearea->links.at(i).link->aka->zone == ticfrom->zone && filearea->links.at(i).link->aka->net == ticfrom->net &&
            filearea->links.at(i).link->aka->node == ticfrom->node && filearea->links.at(i).link->aka->point == ticfrom->point) {
          if (filearea->links.at(i).link->ticpwd == tic.password) {
            passok = true;
            canforward = filearea->links.at(i).forward_allowed;
            canprocess = filearea->links.at(i).process_allowed;
          }
          break;
        }
      }

      free(ticfrom);

      if (!passok) {
        log.log(LOG_ERROR, "%s contains an invalid password!", filepth.u8string().c_str());
        for (NETADDR *addr : tic.seenbys) {
          free(addr);
        }

        continue;
      }

      if (!canprocess) {
        log.log(LOG_ERROR, "%s cant process tic file!", filepth.u8string().c_str());
        for (NETADDR *addr : tic.seenbys) {
          free(addr);
        }

        continue;
      }

      std::filesystem::path fsrc(c.protinbound() + "/" + tic.file);
      std::filesystem::path fdest(filearea->directory);

      if (tic.lname != "") {
        fdest.append(tic.lname);
      } else {
        fdest.append(tic.file);
      }

      if (!add_file_to_area(&tic, fsrc, fdest, _datapath + "/" + filearea->database + ".sqlite3")) {
        log.log(LOG_ERROR, "%s failed to add to area...", filepth.u8string().c_str());
        for (NETADDR *addr : tic.seenbys) {
          free(addr);
        }
        continue;
      } else {
        if (filearea->hook != "") {
          std::stringstream ss;
          ss << filearea->hook << " " << fdest.u8string();
          log.log(LOG_INFO, "Running hook: %s", ss.str().c_str());
          Archiver::runexec(ss.str());
        }
      }

      if (canforward) {

        std::vector<struct link_conf_t *> downlinks;

        //    forward any files to downlinks
        for (size_t l = 0; l < filearea->links.size(); l++) {
          bool inseenby = false;
          for (size_t s = 0; s < tic.seenbys.size(); s++) {
            if (tic.seenbys.at(s)->zone == filearea->links.at(l).link->aka->zone && tic.seenbys.at(s)->net == filearea->links.at(l).link->aka->net &&
                tic.seenbys.at(s)->node == filearea->links.at(l).link->aka->node && tic.seenbys.at(s)->point == filearea->links.at(l).link->aka->point) {
              inseenby = true;
              break;
            }
          }
          if (!inseenby) {
            downlinks.push_back(filearea->links.at(l).link);
          }
        }

        for (size_t l = 0; l < downlinks.size(); l++) {
          std::ifstream ifs(filepth);
          std::string line;
          std::vector<std::string> ticlines;
          while (std::getline(ifs, line)) {
            ticlines.push_back(line);
          }

          ifs.close();

          // create tic file for link
          std::filesystem::path newtic(temppth);
          newtic.append(filepth.filename().u8string());

          FILE *fptr = fopen(newtic.u8string().c_str(), "wb");

          if (fptr) {
            for (size_t ln = 0; ln < ticlines.size(); ln++) {
              if (strncasecmp(ticlines.at(ln).c_str(), "Pw ", 3) == 0) {
                fprintf(fptr, "Pw %s\r\n", downlinks.at(l)->ticpwd.c_str());
              } else if (strncasecmp(ticlines.at(ln).c_str(), "To ", 3) == 0) {
                fprintf(fptr, "To %d:%d/%d.%d\r\n", downlinks.at(l)->aka->zone, downlinks.at(l)->aka->net, downlinks.at(l)->aka->node,
                        downlinks.at(l)->aka->point);
              } else if (strncasecmp(ticlines.at(ln).c_str(), "From ", 5) == 0) {
                fprintf(fptr, "From %d:%d/%d.%d\r\n", downlinks.at(l)->ouraka->zone, downlinks.at(l)->ouraka->net, downlinks.at(l)->ouraka->node,
                        downlinks.at(l)->ouraka->point);
              } else if (strncasecmp(ticlines.at(ln).c_str(), "Seenby ", 7) != 0 && strncasecmp(ticlines.at(ln).c_str(), "Path ", 5) != 0) {
                fprintf(fptr, "%s\r\n", ticlines.at(ln).c_str());
              }
            }

            // print path
            for (size_t pt = 0; pt < tic.path.size(); pt++) {
              fprintf(fptr, "Path %s\r\n", tic.path.at(pt).c_str());
            }
            time_t now = time(NULL);
            struct tm ltime;
#ifdef _MSC_VER
            gmtime_s(&ltime, &now);
#else
            gmtime_r(&now, &ltime);
#endif

            fprintf(fptr, "Path %d:%d/%d.%d %" PRId64 " %s %s %d %02d:%02d:%02d %d UTC\r\n", downlinks.at(l)->ouraka->zone, downlinks.at(l)->ouraka->net,
                    downlinks.at(l)->ouraka->node, downlinks.at(l)->ouraka->point, now, days[ltime.tm_wday], months[ltime.tm_mon], ltime.tm_mday, ltime.tm_hour,
                    ltime.tm_min, ltime.tm_sec, ltime.tm_year + 1900);

            std::vector<NETADDR *> addresses;

            // print seenbys
            for (size_t sb = 0; sb < tic.seenbys.size(); sb++) {
              addresses.push_back(tic.seenbys.at(sb));
            }
            for (size_t sb = 0; sb < downlinks.size(); sb++) {
              addresses.push_back(downlinks.at(sb)->aka);
            }

            Config::sort_addr(&addresses);

            for (size_t sb = 0; sb < addresses.size(); sb++) {
              if (addresses.at(sb)->point > 0) {
                fprintf(fptr, "Seenby %d:%d/%d.%d\r\n", addresses.at(sb)->zone, addresses.at(sb)->net, addresses.at(sb)->node, addresses.at(sb)->point);
              } else {
                fprintf(fptr, "Seenby %d:%d/%d\r\n", addresses.at(sb)->zone, addresses.at(sb)->net, addresses.at(sb)->node);
              }
            }

            fclose(fptr);

            // copy new tic file, and actual file to outbox

            std::filesystem::path desttic(downlinks.at(l)->filebox);
            desttic.append(filepth.filename().u8string());

            std::filesystem::path destfile(downlinks.at(l)->filebox);
            destfile.append(tic.file);

            std::filesystem::copy(newtic, desttic);

            if (std::filesystem::exists(destfile)) {
              std::filesystem::remove(destfile);
            }
            std::filesystem::copy(fsrc, destfile);

            std::filesystem::remove(newtic);
          }
        }
      }
      for (NETADDR *addr : tic.seenbys) {
        free(addr);
      }
      removelist.push_back(fsrc);
      removelist.push_back(filepth);
    }
  }

  for (size_t r = 0; r < removelist.size(); r++) {
    std::filesystem::remove(removelist.at(r));
  }

  std::filesystem::remove_all(temppth);
  std::filesystem::remove(pidfile);
  return true;
}
