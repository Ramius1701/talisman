#pragma once
#include <sqlite3.h>
#include <string>
class Node;
class Config;

struct caller_t {
    int id;
    std::string username;
    int node;
    time_t timeon;
    time_t timeoff;
    int upload;
    int download;
    int msgpost;
    int doors;
};

class CallLog
{
public:
	CallLog(Config *c);
    static bool get_last_x(Node *n, int x, struct caller_t *ct);
	void log_on(std::string username, int node);
	void log_off();
	void ran_door();
	void up_bytes(uint32_t bytes);
	void down_bytes(uint32_t bytes);
	void post_msg();
	static int total_calls(Node *n, std::string username);
    static int total_bbs_calls(Node *n);
	time_t last_call(std::string username);
	static void last10_callers(Node* n);
    static int get_tot_msgpost(Node *n, const char *username);
    static int get_tot_uploads(Node *n, const char *username);
    static int get_tot_downloads(Node *n, const char *username);
    static int get_tot_doors(Node *n, const char *username);
    static int get_bbs_tot_msgpost(Node *n);
    static int get_bbs_tot_uploads(Node *n);
    static int get_bbs_tot_downloads(Node *n);
    static int get_bbs_tot_doors(Node *n);

private:
	static bool open_database(std::string filename, sqlite3** db);
	int bytesup;
	int bytesdown;
	int msgsposted;
	int doorsrun;
	int id;
	Config* c;
};

