#include "../Common/Logger.h"
#include "Editor.h"
#include "Node.h"
#include "Phlog.h"
#include "MessageReader.h"
#include <cstring>
#include <sqlite3.h>
#include <sstream>
#include <string>

struct recent_phlog_t {
  int id;
  std::string author;
  std::string subject;
  time_t datestamp;
};

void Phlog::recent_articles(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  std::vector<struct recent_phlog_t> recent;

  static const char *load_recent_sql = "SELECT id, author, subject, datestamp FROM phlog WHERE draft = 0 ORDER BY datestamp DESC LIMIT 10";
  static const char *load_body_sql = "SELECT body FROM phlog WHERE id=?";
  if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
    return;
  }
  if (sqlite3_prepare_v2(db, load_recent_sql, strlen(load_recent_sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare load_recent_sql (gopher) sql");
    sqlite3_close(db);
    return;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    struct recent_phlog_t r;

    r.id = sqlite3_column_int(stmt, 0);
    r.author = std::string((const char *)sqlite3_column_text(stmt, 1));
    r.subject = std::string((const char *)sqlite3_column_text(stmt, 2));
    r.datestamp = sqlite3_column_int64(stmt, 3);

    recent.push_back(r);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  size_t lines = 1;
  n->cls();

  n->print_f("|14Recent User Phlogs|07\r\n\r\n");

  for (size_t i = 0; i < recent.size(); i++) {
    struct tm rt;

#ifdef _MSC_VER
    localtime_s(&rt, &recent.at(i).datestamp);
#else
    localtime_r(&recent.at(i).datestamp, &rt);
#endif

    n->print_f("|14%4d. |07%04d-%02d-%02d |15%-54.54s\r\n", i + 1, rt.tm_year + 1900, rt.tm_mon + 1, rt.tm_mday, recent.at(i).subject.c_str());
    n->print_f("      |07by |11%-32.32s|07\r\n", recent.at(i).author.c_str());
  }

  n->print_f("\r\n|14View |08[|151|08-|15%d|08]|14: |07", recent.size());
  std::string res = n->get_string(2, false);

  if (res.size() == 0) {
    return;
  }

  size_t choice = 0;
  try {
    choice = std::stoi(res);
  } catch (std::invalid_argument const &) {

  } catch (std::out_of_range const &) {
  }

  if (choice <= 0 || choice > recent.size()) {
    return;
  }
  while (true) {
    if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
      n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
      return;
    }
    if (sqlite3_prepare_v2(db, load_body_sql, strlen(load_body_sql), &stmt, NULL) != SQLITE_OK) {
      n->log->log(LOG_ERROR, "Unable to prepare load_recent_sql (gopher) sql");
      sqlite3_close(db);
      return;
    }

    std::vector<line_t> body;

    sqlite3_bind_int(stmt, 1, recent.at(choice - 1).id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      std::string btmp = std::string((const char *)sqlite3_column_text(stmt, 0));
      std::stringstream ss(btmp);
      std::string ltmp;

      struct line_t hdr;
      hdr.line = " Title: " + recent.at(choice - 1).subject;
      hdr.type = 1;
      body.push_back(hdr);

      hdr.line = "Author: " + recent.at(choice - 1).author;
      hdr.type = 1;
      body.push_back(hdr);

      struct tm post_tm;
#ifdef _MSC_VER
      localtime_s(&post_tm, &recent.at(choice - 1).datestamp);
#else
      localtime_r(&recent.at(choice - 1).datestamp, &post_tm);
#endif

      hdr.line = "  Date: " + std::to_string(post_tm.tm_year + 1900) + "-" + std::to_string(post_tm.tm_mon + 1) + "-" + std::to_string(post_tm.tm_mday) + " " +
                 std::to_string(post_tm.tm_hour) + ":" + std::to_string(post_tm.tm_min);
      hdr.type = 1;
      body.push_back(hdr);

      hdr.line = "--------------------------------------------------------------------";
      hdr.type = 1;
      body.push_back(hdr);

      while (getline(ss, ltmp, '\r')) {
        struct line_t l;
        l.line = ltmp;
        l.type = 0;
        body.push_back(l);
      }
    } else {
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    struct msg_reader_msg_t msg;

    msg.area = NULL;
    msg.ansi = false;
    msg.body = &body;
    msg.date = "";
    msg.msg_no = choice;
    msg.msg_serial = recent.at(choice - 1).id;
    msg.destaddr = NULL;
    msg.origaddr = NULL;
    msg.from = recent.at(choice - 1).author;
    msg.to = "ALL";
    msg.msg_type = 4;
    msg.showkluges = false;
    msg.subject = recent.at(choice - 1).subject;

    int ret = MessageReader::read_message(
        n, &msg, recent.size(), DISABLE_DELETE | DISABLE_DOWNLOAD | DISABLE_HEADER | DISABLE_KLUDGE | DISABLE_REPLY | DISABLE_SEARCH | DISABLE_UNREAD);

    switch (ret) {
    case 0:
      return;
    case 4:
      break;
    case 5:
      choice += 1;
      if (choice > recent.size()) {
        return;
      }
      break;
    case 6:
      choice -= 1;
      if (choice <= 0) {
        return;
      }
      break;
    }
  }
}

bool Phlog::open_database(std::string db_path, sqlite3 **db) {
  static const char *create_gopher_sql =
      "CREATE TABLE IF NOT EXISTS phlog(id INTEGER PRIMARY KEY, uid INTEGER, author TEXT, subject TEXT, datestamp INTEGER, body TEXT, draft INTEGER)";

  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(db_path.c_str(), db) != SQLITE_OK) {
    // std::cerr << "Unable to open database: users.db" << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_gopher_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }

  return true;
}

struct article_t {
  int id;
  time_t datestamp;
  std::string subject;
  bool draft;
};

void Phlog::set_draft(Node *n, int id, bool draft) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char sql[] = "UPDATE phlog SET draft = ? WHERE uid = ? AND id = ?";

  if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare set_draft (gopher) sql");
    sqlite3_close(db);
    return;
  }
  int uid = n->get_user().get_uid();
  int draftstatus = (draft ? 1 : 0);

  sqlite3_bind_int(stmt, 1, draftstatus);
  sqlite3_bind_int(stmt, 2, uid);
  sqlite3_bind_int(stmt, 3, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void Phlog::edit_article(Node *n, int id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char sql[] = "SELECT subject, body FROM phlog WHERE uid = ? AND id = ?";
  static const char sql2[] = "UPDATE phlog SET body = ?, datestamp = ? WHERE uid = ? AND id = ?";
  if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare edit_article (gopher) sql");
    sqlite3_close(db);
    return;
  }
  int uid = n->get_user().get_uid();
  sqlite3_bind_int(stmt, 1, uid);
  sqlite3_bind_int(stmt, 2, id);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string subject = std::string((const char *)sqlite3_column_text(stmt, 0));
    std::string b = std::string((const char *)sqlite3_column_text(stmt, 1));
    sqlite3_finalize(stmt);

    std::vector<std::string> body;
    std::stringstream ss;
    for (size_t i = 0; i < b.size(); i++) {
      if (b.at(i) == '\r') {
        body.push_back(ss.str());
        ss.str("");
      } else {
        ss << b.at(i);
      }
    }

    body.push_back(ss.str());

    std::vector<std::string> newbody = Editor::enter_message(n, "My Phlog", subject, "Gopher", false, nullptr, &body);
    if (newbody.size() > 0) {
      if (sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL) != SQLITE_OK) {
        n->log->log(LOG_ERROR, "Unable to prepare edit_article (gopher) sql");
        sqlite3_close(db);
        return;
      }

      std::stringstream ss2;

      for (size_t i = 0; i < newbody.size(); i++) {
        ss2 << newbody.at(i) << "\r";
      }
      time_t now = time(NULL);

      std::string b = ss2.str();

      sqlite3_bind_text(stmt, 1, b.c_str(), -1, NULL);
      sqlite3_bind_int64(stmt, 2, now);
      sqlite3_bind_int(stmt, 3, uid);
      sqlite3_bind_int(stmt, 4, id);
      sqlite3_step(stmt);
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return;
    } else {
      sqlite3_close(db);
      return;
    }
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void Phlog::delete_article(Node *n, int id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char sql[] = "DELETE FROM phlog WHERE uid = ? AND id = ?";

  if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare delete_article (gopher) sql");
    sqlite3_close(db);
    return;
  }
  int uid = n->get_user().get_uid();
  sqlite3_bind_int(stmt, 1, uid);
  sqlite3_bind_int(stmt, 2, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void Phlog::list_articles(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  struct tm a_tm;
  std::vector<struct article_t> articles;

  static const char sql[] = "SELECT id,subject,datestamp,draft FROM phlog WHERE uid = ? ORDER BY datestamp DESC";

  if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare list_articles (gopher) sql");
    sqlite3_close(db);
    return;
  }
  int uid = n->get_user().get_uid();
  sqlite3_bind_int(stmt, 1, uid);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    struct article_t a;
    a.id = sqlite3_column_int(stmt, 0);
    a.subject = std::string((const char *)sqlite3_column_text(stmt, 1));
    a.datestamp = sqlite3_column_int64(stmt, 2);
    a.draft = (sqlite3_column_int(stmt, 3) == 1 ? true : false);
    articles.push_back(a);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  size_t lines = 1;
  n->cls();
  n->print_f("|09Post#    Subject                          Date                 Draft       |07\r\n");
  for (size_t i = 0; i < articles.size(); i++) {

#ifdef _MSC_VER
    localtime_s(&a_tm, &articles.at(i).datestamp);
#else
    localtime_r(&articles.at(i).datestamp, &a_tm);
#endif

    n->print_f("|08[|15%6d|08] |14%-32.32s |13%04d-%02d-%02d %02d:%02d     %s\r\n", i + 1, articles.at(i).subject.c_str(), a_tm.tm_year + 1900, a_tm.tm_mon + 1,
               a_tm.tm_mday, a_tm.tm_hour, a_tm.tm_min, (articles.at(i).draft ? "|08DRAFT" : "|11PUBLISHED"));
    lines++;
    if (lines == n->get_term_height() - 2) {
      n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", 1, articles.size());

      std::string res = n->get_string(6, false);

      if (res.size() == 0) {
        lines = 1;
        n->cls();
        n->print_f("|09Post#    Subject                          Date                 Draft       |07\r\n");
        continue;
      } else if (tolower(res[0]) == 'q') {
        return;
      } else {
        size_t choice = 0;
        try {
          choice = std::stoi(res);
        } catch (std::invalid_argument const &) {

        } catch (std::out_of_range const &) {
        }

        if (choice <= 0 || choice > articles.size()) {
          return;
        }

        n->cls();
#ifdef _MSC_VER
        localtime_s(&a_tm, &articles.at(choice - 1).datestamp);
#else
        localtime_r(&articles.at(choice - 1).datestamp, &a_tm);
#endif
        n->print_f("|14    Article: |07%s\r\n", articles.at(choice - 1).subject.c_str());
        n->print_f("|14Last Edited: |07%04d-%02d-%02d %02d:%02d\r\n", a_tm.tm_year + 1900, a_tm.tm_mon + 1, a_tm.tm_mday, a_tm.tm_hour, a_tm.tm_min);
        n->print_f("|14     Status: %s\r\n", (articles.at(choice - 1).draft ? "|08DRAFT" : "|11PUBLISHED"));
        n->print_f("\r\n|14Do you want to |15E|08=|14Edit|08, |15D|08=|14Delete|08, |15T|08=|14Toggle Draft|08, |15Q|08=|14Quit");
        char c = tolower(n->getch());
        switch (c) {
        case 'e':
          break;
        case 'd':
          delete_article(n, articles.at(choice - 1).id);
          return;
        case 't':
          set_draft(n, articles.at(choice - 1).id, !articles.at(choice - 1).draft);
          return;
        default:
          return;
        }
      }
    }
  }
  n->print_f("|14Select |08[|15%d|08-|15%d|08] |15ENTER|08=|14Quit |07", 1, articles.size());

  std::string res = n->get_string(6, false);

  if (res.size() == 0) {
    return;
  } else if (tolower(res[0]) == 'q') {
    return;
  } else {
    size_t choice = 0;
    try {
      choice = std::stoi(res);
    } catch (std::invalid_argument const &) {

    } catch (std::out_of_range const &) {
    }

    if (choice <= 0 || choice > articles.size()) {
      return;
    }

    n->cls();
#ifdef _MSC_VER
    localtime_s(&a_tm, &articles.at(choice - 1).datestamp);
#else
    localtime_r(&articles.at(choice - 1).datestamp, &a_tm);
#endif
    n->print_f("|14    Article: |07%s\r\n", articles.at(choice - 1).subject.c_str());
    n->print_f("|14Last Edited: |07%04d-%02d-%02d %02d:%02d\r\n", a_tm.tm_year + 1900, a_tm.tm_mon + 1, a_tm.tm_mday, a_tm.tm_hour, a_tm.tm_min);
    n->print_f("|14     Status: %s\r\n", (articles.at(choice - 1).draft ? "|08DRAFT" : "|11PUBLISHED"));
    n->print_f("\r\n|14Do you want to |15E|08=|14Edit|08, |15D|08=|14Delete|08, |15T|08=|14Toggle Draft|08, |15Q|08=|14Quit");
    char c = tolower(n->getch());
    switch (c) {
    case 'e':
      edit_article(n, articles.at(choice - 1).id);
      return;
    case 'd':
      delete_article(n, articles.at(choice - 1).id);
      return;
    case 't':
      set_draft(n, articles.at(choice - 1).id, !articles.at(choice - 1).draft);
      return;
    default:
      return;
    }
  }
}

bool Phlog::save_article(Node *n, std::string subject, std::vector<std::string> msg) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "INSERT INTO phlog (uid, author, subject, datestamp, body, draft) VALUES(?, ?, ?, ?, ?, 1)";

  if (!open_database(n->get_config()->data_path() + "/gopher.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open gopher sqlite database");
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare save_article (gopher) sql");
    sqlite3_close(db);
    return false;
  }

  std::stringstream ss;

  for (size_t i = 0; i < msg.size(); i++) {
    ss << msg.at(i) << "\r";
  }

  std::string msgstr = ss.str();

  time_t now = time(NULL);
  int uid = n->get_user().get_uid();
  std::string uname = n->get_user().get_username();

  sqlite3_bind_int(stmt, 1, uid);
  sqlite3_bind_text(stmt, 2, uname.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 3, subject.c_str(), -1, NULL);
  sqlite3_bind_int64(stmt, 4, now);
  sqlite3_bind_text(stmt, 5, msgstr.c_str(), -1, NULL);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return true;
}
