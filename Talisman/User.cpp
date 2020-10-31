#include <filesystem>
#include <fstream>
#include <sstream>
#include <sqlite3.h>
#ifdef _MSC_VER
#include <Windows.h>
#include <bcrypt.h>
#endif
#include <openssl/evp.h>
#include "User.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

User::User() {
}

void User::set_config(Config c) {
	this->c = c;
}

bool User::load_user(std::string username, std::string password)
{
	sqlite3* db;
	static const char* sql = "SELECT id, username, password, salt FROM users WHERE username = ?";
	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return false;
	}
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}

	sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		uid = sqlite3_column_int(stmt, 0);
		this->username = std::string((const char*)sqlite3_column_text(stmt, 1));
		std::string pass = std::string((const char*)sqlite3_column_text(stmt, 2));
		std::string salt = std::string((const char*)sqlite3_column_text(stmt, 3));
		std::string hash = hash_sha256(password, salt);
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		if (hash == pass) {
			return true;
		}
	}
	else {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
	}
	return false;
}

void User::set_attribute(std::string attrib, std::string value) {
	sqlite3* db;
	sqlite3_stmt* res;
	int rc = 0;
	static const char* chk_sql = "SELECT value FROM details WHERE attrib = ? and uid = ?";
	static const char* ins_sql = "INSERT INTO details (uid, attrib, value) VALUES(?, ?, ?)";
	static const char* upd_sql = "UPDATE details SET value = ? WHERE uid = ? and attrib = ?";

	//assert(uid != -1);

	// check if row exists
	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return;
	}
	rc = sqlite3_prepare_v2(db, chk_sql, strlen(chk_sql), &res, 0);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_text(res, 1, attrib.c_str(), -1, 0);
	sqlite3_bind_int(res, 2, uid);
	if (sqlite3_step(res) != SQLITE_ROW) {
		sqlite3_finalize(res);
		rc = sqlite3_prepare_v2(db, ins_sql, strlen(ins_sql), &res, 0);
		if (rc != SQLITE_OK) {
			sqlite3_close(db);
			return;
		}
		sqlite3_bind_int(res, 1, uid);
		sqlite3_bind_text(res, 2, attrib.c_str(), -1, 0);
		sqlite3_bind_text(res, 3, value.c_str(), -1, 0);
		if (sqlite3_step(res) != SQLITE_DONE) {
			sqlite3_finalize(res);
			sqlite3_close(db);
			return;
		}
	}
	else {
		sqlite3_finalize(res);
		rc = sqlite3_prepare_v2(db, upd_sql, strlen(upd_sql), &res, 0);
		if (rc != SQLITE_OK) {
			sqlite3_close(db);
			return;
		}
		sqlite3_bind_text(res, 1, value.c_str(), -1, 0);
		sqlite3_bind_int(res, 2, uid);
		sqlite3_bind_text(res, 3, attrib.c_str(), -1, 0);
		if (sqlite3_step(res) != SQLITE_DONE) {
			sqlite3_finalize(res);
			sqlite3_close(db);
			return;
		}
	}
	sqlite3_finalize(res);
	sqlite3_close(db);
}

std::string User::hash_sha256(std::string pass, std::string salt) {
	std::stringstream ss;
	std::stringstream sh;
	char* shash = NULL;
	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int length_of_hash = 0;
	int i;

	ss.str("");
	ss << pass << salt;
	sh.str("");

	EVP_MD_CTX* context = EVP_MD_CTX_new();
	if (context != NULL) {
		if (EVP_DigestInit_ex(context, EVP_sha256(), NULL)) {
			if (EVP_DigestUpdate(context, ss.str().c_str(), strlen(ss.str().c_str()))) {
				if (EVP_DigestFinal_ex(context, hash, &length_of_hash)) {
					for (i = 0; i < length_of_hash; i++)
						sh << hash[i];
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

bool User::inst_user(std::string username, std::string password, std::string firstname, std::string lastname, std::string location, std::string email)
{
	sqlite3* db;

	unsigned char salt[11];
	std::string hash;

	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return false;
	}
	memset(salt, 0, 11);
#ifdef _MSC_VER		
	BCRYPT_ALG_HANDLE hCrypt;
	BCryptOpenAlgorithmProvider(&hCrypt, L"RNG", NULL, 0);
	BCryptGenRandom(hCrypt, salt, 10, 0);
#else
	FILE* fptr = fopen("/dev/urandom", "r");
	if (!fptr) {
		sqlite3_close(db);
		return false;
	}

	fread(salt, 1, 10, fptr);

	fclose(fptr);
#endif

	hash = hash_sha256(password, std::string((char *)salt));
	if (hash.size() == 0) {
		sqlite3_close(db);
		return false;
	}

	static const char* ins_sql = "INSERT INTO users (username, password, salt) VALUES(?, ?, ?)";
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, ins_sql, strlen(ins_sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}

	sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 2, hash.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 3, (char *)salt, -1, NULL);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	uid = sqlite3_last_insert_rowid(db);

	sqlite3_close(db);

	set_attribute("fullname", firstname + " " + lastname);
	set_attribute("location", location);
	set_attribute("email", email);

	this->username = username;
	return true;
}

bool User::open_database(std::string filename, sqlite3** db)
{
	static const char* create_users_sql = "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";
	static const char* create_details_sql = "CREATE TABLE IF NOT EXISTS details(uid INTEGER, attrib TEXT COLLATE NOCASE, value TEXT COLLATE NOCASE);";
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
		free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	rc = sqlite3_exec(*db, create_details_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create details table: " << err_msg << std::endl;
		free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}

bool User::check_fullname(Config c, std::string fullname) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	const char* check_sql = "SELECT value FROM details WHERE attrib = \"fullname\" AND value=?";

	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return false;
	}

	if (sqlite3_prepare_v2(db, check_sql, strlen(check_sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}

	sqlite3_bind_text(stmt, 1, fullname.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return false;
	}
	else {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return true;
	}
}

bool User::username_allowed(Config config, std::string username) {
	if (username.size() < 2) {
		return false;
	}
	
	std::filesystem::path p(config.data_path());
	p.append("trashcan.txt");

	if (std::filesystem::exists(p)) {
		std::ifstream infile(p);
		std::string line;
		
		while (std::getline(infile, line))
		{
			std::istringstream iss(line);
			if (strcasecmp(username.c_str(), line.c_str()) == 0) {
				infile.close();
				return false;
			}
		}
	}

	sqlite3* db;
	sqlite3_stmt* stmt;
	bool ret = true;
	static const char* sql = "SELECT id FROM users WHERE username = ?";
	if (!open_database(config.data_path() + "/users.sqlite3", &db)) {
		return false;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}
	sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		ret = false;
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return ret;
}