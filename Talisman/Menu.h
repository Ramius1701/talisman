#pragma once

#include <string>
#include <vector>

class Node;

struct menuitem_t {
	std::string command;
	std::string data;
	std::string hotkey;
	int sec_level = 0;
};

class Menu
{
public:
	Menu(Node *n);

	bool load(std::string filename);
	bool run();

private:
	static void qwk_down(Node* n);
	static void qwk_up(Node* n);
	std::string gfile;
	std::string prompt;
	std::vector<struct menuitem_t> items;
	bool isloaded;
	Node* n;
};
