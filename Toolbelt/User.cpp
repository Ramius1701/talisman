#include <sqlite3.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#ifdef _MSC_VER
#include <Windows.h>
#include <bcrypt.h>
#endif
#include <openssl/evp.h>
#include "User.h"

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

bool User::open_database(std::string filename, sqlite3** db)
{
	static const char* create_users_sql = "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";
	static const char* create_details_sql = "CREATE TABLE IF NOT EXISTS details(uid INTEGER, attrib TEXT COLLATE NOCASE, value TEXT COLLATE NOCASE);";
	static const char* create_lastread_sql = "CREATE TABLE IF NOT EXISTS lastr(uid INTEGER, msgbase TEXT, mid INTEGER);";
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
	rc = sqlite3_exec(*db, create_lastread_sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		//std::cerr << "Unable to create details table: " << err_msg << std::endl;
		free(err_msg);
		sqlite3_close(*db);
		return false;
	}
	return true;
}
