#include <cstring>
#include <iostream>
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

struct node_list_entry_t {
    std::string nodeno;
    std::string bbsname;
    std::string location;
    std::string sysop;
};

void Nodelist::browse_nodelist(Node* n, std::string domain) {
    sqlite3* db;
    sqlite3_stmt* stmt;
    std::vector<node_list_entry_t> entries;

    static const char sql[] = "SELECT nodeno, bbsname, location, sysop FROM nodes WHERE domain = ?";
    if (!open_database(n->get_config()->data_path() + "/nodelist.sqlite3", &db)) {
        return;
    }

    if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_text(stmt, 1, domain.c_str(), -1, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        struct node_list_entry_t entry;

        entry.nodeno = std::string((const char *)sqlite3_column_text(stmt, 0));
        entry.bbsname = std::string((const char*)sqlite3_column_text(stmt, 1));
        entry.location = std::string((const char*)sqlite3_column_text(stmt, 2));
        entry.sysop = std::string((const char*)sqlite3_column_text(stmt, 3));

        entries.push_back(entry);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    n->cls();
    n->print_f("|14Enter a keyword to filter by, or nothing for all : ");
    std::string filter = n->get_string(24, false);
    n->print_f("\r\n\r\n");
    int lines = 0;

    for (size_t i = 0; i < entries.size(); i++) {
        bool show = false;
        if (filter.size() > 0) {
            if (entries.at(i).bbsname.find(filter) != std::string::npos) {
                show = true;
            }
            else if (entries.at(i).location.find(filter) != std::string::npos) {
                show = true;
            }
            else if (entries.at(i).sysop.find(filter) != std::string::npos) {
                show = true;
            }
            else if (entries.at(i).nodeno.find(filter) != std::string::npos) {
                show = true;
            }
        }
        else {
            show = true;
        }

        if (show) {
            n->print_f("|14     Node: |15 %s |08(|15%s|08)\r\n", entries.at(i).nodeno.c_str(), entries.at(i).bbsname.c_str());
            n->print_f("|14    Sysop: |15 %s\r\n", entries.at(i).sysop.c_str());
            n->print_f("|14 Location: |15 %s\r\n\r\n", entries.at(i).location.c_str());
            lines += 4;

            if (lines + 4 > 23) {
                n->print_f("|14Continue (Y/N) : ");
                if (tolower(n->getch()) == 'n') {
                    n->print_f("\r\n");
                    break;
                }
                n->print_f("\r\n");
                lines = 0;
            }
        }
    }
    n->print_f("|14Press any key...|07");
    n->getch();
}

void Nodelist::browse_nodelist(Node* n) {
    sqlite3 *db;
    sqlite3_stmt* stmt;

    static const char sql[] = "SELECT DISTINCT domain FROM nodes";

    if (!open_database(n->get_config()->data_path() + "/nodelist.sqlite3", &db)) {
        return;
    }

    if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        std::cerr << sqlite3_errstr(sqlite3_errcode(db)) << std::endl;
        return;
    }

    std::vector<std::string> domains;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        domains.push_back(std::string((const char*)sqlite3_column_text(stmt, 0)));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    if (domains.size() == 0) {

        return;
    }

    n->cls();
    n->print_f("|14Select Domain...|07\r\n\r\n");

    for (size_t i = 0; i < domains.size(); i++) {
        n->print_f("|14%d. |15%s\r\n", i + 1, domains.at(i).c_str());
    }

    n->print_f("|14Q. |15Quit\r\n\r\n");

    n->print_f("|14Domain |08[|151|08-|15%d|08] : |07", domains.size());

    std::string inp = n->get_string(2, false);

    if (inp.size() > 0) {
        if (tolower(inp[0]) == 'q') {
            return;
        }
        try {
            int i = stoi(inp) - 1;

            if (i >= 0 && i < domains.size()) {
                browse_nodelist(n, domains.at(i));
            }
        }
        catch (std::invalid_argument) {
           
        }
        catch (std::out_of_range) {

        }

    }
}