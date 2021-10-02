#pragma once

#include <string>
#include <vector>

struct area_t {
    std::string msgarea;
    int qwkbaseno;
};

struct network_t {
    std::string name;
    std::string qwkid;
    std::string myqwkid;
    std::string ftpserver;
    std::string archiver;
    int port;
    std::string password;
    std::string tagline;
    std::vector<struct area_t> areas;
};

class Archiver;

class Qwkie {
public:
    bool scan(int net);
    bool scan(std::string network);
    bool scanall();
    bool toss();
    bool poll(int net);
    bool poll(std::string network);
    bool pollall();
    bool loadConfig();
    ~Qwkie();
private:
    bool load_archivers();
    std::string msgpath;
    std::string datapath;
    std::string temppath;

    std::vector<struct network_t> networks;
    std::vector<Archiver *> archivers;
};
