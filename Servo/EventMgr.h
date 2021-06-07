#pragma once

#include <string>
#include <vector>

struct event_t {
    std::string name;
    std::string execute;
    int interval;
    time_t nextrun;
};

class EventMgr {
    public:
        void run(std::string datapath);
        static void executor(EventMgr *ev);
        std::vector<struct event_t> events;
        bool shutdown;
    private:
        bool load_config(std::string datapath);

};
