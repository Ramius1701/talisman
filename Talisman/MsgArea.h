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
	bool is_netmail() {
		return _is_netmail;
	}

	std::string get_file() {
		return file;
	}
	void do_semaphore(std::string sem);
	int get_total_msgs();
	int list_messages(int start);
	void read_message(int start);
	bool read_message(int start, bool search, bool unread, bool set_last_read);
	std::vector<std::string> demangle_ansi(const char* msg, int len);
	std::vector<std::string> strip_ansi(const char* msg, int len);
	static std::vector<std::string> word_wrap(std::string str, int len);
	bool save_message(std::string to, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to);
	bool search(std::vector<std::string> keywords, int type, bool newonly);
	void update_lr(time_t date);
private:
	std::string name;
	std::string file;
	int read_sec_level;
	int write_sec_level;
	bool _is_netmail;
	std::string orig_addr;
	std::string tagline;
	Node* n;
};

