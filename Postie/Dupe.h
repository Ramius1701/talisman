#pragma once
#include <cstdint>

class Config;

class Dupe
{
public:
	static uint32_t crc32buf(const char* buf, size_t len);
	static bool is_dupe(std::string crcfile, std::string msgid);
	static bool crc32file(const char *name, uint32_t *crc);
};

