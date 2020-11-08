#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>

class Archiver;

class Files
{
public:
	bool load_archivers(std::string datapath);
	bool add_file(std::string temppath, std::string dbname, std::string filename, std::string uploader);
private:
	bool insert_file(std::string database, std::string filename, std::vector<std::string> descr, std::string uploader);
	bool file_exists(std::string filename, std::string database);
	bool open_database(std::string filename, sqlite3** db);
	std::vector<Archiver*> archivers;
};

