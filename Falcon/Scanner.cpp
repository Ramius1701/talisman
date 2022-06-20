#include "../Common/INIReader.h"
#include "../Common/Squish.h"
#include "../Common/wwivnet.h"
#include "../Common/Logger.h"
#include "Config.h"
#include "Scanner.h"
#include <filesystem>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#ifdef _MSC_VER
#define strcasecmp stricmp
#else
#include <endian.h>
#endif

static inline uint16_t host2le_s(uint16_t s) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return s;
#else
  return (((s >> 8) & 0xffu) | ((s & 0xffu) << 8));
#endif
}

static inline uint32_t host2le_l(uint32_t s) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return s;
#else
  return (((s & 0xff000000u) >> 24) | ((s & 0x00ff0000u) >> 8) | ((s & 0x0000ff00u) << 8) | ((s & 0x000000ffu) << 24));
#endif
}

bool Scanner::open_user_database(sqlite3 **db) {
  static const char *create_users_sql =
      "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";

  int rc;
  char *err_msg = NULL;
  std::string filepath = _datapath + "/users.sqlite3";
  if (sqlite3_open(filepath.c_str(), db) != SQLITE_OK) {
    // std::cerr << "Unable to open database: users.db" << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create user table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }

  return true;
}

int Scanner::username_to_id(const char *uname) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id FROM users WHERE username = ?";

  int id = -1;

  if (!open_user_database(&db)) {
    return -1;
  }

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return -1;
  }
  sqlite3_bind_text(stmt, 1, uname, -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return id;
}

void Scanner::run() {
  INIReader inir("talisman.ini");
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
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

  // check email for outbound
  sq_msg_base_t *mb;

  for (size_t i = 0; i < config.networks.size(); i++) {

    mb = SquishOpenMsgBase(std::string(_msgpath + "/" + config.networks.at(i).emailbase).c_str());

    if (!mb) {
      log.log(LOG_ERROR, "Unable to open message base %s", config.networks.at(i).emailbase.c_str());
      continue;
    }

    std::filesystem::path fspath(config.networks.at(i).outbox + "/s" + std::to_string(config.networks.at(i).upnode) + ".net");

    FILE *fptr = NULL;

    for (size_t mid = 1; mid <= mb->basehdr.num_msg; mid++) {

      sq_msg_t *msg = SquishReadMsg(mb, mid);
      if (!msg) {
        // log.log(LOG_ERROR, "Error Reading Message %d from %s", mid, std::string(_msgpath + "/" + c.areas.at(i).file).c_str());
        break;
      }

      if (msg->xmsg.attr & MSGLOCAL && !(msg->xmsg.attr & MSGSENT)) {

        if (fptr == NULL) {
          if (std::filesystem::exists(fspath)) {
            fptr = fopen(fspath.u8string().c_str(), "ab");
          } else {
            fptr = fopen(fspath.u8string().c_str(), "wb");
          }
          if (!fptr) {
            log.log(LOG_ERROR, "Error opening %s", fspath.u8string().c_str());
            break;
          }
        }

        struct net_header_rec msgrec;

        memset(&msgrec, 0, sizeof(struct net_header_rec));

        msgrec.fromsys = host2le_s((uint16_t)config.networks.at(i).mynode);
        msgrec.tosys = host2le_s(msg->xmsg.dest.node);
        msgrec.main_type = host2le_s(7);
        int id = username_to_id(msg->xmsg.from);
        if (id > 0) {
          msgrec.fromuser = host2le_s((uint16_t)id);
        }
        std::stringstream ss;

        for (int i = 0; i < msg->msg_len; i++) {
          if (msg->msg[i] == '\r') {
            ss << "\r\n";
          } else if (msg->msg[i] != '\n') {
            ss << msg->msg[i];
          }
        }

        char buffer[90];

        memset(buffer, 0, sizeof buffer);

        struct tm localtm;

        memset(&localtm, 0, sizeof(struct tm));

        localtm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980 - 1900;
        localtm.tm_mday = msg->xmsg.date_written.date & 31;
        localtm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
        localtm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
        localtm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
        localtm.tm_sec = msg->xmsg.date_written.time & 31;
        localtm.tm_isdst = -1;

        mktime(&localtm);
        snprintf(buffer, sizeof buffer, "%s %s %2d %02d:%02d:%02d %4d", days[localtm.tm_wday], months[localtm.tm_mon], localtm.tm_mday, localtm.tm_hour,
                 localtm.tm_min, localtm.tm_sec, localtm.tm_year + 1900);

        char buffer2[256];
        snprintf(buffer2, sizeof buffer2, "%s #%d @%d", msg->xmsg.from, id, config.networks.at(i).mynode);

        msgrec.daten = host2le_l((uint32_t)time(NULL));
        msgrec.length = strlen(msg->xmsg.to) + 1;
        msgrec.length += strlen(msg->xmsg.subject) + 1;
        msgrec.length += strlen(buffer2) + 2;
        msgrec.length += strlen(buffer) + 2;
        msgrec.length += ss.str().size() + 1;

        msgrec.length = host2le_l(msgrec.length);

        fwrite(&msgrec, sizeof(net_header_rec), 1, fptr);
        fwrite(msg->xmsg.to, strlen(msg->xmsg.to) + 1, 1, fptr);
        fwrite(msg->xmsg.subject, strlen(msg->xmsg.subject) + 1, 1, fptr);
        fwrite(buffer2, strlen(buffer2), 1, fptr);
        fwrite("\r\n", 2, 1, fptr);
        fwrite(buffer, strlen(buffer), 1, fptr);
        fwrite("\r\n", 2, 1, fptr);
        fwrite(ss.str().c_str(), ss.str().size(), 1, fptr);
        fwrite("\x1a", 1, 1, fptr);

        msg->xmsg.attr |= MSGSENT;
        SquishLockMsgBase(mb);
        SquishUpdateHdr(mb, msg);
        SquishUnlockMsgBase(mb);
      }
      SquishFreeMsg(msg);
    }
    SquishCloseMsgBase(mb);

    // loop areas and add messages to packets
    for (size_t a = 0; a < config.areas.size(); a++) {
      if (strcasecmp(config.areas.at(a).netname.c_str(), config.networks.at(i).name.c_str()) == 0) {
        mb = SquishOpenMsgBase(std::string(_msgpath + "/" + config.areas.at(a).basefile).c_str());

        if (!mb) {
          log.log(LOG_ERROR, "Unable to open message base! %s", config.areas.at(a).basefile.c_str());
          continue;
        }
        for (size_t mid = 1; mid <= mb->basehdr.num_msg; mid++) {

          sq_msg_t *msg = SquishReadMsg(mb, mid);
          if (!msg) {
            // log.log(LOG_ERROR, "Error Reading Message %d from %s", mid, std::string(_msgpath + "/" + c.areas.at(i).file).c_str());
            break;
          }

          if (msg->xmsg.attr & MSGLOCAL && !(msg->xmsg.attr & MSGSENT)) {

            if (fptr == NULL) {
              if (std::filesystem::exists(fspath)) {
                fptr = fopen(fspath.u8string().c_str(), "ab");
              } else {
                fptr = fopen(fspath.u8string().c_str(), "wb");
              }
              if (!fptr) {
                log.log(LOG_ERROR, "Error opening %s", fspath.u8string().c_str());
                break;
              }
            }

            struct net_header_rec msgrec;

            memset(&msgrec, 0, sizeof(struct net_header_rec));

            msgrec.fromsys = host2le_s((uint16_t)config.networks.at(i).mynode);
            msgrec.tosys = host2le_s(config.areas.at(a).hostnode);
            msgrec.main_type = host2le_s(26);
            int id = username_to_id(msg->xmsg.from);
            if (id > 0) {
              msgrec.fromuser = host2le_s((uint16_t)id);
            }
            std::stringstream ss;

            for (int i = 0; i < msg->msg_len; i++) {
              if (msg->msg[i] == '\r') {
                ss << "\r\n";
              } else if (msg->msg[i] != '\n') {
                ss << msg->msg[i];
              }
            }

            char buffer[90];

            memset(buffer, 0, sizeof buffer);

            struct tm localtm;
            memset(&localtm, 0, sizeof(struct tm));

            localtm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980 - 1900;
            localtm.tm_mday = msg->xmsg.date_written.date & 31;
            localtm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
            localtm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
            localtm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
            localtm.tm_sec = msg->xmsg.date_written.time & 31;
            localtm.tm_isdst = -1;
            mktime(&localtm);
            snprintf(buffer, sizeof buffer, "%s %s %2d %02d:%02d:%02d %4d", days[localtm.tm_wday], months[localtm.tm_mon], localtm.tm_mday, localtm.tm_hour,
                     localtm.tm_min, localtm.tm_sec, localtm.tm_year + 1900);

            msgrec.daten = host2le_l((uint32_t)time(NULL));
            msgrec.length = strlen(config.areas.at(a).subtype.c_str()) + 1;

            char buffer2[256];

            snprintf(buffer2, sizeof buffer2, "%s #%d @%d", msg->xmsg.from, id, config.networks.at(i).mynode);

            msgrec.length += strlen(msg->xmsg.subject) + 1;
            msgrec.length += strlen(buffer2) + 2;
            msgrec.length += strlen(buffer) + 2;
            msgrec.length += ss.str().size() + 1;
            msgrec.length = host2le_l(msgrec.length);
            fwrite(&msgrec, sizeof(net_header_rec), 1, fptr);
            fwrite(config.areas.at(a).subtype.c_str(), strlen(config.areas.at(a).subtype.c_str()) + 1, 1, fptr);
            fwrite(msg->xmsg.subject, strlen(msg->xmsg.subject) + 1, 1, fptr);
            fwrite(buffer2, strlen(buffer2), 1, fptr);
            fwrite("\r\n", 2, 1, fptr);
            fwrite(buffer, strlen(buffer), 1, fptr);
            fwrite("\r\n", 2, 1, fptr);
            fwrite(ss.str().c_str(), ss.str().size(), 1, fptr);
            fwrite("\x1a", 1, 1, fptr);
            msg->xmsg.attr |= MSGSENT;
            SquishLockMsgBase(mb);
            SquishUpdateHdr(mb, msg);
            SquishUnlockMsgBase(mb);
          }
          SquishFreeMsg(msg);
        }
        SquishCloseMsgBase(mb);
      }
    }
    if (fptr != NULL) {
      fclose(fptr);
    }
  }
}
