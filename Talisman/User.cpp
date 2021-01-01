#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <ctime>
#include <sqlite3.h>
#ifdef _MSC_VER
#include <Windows.h>
#include <bcrypt.h>
#endif
#include <openssl/evp.h>
#include "User.h"
#include "Node.h"
#include "CallLog.h"
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

User::User() {
	sec_level = 0;
	uid = 0;
}

void User::set_config(Config c) {
	this->c = c;
}

bool User::check_password(std::string password) {
	sqlite3* db;
	static const char* sql = "SELECT password, salt FROM users WHERE username = ?";
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
		std::string pass = std::string((const char*)sqlite3_column_text(stmt, 0));
		std::string salt = std::string((const char*)sqlite3_column_text(stmt, 1));
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

int User::get_sec_level() {
	if (sec_level <= 0) {
		sec_level = stoi(get_attribute("seclevel", "10"));
	}
	return sec_level;
}

std::string User::get_attribute(std::string attrib, std::string def) {
	return User::get_attribute_s(&c, uid, attrib, def);
}

std::string User::get_attribute_s(Config *c, int id, std::string attrib, std::string def) {
	sqlite3* db;
	sqlite3_stmt* res;
	std::string ret;

	int rc = 0;
	static const char* sql = "SELECT value FROM details WHERE uid = ? and attrib = ?";

	if(!open_database(c->data_path() + "/users.sqlite3", &db)) {
		return def;
	}
	rc = sqlite3_prepare_v2(db, sql, strlen(sql), &res, 0);
	if (rc != SQLITE_OK) {
		sqlite3_close(db);
		return def;
	}
	sqlite3_bind_int(res, 1, id);
	sqlite3_bind_text(res, 2, attrib.c_str(), -1, 0);
	rc = sqlite3_step(res);
	if (rc == SQLITE_ROW) {
		ret = std::string((const char*)sqlite3_column_text(res, 0));
	}
	else {
		ret = def;
	}

	sqlite3_finalize(res);
	sqlite3_close(db);
	return ret;
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

bool User::update_password(std::string password) {
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

	hash = hash_sha256(password, ssalt.str());
	if (hash.size() == 0) {
		return false;
	}
	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return false;
	}
	static const char* ins_sql = "UPDATE users SET password=?, salt=? WHERE id=?";
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, ins_sql, strlen(ins_sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}
	std::string sssalt = ssalt.str();
	sqlite3_bind_text(stmt, 1, hash.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 2, sssalt.c_str(), -1, NULL);
	sqlite3_bind_int(stmt, 3, uid);

	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return true;
}

bool User::inst_user(std::string username, std::string password, std::string firstname, std::string lastname, std::string location, std::string email)
{
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


	hash = hash_sha256(password, ssalt.str());
	if (hash.size() == 0) {
		return false;
	}

	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return false;
	}
	static const char* ins_sql = "INSERT INTO users (username, password, salt) VALUES(?, ?, ?)";
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, ins_sql, strlen(ins_sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return false;
	}

	std::string sssalt = ssalt.str();

	sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 2, hash.c_str(), -1, NULL);
	sqlite3_bind_text(stmt, 3, sssalt.c_str(), -1, NULL);

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
	rc = sqlite3_exec(*db, create_subscription_sql, 0, 0, &err_msg);
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

void User::user_set_lastread(std::string msgbase, int mid) {
	sqlite3* db;
	sqlite3_stmt* stmt;


	static const char* sql = "UPDATE lastr SET mid=? WHERE uid = ? and msgbase = ?";
	static const char* sql2 = "INSERT INTO lastr (mid, uid, msgbase) VALUES(?, ?, ?)";
	static const char* sql3 = "SELECT mid FROM lastr WHERE uid = ? and msgbase = ?";

	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return;
	}
	if (sqlite3_prepare_v2(db, sql3, strlen(sql3), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_int(stmt, 1, uid);
	sqlite3_bind_text(stmt, 2, msgbase.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_finalize(stmt);
		if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
			sqlite3_close(db);
			return;
		}
	}
	else {
		sqlite3_finalize(stmt);
		if (sqlite3_prepare_v2(db, sql2, strlen(sql2), &stmt, NULL) != SQLITE_OK) {
			sqlite3_close(db);
			return;
		}
	}

	sqlite3_bind_int(stmt, 1, mid);
	sqlite3_bind_int(stmt, 2, uid);
	sqlite3_bind_text(stmt, 3, msgbase.c_str(), -1, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
}


int User::user_get_lastread(std::string msgbase) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char* sql = "SELECT mid FROM lastr WHERE uid = ? and msgbase = ?";

	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return 0;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return 0;
	}
	sqlite3_bind_int(stmt, 1, uid);
	sqlite3_bind_text(stmt, 2, msgbase.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int ret = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return ret;
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return 0;
}

bool User::username_allowed(Config config, std::string username) {
	if (username.size() < 2) {
		return false;
	}

	for (size_t i = 0; i < username.size(); i++) {
		if (!(username[i] == ' ' || isalnum(username[i]))) {
			return false;
		}
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

std::string User::user_exists(Config *c, std::string usern) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	static const char* sql = "SELECT username FROM users WHERE username = ?";
	static const char* sql2 = "SELECT uid FROM details WHERE attrib = \"fullname\" AND value=?";
	static const char* sql3 = "SELECT username FROM users WHERE id = ?";

	if (!open_database(c->data_path() + "/users.sqlite3", &db)) {
		return "";
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return "";
	}
	sqlite3_bind_text(stmt, 1, usern.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return usern;
	}
	
	if (sqlite3_prepare_v2(db, sql2, strlen(sql2), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return "";
	}

	sqlite3_bind_text(stmt, 1, usern.c_str(), -1, NULL);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int id = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
		if (sqlite3_prepare_v2(db, sql3, strlen(sql3), &stmt, NULL) != SQLITE_OK) {
			sqlite3_close(db);
			return "";
		}
		sqlite3_bind_int(stmt, 1, id);
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			sqlite3_finalize(stmt);
			sqlite3_close(db);
			std::string ret = std::string((const char*)sqlite3_column_text(stmt, 0));
			return ret;
		}
		else {
			sqlite3_finalize(stmt);
			sqlite3_close(db);
			return "";
		}
	}
	else {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return "";
	}
}

void User::user_list(Node* n) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	int lines = 0;
	static const char* sql = "SELECT id, username FROM users";

	if (!open_database(n->get_config()->data_path() + "/users.sqlite3", &db)) {
		return;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}
	n->print_f("|14Username         Location                         Calls  Last Call\r\n");
	lines++;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		std::string username = std::string((const char *)sqlite3_column_text(stmt, 1));
		int uid = sqlite3_column_int(stmt, 0);
		int total_calls = n->clog->total_calls(username);
		std::string location = get_attribute_s(n->get_config(), uid, "location", "Somewhere, The World");
		time_t lastcall = n->clog->last_call(username);
		struct tm thetm;
		time_t now = time(NULL);
		struct tm nowtm;

#ifdef _MSC_VER
		localtime_s(&thetm, &lastcall);
		localtime_s(&nowtm, &now);
#else
		localtime_r(&lastcall, &thetm);
		localtime_r(&now, &nowtm);
#endif
		if (thetm.tm_year == nowtm.tm_year && thetm.tm_yday == nowtm.tm_yday) {
			n->print_f("|15%-16.16s |13%-32.32s |11%6d |10Today\r\n", username.c_str(), location.c_str(), total_calls);
		}
		else if (thetm.tm_year == nowtm.tm_year && thetm.tm_yday == nowtm.tm_yday - 1) {
			n->print_f("|15%-16.16s |13%-32.32s |11%6d |10Yesterday\r\n", username.c_str(), location.c_str(), total_calls);
		}
		else {
			n->print_f("|15%-16.16s |13%-32.32s |11%6d |10%04d/%02d/%02d\r\n", username.c_str(), location.c_str(), total_calls, thetm.tm_year + 1900, thetm.tm_mon + 1, thetm.tm_mday);
		}
		lines++;
		if (lines == n->get_term_height() - 2) {
			n->print_f("|14Continue? (Y/N) : ");
			if (tolower(n->getche()) == 'n') {
				break;
			}
			n->cls();
			n->print_f("|14Username         Location                         Calls  Last Call\r\n");
			lines = 1;
		}
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	n->pause();
}

bool User::is_subscribed(std::string msgbase) {
	sqlite3* db;
	sqlite3_stmt* stmt;
	static const char sql[] = "SELECT uid FROM subs WHERE uid = ? and msgbase = ?";


	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return true;
	}
	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return true;
	}

	sqlite3_bind_int(stmt, 1, uid);
	sqlite3_bind_text(stmt, 2, msgbase.c_str(), -1, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return true;
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return false;
}
void User::set_subscribed(std::string msgbase, bool value) {
	sqlite3* db;
	sqlite3_stmt* stmt;

	static const char insert_sql[] = "INSERT INTO subs (uid, msgbase) VALUES(?,?)";
	static const char delete_sql[] = "DELETE FROM subs WHERE uid=? AND msgbase=?";

	if (!open_database(c.data_path() + "/users.sqlite3", &db)) {
		return;
	}

	if (value) {
		if (is_subscribed(msgbase)) {
			sqlite3_close(db);
			return;
		}
		else {
			if (sqlite3_prepare_v2(db, insert_sql, strlen(insert_sql), &stmt, NULL) != SQLITE_OK) {
				sqlite3_close(db);
				return;
			}
		}
	}
	else {
		if (!is_subscribed(msgbase)) {
			sqlite3_close(db);
			return;
		}
		else {
			if (sqlite3_prepare_v2(db, delete_sql, strlen(delete_sql), &stmt, NULL) != SQLITE_OK) {
				sqlite3_close(db);
				return;
			}
		}
	}

	sqlite3_bind_int(stmt, 1, uid);
	sqlite3_bind_text(stmt, 2, msgbase.c_str(), -1, NULL);

	sqlite3_step(stmt);

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return;
}