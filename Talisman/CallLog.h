#pragma once
#include <sqlite3.h>
#include <string>
class Node;
class Config;


class CallLog
{
public:
	CallLog(Config *c);
	void log_on(std::string username, int node);
	void log_off();
	void ran_door();
	void up_bytes(int bytes);
	void down_bytes(int bytes);
	void post_msg();
	int total_calls(std::string username);
	time_t last_call(std::string username);
	static void last10_callers(Node* n);
private:
	static bool open_database(std::string filename, sqlite3** db);
	int bytesup;
	int bytesdown;
	int msgsposted;
	int doorsrun;
	int id;
	Config* c;
};

