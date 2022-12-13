#pragma once

#define VERSION "0.2"

#include "../Common/Squish.h"

class Logger;

struct brlink {
    std::string linkname;
    std::string msgfile1;
    std::string msgfile2;
    NETADDR *address1;
    NETADDR *address2;
    std::string tagline1;
    std::string tagline2;
    int msgbase_type1;
    int msgbase_type2;
};

class Bridge {
public:
    void do_bridge(std::string msg_path, struct brlink *link, Logger *log);
};