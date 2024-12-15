#include "../Common/INIReader.h"
#include "../Common/Squish.h"
#include "../Common/wwivnet.h"
#include "../Common/Logger.h"
#include "../Common/tendian.h"
#include "Config.h"
#include "Tosser.h"
#include "Dupe.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <fstream>
#ifdef _MSC_VER
#define strcasecmp stricmp
#else
#ifdef __FreeBSD__
#include <sys/endian.h>
#elif defined(__APPLE__)
#include <machine/endian.h>
#else
#include <endian.h>
#endif
#endif

static inline uint16_t host2le_s(uint16_t s) {
#if defined(__LITTLE_ENDIAN__)
  return s;
#else
  return (((s >> 8) & 0xffu) | ((s & 0xffu) << 8));
#endif
}

static inline uint32_t host2le_l(uint32_t s) {
#if defined(__LITTLE_ENDIAN__)
  return s;
#else
  return (((s & 0xff000000u) >> 24) | ((s & 0x00ff0000u) >> 8) | ((s & 0x0000ff00u) << 8) | ((s & 0x000000ffu) << 24));
#endif
}

bool Tosser::open_user_database(Logger *log, sqlite3 **db) {
  static const char *create_users_sql =
      "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";

  int rc;
  char *err_msg = NULL;
  std::string filepath = _datapath + "/users.sqlite3";
  if (sqlite3_open(filepath.c_str(), db) != SQLITE_OK) {
    log->log(LOG_ERROR, "Unable to open database: users.db");
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    log->log(LOG_ERROR, "Unable to create user table %s", err_msg);
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }

  return true;
}

bool Tosser::import_email(Logger *log, int to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network, time_t sent) {
  // lookup user num -> username
  sqlite3 *db;

  sqlite3_stmt *stmt;
  static const char *sql = "SELECT username FROM users WHERE id = ?";
  if (!open_user_database(log, &db)) {
    return false;
  }
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    log->log(LOG_ERROR, "Failed to prepare statement");
    return false;
  }

  std::string username;

  sqlite3_bind_int(stmt, 1, to);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    username = std::string((const char *)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return import_email(log, username, from, fromsys, subject, msg, network, sent);
  }

  std::cerr << "Unknown user (user num " << to << ")" << std::endl;

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return false;
}

bool Tosser::import_email(Logger *log, std::string to, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network,
                          time_t sent) {
  sq_msg_base_t *mb;
  sq_msg_t newmsg;

  memset(&newmsg, 0, sizeof(sq_msg_t));

  std::stringstream ss;
  std::stringstream cs;
  bool ctrlline = false;

  for (size_t line = 0; line < msg.size(); line++) {
    std::string str = strip_hearts(msg.at(line));
    for (size_t ch = 0; ch < str.size(); ch++) {
      if (ch < str.size() - 1 && str.at(ch) == 0x4 && str.at(ch + 1) == '0') {
        ctrlline = true;
        cs << 0x01;
        continue;
      }
      if (ctrlline) {
        cs << str.at(ch);
      } else {
        ss << str.at(ch);
      }
    }
    if (ctrlline) {
      ctrlline = false;
    } else {
      ss << '\r';
    }
  }

  newmsg.ctrl_len = cs.str().size();
  newmsg.ctrl = (char *)malloc(newmsg.ctrl_len + 1);
  if (!newmsg.ctrl) {
    return false;
  }

  strncpy(newmsg.ctrl, cs.str().c_str(), newmsg.ctrl_len);

  newmsg.msg_len = ss.str().size();
  newmsg.msg = (char *)malloc(newmsg.msg_len + 1);
  if (!newmsg.msg) {
    free(newmsg.ctrl);
    return false;
  }
  strncpy(newmsg.msg, ss.str().c_str(), newmsg.msg_len);

  newmsg.xmsg.attr = MSGUID | MSGPRIVATE;
  newmsg.xmsg.dest.zone = 20000;
  newmsg.xmsg.dest.net = 20000;
  newmsg.xmsg.dest.node = config.networks.at(network).mynode;
  newmsg.xmsg.dest.point = 0;

  newmsg.xmsg.orig.zone = 20000;
  newmsg.xmsg.orig.net = 20000;
  newmsg.xmsg.orig.node = fromsys;
  newmsg.xmsg.orig.point = 0;

  size_t hashloc = from.rfind('#');
  if (hashloc != std::string::npos) {
    strncpy(newmsg.xmsg.from, from.substr(0, hashloc - 1).c_str(), 35);
  } else {
    strncpy(newmsg.xmsg.from, from.c_str(), 35);
  }
  strncpy(newmsg.xmsg.to, to.c_str(), 35);
  strncpy(newmsg.xmsg.subject, subject.c_str(), 71);

  std::tm at;

  time_t now = time(NULL);
#ifdef _MSC_VER
  localtime_s(&at, &now);
#else
  localtime_r(&now, &at);
#endif
  newmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
  newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
  newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

  newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
  newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
  newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

#ifdef _MSC_VER
  localtime_s(&at, &sent);
#else
  localtime_r(&sent, &at);
#endif
  newmsg.xmsg.date_written.date |= (((sq_word)at.tm_mday) & 31);
  newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
  newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

  newmsg.xmsg.date_written.time |= (((sq_word)at.tm_sec) & 31);
  newmsg.xmsg.date_written.time |= (((sq_word)at.tm_min) & 63) << 5;
  newmsg.xmsg.date_written.time |= (((sq_word)at.tm_hour) & 31) << 11;

  mb = SquishOpenMsgBase(std::string(_msgpath + "/" + config.networks.at(network).emailbase).c_str());
  if (!mb) {
    free(newmsg.msg);
    free(newmsg.ctrl);
    log->log(LOG_ERROR, "Failed to open message base %s", std::string(_msgpath + "/" + config.networks.at(network).emailbase).c_str());
    return false;
  }
  if (!SquishLockMsgBase(mb)) {
    free(newmsg.msg);
    free(newmsg.ctrl);
    log->log(LOG_ERROR, "Failed to lock message base %s", std::string(_msgpath + "/" + config.networks.at(network).emailbase).c_str());
    return false;
  }
  SquishWriteMsg(mb, &newmsg);

  SquishUnlockMsgBase(mb);
  SquishCloseMsgBase(mb);
  free(newmsg.msg);
  free(newmsg.ctrl);

  return true;
}

bool Tosser::import_message(Logger *log, std::string subtype, std::string from, int fromsys, std::string subject, std::vector<std::string> msg, int network,
                            time_t sent) {
  for (size_t i = 0; i < config.areas.size(); i++) {
    if (strcasecmp(config.areas.at(i).subtype.c_str(), subtype.c_str()) == 0 &&
        strcasecmp(config.areas.at(i).netname.c_str(), config.networks.at(network).name.c_str()) == 0) {

      sq_msg_base_t *mb;
      sq_msg_t newmsg;

      memset(&newmsg, 0, sizeof(sq_msg_t));
      std::stringstream ss;
      std::stringstream cs;
      bool ctrlline = false;

      for (size_t line = 0; line < msg.size(); line++) {
        std::string str = strip_hearts(msg.at(line));
        for (size_t ch = 0; ch < str.size(); ch++) {
          if (ch < str.size() - 1 && str.at(ch) == 0x4 && str.at(ch + 1) == '0') {
            ctrlline = true;
            cs << 0x01;
            continue;
          }
          if (ctrlline) {
            cs << str.at(ch);
          } else {
            ss << str.at(ch);
          }
        }
        if (ctrlline) {
          ctrlline = false;
        } else {
          ss << '\r';
        }
      }

      if (Dupe::is_dupe(_datapath + "/falcon_dupehist.dat", ss.str(), (uint32_t)sent)) {
        log->log(LOG_INFO, "Duplicate Found!");
        return false;
      }

      newmsg.ctrl_len = cs.str().size();
      newmsg.ctrl = (char *)malloc(newmsg.ctrl_len + 1);
      if (!newmsg.ctrl) {
        return false;
      }

      strncpy(newmsg.ctrl, cs.str().c_str(), newmsg.ctrl_len);

      newmsg.msg_len = ss.str().size();
      newmsg.msg = (char *)malloc(newmsg.msg_len + 1);
      if (!newmsg.msg) {
        free(newmsg.ctrl);
        return false;
      }
      strncpy(newmsg.msg, ss.str().c_str(), newmsg.msg_len);

      newmsg.xmsg.attr = MSGUID;

      newmsg.xmsg.orig.zone = 20000;
      newmsg.xmsg.orig.net = 20000;
      newmsg.xmsg.orig.node = fromsys;
      newmsg.xmsg.orig.point = 0;

      size_t hashloc = from.rfind('#');
      if (hashloc != std::string::npos) {
        strncpy(newmsg.xmsg.from, from.substr(0, hashloc - 1).c_str(), 35);
      } else {
        strncpy(newmsg.xmsg.from, from.c_str(), 35);
      }
      strncpy(newmsg.xmsg.to, "ALL", 35);
      strncpy(newmsg.xmsg.subject, subject.c_str(), 71);

      std::tm at;

      time_t now = time(NULL);
#ifdef _MSC_VER
      localtime_s(&at, &now);
#else
      localtime_r(&now, &at);
#endif
      newmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
      newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
      newmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

      newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
      newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
      newmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

#ifdef _MSC_VER
      localtime_s(&at, &sent);
#else
      localtime_r(&sent, &at);
#endif
      newmsg.xmsg.date_written.date |= (((sq_word)at.tm_mday) & 31);
      newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
      newmsg.xmsg.date_written.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

      newmsg.xmsg.date_written.time |= (((sq_word)at.tm_sec) & 31);
      newmsg.xmsg.date_written.time |= (((sq_word)at.tm_min) & 63) << 5;
      newmsg.xmsg.date_written.time |= (((sq_word)at.tm_hour) & 31) << 11;

      mb = SquishOpenMsgBase(std::string(_msgpath + "/" + config.areas.at(i).basefile).c_str());
      if (!mb) {
        free(newmsg.msg);
        free(newmsg.ctrl);
        log->log(LOG_ERROR, "Failed to open message base %s", std::string(_msgpath + "/" + config.areas.at(i).basefile).c_str());
        return false;
      }
      if (!SquishLockMsgBase(mb)) {
        free(newmsg.msg);
        free(newmsg.ctrl);
        log->log(LOG_ERROR, "Failed to lock message base %s", std::string(_msgpath + "/" + config.areas.at(i).basefile).c_str());
        return false;
      }
      SquishWriteMsg(mb, &newmsg);

      SquishUnlockMsgBase(mb);
      SquishCloseMsgBase(mb);
      free(newmsg.msg);
      free(newmsg.ctrl);

      return true;
    }
  }

  return false;
}

bool Tosser::remove_subscriber(std::string subtype, std::string network, int system) {
  std::filesystem::path subfile(_datapath);
  subfile.append("wwiv");
  subfile.append(network);
  subfile.append("n" + subtype + ".new");

  std::filesystem::path origfile(_datapath);
  origfile.append("wwiv");
  origfile.append(network);
  origfile.append("n" + subtype + ".net");

  std::ofstream out(subfile, std::ios_base::out);
  std::ifstream in(origfile);

  bool ret = false;

  if (out.is_open() && in.is_open()) {
    std::string str;
    while (getline(in, str)) {
      try {
        uint16_t sys = (uint16_t)std::stoi(str);
        if (sys != system) {
          out << str << std::endl;
        } else {
          ret = true;
        }
      } catch (std::invalid_argument const &) {
        out << str << std::endl;
      } catch (std::out_of_range const &) {
        out << str << std::endl;
      }
    }
    out.close();
    in.close();
    if (ret) {
      std::filesystem::remove(origfile);
      std::filesystem::rename(subfile, origfile);
    } else {
      std::filesystem::remove(subfile);
    }
  } else {
    if (in.is_open())
      in.close();
    if (out.is_open())
      out.close();
  }

  return ret;
}

bool Tosser::add_subscriber(std::string subtype, std::string network, int system) {
  std::filesystem::path subfile(_datapath);
  subfile.append("wwiv");
  subfile.append(network);
  subfile.append("n" + subtype + ".net");

  std::ofstream out(subfile, std::ios_base::app | std::ios_base::out);

  if (out.is_open()) {
    out << system << std::endl;
    out.close();
    return true;
  }
  return false;
}

std::vector<uint16_t> Tosser::get_subscribers(std::string dpath, std::string subtype, std::string network) {
  std::vector<uint16_t> subscribers;

  std::filesystem::path subfile(dpath);
  subfile.append("wwiv");
  subfile.append(network);
  subfile.append("n" + subtype + ".net");

  if (!std::filesystem::exists(subfile)) {
    return subscribers;
  }

  std::ifstream in(subfile);
  if (in.is_open()) {
    std::string line;
    while (getline(in, line)) {
      try {
        uint16_t sys = (uint16_t)std::stoi(line);
        subscribers.push_back(sys);
      } catch (std::invalid_argument const &) {
      } catch (std::out_of_range const &) {
      }
    }
    in.close();
  }
  return subscribers;
}

int Tosser::check_if_subscriber(std::string subtype, std::string network, int system) {
  std::filesystem::path subfile(_datapath);
  subfile.append("wwiv");
  subfile.append(network);
  subfile.append("n" + subtype + ".net");

  if (!std::filesystem::exists(subfile)) {
    return 2;
  }

  std::ifstream in(subfile);
  if (in.is_open()) {
    std::string line;
    while (getline(in, line)) {
      try {
        int sys = std::stoi(line);

        if (sys == system) {
          in.close();
          return 1;
        }
      } catch (std::invalid_argument const &) {
      } catch (std::out_of_range const &) {
      }
    }
    in.close();
    return 0;
  }
  return -1;
}

std::string Tosser::strip_hearts(std::string line) {
  std::stringstream ss;
  uint8_t lastc = 'x';
  for (size_t i = 0; i < line.length(); i++) {
    uint8_t c = line.at(i);
    // remove heart codes
    if (c != 0x3 && c != 0x1 && c != 0x1a && c != 0x4) {
      if (lastc == 0x3) {
        if (!config.striphearts()) {
          switch (c) {
          case '0':
            ss << "|16|07";
            break;
          case '1':
            ss << "|16|11";
            break;
          case '2':
            ss << "|16|14";
            break;
          case '3':
            ss << "|16|13";
            break;
          case '4':
            ss << "|17|15";
            break;
          case '5':
            ss << "|16|10";
            break;
          case '6':
            ss << "|16|12";
            break;
          case '7':
            ss << "|16|09";
            break;
          case '8':
            ss << "|16|05";
            break;
          case '9':
            ss << "|16|03";
            break;
          }
        }
      } else if (lastc == 0x4) {
        if (c == '0') {
          ss << '\x04';
          ss << c;
        }
      } else {
        ss << c;
      }
    }
    lastc = c;
  }

  return ss.str();
}

std::vector<std::string> Tosser::read_message(FILE *fptr, uint32_t length) {
  std::vector<std::string> msg;
  char lastc = 'x';

  bool sr = false;
  std::stringstream ss;

  for (size_t j = 0; j < length; j++) {
    char c;
    if (fread(&c, sizeof(char), 1, fptr) != 1) {
      sr = true;
      break;
    }
    if (c == '\r' || (c == '\n' && lastc != '\r')) {
      msg.push_back(ss.str());
      ss.str("");
    } else if (c != '\n') {
      ss << c;
    }
    lastc = c;
  }

  if (ss.str().size() > 0) {
    msg.push_back(ss.str());
    ss.str("");
  }

  if (sr) {
    msg.clear();
  }

  return msg;
}

void Tosser::run() {
  INIReader inir("talisman.ini");

  if (inir.ParseError()) {
    std::cerr << "Failed to parse talisman.ini" << std::endl;
    return;
  }

  _datapath = inir.Get("Paths", "Data Path", "data");
  _msgpath = inir.Get("Paths", "Message Path", "msgs");
  _logpath = inir.Get("Paths", "Log Path", "logs");
  _tmppath = inir.Get("Paths", "Temp Path", "temp");

  Logger log;

  log.load(_logpath + "/falcon.log");

  if (!config.load(_datapath, &log)) {
    std::cerr << "Failed to parse falcon.toml" << std::endl;
    return;
  }

  log.log(LOG_DEBUG, "Tosser starting...");

  for (size_t i = 0; i < config.networks.size(); i++) {
    std::filesystem::path ibpath = config.inbound();
    for (const auto &di : std::filesystem::directory_iterator(ibpath)) {
      std::string lookingfor = "s" + std::to_string(config.networks.at(i).mynode) + ".net";
      if (di.path().filename().u8string() == lookingfor || di.path().stem().u8string() == lookingfor) {

        std::filesystem::path fspath = di.path();

        // toss file for network.
        FILE *fptr = fopen(fspath.u8string().c_str(), "rb");
        if (!fptr) {
          log.log(LOG_ERROR, "Unable to load %s", fspath.u8string().c_str());
          continue;
        }

        while (!feof(fptr)) {
          struct net_header_rec msgrec;
          std::vector<uint16_t> nlist;
          if (fread(&msgrec, sizeof(struct net_header_rec), 1, fptr) != 1) {
            break;
          }

          msgrec.tosys = host2le_s(msgrec.tosys);
          msgrec.touser = host2le_s(msgrec.touser);
          msgrec.fromsys = host2le_s(msgrec.fromsys);
          msgrec.fromuser = host2le_s(msgrec.fromuser);
          msgrec.main_type = host2le_s(msgrec.main_type);
          msgrec.minor_type = host2le_s(msgrec.minor_type);
          msgrec.list_len = host2le_s(msgrec.list_len);
          msgrec.daten = host2le_l(msgrec.daten);
          msgrec.length = host2le_l(msgrec.length);
          msgrec.method = host2le_s(msgrec.method);

          bool sr = false;
          for (uint16_t j = 0; j < msgrec.list_len; j++) {
            uint16_t n;
            if (fread(&n, sizeof(uint16_t), 1, fptr) != 1) {
              log.log(LOG_ERROR, "Short read (2) %s", fspath.u8string().c_str());
              sr = true;
              break;
            }
            nlist.push_back(host2le_s(n));
          }

          if (sr)
            break;

          switch (msgrec.main_type) {
          case 1: {
            switch (msgrec.minor_type) {
            case 0: {
              if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {
                if (msgrec.tosys == 0) {
                  bool found = false;
                  for (size_t k = 0; k < nlist.size(); k++) {
                    if (nlist.at(k) == config.networks.at(i).mynode) {
                      found = true;
                      break;
                    }
                  }
                  if (!found) {
                    break;
                  }
                }
                std::string subj;
                std::string sender;
                std::string datestr;
                std::stringstream ss;

                std::vector<std::string> msg = read_message(fptr, msgrec.length);

                if (msg.size() == 0)
                  break;

                for (size_t h = 0; h < msg.at(0).size(); h++) {
                  if (msg.at(0).at(h) == '\0') {
                    subj = ss.str();
                    ss.str("");
                  } else {
                    ss << msg.at(0).at(h);
                  }
                }

                sender = ss.str();

                datestr = msg.at(1);

                msg.erase(msg.begin(), msg.begin() + 1);
                log.log(LOG_INFO, "Importing email from Network Coodinator (%s) @%d (%s)", sender.c_str(), msgrec.fromsys, config.networks.at(i).name.c_str());
                import_email(&log, 1, sender, msgrec.fromsys, subj, msg, i, msgrec.daten);
              }

            } break;
            case 9: {
              if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {
                if (msgrec.tosys == 0) {
                  bool found = false;
                  for (size_t k = 0; k < nlist.size(); k++) {
                    if (nlist.at(k) == config.networks.at(i).mynode) {
                      found = true;
                      break;
                    }
                  }
                  if (!found) {
                    break;
                  }
                }
                uint8_t *bytes = (uint8_t *)malloc(msgrec.length);
                if (!bytes)
                  break;
                if (fread(bytes, msgrec.length, 1, fptr) != 1) {
                  log.log(LOG_ERROR, "Short read (2) %s", fspath.u8string().c_str());
                  free(bytes);
                  break;
                }
                if (msgrec.length < 4) {
                  free(bytes);
                  break;
                }
                uint16_t flags = (bytes[1] << 8) | bytes[0];
                const char *fntemp = (const char *)&bytes[2];
                std::string filename(fntemp);

                if (filename.empty() || filename.length() > 8) {
                  free(bytes);
                  break;
                }

                size_t pos = filename.length() + 3;

                if ((flags & 0x02) != 0) {
                  filename = filename + ".zip";
                } else {
                  filename = filename + ".net";
                }

                std::filesystem::path fp(_datapath);
                fp.append("wwiv");
                fp.append(config.networks.at(i).name);
                if (!std::filesystem::exists(fp)) {
                  std::filesystem::create_directories(fp);
                }
                fp.append(filename);
                FILE *fptr2 = NULL;
                if ((flags & 1) != 0) {
                  fptr2 = fopen(fp.u8string().c_str(), "wb");
                } else {
                  fptr2 = fopen(fp.u8string().c_str(), "ab");
                }
                if (fptr2) {
                  fwrite(&bytes[pos], msgrec.length - pos, 1, fptr2);
                  fclose(fptr2);
                  log.log(LOG_INFO, "Saved file \"%s\"", fp.u8string().c_str());
                }
                free(bytes);
              }
            } break;
            default: {
              if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {
                if (msgrec.tosys == 0) {
                  bool found = false;
                  for (size_t k = 0; k < nlist.size(); k++) {
                    if (nlist.at(k) == config.networks.at(i).mynode) {
                      found = true;
                      break;
                    }
                  }
                  if (!found) {
                    break;
                  }
                }
                std::string filename;
                bool append = false;
                switch (msgrec.minor_type) {
                case 1:
                  filename = "bbslist.net";
                  break;
                case 2:
                  filename = "connect.net";
                  break;
                case 3:
                  filename = "subs.lst";
                  break;
                case 4:
                  filename = "wwivnews.net";
                  break;
                case 5:
                  filename = "fbackhdr.net";
                  break;
                case 6:
                  filename = "wwivnews.net";
                  append = true;
                  break;
                case 7:
                  filename = "categ.net";
                  break;
                case 8:
                  filename = "networks.lst";
                  break;
                case 0x10:
                  filename = "binkp.net";
                  break;
                }

                uint8_t *bytes = (uint8_t *)malloc(msgrec.length);
                if (!bytes)
                  break;
                if (fread(bytes, msgrec.length, 1, fptr) != 1) {
                  log.log(LOG_ERROR, "Short read (2) %s", fspath.u8string().c_str());
                  free(bytes);
                  break;
                }

                std::filesystem::path fp(_datapath);
                fp.append("wwiv");
                fp.append(config.networks.at(i).name);
                if (!std::filesystem::exists(fp)) {
                  std::filesystem::create_directories(fp);
                }
                fp.append(filename);
                FILE *fptr2 = NULL;
                if (!append) {
                  fptr2 = fopen(fp.u8string().c_str(), "wb");
                } else {
                  fptr2 = fopen(fp.u8string().c_str(), "ab");
                }
                if (fptr2) {
                  fwrite(bytes, msgrec.length, 1, fptr2);
                  fclose(fptr2);
                  log.log(LOG_INFO, "Saved file \"%s\"", fp.u8string().c_str());
                }
                free(bytes);
              }
            } break;
            }
          } break;
          case 2: // email to num type
            if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {
              if (msgrec.tosys == 0) {
                bool found = false;
                for (size_t k = 0; k < nlist.size(); k++) {
                  if (nlist.at(k) == config.networks.at(i).mynode) {
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  break;
                }
              }
              std::string subj;
              std::string sender;
              std::string datestr;
              std::stringstream ss;
              std::vector<std::string> msg = read_message(fptr, msgrec.length);

              if (msg.size() == 0)
                break;

              for (size_t h = 0; h < msg.at(0).size(); h++) {
                if (msg.at(0).at(h) == '\0') {
                  subj = ss.str();
                  ss.str("");
                } else {
                  ss << msg.at(0).at(h);
                }
              }

              sender = ss.str();

              datestr = msg.at(1);

              msg.erase(msg.begin(), msg.begin() + 1);
              log.log(LOG_INFO, "Importing email from %s @%d (%s)", sender.c_str(), msgrec.fromsys, config.networks.at(i).name.c_str());
              import_email(&log, msgrec.touser, sender, msgrec.fromsys, subj, msg, i, msgrec.daten);
            }
            break;
          case 7: // email to name type

            if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {

              if (msgrec.tosys == 0) {
                bool found = false;
                for (size_t k = 0; k < nlist.size(); k++) {
                  if (nlist.at(k) == config.networks.at(i).mynode) {
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  break;
                }
              }
              std::string subj;
              std::string sender;
              std::string toname;
              std::string datestr;
              std::stringstream ss;
              bool gottoname = false;
              std::vector<std::string> msg = read_message(fptr, msgrec.length);

              if (msg.size() == 0)
                break;

              for (size_t h = 0; h < msg.at(0).size(); h++) {
                if (msg.at(0).at(h) == '\0') {
                  if (!gottoname) {
                    toname = ss.str();
                    gottoname = true;
                  } else {
                    subj = ss.str();
                  }
                  ss.str("");
                } else {
                  ss << msg.at(0).at(h);
                }
              }

              sender = ss.str();

              datestr = msg.at(1);

              msg.erase(msg.begin(), msg.begin() + 1);

              import_email(&log, toname, sender, msgrec.fromsys, subj, msg, i, msgrec.daten);
            }
            break;
          case 16: {
            // main_type_sub_add_req
            std::string subtype;
            bool numericsubtype = false;
            if (msgrec.minor_type != 0) {
              numericsubtype = true;
              subtype = std::to_string(msgrec.minor_type);
            }

            std::vector<std::string> msg = read_message(fptr, msgrec.length);

            if (msg.size() == 0)
              break;

            std::stringstream ss;

            if (!numericsubtype) {
              for (size_t h = 0; h < msg.at(0).size(); h++) {
                if (msg.at(0).at(h) == '\0') {
                  subtype = ss.str();
                  ss.str("");
                  break;
                } else {
                  ss << msg.at(0).at(h);
                }
              }
            }

            struct net_header_rec rmsgrec;

            memset(&rmsgrec, 0, sizeof(struct net_header_rec));

            rmsgrec.fromsys = config.networks.at(i).mynode;
            rmsgrec.tosys = msgrec.fromsys;
            rmsgrec.main_type = 18;

            if (numericsubtype) {
              rmsgrec.minor_type = msgrec.minor_type;
            } else {
              rmsgrec.minor_type = 0;
            }

            rmsgrec.touser = 0;
            rmsgrec.fromuser = 1;
            rmsgrec.list_len = 0;
            rmsgrec.daten = (uint32_t)time(NULL);

            // check if system is on list
            int ret = check_if_subscriber(subtype, config.networks.at(i).name, msgrec.fromsys);
            uint8_t status;

            if (ret == 1) {
              // system is already subscribed
              status = 4;
            } else if (ret == 2) {
              // not host
              status = 1;
            } else if (ret == 0) {
              // system is not subscribed
              bool should_add = true;

              for (size_t a = 0; a < config.areas.size(); a++) {
                if (config.areas.at(a).subtype == subtype) {
                  if (config.areas.at(a).manual_subsciption) {
                    should_add = false;
                    break;
                  }
                }
              }
              if (should_add) {
                if (!add_subscriber(subtype, config.networks.at(i).name, msgrec.fromsys)) {
                  // failed to add
                  break;
                } else {
                  status = 0;
                  // successfully added
                }
              } else {
                status = 3;
              }
            } else {
              // fail
              log.log(LOG_ERROR, "%d tried to join sub: %s, but something went wrong.", msgrec.fromsys, subtype.c_str());
              break;
            }
            // TODO: build and send response message

            if (status == 0) {
              ss << "You have successfully joined " << subtype << "!\r\n\r\n";
              log.log(LOG_INFO, "%d joined sub: %s", msgrec.fromsys, subtype.c_str());
              std::filesystem::path welmsg(_datapath);
              welmsg.append("wwiv");
              welmsg.append(config.networks.at(i).name);
              welmsg.append("sa" + subtype + ".net");

              if (std::filesystem::exists(welmsg)) {
                std::ifstream in(welmsg);
                std::string str;
                while (getline(in, str)) {
                  ss << str << "\r\n";
                }
                in.close();
              }
            } else if (status == 3) {
              ss << "Subscribers to " << subtype << " can not be automatically added.\r\n\r\n";
              log.log(LOG_INFO, "%d tried to join sub: %s, but subscriptions are manual", msgrec.fromsys, subtype.c_str());
              std::filesystem::path welmsg(_datapath);
              welmsg.append("wwiv");
              welmsg.append(config.networks.at(i).name);
              welmsg.append("sr" + subtype + ".net");

              if (std::filesystem::exists(welmsg)) {
                std::ifstream in(welmsg);
                std::string str;
                while (getline(in, str)) {
                  ss << str << "\r\n";
                }
                in.close();
              }
            } else if (status == 4) {
              ss << "You're already subscribed to " << subtype << "!\r\n\r\n";
              log.log(LOG_INFO, "%d tried to join sub: %s, but are already joined", msgrec.fromsys, subtype.c_str());
            } else if (status == 1) {
              ss << "This system is not the host of " << subtype << "!\r\n\r\n";
              log.log(LOG_INFO, "%d tried to join sub: %s, but we are not the host", msgrec.fromsys, subtype.c_str());
            }

            rmsgrec.length = subtype.length() + 2 + ss.str().length();
            rmsgrec.tosys = host2le_s(rmsgrec.tosys);
            rmsgrec.touser = host2le_s(rmsgrec.touser);
            rmsgrec.fromsys = host2le_s(rmsgrec.fromsys);
            rmsgrec.fromuser = host2le_s(rmsgrec.fromuser);
            rmsgrec.main_type = host2le_s(rmsgrec.main_type);
            rmsgrec.minor_type = host2le_s(rmsgrec.minor_type);
            rmsgrec.list_len = host2le_s(rmsgrec.list_len);
            rmsgrec.daten = host2le_l(rmsgrec.daten);
            rmsgrec.length = host2le_l(rmsgrec.length);
            rmsgrec.method = host2le_s(rmsgrec.method);

            std::filesystem::path fspath(config.networks.at(i).outbox + "/s" + std::to_string(config.networks.at(i).upnode) + ".net");

            FILE *fptr2 = NULL;

            if (!std::filesystem::exists(fspath)) {
              // open for writing
              fptr2 = fopen(fspath.u8string().c_str(), "wb");
            } else {
              // open for appending
              fptr2 = fopen(fspath.u8string().c_str(), "ab");
            }
            if (fptr2) {
              fwrite(&rmsgrec, sizeof(struct net_header_rec), 1, fptr2);
              fwrite(subtype.c_str(), subtype.length(), 1, fptr2);
              fputc('\0', fptr2);
              fwrite(&status, 1, 1, fptr2);
              fwrite(ss.str().c_str(), ss.str().length(), 1, fptr2);
              fclose(fptr2);
            }
          } break;
          case 9: {
            if (msgrec.tosys == config.networks.at(i).mynode || msgrec.tosys == 0) {
              if (msgrec.tosys == 0) {
                bool found = false;
                for (size_t k = 0; k < nlist.size(); k++) {
                  if (nlist.at(k) == config.networks.at(i).mynode) {
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  break;
                }
              }

              uint8_t *bytes = (uint8_t *)malloc(msgrec.length);
              if (!bytes)
                break;
              if (fread(bytes, msgrec.length, 1, fptr) != 1) {
                log.log(LOG_ERROR, "Short read (2) %s", fspath.u8string().c_str());
                free(bytes);
                break;
              }

              std::filesystem::path fp(_datapath);
              fp.append("wwiv");
              fp.append(config.networks.at(i).name);
              if (!std::filesystem::exists(fp)) {
                std::filesystem::create_directories(fp);
              }

              std::string filename;
              if (msgrec.minor_type == 0) {
                filename = "subs.lst";
              } else {
                filename = "subs." + std::to_string(msgrec.minor_type);
              }

              fp.append(filename);
              FILE *fptr2 = NULL;
              fptr2 = fopen(fp.u8string().c_str(), "wb");
              if (fptr2) {
                fwrite(bytes, msgrec.length, 1, fptr2);
                fclose(fptr2);
                log.log(LOG_INFO, "Saved file \"%s\"", fp.u8string().c_str());
              }
              free(bytes);
            }
          } break;
          case 17: {
            std::string subtype;
            bool numericsubtype = false;
            if (msgrec.minor_type != 0) {
              numericsubtype = true;
              subtype = std::to_string(msgrec.minor_type);
            }

            std::vector<std::string> msg = read_message(fptr, msgrec.length);
            if (msg.size() == 0)
              break;

            std::stringstream ss;

            if (!numericsubtype) {
              for (size_t h = 0; h < msg.at(0).size(); h++) {
                if (msg.at(0).at(h) == '\0') {
                  subtype = ss.str();
                  ss.str("");
                  break;
                } else {
                  ss << msg.at(0).at(h);
                }
              }
            }

            struct net_header_rec rmsgrec;

            memset(&rmsgrec, 0, sizeof(struct net_header_rec));

            rmsgrec.fromsys = config.networks.at(i).mynode;
            rmsgrec.tosys = msgrec.fromsys;
            rmsgrec.main_type = 19;

            if (numericsubtype) {
              rmsgrec.minor_type = msgrec.minor_type;
            } else {
              rmsgrec.minor_type = 0;
            }

            rmsgrec.touser = 0;
            rmsgrec.fromuser = 1;
            rmsgrec.list_len = 0;
            rmsgrec.daten = (uint32_t)time(NULL);

            // check if system is on list
            int ret = check_if_subscriber(subtype, config.networks.at(i).name, msgrec.fromsys);
            uint8_t status;

            if (ret == 1) {
              // system is already subscribed
              bool should_remove = true;

              for (size_t a = 0; a < config.areas.size(); a++) {
                if (config.areas.at(a).subtype == subtype) {
                  if (config.areas.at(a).manual_subsciption) {
                    should_remove = false;
                    break;
                  }
                }
              }
              if (should_remove) {
                if (!remove_subscriber(subtype, config.networks.at(i).name, msgrec.fromsys)) {
                  // failed to remove
                  break;
                } else {
                  status = 0;
                  // successfully removed
                }
              } else {
                status = 3;
              }
            } else if (ret == 1) {
              // not host
              status = 1;
            } else if (ret == 0) {
              // system is not subscribed
              status = 2;
            } else {
              // fail
              log.log(LOG_ERROR, "%d tried to depart sub: %s, but something went wrong.", msgrec.fromsys, subtype.c_str());
              break;
            }
            // TODO: build and send response message

            if (status == 0) {
              ss << "You have successfully departed " << subtype << "!\r\n\r\n";
              log.log(LOG_INFO, "%d departed sub: %s", msgrec.fromsys, subtype.c_str());
            } else if (status == 3) {
              ss << "Subscribers to " << subtype << " can not be automatically removed.\r\n\r\n";
              log.log(LOG_INFO, "%d tried to depart sub: %s, but subscriptions are manual", msgrec.fromsys, subtype.c_str());

            } else if (status == 2) {
              ss << "You're not subscribed to " << subtype << "!\r\n\r\n";
              log.log(LOG_INFO, "%d tried to depart sub: %s, but is not joined", msgrec.fromsys, subtype.c_str());
            } else if (status == 1) {
              ss << "This system is not the host of " << subtype << "!\r\n\r\n";
              log.log(LOG_INFO, "%d tried to depart sub: %s, but we are not the host", msgrec.fromsys, subtype.c_str());
            }

            rmsgrec.length = subtype.length() + 2 + ss.str().length();
            rmsgrec.tosys = host2le_s(rmsgrec.tosys);
            rmsgrec.touser = host2le_s(rmsgrec.touser);
            rmsgrec.fromsys = host2le_s(rmsgrec.fromsys);
            rmsgrec.fromuser = host2le_s(rmsgrec.fromuser);
            rmsgrec.main_type = host2le_s(rmsgrec.main_type);
            rmsgrec.minor_type = host2le_s(rmsgrec.minor_type);
            rmsgrec.list_len = host2le_s(rmsgrec.list_len);
            rmsgrec.daten = host2le_l(rmsgrec.daten);
            rmsgrec.length = host2le_l(rmsgrec.length);
            rmsgrec.method = host2le_s(rmsgrec.method);

            std::filesystem::path fspath(config.networks.at(i).outbox + "/s" + std::to_string(config.networks.at(i).upnode) + ".net");

            FILE *fptr2 = NULL;

            if (!std::filesystem::exists(fspath)) {
              // open for writing
              fptr2 = fopen(fspath.u8string().c_str(), "wb");
            } else {
              // open for appending
              fptr2 = fopen(fspath.u8string().c_str(), "ab");
            }
            if (fptr2) {
              fwrite(&rmsgrec, sizeof(struct net_header_rec), 1, fptr2);
              fwrite(subtype.c_str(), subtype.length(), 1, fptr2);
              fputc('\0', fptr2);
              fwrite(&status, 1, 1, fptr2);
              fwrite(ss.str().c_str(), ss.str().length(), 1, fptr2);
              fclose(fptr2);
            }

          } break;
          case 18: {
            std::string subtype;
            uint8_t status;
            std::stringstream ss;
            std::string subject;
            std::string sender;
            bool gotsubtype = false;

            std::vector<std::string> msg = read_message(fptr, msgrec.length);

            if (msg.size() == 0)
              break;

            if (msgrec.minor_type != 0) {
              gotsubtype = true;
              subtype = std::to_string(msgrec.minor_type);
            }

            for (size_t h = 0; h < msg.at(0).size(); h++) {
              if (msg.at(0).at(h) == '\0') {
                if (!gotsubtype) {
                  subtype = ss.str();
                  gotsubtype = true;
                  ss.str("");
                } else if (ss.str().size() > 0) {
                  status = (uint8_t)ss.str().at(0);
                  subject = ss.str().substr(1);
                  ss.str("");
                } else {
                  ss << msg.at(0).at(h);
                }
              } else {
                ss << msg.at(0).at(h);
              }
            }
            sender = ss.str();

            msg.erase(msg.begin(), msg.begin() + 1);
            std::string stat_msg;

            switch (status) {
            case 0:
              stat_msg = "|10SUCCESS - You have been successfully added to the area.|07";
              break;
            case 1:
              stat_msg = "|12FAILED - I (" + std::to_string(msgrec.fromsys) + ") am not the host!|07";
              break;
            case 3:
              stat_msg = "|12FAILED - Not allowed to add subscribers automatically.|07";
              break;
            case 4:
              stat_msg = "|12FAILED - You are already subscribed!|07";
              break;
            default:
              stat_msg = "|12ERROR - Unknown status byte " + std::to_string(status);
              break;
            }

            msg.insert(msg.begin(), stat_msg);

            import_email(&log, 1, sender, msgrec.fromsys, subject, msg, i, msgrec.daten);

          } break;
          case 19: {
            std::string subtype;
            uint8_t status = 0;
            std::stringstream ss;
            std::string subject;
            std::string sender;
            bool gotsubtype = false;

            std::vector<std::string> msg = read_message(fptr, msgrec.length);

            if (msg.size() == 0)
              break;

            if (msgrec.minor_type != 0) {
              gotsubtype = true;
              subtype = std::to_string(msgrec.minor_type);
            }

            for (size_t h = 0; h < msg.at(0).size(); h++) {
              if (msg.at(0).at(h) == '\0') {
                if (!gotsubtype) {
                  subtype = ss.str();
                  gotsubtype = true;
                  ss.str("");
                } else if (ss.str().size() > 0) {
                  status = (uint8_t)ss.str().at(0);
                  subject = ss.str().substr(1);
                  ss.str("");
                } else {
                  ss << msg.at(0).at(h);
                }
              } else {
                ss << msg.at(0).at(h);
              }
            }
            sender = ss.str();

            msg.erase(msg.begin(), msg.begin() + 1);
            std::string stat_msg;

            switch (status) {
            case 0:
              stat_msg = "|10SUCCESS - You have been successfully removed from the area.|07";
              break;
            case 1:
              stat_msg = "|12FAILED - I (" + std::to_string(msgrec.fromsys) + ") am not the host!|07";
              break;
            case 3:
              stat_msg = "|12FAILED - Not allowed to remove subscribers automatically.|07";
              break;
            case 2:
              stat_msg = "|12FAILED - You are not subscribed!|07";
              break;
            default:
              stat_msg = "|12ERROR - Unknown status byte " + std::to_string(status);
              break;
            }

            msg.insert(msg.begin(), stat_msg);

            import_email(&log, 1, sender, msgrec.fromsys, subject, msg, i, msgrec.daten);
          } break;
          case 20: {
            // sub.inf ping
            if (msgrec.minor_type == 0) {
              std::stringstream rmsgtxt;
              bool should_send = false;
              for (size_t s = 0; s < config.areas.size(); s++) {
                if (config.areas.at(s).netname == config.networks.at(i).name) {
                  if (config.areas.at(s).mynode == config.areas.at(s).hostnode && config.areas.at(s).hidden_sub == false) {
                    char buffer[256];
                    std::string flags;

                    if (!config.areas.at(s).manual_subsciption) {
                      flags = "R";
                    } else {
                      flags = "";
                    }
                    std::string desc;
                    if (config.areas.at(s).description.length() > 60) {
                      desc = config.areas.at(s).description.substr(0, 60);
                    } else {
                      desc = config.areas.at(s).description;
                    }
                    snprintf(buffer, 256, "%-7s %5u %-5s %s~%u", config.areas.at(s).subtype.c_str(), config.areas.at(s).mynode, flags.c_str(), desc.c_str(),
                             config.areas.at(s).category);
                    rmsgtxt << buffer << "\r\n";
                    should_send = true;
                  }
                }
              }

              if (should_send) {
                struct net_header_rec rmsg;

                rmsg.fromsys = host2le_s(config.networks.at(i).mynode);
                rmsg.touser = msgrec.fromuser;
                rmsg.fromuser = host2le_s(1);
                rmsg.daten = host2le_l(time(NULL));
                rmsg.main_type = host2le_s(20);
                rmsg.minor_type = host2le_s(1);
                rmsg.method = 0;
                rmsg.length = host2le_l(rmsgtxt.str().length());
                rmsg.list_len = 0;
                FILE *fptr2 = NULL;

                std::filesystem::path fspath(config.networks.at(i).outbox + "/s" + std::to_string(config.networks.at(i).upnode) + ".net");

                if (!std::filesystem::exists(fspath)) {
                  // open for writing
                  fptr2 = fopen(fspath.u8string().c_str(), "wb");
                } else {
                  // open for appending
                  fptr2 = fopen(fspath.u8string().c_str(), "ab");
                }
                if (fptr2) {
                  fwrite(&rmsg, sizeof(struct net_header_rec), 1, fptr2);
                  fwrite(rmsgtxt.str().c_str(), rmsgtxt.str().length(), 1, fptr2);
                  fclose(fptr2);
                }
              }
            } else if (msgrec.minor_type == 1) {
              // do nothing.. we didn't request this.
            }
          } break;
          case 26: // main type post
          {
            std::string subtype;
            std::string subject;
            std::string sender;
            std::stringstream ss;
            bool gotsubtype = false;

            std::vector<std::string> msg = read_message(fptr, msgrec.length);

            if (msg.size() == 0)
              break;

            if (msgrec.minor_type != 0) {
              gotsubtype = true;
              subtype = std::to_string(msgrec.minor_type);
            }

            for (size_t h = 0; h < msg.at(0).size(); h++) {
              if (msg.at(0).at(h) == '\0') {
                if (!gotsubtype) {
                  subtype = ss.str();
                  gotsubtype = true;
                } else {
                  subject = ss.str();
                }
                ss.str("");
              } else {
                ss << msg.at(0).at(h);
              }
            }
            sender = ss.str();

            msg.erase(msg.begin(), msg.begin() + 1);

            bool should_import = true;

            for (size_t a = 0; a < config.areas.size(); a++) {
              if (strcasecmp(config.areas.at(a).subtype.c_str(), subtype.c_str()) == 0 &&
                  strcasecmp(config.areas.at(a).netname.c_str(), config.networks.at(i).name.c_str()) == 0) {

                if (config.areas.at(a).hostnode == config.areas.at(a).mynode) {
                  // I'm the host node, send to subscribers

                  std::vector<uint16_t> subscribers;

                  subscribers = get_subscribers(_datapath, subtype, config.networks.at(i).name);

                  should_import = false;
                  if (subscribers.size() > 1) {
                    size_t s;
                    for (s = 0; s < subscribers.size(); s++) {
                      if (subscribers.at(s) == msgrec.fromsys) {
                        should_import = true;
                        break;
                      }
                    }
                    if (should_import) {
                      // remove sender from outlist
                      subscribers.erase(subscribers.begin() + s);

                      // send out message

                      struct net_header_rec rmsg;

                      rmsg.fromsys = host2le_s(msgrec.fromsys);
                      rmsg.touser = 0;
                      rmsg.fromuser = host2le_s(msgrec.fromuser);
                      rmsg.daten = host2le_l(msgrec.daten);
                      rmsg.main_type = host2le_s(msgrec.main_type);
                      rmsg.minor_type = host2le_s(msgrec.minor_type);
                      rmsg.method = host2le_s(msgrec.method);

                      if (msgrec.minor_type == 0) {
                        rmsg.length = host2le_l(subtype.length() + subject.length() + sender.length() + calc_length(msg) + 3);
                      } else {
                        rmsg.length = host2le_l(subject.length() + sender.length() + calc_length(msg) + 2);
                      }

                      FILE *fptr2 = NULL;

                      std::filesystem::path fspath(config.networks.at(i).outbox + "/s" + std::to_string(config.networks.at(i).upnode) + ".net");

                      if (!std::filesystem::exists(fspath)) {
                        // open for writing
                        fptr2 = fopen(fspath.u8string().c_str(), "wb");
                      } else {
                        // open for appending
                        fptr2 = fopen(fspath.u8string().c_str(), "ab");
                      }
                      if (fptr2) {
                        if (subscribers.size() > 1) {
                          rmsg.tosys = 0;
                          rmsg.list_len = host2le_s(subscribers.size());
                          fwrite(&rmsg, sizeof(struct net_header_rec), 1, fptr2);
                          for (size_t su = 0; su < subscribers.size(); su++) {
                            uint16_t subscriber = host2le_s(subscribers.at(su));
                            fwrite(&subscriber, sizeof(uint16_t), 1, fptr2);
                          }
                          log.log(LOG_INFO, "Relaying message to %d subscribers", subscribers.size());

                        } else {
                          log.log(LOG_INFO, "Relaying message to node %d", subscribers.at(0));
                          rmsg.tosys = host2le_s(subscribers.at(0));
                          rmsg.list_len = 0;
                          fwrite(&rmsg, sizeof(struct net_header_rec), 1, fptr2);
                        }

                        if (msgrec.minor_type == 0) {
                          fwrite(subtype.c_str(), subtype.size() + 1, 1, fptr2);
                        }

                        fwrite(subject.c_str(), subject.size(), 1, fptr2);
                        fwrite("\0", 1, 1, fptr2);
                        fwrite(sender.c_str(), sender.size(), 1, fptr2);
                        fwrite("\r\n", 2, 1, fptr2);

                        for (size_t ml = 0; ml < msg.size(); ml++) {
                          fwrite(msg.at(ml).c_str(), msg.at(ml).size(), 1, fptr2);
                          fwrite("\r\n", 2, 1, fptr2);
                        }
                        fclose(fptr2);
                      }
                    }
                  } else if (subscribers.size() == 1) {
                    if (subscribers.at(0) == msgrec.fromsys) {
                      should_import = true;
                    }
                  }
                  break;
                }
              }
            }
            if (should_import) {
              log.log(LOG_INFO, "Importing message %s by %s @ %d (%s)", subject.c_str(), sender.c_str(), msgrec.fromsys, config.networks.at(i).name.c_str());
              import_message(&log, subtype, sender, msgrec.fromsys, subject, msg, i, msgrec.daten);
            }
          }

          break;
          }
        }

        fclose(fptr);
        try {
          std::filesystem::remove(fspath);
        } catch (std::exception const &) {
          log.log(LOG_ERROR, "Failed to remove file %s", fspath.u8string().c_str());
        }
      }
    }
  }
}

int Tosser::calc_length(std::vector<std::string> msg) {
  int ret = 0;

  for (size_t i = 0; i < msg.size(); i++) {
    ret += msg.at(i).length() + 2;
  }

  return ret;
}
