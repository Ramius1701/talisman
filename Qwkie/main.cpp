#include <cstring>
#include <iostream>
#include "qwkie.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage ./qwkie [scan|toss|poll]" << std::endl;
        return -1;
    }

    Qwkie q;

    if (!q.loadConfig()) {
        std::cerr << "Failed to load config!" << std::endl;
        return -1;
    }

    if (strcasecmp(argv[1], "scan") == 0) {
        if (argc > 2) {
            q.scan(std::string(argv[2]));
        } else {
            q.scanall();
        }
    } else if (strcasecmp(argv[1], "toss") == 0) {
        q.toss();
    } else if (strcasecmp(argv[1], "poll") == 0) {
        if (argc > 2) {
            q.poll(std::string(argv[2]));
        } else {
            q.pollall();
        }
    } else {
        std::cerr << "Usage ./qwkie [scan|toss|poll]" << std::endl;
        return -1;
    }

    return 0;
}
