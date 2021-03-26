#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
class Node;

class Phlog
{
public:
	static bool save_article(Node* n, std::string subject, std::vector<std::string> msg);
private:
	static bool open_database(std::string filename, sqlite3** db);
};

