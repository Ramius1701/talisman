#include "../Common/INIReader.h"
#include "../Common/wwivnet.h"
#include "../Common/Logger.h"
#include "Config.h"
#include "Scanner.h"
#include "SubReq.h"
#include <filesystem>
#include <iostream>

#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

void SubReq::add(const char *netname, int hostid, const char *subtype) {
  INIReader inir("talisman.ini");
  // static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  // static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  if (inir.ParseError()) {
    std::cerr << "Failed to parse talisman.ini" << std::endl;
    return;
  }

  _datapath = inir.Get("Paths", "Data Path", "data");
  _msgpath = inir.Get("Paths", "Message Path", "msgs");
  _logpath = inir.Get("Paths", "Log Path", "logs");
  _tmppath = inir.Get("Paths", "Temp Path", "temp");

  Logger log;

  log.load(_logpath + "/falcon.log");

  if (!config.load(_datapath, &log)) {
    std::cerr << "Failed to parse falcon.toml" << std::endl;
    return;
  }

  FILE *fptr;

  for (size_t i = 0; i < config.networks.size(); i++) {
    if (strcasecmp(config.networks.at(i).name.c_str(), netname) == 0) {

      std::filesystem::path fspath(config.networks.at(i).outbox + "/s" + std::to_string(config.networks.at(i).upnode) + ".net");

      if (std::filesystem::exists(fspath)) {
        fptr = fopen(fspath.u8string().c_str(), "ab");
      } else {
        fptr = fopen(fspath.u8string().c_str(), "wb");
      }
      if (!fptr) {
        log.log(LOG_ERROR, "Error opening %s", fspath.u8string().c_str());
        break;
      }

      struct net_header_rec msgrec;

      memset(&msgrec, 0, sizeof(struct net_header_rec));

      msgrec.main_type = 16;
      msgrec.fromsys = config.networks.at(i).mynode;
      msgrec.tosys = hostid;
      msgrec.daten = (uint32_t)time(NULL);
      msgrec.length = strlen(subtype) + 1;
      fwrite(&msgrec, sizeof(struct net_header_rec), 1, fptr);
      fwrite(subtype, msgrec.length, 1, fptr);
      fclose(fptr);
      return;
    }
  }
}

void SubReq::drop(const char *netname, int hostid, const char *subtype) {
  INIReader inir("talisman.ini");
  // static const char* months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  // static const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
  if (inir.ParseError()) {
    std::cerr << "Failed to parse talisman.ini" << std::endl;
    return;
  }

  _datapath = inir.Get("Paths", "Data Path", "data");
  _msgpath = inir.Get("Paths", "Message Path", "msgs");
  _logpath = inir.Get("Paths", "Log Path", "logs");
  _tmppath = inir.Get("Paths", "Temp Path", "temp");

  Logger log;

  log.load(_logpath + "/falcon.log");

  if (!config.load(_datapath, &log)) {
    std::cerr << "Failed to parse falcon.toml" << std::endl;
    return;
  }

  FILE *fptr;

  for (size_t i = 0; i < config.networks.size(); i++) {
    if (strcasecmp(config.networks.at(i).name.c_str(), netname) == 0) {

      std::filesystem::path fspath(config.networks.at(i).outbox + "/s" + std::to_string(config.networks.at(i).upnode) + ".net");

      if (std::filesystem::exists(fspath)) {
        fptr = fopen(fspath.u8string().c_str(), "ab");
      } else {
        fptr = fopen(fspath.u8string().c_str(), "wb");
      }
      if (!fptr) {
        log.log(LOG_ERROR, "Error opening %s", fspath.u8string().c_str());
        break;
      }

      struct net_header_rec msgrec;

      memset(&msgrec, 0, sizeof(struct net_header_rec));

      msgrec.main_type = 17;
      msgrec.fromsys = config.networks.at(i).mynode;
      msgrec.tosys = hostid;
      msgrec.daten = (uint32_t)time(NULL);
      msgrec.length = strlen(subtype) + 1;
      fwrite(&msgrec, sizeof(struct net_header_rec), 1, fptr);
      fwrite(subtype, msgrec.length, 1, fptr);
      fclose(fptr);
      return;
    }
  }
}
