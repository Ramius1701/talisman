#pragma once
#include <string>
#include <vector>
class Archiver {
public:
  Archiver(std::string name, std::string extension, std::string unarc, std::string arc, int offset, uint8_t *bytes, int bytelen) {
    this->name = name;
    this->extension = extension;
    this->unarc = unarc;
    this->arc = arc;
    this->offset = offset;
    this->bytes = bytes;
    this->bytelen = bytelen;
  }

  std::string name;
  std::string extension;
  std::string unarc;
  std::string arc;
  int offset;
  uint8_t *bytes;
  int bytelen;

  void extract(std::string archive, std::string outdir);
  void extract(std::string archive, std::vector<std::string> filelist, std::string outdir);
  void compress(std::string archive, std::vector<std::string> filelist);

private:
  static void runexec(std::string cmd);
};
