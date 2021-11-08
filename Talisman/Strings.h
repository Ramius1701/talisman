#pragma once
#include <string>
#include <unordered_map>

class Strings {
public:
  Strings();
  void load(std::string stringfile);
  bool validate(std::string msgid, std::string msgstr);
  const char *fetch(const char *str);

private:
  std::string escapechars(std::string in);
  bool loaded;
  std::unordered_map<std::string, std::string> string_map;
};
