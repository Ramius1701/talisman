#include <sqlite3.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <iostream>
#ifdef _MSC_VER
#include <Windows.h>
#include <bcrypt.h>
#endif
#include <openssl/evp.h>
#include "User.h"

std::string User::hash_sha256(std::string pass, std::string salt) {
	std::stringstream ss;
	std::stringstream sh;
	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int length_of_hash = 0;
	unsigned int i;

	ss.str("");
	ss << pass << salt;
	sh.str("");

	EVP_MD_CTX* context = EVP_MD_CTX_new();
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
	}
	else {
		return "";
	}

	return "";
}

bool User::update_password(std::string datapath, std::string username, std::string newpassword) {
	sqlite3* db;

	unsigned char salt[11];
	std::string hash;
	std::stringstream ssalt;


	memset(salt, 0, 11);
#ifdef _MSC_VER		
	BCRYPT_ALG_HANDLE hCrypt;
	BCryptOpenAlgorithmProvider(&hCrypt, L"RNG", NULL, 0);
	BCryptGenRandom(hCrypt, salt, 10, 0);
#else
	FILE* fptr = fopen("/dev/urandom", "r");
	if (!fptr) {
		return false;
	}

	fread(salt, 1, 10, fptr);

	fclose(fptr);
#endif

	for (int i = 0; i < 10; i++) {
		ssalt << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)salt[i];
	}

	hash = hash_sha256(newpassword, ssalt.str());
	if (hash.size() == 0) {
		return false;
	}
	if (!open_database(datapath + "/users.sqlite3", &db)) {
		return false;
	}
	static const char* ins_sql = "UPDATE users SET password=?, salt=? WHERE username=?";
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, ins_sql, strlen(ins_sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}
	std::string sssalt = ssalt.str();
	sqlite3_bind_text(stmt, 1, hash.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 2, sssalt.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 3, username.c_str(), -1, NULL);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return true;
}

int User::get_uid(std::string datapath, std::string username) {
	sqlite3* db;
	sqlite3_stmt* res;
	int uid = -1;
	static const char* sql = "SELECT id FROM users WHERE username = ?";
	if (!open_database(datapath + "/users.sqlite3", &db)) {
		return -1;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &res, 0) != SQLITE_OK) {
		sqlite3_close(db);
		return -1;
	}

	sqlite3_bind_text(res, 1, username.c_str(), -1, NULL);

	if (sqlite3_step(res) == SQLITE_ROW) {
		uid = sqlite3_column_int(res, 0);
	}
	sqlite3_finalize(res);
	sqlite3_close(db);
	return uid;
}

bool User::set_attribute(std::string datapath, std::string username, std::string attrib, std::string value) {
	sqlite3* db;
	sqlite3_stmt* res;
	int rc = 0;
	static const char* chk_sql = "SELECT value FROM details WHERE attrib = ? and uid = ?";
	static const char* ins_sql = "INSERT INTO details (uid, attrib, value) VALUES(?, ?, ?)";
	static const char* upd_sql = "UPDATE details SET value = ? WHERE uid = ? and attrib = ?";
	int uid = get_uid(datapath, username);

	if (uid == -1) {
		return false;
	}

	//assert(uid != -1);

	// check if row exists
	if (!open_database(datapath + "/users.sqlite3", &db)) {
		return false;
	}
	rc = sqlite3_prepare_v2(db, chk_sql, strlen(chk_sql), &res, 0);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}
	sqlite3_bind_text(res, 1, attrib.c_str(), -1, 0);
	sqlite3_bind_int(res, 2, uid);
	if (sqlite3_step(res) != SQLITE_ROW) {
		sqlite3_finalize(res);
		rc = sqlite3_prepare_v2(db, ins_sql, strlen(ins_sql), &res, 0);
		if (rc != SQLITE_OK) {
			sqlite3_close(db);
			return false;
		}
		sqlite3_bind_int(res, 1, uid);
		sqlite3_bind_text(res, 2, attrib.c_str(), -1, 0);
		sqlite3_bind_text(res, 3, value.c_str(), -1, 0);
		if (sqlite3_step(res) != SQLITE_DONE) {
			sqlite3_finalize(res);
			sqlite3_close(db);
			return false;
		}
	}
	else {
		sqlite3_finalize(res);
		rc = sqlite3_prepare_v2(db, upd_sql, strlen(upd_sql), &res, 0);
		if (rc != SQLITE_OK) {
			sqlite3_close(db);
			return false;
		}
		sqlite3_bind_text(res, 1, value.c_str(), -1, 0);
		sqlite3_bind_int(res, 2, uid);
		sqlite3_bind_text(res, 3, attrib.c_str(), -1, 0);
		if (sqlite3_step(res) != SQLITE_DONE) {
			sqlite3_finalize(res);
			sqlite3_close(db);
			return false;
		}
	}
	sqlite3_finalize(res);
	sqlite3_close(db);

	return true;
}

bool User::clear_lastread(std::string datapath, std::string msgbase, std::string user) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    static const char *delete_all = "DELETE FROM lastr";
    static const char *delete_user = "DELETE FROM lastr WHERE uid = ?";
    static const char *delete_base = "DELETE FROM lastr WHERE msgbase = ?";
    static const char *delete_base_user = "DELETE FROM lastr WHERE uid = ? AND msgbase = ?";

    int uid;

    if (user != "") {
        uid = get_uid(datapath, user);
        if (uid == -1) return false;
    }

    if (!open_database(datapath + "/users.sqlite3", &db)) {
        return false;
    }

    if (msgbase != "" && user != "") {
        if (sqlite3_prepare_v2(db, delete_base_user, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_int(stmt, 1, uid);
        sqlite3_bind_text(stmt, 2, msgbase.c_str(), -1, 0);
    } else if (user != "") {
        if (sqlite3_prepare_v2(db, delete_user, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_int(stmt, 1, uid);
    } else if (msgbase != "") {
        if (sqlite3_prepare_v2(db, delete_base, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            return false;
        }
        sqlite3_bind_text(stmt, 1, msgbase.c_str(), -1, 0);
    } else {
        if (sqlite3_prepare_v2(db, delete_all, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            return false;
        }
    }

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return true;
}

bool User::delete_user(std::string datapath, std::string username)
{
    static const char *delete_details = "DELETE FROM details WHERE uid = ?";
    static const char *delete_subscriptions = "DELETE FROM subs WHERE uid = ?";
    static const char *delete_user = "DELETE FROM users WHERE id = ?";
    static const char *delete_lastread = "DELETE FROM lastr WHERE uid = ?";
    static const char *delete_emails = "DELETE FROM email WHERE recpient = ?";
    static const char *delete_phlogs = "DELETE FROM phlog WHERE uid = ?";
    sqlite3 *db;
    sqlite3_stmt *stmt;


    int uid = get_uid(datapath, username);

    if (uid == -1) {
        return false;
    }

    std::cout << "Deleting Emails....";

    if (!open_email_database(datapath + "/email.sqlite3", &db)) {
        return false;
    }

    if (sqlite3_prepare_v2(db, delete_emails, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, 0);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Done" << std::endl;

    std::cout << "Deleting Phlogs....";

    if (!open_gopher_database(datapath + "/gopher.sqlite3", &db)) {
        return false;
    }

    if (sqlite3_prepare_v2(db, delete_phlogs, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);


    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Done" << std::endl;

    std::cout << "Deleting user attributes...";

    if (!open_database(datapath + "/users.sqlite3", &db)) {
        return false;
    }

    if (sqlite3_prepare_v2(db, delete_details, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    std::cout << "Done" << std::endl;


    std::cout << "Deleting user subscriptions...";

    if (sqlite3_prepare_v2(db, delete_subscriptions, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    std::cout << "Done" << std::endl;

    sqlite3_close(db);

    std::cout << "Deleting user last read pointers...";

    if (sqlite3_prepare_v2(db, delete_lastread, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    std::cout << "Done" << std::endl;

    std::cout << "Deleting user...";

    if (sqlite3_prepare_v2(db, delete_user, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, uid);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_close(db);

    std::cout << "Done" << std::endl;

    return true;
}


bool User::open_database(std::string filename, sqlite3** db)
{
	static const char* create_users_sql = "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";
	static const char* create_details_sql = "CREATE TABLE IF NOT EXISTS details(uid INTEGER, attrib TEXT COLLATE NOCASE, value TEXT COLLATE NOCASE);";
	static const char* create_lastread_sql = "CREATE TABLE IF NOT EXISTS lastr(uid INTEGER, msgbase TEXT, mid INTEGER);";
	static const char* create_subscription_sql = "CREATE TABLE IF NOT EXISTS subs(uid INTEGER, msgbase TEXT)";
	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
		//std::cerr << "Unable to open database: users.db" << std::endl;
		return false;
	}
	sqlite3_busy_timeout(*db, 5000);

	rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create user table: " << err_msg << std::endl;
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	rc = sqlite3_exec(*db, create_details_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create details table: " << err_msg << std::endl;
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	rc = sqlite3_exec(*db, create_lastread_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create details table: " << err_msg << std::endl;
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}

	rc = sqlite3_exec(*db, create_subscription_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create details table: " << err_msg << std::endl;
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}

bool User::open_email_database(std::string filename, sqlite3** db) {
	const char* create_users_sql = "CREATE TABLE IF NOT EXISTS email(id INTEGER PRIMARY KEY, sender TEXT COLLATE NOCASE, recipient TEXT COLLATE NOCASE, subject TEXT, body TEXT, date INTEGER, seen INTEGER)";

	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
		//std::cerr << "Unable to open database: " << filename << std::endl;
		return false;
	}
	sqlite3_busy_timeout(*db, 5000);

	rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create email table: " << err_msg << std::endl;
		sqlite3_free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}

bool User::open_gopher_database(std::string filename, sqlite3** db) {
	static const char* create_gopher_sql = "CREATE TABLE IF NOT EXISTS phlog(id INTEGER PRIMARY KEY, uid INTEGER, author TEXT, subject TEXT, datestamp INTEGER, body TEXT, draft INTEGER)";

	int rc;
	char* err_msg = NULL;

	if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
		//std::cerr << "Unable to open database: users.db" << std::endl;
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

