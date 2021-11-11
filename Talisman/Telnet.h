#pragma once

#include <string>

class Node;

class Telnet {
public:
  static bool session(Node *n, std::string host, int port, bool ipv6);

};

