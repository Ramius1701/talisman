// Trinket.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <iomanip>
#include <openssl/evp.h>
#include "../Common/INIReader.h"

bool open_database(std::string filename, sqlite3 **db) {
  static const char *create_users_sql =
      "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";
  static const char *create_details_sql = "CREATE TABLE IF NOT EXISTS details(uid INTEGER, attrib TEXT COLLATE NOCASE, value TEXT COLLATE NOCASE);";
  static const char *create_lastread_sql = "CREATE TABLE IF NOT EXISTS lastr(uid INTEGER, msgbase TEXT, mid INTEGER);";
  static const char *create_subscription_sql = "CREATE TABLE IF NOT EXISTS subs(uid INTEGER, msgbase TEXT)";
  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
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
  rc = sqlite3_exec(*db, create_details_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create details table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  rc = sqlite3_exec(*db, create_lastread_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create details table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }

  rc = sqlite3_exec(*db, create_subscription_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create details table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

std::string hash_sha256(std::string pass, std::string salt) {
  std::stringstream ss;
  std::stringstream sh;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int length_of_hash = 0;
  unsigned int i;

  ss.str("");
  ss << pass << salt;
  sh.str("");

  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (context != NULL) {
    if (EVP_DigestInit_ex(context, EVP_sha256(), NULL)) {
      if (EVP_DigestUpdate(context, ss.str().c_str(), strlen(ss.str().c_str()))) {
        if (EVP_DigestFinal_ex(context, hash, &length_of_hash)) {
          for (i = 0; i < length_of_hash; i++)
            sh << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)hash[i];
          EVP_MD_CTX_free(context);
          return sh.str();
        }
      }
    }
    EVP_MD_CTX_free(context);
  } else {
    return "";
  }

  return "";
}

bool check_password(std::string datapath, std::string username, std::string password) {
  sqlite3 *db;

  std::string salt;
  std::string hash;

  if (!open_database(datapath + "/users.sqlite3", &db)) {
    return false;
  }

  static const char *ins_sql = "SELECT password, salt FROM users WHERE username=?";
  sqlite3_stmt *stmt;

  if (sqlite3_prepare_v2(db, ins_sql, strlen(ins_sql), &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    hash = (const char *)sqlite3_column_text(stmt, 0);
    salt = (const char *)sqlite3_column_text(stmt, 1);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (hash_sha256(password, salt) == hash) {
      return true;
    }
  } else {
    sqlite3_finalize(stmt);
    sqlite3_close(db);
  }

  return false;
}

int main(int argc, char **argv) {
  INIReader inir("talisman.ini");

  if (inir.ParseError() != 0) {
    std::cerr << "Unable to parse talisman.ini!" << std::endl;
    return -1;
  }

  if (argc > 1) {
    if (strcmp(argv[1], "auth") == 0) {
      // authorize user
      std::string username;
      std::string password;

      std::getline(std::cin, username);
      std::getline(std::cin, password);

      if (check_password(inir.Get("paths", "data path", "data"), username, password)) {
        std::cout << "OK";
        return 0;
      }
      std::cout << "FAIL";
      return -1;
    } else if (strcmp(argv[1], "push") == 0) {
      // push from LASTREAD to current to database

    } else if (strcmp(argv[1], "pull") == 0) {
      // pull new messages from database to message base
    }
  }
  return 0;
}
