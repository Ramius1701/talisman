#pragma once

#include "../Common/Logger.h"
#include "Config.h"
#include <filesystem>

#define BINKI_VERSION "0.2"

#define M_NUL 0
#define M_ADR 1
#define M_PWD 2
#define M_FILE 3
#define M_OK 4
#define M_EOB 5
#define M_GOT 6
#define M_ERR 7
#define M_BSY 8
#define M_GET 9
#define M_SKIP 10

class Server {
public:
  Server();
  static int receive(int socket, char *buffer, int size, int timeout);
  bool send_file_packet(std::filesystem::path file, std::string name);
  int run(int socket);
  int run(NETADDR *addr, std::string domain);
  int send_command_packet(uint8_t type, std::string data);
  bool transfer_files(std::string domain, NETADDR *theirnode, std::filesystem::path outdir, std::filesystem::path outbox);
  bool process_data(uint16_t header, int timeout);
  uint8_t process_command(uint16_t header, int timeout);
  bool process_frames(int timeout, uint8_t upto);
  int load_config();
  int runall();
  bool send_data_packet(int size, char *data);
  void cleanup();
  std::string genpktname();

private:
#ifdef _MSC_VER
  bool winsock_init;
#endif
  void cram5_init_challenge_data();
  std::string cram5_create_hashed_pwd(std::string challenge, std::string password);
  bool cram5_validate_password(std::string challenge, std::string password, std::string hash);
  bool cram5_init;
  bool cram5_opt;
  std::string cram5_challenge_data;
  std::string _datapath;
  std::string _logpath;
  std::string _tmppath;
  std::string _sysop_name;
  std::string _system_name;
  std::string _location;

  bool pwdack;
  bool secure;
  Config c;
  int socket;
  std::vector<struct address_t> remote_addresses;
  std::string remote_password;
  FILE *current_file;
  std::string current_filename;
  uint32_t current_len;
  uint32_t current_received;
  time_t current_timestamp;
  std::string sending_filename;
  uint32_t sending_len;
  time_t sending_timestamp;

  Logger log;

  bool senteob;
  bool goteob;
  time_t last_time;
};
