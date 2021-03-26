// Gofer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
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
#include <sstream>

#include "Request.h"


int main(int argc, char** argv)
{
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

    while (true) {
        char c;

        int ret = recv(socket, &c, 1, NULL);

        if (ret == 0) {
            closesocket(socket);
            WSACleanup();
            return 0;
        }
        else if (ret == -1) {
            closesocket(socket);
            WSACleanup();
            return 0;
        }
        if (c == '\n') {
            Request r;

            r.dorequest(socket, ss.str());
#ifdef _MSC_VER
            closesocket(socket);
#else
            close(socket);
#endif
            WSACleanup();
            exit(0);
        }
        else if (c != '\r') {
            ss << c;
        }

    }
}
