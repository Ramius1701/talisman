#pragma once
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include <string>
#include <filesystem>
#include "Config.h"
#include "User.h"

class Bulletins;
class Logger;
class CallLog;
class FileArea;

struct tagged_file_t {
	std::string filename;
	FileArea* fa;
};

class Node
{
public:
	Node(int node, int socket, bool telnet);
	~Node();
	void print_f_nc(const char* fmt, ...);
	void print_f(const char* fmt, ...);
	void pause();
	char getch();
	char getche();
	void putch(const char c);
	int run();
	int run(std::string *username, std::string *password);
	bool newuser();
	bool detectANSI();
	void disconnected();
	void send_gfile(std::string filename, bool pause, bool script);
	void send_gfile(std::string filename, bool pause);
	void send_gfile(std::string filename);
	void cls();
	void update_node_use(std::string usage);
	void display_nodes();
	std::string get_string(int maxlen, bool masked);
	std::string get_string(int maxlen, bool masked, bool clear);
	std::string get_string(int maxlen, bool masked, bool clear, std::string def);
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

	int get_timeleft() {
		return timeleft;
	}

	bool is_telnet() {
		return telnet;
	}
	time_t get_last_on() {
		return last_on;
	}
	void system_info();
	std::string operating_system();
	bool hasANSI;
	CallLog *clog;
	Bulletins* bulletins;
	bool stop_timeout;
	Logger* log;
	void tag_file(std::string filename, FileArea* fa);
	int get_term_width();
	int get_term_height();
	void set_term_height(int h);
	void set_term_width(int w);

	int override_width;
	int override_height;
	int override_on;

	std::vector<struct tagged_file_t> tagged_files;
private:
	int node;
	int socket;
	bool telnet;

	Config config;
	User u;
	void send_str(const char* str);
	void send_file(std::filesystem::path p, bool pause, bool script);
	time_t last_on;
	time_t last_time_check;
	bool time_check();

	int timeleft;
	int timeout;

	int timeoutmax;
	int term_width;
	int term_height;
#ifdef _MSC_VER
	HANDLE hOutput;
#endif
};

