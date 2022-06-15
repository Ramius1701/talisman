#pragma once
#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "Config.h"
#include "Strings.h"
#include "User.h"
#include <filesystem>
#include <string>
#include <thread>

class Bulletins;
class Logger;
class CallLog;
class FileArea;
class SshClient;

struct tagged_file_t {
  std::string filename;
  FileArea *fa;
};

struct gfile_t {
  std::filesystem::path fspath;
  int width;
  int height;
};

class Node {
public:
  Node(int node, int socket, bool telnet);
  ~Node();
  void print_f_nc(const char *fmt, ...);
  void print_f(const char *fmt, ...);
  void pause();
  char getch(int timeout);
  char getch();
  char getche();
  void putch(const char c);
  int run();
  int run(std::string *username, std::string *password);
  bool newuser();
  bool detectANSI();
  bool detectUTF8();
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
  Config *get_config() { return &config; }

  User& get_user() { return u; }
  int getnodenum() { return node; }

  int get_socket() { return socket; }

  time_t get_timeleft() { return timeleft; }

  bool is_telnet() { return telnet; }
  time_t get_last_on() { return last_on; }
  void system_info();
  std::string operating_system();
  bool hasANSI;
  CallLog *clog;
  Bulletins *bulletins;
  bool stop_timeout;
  Logger *log;
  void tag_file(std::string filename, FileArea *fa);
  size_t get_term_width();
  size_t get_term_height();
  void set_term_height(size_t h);
  void set_term_width(size_t w);
  void set_term_type(const char *tt);
  const char *get_term_type();
  size_t override_width;
  size_t override_height;
  int override_on;
  SshClient *sshc;
  std::thread *ssht;

  std::vector<struct tagged_file_t> tagged_files;
  void chat(int othernode);
  int timeoutmax;
  bool time_check();
  void send_str(const char *str);
  void send_str(const char *str, int len);
  bool utf8() { return isutf8; }
  std::string ipaddr;
  void switch_font();
  void switch_font(int fontslot, int place);

private:
  int node;
  int socket;
  bool telnet;
  bool isutf8;

  void send_font(int slot, std::string filename);

  Config config;
  User u;
  
  void send_file(std::filesystem::path p, bool pause, bool script);
  time_t last_on;
  time_t last_time_check;


  time_t timeleft;
  int timeout;

  char term_type[256];
  size_t term_width;
  size_t term_height;
  std::vector<struct gfile_t> get_gfiles(std::string filename, bool ansi);
  std::vector<std::string> pausefiles;
  bool pause_loaded;
  Strings fmt_strings;
#ifdef _MSC_VER
  HANDLE hOutput;
#endif
};
