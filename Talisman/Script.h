#pragma once

#include <string>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

class Node;

class Script {
public:
  static void exec(Node *n, std::string script);
  static bool login(Node *n, std::string script, std::string *uname, std::string *password);
  static bool msgheader(Node *n, std::string script, std::string file, unsigned int mid, std::string from, std::string to, std::string subject);

private:
  static void init_state(Node *n, lua_State *l);
};
