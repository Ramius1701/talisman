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
#include "IPBlockItem.h"

#ifndef _MSC_VER
void sigchld_handler(int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	errno = saved_errno;
}
#endif

struct node_t {
	unsigned long pid;
	std::string ip;
};

std::vector<IPBlockItem*>* blocklist;
std::string datapath;

bool should_pass(std::string ip) {
	for (size_t i = 0; i < blocklist->size(); i++) {
		if (ip == blocklist->at(i)->getip()) {
			return blocklist->at(i)->should_pass();
		}
	}

	IPBlockItem* blockitem = new IPBlockItem(ip, datapath, false, false);
	blocklist->push_back(blockitem);
	return true;
}

int main()
{
	int sshport;
	int port;
	struct sockaddr_in ssh_serv_addr, serv_addr, client_addr;
	int csockfd;
	int clen = sizeof(struct sockaddr_in);
	int on = 1;
	int max_nodes = 4;
	int i;

	char str[INET6_ADDRSTRLEN];
	std::vector<struct node_t> nodes;
#ifdef _MSC_VER
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "Error initializing winsock!" << std::endl;
		return -1;
	}
#else 
	struct sigaction sa;
	char sockstr[10];
	char nodestr[10];
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
	sshport = inir.GetInteger("main", "ssh port", -1);
	max_nodes = inir.GetInteger("main", "max nodes", 4);
	datapath = inir.Get("paths", "data path", "data");

	blocklist = new std::vector<IPBlockItem*>();

	std::ifstream passlistf(datapath + "/passlist.ip");
	std::string line;

	while (std::getline(passlistf, line)) {
		IPBlockItem* item = new IPBlockItem(line, datapath, false, true);
		blocklist->push_back(item);
	}
	passlistf.close();

	std::ifstream blocklistf(datapath + "/blocklist.ip");
	while (std::getline(blocklistf, line)) {
		IPBlockItem* item = new IPBlockItem(line, datapath, true, false);
		blocklist->push_back(item);
	}
	blocklistf.close();

	for (i = 0; i < max_nodes; i++) {
		struct node_t n;
		n.pid = 0;
		n.ip = "";

		nodes.push_back(n);
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
	if (bind(telnetfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in)) < 0) {
		std::cerr << "Error binding. (Telnet)" << std::endl;
		return -1;
	}

	listen(telnetfd, 5);
	std::cerr << "Listening on port " << port << "(TELNET)" << std::endl;

	int sshfd;

	if (sshport != -1) {
		sshfd = socket(AF_INET, SOCK_STREAM, 0);

		memset(&serv_addr, 0, sizeof(struct sockaddr_in));

		ssh_serv_addr.sin_family = AF_INET;
		ssh_serv_addr.sin_addr.s_addr = INADDR_ANY;
		ssh_serv_addr.sin_port = htons(sshport);
		if (setsockopt(sshfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
			std::cerr << "Error setting SO_REUSEADDR (SSH)" << std::endl;
			return -1;
		}
		if (setsockopt(sshfd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) < 0) {
			std::cerr << "Error setting TCP_NODELAY (SSH)" << std::endl;
			return -1;
		}
		if (bind(sshfd, (struct sockaddr*)&ssh_serv_addr, sizeof(struct sockaddr_in)) < 0) {
			std::cerr << "Error binding. (SSH)" << std::endl;
			return -1;
		}

		listen(sshfd, 5);
		std::cerr << "Listening on port " << sshport << "(SSH)" << std::endl;
	}
	int nfds;
	fd_set server_fds;
	FD_ZERO(&server_fds);
	FD_SET(telnetfd, &server_fds);
	if (sshport != -1) {
		FD_SET(sshfd, &server_fds);

		if (telnetfd > sshfd) {
			nfds = telnetfd;
		}
		else {
			nfds = sshfd;
		}
	}
	else {
		nfds = telnetfd;
	}
	nfds++;

	while (1) {
		csockfd = -1;
		bool telnet = false;
		fd_set copy_fds = server_fds;
		select(nfds, &copy_fds, NULL, NULL, NULL);
		if (FD_ISSET(telnetfd, &copy_fds)) {
			csockfd = accept(telnetfd, (struct sockaddr*)&client_addr, (socklen_t*)&clen);
			telnet = true;
		}
		if (sshport != -1) {
			if (FD_ISSET(sshfd, &copy_fds)) {
				csockfd = accept(sshfd, (struct sockaddr*)&client_addr, (socklen_t*)&clen);
			}
		}
		if (csockfd != -1) {
			std::string ipaddr = std::string(inet_ntop(AF_INET, &((struct sockaddr_in*)&client_addr)->sin_addr, str, sizeof(str)));
			if (!should_pass(ipaddr)) {
				std::cerr << "Blocking ip " << ipaddr << " (Blocklist)" << std::endl;
#ifdef _MSC_VER
				closesocket(csockfd);
#else
				close(csockfd);
#endif
				continue;
			}


#ifdef _MSC_VER

			for (i = 0; i < max_nodes; i++) {
				if (nodes.at(i).pid != 0) {
					HANDLE Handle = OpenProcess(
						PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
						FALSE,
						nodes.at(i).pid
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
					nodes.at(i).pid = 0;
					nodes.at(i).ip = "";
				}
			}

			bool alreadyloggedin = false;
			for (size_t i = 0; i < nodes.size(); i++) {
				if (nodes.at(i).ip == ipaddr) {
					alreadyloggedin = true;
					break;
				}
			}

			if (alreadyloggedin) {
				std::cerr << "Blocking ip " << ipaddr << " (Already logged in)" << std::endl;
				closesocket(csockfd);
				continue;
			}

			for (i = 0; i < max_nodes; i++) {
				if (nodes.at(i).pid == 0) {
					std::stringstream ss;
					ss.str("");
					ss << "\"talisman.exe\"" << " -S " << csockfd << " -N " << std::to_string(i + 1);
					if (telnet) {
						ss << " -T";
					}
					else {
						ss << " -SSH";
					}
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
					nodes.at(i).pid = pi.dwProcessId;
					nodes.at(i).ip = ipaddr;
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
				if (nodes.at(i).pid != 0) {
					char buffer[PATH_MAX];
					snprintf(buffer, sizeof buffer, "/proc/%d/cmdline", nodes.at(i).pid);
					FILE* fptr = fopen(buffer, "r");

					if (fptr) {
						fgets(buffer, sizeof buffer, fptr);
						fclose(fptr);

						if (strncmp(buffer, "./talisman", 10) == 0) {
							continue;
						}
					}
					nodes.at(i).pid = 0;
					nodes.at(i).ip = "";
				}
			}
			bool alreadyloggedin = false;
			for (size_t i = 0; i < nodes.size(); i++) {
				if (nodes.at(i).ip == ipaddr) {
					alreadyloggedin = true;
					break;
				}
			}

			if (alreadyloggedin) {
				std::cerr << "Blocking ip " << ipaddr << " (Already logged in)" << std::endl;
				close(csockfd);
				continue;
			}
			for (i = 0; i < max_nodes; i++) {
				if (nodes.at(i).pid == 0) {

					pid_t pid = fork();

					if (pid > 0) {
						nodes.at(i).pid = pid;
						nodes.at(i).ip = ipaddr;
						close(csockfd);
					}
					else if (pid == 0) {
						close(telnetfd);

						snprintf(sockstr, 10, "%d", csockfd);
						snprintf(nodestr, 10, "%d", i + 1);
						if (telnet) {
							if (execlp("./talisman", "./talisman", "-S", sockstr, "-N", nodestr, "-T", NULL) == -1) {
								perror("Execlp: ");
								exit(-1);
							}
						}
						else {
							if (execlp("./talisman", "./talisman", "-S", sockstr, "-N", nodestr, "-SSH", NULL) == -1) {
								perror("Execlp: ");
								exit(-1);
							}
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
