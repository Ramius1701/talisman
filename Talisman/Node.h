#pragma once
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include <string>
#include <filesystem>
#include <thread>
#include "Config.h"
#include "User.h"

class Bulletins;
class Logger;
class CallLog;
class FileArea;
class SshClient;

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
	bool compare_token(std::string field, std::string token);
	std::string get_string(size_t maxlen, bool masked);
	std::string get_string(size_t maxlen, bool masked, bool clear);
	std::string get_string(size_t maxlen, bool masked, bool clear, std::string def);
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

	time_t get_timeleft() {
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
	size_t get_term_width();
	size_t get_term_height();
	void set_term_height(size_t h);
	void set_term_width(size_t w);

	size_t override_width;
	size_t override_height;
	int override_on;
	SshClient* sshc;
	std::thread* ssht;

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

	time_t timeleft;
	int timeout;

	int timeoutmax;
	size_t term_width;
	size_t term_height;


#ifdef _MSC_VER
	HANDLE hOutput;
#endif
};

