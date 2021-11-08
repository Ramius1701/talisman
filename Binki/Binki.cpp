#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

#include "Server.h"

#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
  if (argc == 3 && strcasecmp(argv[1], "-S") == 0) {
    Server s;
    int ret = s.load_config();
    if (ret == 0) {
      ret = s.run(strtol(argv[2], NULL, 10));
      s.cleanup();
    }
    return ret;
  } else if (argc == 3 && strcasecmp(argv[1], "-P") == 0) {
    // poll
    std::string arg(argv[2]);
    if (arg.find('@') != std::string::npos) {
      NETADDR *addr = parse_fido_addr(arg.substr(0, arg.find('@')).c_str());
      if (addr) {
        Server s;
        int ret = s.load_config();
        if (ret == 0) {
          ret = s.run(addr, arg.substr(arg.find('@') + 1));
          s.cleanup();
        }
        free(addr);
        return ret;
      }
      return -1;
    } else {
      NETADDR *addr = parse_fido_addr(arg.c_str());
      if (addr) {
        Server s;
        int ret = s.load_config();
        if (ret == 0) {
          ret = s.run(addr, "");
          s.cleanup();
        }
        free(addr);
        return ret;
      }
      return -1;
    }
  } else if (argc == 2 && strcasecmp(argv[1], "-O") == 0) {
    Server s;
    int ret = s.load_config();
    if (ret == 0) {
      ret = s.runall();
      s.cleanup();
    }

    return ret;
  } else {
    std::cerr << "Usage: binki -P addr" << std::endl;
    return -1;
  }
}
