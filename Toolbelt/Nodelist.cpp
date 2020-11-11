#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <sqlite3.h>
#include "Nodelist.h"



bool Nodelist::parse(std::string domain, std::string file, std::string database)
{
    sqlite3* db;
    sqlite3_stmt* stmt;

    static const char sql_del[] = "DELETE FROM nodes WHERE domain = ?";
    static const char sql_ins[] = "INSERT INTO nodes (domain, nodeno, bbsname, location, sysop) VALUES(?, ?, ?, ?, ?)";

    std::ifstream infile(file);
    std::string line;
   
    int zone = 0;
    int region = 0;
    int net = 0;
    int node = 0;

    if (!open_database(database, &db)) {
        return false;
    }

    if (sqlite3_prepare_v2(db, sql_del, strlen(sql_del), &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }
    sqlite3_bind_text(stmt, 1, domain.c_str(), -1, NULL);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    while (std::getline(infile, line))
    {
        std::istringstream iss(line);

        if (line.size() > 0) {
            if (line[0] == ';') {
                continue;
            }

            std::string segment;
            std::vector<std::string> seglist;
            std::stringstream ss(line);


            while (std::getline(ss, segment, ','))
            {
                seglist.push_back(segment);
            }
            
            if (seglist.size() < 5) {
                continue;
            }

            if (seglist.at(0) == "Zone") {
                zone = stoi(seglist.at(1));
                node = 0;
            }
            else if (seglist.at(0) == "Region") {
                region = stoi(seglist.at(1));
                net = region;
                node = 0;
            }
            else if (seglist.at(0) == "Host") {
                net = stoi(seglist.at(1));
                node = 0;
            }
            else {
                node = stoi(seglist.at(1));
            }

            std::string name = replace_underscores(seglist.at(2));
            std::string location = replace_underscores(seglist.at(3));
            std::string sysop = replace_underscores(seglist.at(4));

            std::string nodeno = std::to_string(zone) + ":" + std::to_string(net) + "/" + std::to_string(node);



            if (sqlite3_prepare_v2(db, sql_ins, strlen(sql_ins), &stmt, NULL) != SQLITE_OK) {
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, domain.c_str(), -1, NULL);
            sqlite3_bind_text(stmt, 2, nodeno.c_str(), -1, NULL);
            sqlite3_bind_text(stmt, 3, name.c_str(), -1, NULL);
            sqlite3_bind_text(stmt, 4, location.c_str(), -1, NULL);
            sqlite3_bind_text(stmt, 5, sysop.c_str(), -1, NULL);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    infile.close();
    sqlite3_close(db);
    return true;
}

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

std::string Nodelist::replace_underscores(std::string in)
{
    std::stringstream ss;

    for (size_t i = 0; i < in.size(); i++) {
        if (in.at(i) == '_') {
            ss << ' ';
        }
        else {
            ss << in.at(i);
        }
    }

    return ss.str();
}
