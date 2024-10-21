#include "../Common/Logger.h"
#include "Editor.h"
#include "Email.h"
#include "Node.h"
#include "Qwk.h"
#include "bluewave.h"
#include "MessageReader.h"
#include "Protocol.h"
#include <cstring>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <fstream>

bool Email::open_database(std::string filename, sqlite3 **db) {
  const char *create_users_sql = "CREATE TABLE IF NOT EXISTS email(id INTEGER PRIMARY KEY, sender TEXT COLLATE NOCASE, recipient TEXT COLLATE NOCASE, subject "
                                 "TEXT, body TEXT, date INTEGER, seen INTEGER)";

  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
    std::cerr << "Unable to open database: " << filename << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    std::cerr << "Unable to create email table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

int Email::unread_email(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "SELECT COUNT(*) FROM email WHERE recipient=? AND seen=\"0\"";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare save_message (email) sql");
    sqlite3_close(db);
    return 0;
  }
  std::string uname = n->get_user().get_username();

  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return 0;
}

int Email::count_email(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "SELECT COUNT(*) FROM email WHERE recipient=?";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare save_message (email) sql");
    sqlite3_close(db);
    return 0;
  }
  std::string uname = n->get_user().get_username();

  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return 0;
}

bool Email::save_message(Node *n, std::string to, std::string from, std::string subject, std::vector<std::string> msg) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "INSERT INTO email (sender, recipient, subject, body, date, seen) VALUES(?, ?, ?, ?, ?, 0)";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare save_message (email) sql");
    sqlite3_close(db);
    return false;
  }

  std::stringstream ss;

  for (size_t i = 0; i < msg.size(); i++) {
    ss << msg.at(i) << "\r";
  }

  std::string msgstr = ss.str();

  time_t now = time(NULL);

  sqlite3_bind_text(stmt, 1, from.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 2, to.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 3, subject.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 4, msgstr.c_str(), -1, NULL);
  sqlite3_bind_int64(stmt, 5, now);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return true;
}

bool Email::load_emails(Node *n, std::vector<Email> *emails) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "SELECT id, sender, subject, body, date, seen FROM email WHERE recipient = ?";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
    sqlite3_close(db);
    return false;
  }
  std::string uname = n->get_user().get_username();
  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Email e;
    std::string body;
    std::stringstream ss;

    e.id = sqlite3_column_int(stmt, 0);
    e.sender = std::string((const char *)sqlite3_column_text(stmt, 1));
    e.subject = std::string((const char *)sqlite3_column_text(stmt, 2));
    body = std::string((const char *)sqlite3_column_text(stmt, 3));
    e.date = sqlite3_column_int64(stmt, 4);
    e.seen = (sqlite3_column_int(stmt, 5) == 1);

    for (size_t i = 0; i < body.size(); i++) {
      if (body[i] == '\r') {
        e.msg.push_back(ss.str());
        ss.str("");
      } else {
        ss << body[i];
      }
    }

    emails->push_back(e);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return true;
}

void Email::list_email(Node *n) {

  std::vector<Email> emails;
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  bool reload;

  size_t lines = 1;
  while (true) {

    emails.clear();
    if (!load_emails(n, &emails)) {
      return;
    }

    reload = false;

    n->cls();
    n->print_f("|09 Msg#    Subject                          From             Date            |07\r\n");
    for (size_t i = 0; i < emails.size(); i++) {
      struct tm time_tm;
#ifdef _MSC_VER
      localtime_s(&time_tm, &emails.at(i).date);
#else
      localtime_r(&emails.at(i).date, &time_tm);
#endif

      if (emails.at(i).seen) {
        n->print_f("|08[|15%6d|08] |14%-32.32s |13%-16.16s |11%02d:%02d %s %02d\r\n", i + 1, emails.at(i).subject.c_str(), emails.at(i).sender.c_str(),
                   time_tm.tm_hour, time_tm.tm_min, months[time_tm.tm_mon], time_tm.tm_mday);
      } else {
        n->print_f("|08[|15%6d|08]|12*|14%-32.32s |13%-16.16s |11%02d:%02d %s %02d\r\n", i + 1, emails.at(i).subject.c_str(), emails.at(i).sender.c_str(),
                   time_tm.tm_hour, time_tm.tm_min, months[time_tm.tm_mon], time_tm.tm_mday);
      }

      lines++;
      if (lines == n->get_term_height() - 2) {
        n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", 1, emails.size());

        std::string res = n->get_string(6, false);

        if (res.size() == 0) {
          lines = 1;
          n->cls();
          n->print_f("|09 Msg#    Subject                          From             Date            |07\r\n");
          continue;
        } else if (tolower(res[0]) == 'q') {
          return;
        } else {
          size_t emailno;
          try {
            emailno = (size_t)(std::stoi(res) - 1);
            while (emailno >= 0 && emailno < emails.size()) {
              int ret = view_email(n, emails.at(emailno), emailno, emails.size());

              emails.at(emailno).seen = true;

              if (ret == 0) {
                lines = 0;
                reload = true;
                break;
              } else {
                emailno += ret;
              }
            }
          } catch (std::invalid_argument const &) {
            return;
          } catch (std::out_of_range const &) {
            return;
          }
          if (reload) {
            break;
          }
        }
      }
    }

    if (reload) {
      continue;
    }

    n->print_f("|14Select |08[|15%d|08-|15%d|08], |15ENTER|08=|14Quit |07", 1, emails.size());
    std::string res = n->get_string(6, false);

    if (res.size() == 0) {
      return;
    } else {
      size_t emailno;
      try {
        emailno = (size_t)(std::stoi(res) - 1);
        // view email
        while (emailno >= 0 && emailno < emails.size()) {
          int ret = view_email(n, emails.at(emailno), emailno, emails.size());
          emails.at(emailno).seen = true;
          if (ret == 0) {
            lines = 0;
            break;
          } else {
            emailno += ret;
          }
        }
      } catch (std::invalid_argument const &) {
        return;
      } catch (std::out_of_range const &) {
        return;
      }
    }
  }
}

int Email::view_email(Node *n, Email e, int emailno, int tot_emails) {
  struct tm time_tm;
  size_t lines = 0;
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "UPDATE email SET seen=1 WHERE id=?";

  if (!e.seen) {
    if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
      n->log->log(LOG_ERROR, "Unable to open email sqlite database");
      return 0;
    }

    if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
      n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
      sqlite3_close(db);
      return 0;
    }

    sqlite3_bind_int(stmt, 1, e.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
  }
#ifdef _MSC_VER
  localtime_s(&time_tm, &e.date);
#else
  localtime_r(&e.date, &time_tm);
#endif

  struct msg_reader_msg_t msg;

  msg.subject = e.subject;
  msg.from = e.sender;
  msg.to = n->get_user().get_username();
  char datebuf[17];
  snprintf(datebuf, 17, "%04d-%02d-%02d %02d:%02d", time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min);
  msg.date = std::string(datebuf);
  msg.msg_no = emailno + 1;
  msg.msg_serial = e.id;
  msg.area = NULL;
  msg.destaddr = NULL;
  msg.origaddr = NULL;
  msg.msg_type = 3;
  msg.showkluges = false;
  msg.ansi = false;
  msg.body = new std::vector<struct line_t>();
  std::stringstream ss;

  for (size_t i = 0; i < e.msg.size(); i++) {
    struct line_t line;

    line.line = e.msg.at(i);

    if (e.msg.at(i).find('>') < 5) {
      line.type = 1;
    } else {
      line.type = 0;
    }

    msg.body->push_back(line);
  }

  while (true) {
    int ret = MessageReader::read_message(n, &msg, tot_emails, DISABLE_SEARCH | DISABLE_UNREAD);

    switch (ret) {
    case 0:
      delete msg.body;
      return 0;
    case 2:
      delete_email(n, e.id);
      delete msg.body;
      return 0;
    case 5:
      delete msg.body;
      return 1;
    case 6:
      delete msg.body;
      return -1;
    case 3: {
      std::vector<std::string> quotemsg;

      for (size_t i = 0; i < e.msg.size(); i++) {
        if (e.msg.at(i).size() > 70) {
          std::vector<std::string> newline = MsgArea::word_wrap(e.msg.at(i), 70);
          for (size_t y = 0; y < newline.size(); y++) {
            quotemsg.push_back(" > " + newline.at(y));
          }
        } else {
          quotemsg.push_back(" > " + e.msg.at(i));
        }
      }

      std::vector<std::string> newmsg = Editor::enter_message(n, e.sender, e.subject, "E-Mail", true, &quotemsg);
      if (newmsg.size() > 0) {
        if (n->get_user().get_attribute("signature_enabled", "false") == "true") {
          MsgArea::attach_sig(&newmsg, n->get_user().get_attribute("signature", ""));
        }
        Email::save_message(n, e.sender, n->get_user().get_username(), e.subject, newmsg);
      }
    }
      delete msg.body;
      return 0;
    case 8:
      download(n, &e);
      break;
    default:
      break;
    }
  }
}

void Email::set_all_seen(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char sql[] = "UPDATE email SET seen=1 WHERE recipient = ?";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
    sqlite3_close(db);
    return;
  }

  std::string uname = n->get_user().get_username();

  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void Email::delete_email(Node *n, int id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char sql[] = "DELETE FROM email WHERE id=?";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_int(stmt, 1, id);
  sqlite3_step(stmt);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

static int ieee_to_msbin(float *src4, float *dest4) {
  unsigned char *ieee = (unsigned char *)src4;
  unsigned char *msbin = (unsigned char *)dest4;
  unsigned char sign = 0x00;
  unsigned char msbin_exp = 0x00;
  int i;
  /* See _fmsbintoieee() for details of formats   */
  sign = ieee[3] & 0x80;
  msbin_exp |= ieee[3] << 1;
  msbin_exp |= ieee[2] >> 7;
  /* An ieee exponent of 0xfe overflows in MBF    */
  if (msbin_exp == 0xfe)
    return 1;
  msbin_exp += 2; /* actually, -127 + 128 + 1 */
  for (i = 0; i < 4; i++)
    msbin[i] = 0;
  msbin[3] = msbin_exp;
  msbin[2] |= sign;
  msbin[2] |= ieee[2] & 0x7f;
  msbin[1] = ieee[1];
  msbin[0] = ieee[0];

  *dest4 = (float)convertl((tLONG)*dest4);
  return 0;
}

int Email::bwave_scan(Node *n, FILE *fti_file, FILE *mix_file, FILE *dat_file, int *last_ptr) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  MIX_REC mix;
  FTI_REC fti;

  long mixptr = ftell(fti_file);
  const char *month_name[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  // char buffer[256];

  int tot_msgs = 0;
  int area_msgs = 0;
  char *body;
  time_t thetime;
  struct tm timeStruct;
  static const char sql[] = "SELECT id, sender, subject, body, date FROM email WHERE recipient = ? AND seen = 0";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
    sqlite3_close(db);
    return 0;
  }
  std::string uname = n->get_user().get_username();
  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    memset(&fti, 0, sizeof(FTI_REC));
    strncpy((char *)fti.from, (char *)sqlite3_column_text(stmt, 1), sizeof(fti.from) - 1);
    strncpy((char *)fti.to, n->get_user().get_username().c_str(), sizeof(fti.to) - 1);
    strncpy((char *)fti.subject, (char *)sqlite3_column_text(stmt, 2), sizeof(fti.subject) - 1);
    thetime = sqlite3_column_int64(stmt, 4);

#ifdef _MSC_VER
    localtime_s(&timeStruct, &thetime);
#else
    localtime_r(&thetime, &timeStruct);
#endif
    snprintf((char *)fti.date, sizeof fti.date, "%02d-%s-%04d %02d:%02d", timeStruct.tm_mday, month_name[timeStruct.tm_mon], timeStruct.tm_year + 1900,
             timeStruct.tm_hour, timeStruct.tm_min);
    fti.msgnum = converts((tWORD)sqlite3_column_int(stmt, 0));
    body = strdup((char *)sqlite3_column_text(stmt, 3));
    fti.replyto = 0;
    fti.replyat = 0;
    fti.msgptr = convertl(*last_ptr);
    fti.msglength = convertl(strlen(body));

    *last_ptr += strlen(body);
    fti.flags |= FTI_MSGLOCAL;
    fti.flags = converts(fti.flags);
    fti.orig_zone = 0;
    fti.orig_net = 0;
    fti.orig_node = 0;
    fwrite(body, 1, strlen(body), dat_file);
    fwrite(&fti, sizeof(FTI_REC), 1, fti_file);
    free(body);
    area_msgs++;
    tot_msgs++;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  memset(&mix, 0, sizeof(MIX_REC));

  snprintf((char *)mix.areanum, 6, "%d", 1);
  mix.totmsgs = converts(area_msgs);
  mix.numpers = converts(area_msgs);
  mix.msghptr = convertl(mixptr);
  fwrite(&mix, sizeof(MIX_REC), 1, mix_file);

  return tot_msgs;
}

int Email::qwk_scan(Node *n, FILE *msgs_dat_fptr, FILE *pers_ndx_fptr, FILE *conf_ndx_fptr, int tot) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  char buffer[256];
  static const char sql[] = "SELECT id, sender, subject, body, date FROM email WHERE recipient = ? AND seen = 0";

  if (!open_database(n->get_config()->data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return tot;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to open prepare email sqlite query");
    sqlite3_close(db);
    return tot;
  }
  std::string uname = n->get_user().get_username();
  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string subject((const char *)sqlite3_column_text(stmt, 2));
    std::string sender((const char *)sqlite3_column_text(stmt, 1));
    time_t date = sqlite3_column_int64(stmt, 4);
    struct tm datetm;
#ifdef _MSC_VER
    localtime_s(&datetm, &date);
#else
    localtime_r(&date, &datetm);
#endif

    int hour = datetm.tm_hour;
    int minute = datetm.tm_min;

    int day = datetm.tm_mday;
    int month = datetm.tm_mon + 1;
    int year = datetm.tm_year + 1900;

    std::stringstream msgss;

    unsigned int msgid = sqlite3_column_int(stmt, 0);
    std::string msgbody((const char *)sqlite3_column_text(stmt, 3));
    std::stringstream extra;
    struct QwkHeader q;

    uint32_t ndx = ftell(msgs_dat_fptr);
    float fndx;
    float mndx;
    uint8_t zero = 0;

    fndx = (float)ndx;
    ieee_to_msbin(&fndx, &mndx);

    q.Msgstat = ' ';
    snprintf(buffer, 7, "%d", msgid);

    memset(q.Msgnum, ' ', 7);
    memcpy(q.Msgnum, buffer, strlen(buffer));

    snprintf(buffer, sizeof buffer, "%02d-%02d-%02d", month, day, year - 2000);
    memcpy(q.Msgdate, buffer, 8);

    snprintf(buffer, sizeof buffer, "%02d:%02d", hour, minute);
    memcpy(q.Msgtime, buffer, 5);

    memset(q.Msgpass, ' ', 12);
    memset(q.Msgrply, ' ', 8);

    memset(q.MsgSubj, ' ', 25);

    memset(q.MsgTo, ' ', 25);

    std::stringstream mbody2;
    mbody2.str("");
    for (size_t i = 0; i < msgbody.length(); i++) {
      if (msgbody.at(i) != '\n') {
        mbody2 << msgbody.at(i);
      }
    }

    msgbody = mbody2.str();

    extra.str("");

    if (uname.size() > 25) {
      extra << "To: " << uname << "\r";
      memcpy(q.MsgTo, uname.c_str(), 25);
    } else {
      memcpy(q.MsgTo, uname.c_str(), uname.length());
    }

    memset(q.MsgFrom, ' ', 25);
    if (sender.length() > 25) {
      extra << "From: " << sender << "\r";
      memcpy(q.MsgFrom, sender.c_str(), 25);
    } else {
      memcpy(q.MsgFrom, sender.c_str(), sender.length());
    }

    if (subject.length() > 25) {
      extra << "Subject: " << subject << "\r";
      memcpy(q.MsgSubj, subject.c_str(), 25);
    } else {
      memcpy(q.MsgSubj, subject.c_str(), subject.length());
    }

    if (extra.str().length() > 0) {
      extra << "\r" << msgbody;
      msgbody = extra.str();
    }

    size_t len = msgbody.length() / 128;

    if (len * 128 < msgbody.length()) {
      len++;
    }

    int lenbytes = len * 128;

    char *msgbuf = (char *)malloc(lenbytes);

    if (!msgbuf) {
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return tot;
    }

    memset(msgbuf, ' ', lenbytes);

    for (size_t i = 0; i < msgbody.length(); i++) {
      if (msgbody.c_str()[i] == '\r') {
        msgbuf[i] = '\xe3';
      } else {
        msgbuf[i] = msgbody.c_str()[i];
      }
    }

    snprintf(buffer, 7, "%lu", len + 1);
    memset(q.Msgrecs, ' ', 6);
    memcpy(q.Msgrecs, buffer, strlen(buffer));

    q.Msglive = 0xE1;
    q.Msgarealo = 0;
    q.Msgareahi = 0;

    q.Msgoffhi = (ndx >> 8) & 0xff;
    q.Msgofflo = ndx & 0xff;

    q.Msgtagp = ' ';

    fwrite(&mndx, 4, 1, conf_ndx_fptr);
    fwrite(&zero, 1, 1, conf_ndx_fptr);

    fwrite(&mndx, 4, 1, pers_ndx_fptr);
    fwrite(&zero, 1, 1, pers_ndx_fptr);

    fwrite(&q, sizeof(struct QwkHeader), 1, msgs_dat_fptr);
    fwrite(msgbuf, lenbytes, 1, msgs_dat_fptr);
    free(msgbuf);
    tot++;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

void Email::download(Node *n, Email *e) {

  std::filesystem::path pth = std::filesystem::path("temp/" + std::to_string(n->getnodenum()) + "/message.txt");

  std::ofstream of;
  of.open(pth, std::ofstream::out | std::ofstream::trunc);

  of << "To     : " << n->get_user().get_username() << "\r\n";
  of << "From   : " << e->sender << "\r\n";
  of << "Subject: " << e->subject << "\r\n";
  of << "-------------------------------------------------------------------------------"
     << "\r\n";

  for (size_t i = 0; i < e->msg.size(); i++) {
    of << e->msg.at(i);
    of << "\r\n";
  }
  of << "-------------------------------------------------------------------------------"
     << "\r\n";

  of.close();

  if (!std::filesystem::exists(pth)) {
    return;
  }

  n->cls();

  Protocol *p = n->get_config()->select_protocol(n);

  if (p == nullptr) {
    return;
  }

  std::vector<std::filesystem::path> sendlist;
  sendlist.push_back(pth);

  p->download(n, n->get_socket(), &sendlist);

  return;
}
