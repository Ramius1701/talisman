#pragma once
#include <cstdint>
#include <string>
#include "../Common/Squish.h"
class Config;

class Dupe {
public:
  static uint32_t crc32buf(const char *buf, size_t len);
  static bool is_dupe(std::string crcfile, std::string msgid, struct sq_msg *msg);
  static bool crc32file(const char *name, uint32_t *crc);
};
