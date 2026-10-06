#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (auto window = GetConsoleWindow()) ShowWindow(window, SW_HIDE);
  if (argc != 3) return 10;
  std::ifstream drop(std::string("temp/")+argv[1]+"/door32.sys");
  std::vector<std::string> lines;
  for (std::string line; std::getline(drop,line);) {
    if (!line.empty() && line.back()=='\r') line.pop_back();
    lines.push_back(line);
  }
  if (lines.size()!=11 || lines[0]!="2" || lines[1]!=argv[2] || lines[10]!=argv[1]) return 11;
  WSADATA data;
  if (WSAStartup(MAKEWORD(2,2), &data)) return 12;
  SOCKET socket=static_cast<SOCKET>(std::strtoull(argv[2],nullptr,10));
  u_long blocking=0;
  if (ioctlsocket(socket,FIONBIO,&blocking)) return 18;
  DWORD timeout=10000;
  setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
  std::string hello="\r\nDOOR_BITS:"+std::to_string(sizeof(void*)*8)+":"+lines[6]+"\r\n";
  if (send(socket,hello.data(),static_cast<int>(hello.size()),0)!=hello.size()) return 13;
  std::string input;
  while (input.find('\n')==std::string::npos) {
    char c;
    if (recv(socket,&c,1,0)!=1) return 14;
    input.push_back(c);
    if (input.size()>100) return 15;
  }
  if (input!="CHALLENGE_64BIT\r\n") return 16;
  std::string result="\r\nDOOR_REPLY_OK:"+std::to_string(sizeof(void*)*8)+"\r\n";
  if (send(socket,result.data(),static_cast<int>(result.size()),0)!=result.size()) return 17;
  std::ofstream report(std::string("door-result-")+std::to_string(sizeof(void*)*8)+".txt");
  report << "pointer_bits=" << sizeof(void*)*8 << "\nusername=" << lines[6]
         << "\nsocket=" << argv[2] << "\ndropfile_lines=" << lines.size() << "\nduplex=pass\n";
  // The socket belongs to the BBS session; leave it open for return to menu.
  return 0;
}
