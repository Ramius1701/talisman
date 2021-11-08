#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
class Node;

class Phlog {
public:
  static bool save_article(Node *n, std::string subject, std::vector<std::string> msg);
  static void list_articles(Node *n);

private:
  static bool open_database(std::string filename, sqlite3 **db);
  static void delete_article(Node *n, int id);
  static void set_draft(Node *n, int id, bool draft);
  static void edit_article(Node *n, int id);
};
