#include <cstdio>
#include <string>
#include "bridge.h"

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Usage: bridge msgbase1 oaddr1 msgbase2 oaddr2\n");
        exit(-1);
    }

    Bridge b;

    b.do_bridge(std::string(argv[1]), std::string(argv[2]), std::string(argv[3]), std::string(argv[4]));

    return 0;
}