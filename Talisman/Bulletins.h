#pragma once
#include <vector>
#include <string>
class Node;

struct bulletin_t {
	std::string name;
	std::string file;
	std::string hotkey;
	int seclevel;
};

class Bulletins
{
public:
	Bulletins() {
		isloaded = false;
	}
	bool load(Node* n);
	void display(Node* n);
private:
	bool isloaded;
	std::vector<struct bulletin_t> bullets;
};

