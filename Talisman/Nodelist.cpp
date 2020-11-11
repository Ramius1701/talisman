#include <cstring>
#include "Node.h"
#include "Config.h"
#include "Nodelist.h"

bool Nodelist::open_database(std::string filename, sqlite3** db)
{
    static const char* create_nodelist_sql = "CREATE TABLE IF NOT EXISTS nodes(domain TEXT COLLATE NOCASE, nodeno TEXT, bbsname TEXT, location TEXT, sysop TEXT)";

    int rc;
    char* err_msg = NULL;

    if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
        //std::cerr << "Unable to open database: users.db" << std::endl;
        return false;
    }
    sqlite3_busy_timeout(*db, 5000);

    rc = sqlite3_exec(*db, create_nodelist_sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        //std::cerr << "Unable to create user table: " << err_msg << std::endl;
        free(err_msg);
        sqlite3_close(*db);
        return false;
    }

    return true;
}

std::string Nodelist::lookup_bbsname(Node* n, std::string nodeno) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    static const char sql[] = "SELECT bbsname FROM nodes WHERE nodeno = ?";
    if (!open_database(n->get_config()->data_path() + "/nodelist.sqlite3", &db)) {
        return std::string("Unknown Node");
    }

    if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return std::string("Unknown Node");
    }

    sqlite3_bind_text(stmt, 1, nodeno.c_str(), -1, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string ret = std::string((const char*)sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return ret;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return std::string("Unknown Node");
}