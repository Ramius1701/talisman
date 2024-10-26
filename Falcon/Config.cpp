#include "../Common/toml.hpp"
#include "Config.h"
#include "../Common/Logger.h"
#include <algorithm>
#include <fstream>
#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

bool Config::load(std::string datapath, Logger *log) {
  try {
    auto data = toml::parse_file(datapath + "/falcon.toml");

    auto _inbound = data["falcon"]["inbound"].as_string();

    if (_inbound == nullptr) {
      __inbound = "";
    } else {
      __inbound = _inbound->value_or("");
    }

    auto _hearts = data["falcon"]["heart_codes"].as_string();

    if (_hearts == nullptr) {
      _striphearts = true;
    } else {
      std::string val = _inbound->value_or("strip");

      if (val == "convert") {
        _striphearts = false;
      } else {
        _striphearts = true;
      }
    }

    // iterate over nets
    auto networkitems = data.get_as<toml::array>("network");

    if (networkitems != nullptr) {
      for (size_t i = 0; i < networkitems->size(); i++) {

        struct network_t newnet;

        auto itemtable = networkitems->get(i)->as_table();

        auto _outbox = itemtable->get("outbox");

        if (_outbox == nullptr) {
          newnet.outbox = "";
        } else {
          newnet.outbox = _outbox->as_string()->value_or("");
        }

        auto _netname = itemtable->get("name");

        if (_netname == nullptr) {
          newnet.name = "";
        } else {
          newnet.name = _netname->as_string()->value_or("");
        }

        auto _emailbase = itemtable->get("emailbase");

        if (_emailbase == nullptr) {
          newnet.emailbase = "";
        } else {
          newnet.emailbase = _emailbase->as_string()->value_or("");
        }

        auto _mynode = itemtable->get("mynode");

        if (_mynode == nullptr) {
          newnet.mynode = 0;
        } else {
          newnet.mynode = _mynode->as_integer()->value_or(0);
        }

        auto _upnode = itemtable->get("uplink");

        if (_upnode == nullptr) {
          newnet.upnode = 0;
        } else {
          newnet.upnode = _upnode->as_integer()->value_or(0);
        }

        if (newnet.name == "" || newnet.emailbase == "" || newnet.outbox == "" || newnet.mynode == 0 || newnet.upnode == 0) {
          continue;
        } else {
          networks.push_back(newnet);
        }
      }
    }
    // iterate over areas
    auto areaitems = data.get_as<toml::array>("area");
    if (areaitems != nullptr) {
      for (size_t i = 0; i < areaitems->size(); i++) {
        struct area_t newarea;

        auto itemtable = areaitems->get(i)->as_table();

        auto _netname = itemtable->get("net");

        if (_netname == nullptr) {
          newarea.netname = "";
        } else {
          newarea.netname = _netname->as_string()->value_or("");
        }

        auto _basefile = itemtable->get("file");

        if (_basefile == nullptr) {
          newarea.basefile = "";
        } else {
          newarea.basefile = _basefile->as_string()->value_or("");
        }
        auto _subtype = itemtable->get("subtype");

        if (_subtype == nullptr) {
          newarea.subtype = "";
        } else {
          newarea.subtype = _subtype->as_string()->value_or("");
        }

        auto _mynode = itemtable->get("mynode");

        if (_mynode == nullptr) {
          newarea.mynode = 0;
        } else {
          newarea.mynode = _mynode->as_integer()->value_or(0);
        }

        auto _upnode = itemtable->get("host");

        if (_upnode == nullptr) {
          newarea.hostnode = 0;
        } else {
          newarea.hostnode = _upnode->as_integer()->value_or(0);
        }

        auto _hidden_sub = itemtable->get("hidden");
        if (_hidden_sub == nullptr) {
          newarea.hidden_sub = false;
        } else {
          newarea.hidden_sub = _hidden_sub->as_boolean()->value_or(false);
        }

        auto _manual_sub = itemtable->get("manual subscription");
        if (_manual_sub == nullptr) {
          newarea.manual_subsciption = false;
        } else {
          newarea.manual_subsciption = _manual_sub->as_boolean()->value_or(false);
        }

        auto _description = itemtable->get("description");
        if (_description == nullptr) {
          newarea.description = "No description.";
        } else {
          newarea.description = _description->as_string()->value_or("No description.");
        }

        auto _cat = itemtable->get("category");
        if (_cat == nullptr) {
          newarea.category = 0;
        } else {
          newarea.category = _cat->as_integer()->value_or(0);
        }

        if (newarea.basefile == "" || newarea.subtype == "" || newarea.mynode == 0 || newarea.hostnode == 0) {
          continue;
        }

        bool found = false;

        for (size_t j = 0; j < networks.size(); j++) {
          if (strcasecmp(networks.at(j).name.c_str(), newarea.netname.c_str()) == 0) {
            found = true;
            break;
          }
        }
        if (!found) {
          continue;
        }

        areas.push_back(newarea);
      }
    }
  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/falcon.toml, Line %d, Column %d", datapath.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }

  return true;
}
