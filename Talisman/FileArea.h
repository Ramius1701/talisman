#pragma once

#include <string>
#include <sqlite3.h>
class Node;

class FileArea
{
public:
	FileArea(std::string name, std::string file_path, std::string database, int dl_sec_level, int ul_sec_level) {
		this->name = name;
		this->file_path = file_path;
		this->database = database;
		this->dl_sec_level = dl_sec_level;
		this->ul_sec_level = ul_sec_level;
	}
	int get_d_sec_level() {
		return dl_sec_level;
	}
	int get_u_sec_level() {
		return ul_sec_level;
	}
	std::string get_name() {
		return name;
	}

	int get_total_files(Node *n);
	void list_files(Node* n);
	void list_files(Node* n, time_t date);
	void list_files(Node* n, time_t date, std::vector<std::string> *keywords);

	void inc_download_count(Node* n, std::string filename);
	bool upload_file(Node *n);
	bool file_exists(Node* n, std::string filename);
	bool insert_file(Node* n, std::string filename, std::vector<std::string> descr);
private:
	bool open_database(std::string filename, sqlite3** db);
	std::string name;
	std::string file_path;
	std::string database;
	int dl_sec_level;
	int ul_sec_level;
};

