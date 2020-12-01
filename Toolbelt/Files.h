#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>

class Archiver;

struct file_area_t {
	std::string confname;
	std::string areaname;
	std::string database;
};

class Files
{
public:
	bool load_archivers(std::string datapath);
	bool add_file(std::string temppath, std::string dbname, std::string filename, std::string uploader);
	int trim(std::string dbname);
	int all_files(std::string datapath, int sec_level, time_t date, std::string output);
	bool move_file(std::string datapath, std::string srcfile, std::string destfile, std::string srcdb, std::string destdb);
private:
	std::vector<struct file_area_t> load_areas(std::string datapath, int sec_level);
	bool insert_file(std::string database, std::string filename, std::vector<std::string> descr, std::string uploader);
	bool file_exists(std::string filename, std::string database);
	bool open_database(std::string filename, sqlite3** db);
	std::vector<Archiver*> archivers;
};

