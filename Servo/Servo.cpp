#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Psapi.h>
#define strcasecmp _stricmp
#else
#include <sys/wait.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <limits.h>
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "INIReader.h"

#ifndef _MSC_VER
void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}
#endif

int main()
{
	int port;
	struct sockaddr_in serv_addr, client_addr;
	int csockfd;
	int clen = sizeof(struct sockaddr_in);
	int on = 1;
	int max_nodes = 4;
	int i;
#ifdef _MSC_VER
	WSADATA wsaData;
	std::vector<DWORD> nodes;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "Error initializing winsock!" << std::endl;
		return -1;
	}
#else 
	std::vector<pid_t> nodes;

	struct sigaction sa;
	char sockstr[10];

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction - sigchld");
		exit(1);
	}

#endif
	INIReader inir("talisman.ini");
	if (inir.ParseError() != 0) {
		return -1;
	}

	port = inir.GetInteger("main", "telnet port", 2323);
	max_nodes = inir.GetInteger("main", "max nodes", 4);

	for (i = 0; i < max_nodes; i++) {
		nodes.push_back(0);
	}

	int telnetfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&serv_addr, 0, sizeof(struct sockaddr_in));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);
	if (setsockopt(telnetfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
		std::cerr << "Error setting SO_REUSEADDR (Telnet)" << std::endl;
		return -1;
	}
	if (setsockopt(telnetfd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) < 0) {
		std::cerr << "Error setting TCP_NODELAY (Telnet)" << std::endl;
		return -1;
	}
	if (bind(telnetfd, (struct sockaddr*) & serv_addr, sizeof(struct sockaddr_in)) < 0) {
		std::cerr << "Error binding. (Telnet)" << std::endl;
		return -1;
	}

	listen(telnetfd, 5);
	std::cerr << "Listening on port " << port << "(TELNET)" << std::endl;

	while (1) {
		csockfd = accept(telnetfd, (struct sockaddr*)&client_addr, (socklen_t*)&clen);
#ifdef _MSC_VER

		for (i = 0; i < max_nodes; i++) {
			if (nodes.at(i) != 0) {
				HANDLE Handle = OpenProcess(
					PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
					FALSE,
					nodes.at(i)
				);

				if (Handle) {
					char pname[256];

					if (GetModuleBaseNameA(Handle, 0, pname, 256) != 0) {
						if (strcasecmp(pname, "talisman.exe") == 0) {
							CloseHandle(Handle);
							continue;
						}
					}
					CloseHandle(Handle);
				}
				nodes.at(i) = 0;
			}
		}

		for (i = 0; i < max_nodes; i++) {
			if (nodes.at(i) == 0) {
				std::stringstream ss;
				ss.str("");
				ss << "\"talisman.exe\"" << " -S " << csockfd << " -N " << std::to_string(i + 1) << " -T";
				char* cmd = strdup(ss.str().c_str());

				STARTUPINFOA si;
				PROCESS_INFORMATION pi;

				ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);
				//	si.dwFlags = STARTF_USESTDHANDLES;
				//	si.hStdInput = INVALID_HANDLE_VALUE;
				//	si.hStdError = INVALID_HANDLE_VALUE;
				//	si.hStdOutput = INVALID_HANDLE_VALUE;

				ZeroMemory(&pi, sizeof(pi));

				if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
					std::cerr << "Failed to create process!" << std::endl;
					free(cmd);
					closesocket(csockfd);
					continue;
				}
				nodes.at(i) = pi.dwProcessId;

				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				free(cmd);
				break;
			}
		}
		if (i == max_nodes) {
			send(csockfd, "BUSY\r\n", 6, 0);
		}
		closesocket(csockfd);
#else

		for (i = 0; i < max_nodes; i++) {
			if (nodes.at(i) != 0) {
				char buffer[PATH_MAX];
				snprintf(buffer, sizeof buffer, "/proc/%d/cmdline", nodes.at(i));
				FILE* fptr = fopen(buffer, "r");

				if (fptr) {
					fgets(buffer, sizeof buffer, fptr);
					fclose(fptr);

					if (strncmp(buffer, "./talisman", 10) == 0) {
						continue;
					}
				}
				nodes.at(i) = 0;
			}
		}

		for (i = 0; i < max_nodes; i++) {
			if (nodes.at(i) == 0) {

				pid_t pid = fork();

				if (pid > 0) {
					nodes.at(i) = pid;
					close(csockfd);
				}
				else if (pid == 0) {
					close(telnetfd);

					snprintf(sockstr, 10, "%d", csockfd);

					if (execlp("./talisman", "./talisman", "-S", sockstr, "-T", NULL) == -1) {
						perror("Execlp: ");
						exit(-1);
					}
				}
				else {
					std::cerr << "Failed to create process!" << std::endl;
					close(csockfd);
				}
				break;
			}
		}
#endif
	}
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
