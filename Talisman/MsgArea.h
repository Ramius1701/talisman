#pragma once
#include <vector>
#include <string>
class Node;

class MsgArea
{
public:
	MsgArea(Node *n, std::string name, std::string filename, int r, int w);
	int get_r_sec_level() {
		return read_sec_level;
	}
	std::string get_name() {
		return name;
	}
	int get_total_msgs();
	int list_messages(int start);
	void read_message(int start);
	std::vector<std::string> word_wrap(std::string str, int len);

private:
	std::string name;
	std::string file;
	int read_sec_level;
	int write_sec_level;
	Node* n;
};

