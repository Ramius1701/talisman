#include <cstring>
#include <iostream>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif
#include "qwkie.h"
#include "../Common/Logger.h"
#include "../Common/INIReader.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage ./qwkie [scan|toss|poll] (network)" << std::endl;
    return -1;
  }

  Qwkie q;
  Logger log;
  INIReader inir("talisman.ini");
  if (inir.ParseError()) {
    std::cerr << "Failed to parse talisman.ini" << std::endl;
    return -1;
  }

  std::string datapath = inir.Get("Paths", "Data Path", "data");
  std::string temppath = inir.Get("Paths", "Temp Path", "temp");
  std::string msgpath = inir.Get("Paths", "Message Path", "msgs");
  std::string logpath = inir.Get("Paths", "Log Path", "logs");
  log.load(logpath + "/qwkie.log");

  if (!q.loadConfig(datapath, msgpath, temppath, &log)) {
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
    if (argc > 2) {
      q.toss(std::string(argv[2]));
    } else {
      q.tossall();
    }
  } else if (strcasecmp(argv[1], "poll") == 0) {
    if (argc > 2) {
      q.poll(std::string(argv[2]));
    } else {
      q.pollall();
    }
  } else {
    std::cerr << "Usage ./qwkie [scan|toss|poll] (network)" << std::endl;
    return -1;
  }

  return 0;
}
