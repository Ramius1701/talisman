#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <iostream>
#include <sstream>

#include "Request.h"

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Do not call directly..." << std::endl;
    exit(-1);
  }
  int socket = strtol(argv[1], NULL, 10);

  std::stringstream ss;

#ifdef _MSC_VER
  WSADATA wsaData;

  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "Error initializing winsock!" << std::endl;
    return -1;
  }
#endif
  Request r;
  if (!r.loadconfig()) {
#ifdef _MSC_VER
    closesocket(socket);
    WSACleanup();
#else
    close(socket);
#endif
    return 0;
  }

  std::string welcome = "200 Welcome\r\n";

  send(socket, welcome.c_str(), welcome.size(), 0);

  while (true) {
    char c;

    int ret = recv(socket, &c, 1, 0);

    if (ret == 0) {
#ifdef _MSC_VER
      closesocket(socket);
      WSACleanup();
#else
      close(socket);
#endif
      return 0;
    } else if (ret == -1) {
#ifdef _MSC_VER
      closesocket(socket);
      WSACleanup();
#else
      close(socket);
#endif
      return 0;
    }
    if (c == '\n') {

      if (!r.dorequest(socket, ss.str())) {
#ifdef _MSC_VER
        closesocket(socket);
        WSACleanup();
#else
        close(socket);
#endif
        exit(0);
      } else {
          ss.str("");
      }
    } else if (c != '\r') {
      ss << c;
    }
  }
}
