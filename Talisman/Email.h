#pragma once

#include "MsgArea.h"
#include <sqlite3.h>

class Node;

class Email {
public:
  std::string sender;
  std::string subject;
  std::vector<std::string> msg;
  bool seen = false;
  time_t date = 0;
  int id = 0;
  static int count_email(Node *n);
  static int unread_email(Node *n);
  static int view_email(Node *n, Email e, int emailno, int tot_emails);
  static void list_email(Node *n);
  static bool save_message(Node *n, std::string to, std::string from, std::string subject, std::vector<std::string> msg);
  static int qwk_scan(Node *n, FILE *msgs_dat_fptr, FILE *pers_ndx_fptr, FILE *conf_ndx_fptr, int tot);
  static int bwave_scan(Node *n, FILE *fti_file, FILE *mix_file, FILE *dat_file, int *last_ptr);
  static void delete_email(Node *n, int id);
  static void set_all_seen(Node *n);

private:
  static bool load_emails(Node *n, std::vector<Email> *emails);
  static bool open_database(std::string filename, sqlite3 **db);
  static void download(Node *n, Email *e);
};
