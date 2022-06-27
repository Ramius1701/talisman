#include "CallLog.h"
#include "Config.h"
#include "Node.h"
#include <cstring>
#include <ctime>
#include <iostream>

CallLog::CallLog(Config *c, bool invisible) {
  id = -1;
  bytesup = 0;
  bytesdown = 0;
  msgsposted = 0;
  doorsrun = 0;
  this->invisible = invisible;
  this->c = c;
}

bool CallLog::open_database(std::string filename, sqlite3 **db) {
  const char *create_users_sql = "CREATE TABLE IF NOT EXISTS calllog(id INTEGER PRIMARY KEY, username TEXT, node INTEGER, timeon INTEGER, timeoff INTEGER, "
                                 "rundoor INTEGER, upload INTEGER, download INTEGER, msgpost INTEGER);";

  int rc;
  char *err_msg = NULL;
  int err = sqlite3_open(filename.c_str(), db);

  if (err != SQLITE_OK) {
    std::cerr << "Unable to open database: " << filename << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    std::cerr << "Unable to create calllog table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

void CallLog::log_on(std::string username, int node) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  time_t thetime = time(NULL);
  const char *sql = "INSERT INTO calllog (username, node, timeon, timeoff, rundoor, upload, download, msgpost) VALUES(?, ?, ?, 0, 0, 0, 0, 0)";

  if (invisible) {
    return;
  }

  if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
    return;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);
  sqlite3_bind_int(stmt, 2, node);
  sqlite3_bind_int64(stmt, 3, thetime);
  sqlite3_step(stmt);
  id = (int)sqlite3_last_insert_rowid(db);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void CallLog::log_off() {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  time_t thetime = time(NULL);
  const char *sql = "UPDATE calllog SET timeoff = ? WHERE id = ?";

  if (id == -1) {
    return;
  }

  if (invisible) {
    return;
  }

  if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_int64(stmt, 1, thetime);
  sqlite3_bind_int(stmt, 2, id);

  sqlite3_step(stmt);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void CallLog::ran_door() {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  const char *sql = "UPDATE calllog SET rundoor = ? WHERE id = ?";

  doorsrun++;

  if (id == -1) {
    return;
  }

  if (invisible) {
    return;
  }

  if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_int(stmt, 1, doorsrun);
  sqlite3_bind_int(stmt, 2, id);

  sqlite3_step(stmt);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void CallLog::up_bytes(uint32_t bytes) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  const char *sql = "UPDATE calllog SET upload = ? WHERE id = ?";

  bytesup += bytes;

  if (id == -1) {
    return;
  }

  if (invisible) {
    return;
  }

  if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_int(stmt, 1, bytesup);
  sqlite3_bind_int(stmt, 2, id);

  sqlite3_step(stmt);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void CallLog::down_bytes(uint32_t bytes) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  const char *sql = "UPDATE calllog SET download = ? WHERE id = ?";

  bytesdown += bytes;

  if (id == -1) {
    return;
  }

  if (invisible) {
    return;
  }

  if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_int(stmt, 1, bytesdown);
  sqlite3_bind_int(stmt, 2, id);

  sqlite3_step(stmt);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void CallLog::post_msg() {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  const char *sql = "UPDATE calllog SET msgpost = ? WHERE id = ?";

  msgsposted++;

  if (invisible) {
    return;
  }

  if (id == -1) {
    return;
  }
  if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
    return;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_int(stmt, 1, msgsposted);
  sqlite3_bind_int(stmt, 2, id);

  sqlite3_step(stmt);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

time_t CallLog::last_call(std::string username) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  time_t ret = 0;
  const char *sql = "SELECT timeon FROM calllog WHERE username = ? ORDER BY timeon DESC LIMIT 1";
  if (!open_database(c->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ret = sqlite3_column_int64(stmt, 0);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ret;
}

int CallLog::total_calls(Node *n, std::string username) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int ret = 0;
  const char *sql = "SELECT COUNT(*) FROM calllog WHERE username = ?";
  if (!open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }
  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ret = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ret;
}

int CallLog::total_bbs_calls(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  int ret = 0;
  const char *sql = "SELECT COUNT(*) FROM calllog";
  if (!open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
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

bool CallLog::get_last_x(Node *n, int x, struct caller_t *ct) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, username, node, timeon, timeoff, rundoor, upload, download, msgpost FROM calllog ORDER by id DESC LIMIT ?, 1";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  sqlite3_bind_int(stmt, 1, x);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ct->timeon = sqlite3_column_int(stmt, 3);
    ct->timeoff = sqlite3_column_int(stmt, 4);
    ct->id = sqlite3_column_int(stmt, 0);
    ct->node = sqlite3_column_int(stmt, 2);
    ct->username = std::string((const char *)sqlite3_column_text(stmt, 1));
    ct->doors = sqlite3_column_int(stmt, 5);
    ct->upload = sqlite3_column_int(stmt, 6);
    ct->download = sqlite3_column_int(stmt, 7);
    ct->msgpost = sqlite3_column_int(stmt, 8);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
  } else {
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return false;
  }

  return true;
}

int CallLog::get_bbs_tot_doors(Node *n) {
  int tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT rundoor FROM calllog";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

int CallLog::get_bbs_tot_msgpost(Node *n) {
  int tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT msgpost FROM calllog";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

uint64_t CallLog::get_bbs_tot_uploads(Node *n) {
  uint64_t tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT upload FROM calllog";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int64(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

uint64_t CallLog::get_bbs_tot_downloads(Node *n) {
  uint64_t tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT download FROM calllog";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int64(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

int CallLog::get_tot_doors(Node *n, const char *username) {
  int tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT rundoor FROM calllog where username = ?";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_bind_text(stmt, 1, username, -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

uint64_t CallLog::get_tot_downloads(Node *n, const char *username) {
  uint64_t tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT download FROM calllog where username = ?";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_bind_text(stmt, 1, username, -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

uint64_t CallLog::get_tot_uploads(Node *n, const char *username) {
  uint64_t tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT upload FROM calllog where username = ?";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_bind_text(stmt, 1, username, -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

int CallLog::get_tot_msgpost(Node *n, const char *username) {
  int tot = 0;

  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT msgpost FROM calllog where username = ?";

  if (!CallLog::open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  sqlite3_bind_text(stmt, 1, username, -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    tot += sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return tot;
}

void CallLog::last10_callers(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  const char *sql = "SELECT id, username, node, timeon, timeoff, rundoor, upload, download, msgpost FROM (SELECT id, username, node, timeon, timeoff, rundoor, "
                    "upload, download, msgpost FROM calllog ORDER by id DESC LIMIT 10) ORDER BY id ASC";

  if (!open_database(n->get_config()->data_path() + "/call_log.sqlite3", &db)) {
    return;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  n->send_gfile("last10");

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int callno = sqlite3_column_int(stmt, 0);
    char *username = strdup((const char *)sqlite3_column_text(stmt, 1));
    int node = sqlite3_column_int(stmt, 2);
    time_t timeon = sqlite3_column_int(stmt, 3);
    time_t timeoff = sqlite3_column_int(stmt, 4);
    char rundoor = ' ';
    char upload = ' ';
    char download = ' ';
    char mgpost = ' ';
    struct tm tm_on;
    struct tm tm_off;
    if (sqlite3_column_int(stmt, 5) > 0) {
      rundoor = 'X';
    }

    if (sqlite3_column_int(stmt, 6) > 0) {
      upload = 'U';
    }

    if (sqlite3_column_int(stmt, 7) > 0) {
      download = 'D';
    }

    if (sqlite3_column_int(stmt, 8) > 0) {
      mgpost = 'M';
    }
#ifdef _MSC_VER
    localtime_s(&tm_on, &timeon);
#else
    localtime_r(&timeon, &tm_on);
#endif
    if (timeoff == 0) {
      n->print_f(" |14%5d.   |13%2d   |15%-28.28s |10%02d:%02d |08- |12UNKWN     |09%c |11%c |13%c |10%c\r\n", callno, node, username, tm_on.tm_hour,
                 tm_on.tm_min, rundoor, upload, download, mgpost);
    } else {
#ifdef _MSC_VER
      localtime_s(&tm_off, &timeoff);
#else
      localtime_r(&timeoff, &tm_off);
#endif
      n->print_f(" |14%5d.   |13%2d   |15%-28.28s |10%02d:%02d |08- |12%02d:%02d     |09%c |11%c |13%c |10%c\r\n", callno, node, username, tm_on.tm_hour,
                 tm_on.tm_min, tm_off.tm_hour, tm_off.tm_min, rundoor, upload, download, mgpost);
    }
    free(username);
  }
  n->print_f("\r\n");
  sqlite3_finalize(stmt);
  sqlite3_close(db);
}
