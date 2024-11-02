#include <cstdint>
#include "../Common/INIReader.h"
#include "../Common/wwivnet.h"
#include "../Common/Logger.h"
#include "../Common/tendian.h"
#include "Config.h"
#include "Scanner.h"
#include "SubReq.h"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <cctype>
#include <sstream>


#ifdef _MSC_VER
#define strcasecmp stricmp
#else
#ifdef __FreeBSD__
#include <sys/endian.h>
#elif defined(__APPLE__)
#include <machine/endian.h>
#else
#include <endian.h>
#endif
#endif

static inline uint16_t host2le_s(uint16_t s) {
#if defined(__LITTLE_ENDIAN__)
  return s;
#else
  return (((s >> 8) & 0xffu) | ((s & 0xffu) << 8));
#endif
}

static inline uint32_t host2le_l(uint32_t s) {
#if defined(__LITTLE_ENDIAN__)
  return s;
#else
  return (((s & 0xff000000u) >> 24) | ((s & 0x00ff0000u) >> 8) | ((s & 0x0000ff00u) << 8) | ((s & 0x000000ffu) << 24));
#endif
}

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

  std::stringstream ss;

  for (size_t l = 0; l < strlen(subtype); l++) {
    ss << toupper(subtype[l]);
  }

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

      msgrec.main_type = host2le_s(16);
      msgrec.fromsys = host2le_s(config.networks.at(i).mynode);
      msgrec.tosys = host2le_s(hostid);
      msgrec.daten = host2le_l((uint32_t)time(NULL));
      msgrec.length = host2le_l(ss.str().length() + 1);
      fwrite(&msgrec, sizeof(struct net_header_rec), 1, fptr);
      fwrite(ss.str().c_str(), msgrec.length, 1, fptr);
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


  std::stringstream ss;

  for (size_t l = 0; l < strlen(subtype); l++) {
    ss << toupper(subtype[l]);
  }

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

      msgrec.main_type = host2le_s(17);
      msgrec.fromsys = host2le_s(config.networks.at(i).mynode);
      msgrec.tosys = host2le_s(hostid);
      msgrec.daten = host2le_l((uint32_t)time(NULL));
      msgrec.length = host2le_l(ss.str().length() + 1);
      fwrite(&msgrec, sizeof(struct net_header_rec), 1, fptr);
      fwrite(ss.str().c_str(), msgrec.length, 1, fptr);
      fclose(fptr);
      return;
    }
  }
}
