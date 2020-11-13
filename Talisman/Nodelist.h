#pragma once
#include <string>
#include <sqlite3.h>
class Node;

class Nodelist
{
public:
	static void browse_nodelist(Node* n, std::string domain);
	static void browse_nodelist(Node* n);
	static std::string lookup_bbsname(Node *n, std::string nodeno);
private:
	static bool open_database(std::string filename, sqlite3** db);
};

