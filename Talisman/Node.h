#pragma once

#include <string>

#include "Config.h"
#include "User.h"

class Node
{
public:
	Node(int node, int socket, bool telnet);
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

	Config get_config() {
		return config;
	}

	User get_user() {
		return u;
	}

private:
	int node;
	int socket;
	bool telnet;
	bool hasANSI;
	Config config;
	User u;
	void send_str(const char* str);
};

