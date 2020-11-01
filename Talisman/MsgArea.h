#pragma once
#include <vector>
#include <string>
class Node;

class MsgArea
{
public:
	MsgArea(Node *n, std::string name, std::string filename, int r, int w, std::string oaddr, bool netmail, std::string tagline);
	int get_r_sec_level() {
		return read_sec_level;
	}
	int get_w_sec_level() {
		return write_sec_level;
	}
	std::string get_name() {
		return name;
	}
	int get_total_msgs();
	int list_messages(int start);
	void read_message(int start);
	std::vector<std::string> word_wrap(std::string str, int len);
	void enter_message(std::string to, std::string subject, std::vector<std::string> *quotebuffer);
	bool save_message(std::string to, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to);
private:
	std::string name;
	std::string file;
	int read_sec_level;
	int write_sec_level;
	bool is_netmail;
	std::string orig_addr;
	std::string tagline;
	Node* n;
};

