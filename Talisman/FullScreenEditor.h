#pragma once

class Node;

#include <string>
#include <vector>

class FullScreenEditor {
public:
	FullScreenEditor(Node* n, std::string to, std::string subject, std::vector<std::string> *quotelines);
	~FullScreenEditor();

	std::vector<std::string> edit();
private:
	std::string to;
	std::string subject;
	std::vector<std::string> quotelines;
	Node* n;

	bool reply;
	void gotoyx(int y, int x);
	std::vector<std::string> do_quote();

};
