#include "../Common/Squish.h"
#include "../Common/toml.hpp"
#include "../Common/Logger.h"
#include "Config.h"
#include <fstream>

bool Config::load(std::string datapath, Logger *log) {
  try {
    auto data = toml::parse_file(datapath + "/binki.toml");

    auto _sem = data["binki"]["semaphore"].as_string();

    if (_sem == nullptr) {
      semaphore = "";
    } else {
      semaphore = _sem->value_or("");
    }

    auto _inbound = data["binki"]["inbound"].as_string();

    if (_inbound == nullptr) {
      inbound = "";
    } else {
      inbound = _inbound->value_or("");
    }

    auto _secinbound = data["binki"]["secure_inbound"].as_string();

    if (_secinbound == nullptr) {
      inbound_secure = "";
    } else {
      inbound_secure = _secinbound->value_or("");
    }

    auto _outbound = data["binki"]["outbound"].as_string();
    if (_outbound == nullptr) {
      outbound = "";
    } else {
      outbound = _outbound->value_or("");
    }

    auto _defaultzone = data["binki"]["default_zone"].as_integer();
    if (_defaultzone == nullptr) {
      defaultzone = 0;
    } else {
      defaultzone = _defaultzone->value_or(0);
    }

    auto addressitems = data.get_as<toml::array>("address");
    if (addressitems != nullptr) {
      for (size_t i = 0; i < addressitems->size(); i++) {
        struct address_t newaddr;

        auto itemtable = addressitems->get(i)->as_table();

        auto _addr = itemtable->get("address");
        if (_addr != nullptr) {
          std::string str = _addr->as_string()->value_or("");
          newaddr.addr = parse_fido_addr(str.c_str());
          if (newaddr.addr == NULL) {
            continue;
          }
        } else {
          continue;
        }
        auto _domain = itemtable->get("domain");
        if (_domain != nullptr) {
          newaddr.domain = _domain->as_string()->value_or("");
        } else {
          newaddr.domain = "";
        }

        if (newaddr.domain == "") {
          free(newaddr.addr);
          continue;
        }
        addresses.push_back(newaddr);
      }
    }

    auto linkitems = data.get_as<toml::array>("link");
    if (linkitems != nullptr) {
      for (size_t i = 0; i < linkitems->size(); i++) {
        struct link_t newlink;
        auto itemtable = linkitems->get(i)->as_table();

        auto _network = itemtable->get("domain");

        if (_network != nullptr) {
          newlink.network = _network->as_string()->value_or("");
        } else {
          newlink.network = "";
        }

        auto _host = itemtable->get("host");

        if (_host != nullptr) {
          newlink.host = _host->as_string()->value_or("");
        } else {
          newlink.host = "";
        }

        auto _port = itemtable->get("port");

        if (_port != nullptr) {
          newlink.port = _port->as_integer()->value_or(24554);
        } else {
          newlink.port = 24554;
        }

        auto _addr = itemtable->get("address");
        if (_addr != nullptr) {
          std::string str = _addr->as_string()->value_or("");
          newlink.addr = parse_fido_addr(str.c_str());
          if (newlink.addr == NULL) {
            continue;
          }
        } else {
          continue;
        }

        auto _outbox = itemtable->get("outbox");
        if (_outbox != nullptr) {
          newlink.outbox = _outbox->as_string()->value_or("");
        } else {
          newlink.outbox = "";
        }

        auto _password = itemtable->get("password");
        if (_password != nullptr) {
          newlink.password = _password->as_string()->value_or("");
        } else {
          newlink.password = "";
        }

        auto _crammd5 = itemtable->get("cram-md5");
        if (_crammd5 != nullptr) {
          newlink.crammd5 = _crammd5->as_boolean()->value_or(false);
        } else {
          newlink.crammd5 = false;
        }

        links.push_back(newlink);
      }
    }
  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/binki.toml, Line %d, Column %d", datapath.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", p.description());
    return false;
  }

  return true;
}
