#pragma once

#include <string>
#include <vector>

class Node;

struct menuitem_t {
	std::string command;
	std::string data;
	std::string hotkey;
	int sec_level;
};

class Menu
{
public:
	Menu(Node *n);

	bool load(std::string filename);
	bool run();

private:
	std::string gfile;
	std::vector<struct menuitem_t> items;
	bool isloaded;
	Node* n;
};
