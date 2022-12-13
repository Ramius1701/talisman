#include <cstdio>
#include <iostream>
#include <string>
#include "../Common/INIReader.h"
#include "../Common/toml.hpp"
#include "../Common/Logger.h"
#include "bridge.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: bridge [link | ALL]\n");
        exit(-1);
    }


    INIReader inir("talisman.ini");
    
    std::vector<struct brlink> links;

    if (inir.ParseError()) {
        std::cerr << "Failed to parse talisman.ini" << std::endl;
        return -1;
    }

    std::string _datapath = inir.Get("Paths", "Data Path", "data");
    std::string _msgpath = inir.Get("Paths", "Message Path", "msgs");
    std::string _logpath = inir.Get("Paths", "Log Path", "logs");
    
    Logger log;

    log.load(_logpath + "/bridge.log");

    try {
        auto data = toml::parse_file(_datapath + "/bridge.toml");

        auto linkitems = data.get_as<toml::array>("link");

        if (linkitems != nullptr) {
            for (size_t i = 0; i < linkitems->size(); i++) {
                struct brlink newlink;

                auto itemtable = linkitems->get(i)->as_table();

                auto _linkname = itemtable->get("name");

                if (_linkname == nullptr) {
                    newlink.linkname = "";
                } else {
                    newlink.linkname = _linkname->as_string()->value_or("");
                }

                auto _msgfile1 = itemtable->get("msgfile1");

                if (_msgfile1 == nullptr) {
                    newlink.msgfile1 = "";
                } else {
                    newlink.msgfile1 = _msgfile1->as_string()->value_or("");
                }


                auto _msgfile2 = itemtable->get("msgfile2");

                if (_msgfile2 == nullptr) {
                    newlink.msgfile2 = "";
                } else {
                    newlink.msgfile2 = _msgfile2->as_string()->value_or("");
                }

                auto _msgtype1 = itemtable->get("msgtype1");
                std::string _msg_base_t_1;


                if (_msgtype1 == nullptr) {
                    newlink.msgbase_type1 = 0;
                } else {
                    _msg_base_t_1 = _msgtype1->as_string()->value_or("local");

                    if (strcasecmp(_msg_base_t_1.c_str(), "ftn") == 0) {
                        newlink.msgbase_type1 = 1;
                    } else if (strcasecmp(_msg_base_t_1.c_str(), "wwiv") == 0) {
                        newlink.msgbase_type1 = 2;
                    } else if (strcasecmp(_msg_base_t_1.c_str(), "qwk") == 0) {
                        newlink.msgbase_type1 = 3;
                    } else {
                        newlink.msgbase_type1 = 0;
                    }
                }

                auto _msgtype2 = itemtable->get("msgtype2");
                std::string _msg_base_t_2;


                if (_msgtype2 == nullptr) {
                    newlink.msgbase_type2 = 0;
                } else {
                    _msg_base_t_2 = _msgtype2->as_string()->value_or("local");

                    if (strcasecmp(_msg_base_t_2.c_str(), "ftn") == 0) {
                        newlink.msgbase_type2 = 1;
                    } else if (strcasecmp(_msg_base_t_2.c_str(), "wwiv") == 0) {
                        newlink.msgbase_type2 = 2;
                    } else if (strcasecmp(_msg_base_t_2.c_str(), "qwk") == 0) {
                        newlink.msgbase_type2 = 3;
                    } else {
                        newlink.msgbase_type2 = 0;
                    }
                }

                auto _address1 = itemtable->get("address1");

                if (_address1 == nullptr) {
                    newlink.address1 = NULL;
                } else {
                    if (newlink.msgbase_type1 == 1) {
                        newlink.address1 = parse_fido_addr(_address1->as_string()->value_or(""));
                    } else if (newlink.msgbase_type1 == 2) {
                        newlink.address1 = (NETADDR *)malloc(sizeof(NETADDR));
                        if (newlink.address1) {
                            newlink.address1->zone = 20000;
                            newlink.address1->net = 20000;
                            newlink.address1->node = std::stoi(_address1->as_string()->value_or("0"));
                        }
                    } else {
                        newlink.address1 = NULL;
                    }
                }

                auto _address2 = itemtable->get("address2");

                if (_address2 == nullptr) {
                    newlink.address2 = NULL;
                } else {
                    if (newlink.msgbase_type2 == 1) {
                        newlink.address2 = parse_fido_addr(_address2->as_string()->value_or(""));
                    } else if (newlink.msgbase_type2 == 2) {
                        newlink.address2 = (NETADDR *)malloc(sizeof(NETADDR));
                        if (newlink.address2) {
                            newlink.address2->zone = 20000;
                            newlink.address2->net = 20000;
                            newlink.address2->node = std::stoi(_address2->as_string()->value_or("0"));
                        }
                    } else {
                        newlink.address2 = NULL;
                    }
                }

                auto _tagline1 = itemtable->get("tagline1");

                if (_tagline1 == nullptr) {
                    newlink.tagline1 = "";
                } else {
                    newlink.tagline1 = _tagline1->as_string()->value_or("");
                }
                auto _tagline2 = itemtable->get("tagline2");

                if (_tagline2 == nullptr) {
                    newlink.tagline2 = "";
                } else {
                    newlink.tagline2 = _tagline2->as_string()->value_or("");
                }

                if (newlink.linkname == "" || newlink.msgfile1 == "" || newlink.msgfile2 == "") {
                    free(newlink.address1);
                    free(newlink.address2);
                    continue;
                }
                if (newlink.msgbase_type1 > 0 && newlink.address1 == NULL) {
                    free(newlink.address2);
                    continue;
                }
                if (newlink.msgbase_type2 > 0 && newlink.address2 == NULL) {
                    free(newlink.address1);
                    continue;
                }

                links.push_back(newlink);
            }
        }
    } catch (toml::parse_error const &p) {
        log.log(LOG_ERROR, "Error parsing %s/bridge.toml, Line %d, Column %d", _datapath.c_str(), p.source().begin.line, p.source().begin.column);
        log.log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
        return -1;
    }

    Bridge b;


    if (strcasecmp(argv[1], "all") == 0) {
        for (size_t i = 0; i < links.size(); i++) {
            b.do_bridge(_msgpath, &links.at(i), &log);
        }
    } else {
        for (size_t i = 0; i < links.size(); i++) {
            if (links.at(i).linkname == std::string(argv[1])) {
                b.do_bridge(_msgpath, &links.at(i), &log);
                return 0;
            }
        }
    }

    return 0;
}