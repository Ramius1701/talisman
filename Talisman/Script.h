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

private:
  static void init_state(Node *n, lua_State *l);
};
