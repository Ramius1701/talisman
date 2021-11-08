#pragma once
#include "Node.h"
#include <string>

class Protocol {
public:
  Protocol(std::string name, std::string dl_cmd, std::string ssh_dl_cmd, std::string ul_cmd, std::string ssh_ul_cmd, bool batch, bool prompt);
  void upload(Node *n, int socket, std::string uploadpath);
  void download(Node *n, int socket, std::vector<std::filesystem::path> *files);
  std::string get_name() { return name; }

private:
  std::string name;
  std::string download_cmd;
  std::string upload_cmd;
  std::string ssh_download_cmd;
  std::string ssh_upload_cmd;

  bool batch;
  bool prompt;
};
