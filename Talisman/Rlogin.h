#pragma once

#include <string>

class Node;

class Rlogin {
public:
  static bool session(Node *n, std::string host, int port, std::string luser, std::string ruser, std::string termtype, bool ipv6);
};
