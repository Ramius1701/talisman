#pragma once
#include <vector>
#include <string>
#include "../Common/Squish.h"
class Node;

class MsgArea
{
public:
	MsgArea(Node *n, std::string name, std::string filename, int r, int w, std::string oaddr, bool netmail, std::string tagline, int qwk, bool rn);
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

	bool get_real_names() {
		return real_names;
	}

	void do_semaphore(std::string sem);
	int get_total_msgs();
	int list_messages(int start);
	int list_messages_old(int start);
	int list_messages_full(int start);

	void read_message(int start, int *last);
	bool is_to_me(Node* n, sq_msg_t *msg);
	bool read_message(int start, bool search, bool unread, bool set_last_read, int *last);
	std::vector<std::string> demangle_ansi(const char* msg, size_t len);
	std::vector<std::string> strip_ansi(const char* msg, size_t len);
	static std::vector<std::string> word_wrap(std::string str, size_t len);
	bool save_message(std::string to, std::string from, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to, time_t date);
	bool save_message(std::string to, std::string from, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to);
	bool search(std::vector<std::string> keywords, int type, bool newonly);
	void update_lr(time_t date);
	int qwk_scan(Node* n, FILE* msgs_dat_fptr, FILE* pers_ndx_fptr, FILE* conf_ndx_fptr, int tot, int confno, unsigned int* last_msg_packed);
	int get_qwk_id() {
		return qwk_base_no;
	}
	static void attach_sig(std::vector<std::string> *msg, std::string sig);
private:
	bool print_msg_header(int msgno, int totmsg, sq_msg_t* msg);

	bool prepare_msg(sq_msg_t* msg, std::vector<struct line_t>* linesv, std::vector<std::string>* quotebuffer);
	void reply_to_msg(sq_msg_t* msg, std::vector<std::string>* quotebuffer);
	std::string name;
	std::string file;
	int read_sec_level;
	int write_sec_level;
	bool _is_netmail;
	std::string orig_addr;
	std::string tagline;
	Node* n;
	int qwk_base_no;
	bool real_names;
};

