#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <iostream>
#include "Server.h"
#include "Config.h"
#include "../Common/INIReader.h"

int Server::run(int socket) {
    // load config
    INIReader inir("talisman.ini");
    
    if (inir.ParseError()) {
        std::cerr << "Failed to parse talisman.ini" << std::endl;
        return -1;
    }

    std::string _datapath = inir.Get("Paths", "Data Path", "data");
    std::string _logpath = inir.Get("Paths", "Log Path", "logs");
    std::string _tmppath = inir.Get("Paths", "Temp Path", "temp");

    Config c;

    if (!c.load(_datapath)) {
        std::cerr << "Error loading config!" << std::endl;
        return -1;
    }
    

#ifdef _MSC_VER
    WSADATA wsaData;


    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Error initializing winsock!" << std::endl;
        return -1;
    }
#endif

	return 0;
}