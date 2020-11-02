#pragma once

#include <string>
#include <filesystem>
#include "Config.h"
#include "User.h"

class CallLog;

class Node
{
public:
	Node(int node, int socket, bool telnet);
	~Node();
	void print_f(const char* fmt, ...);
	char getch();
	char getche();
	void putch(const char c);
	int run();
	bool detectANSI();
	void disconnected();
	
	void send_gfile(std::string filename);
	void cls();
	std::string get_string(int maxlen, bool masked);
	std::string get_string(int maxlen, bool masked, bool clear);
	Config *get_config() {
		return &config;
	}

	User get_user() {
		return u;
	}
	int getnodenum() {
		return node;
	}

	int get_socket() {
		return socket;
	}

	void system_info();
	std::string operating_system();
	bool hasANSI;
	CallLog *clog;
	bool stop_timeout;
private:
	int node;
	int socket;
	bool telnet;

	Config config;
	User u;
	void send_str(const char* str);
	void send_file(std::filesystem::path p);
	
	time_t last_time_check;
	bool time_check();

	int timeleft;
	int timeout;

	int timeoutmax;
};

