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
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <limits.h>
#ifdef __APPLE__
#include <libproc.h>
#endif
#endif
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include "../Common/INIReader.h"
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

bool in_multiallowed(std::vector<std::string>* list, std::string item) {
	for (size_t i = 0; i < list->size(); i++) {
		if (list->at(i) == item) {
			return true;
		}
	}

	return false;
}

int main()
{
	int sshport;
	int gopherport;
    bool ipv6 = false;
	int port;
	struct sockaddr_in gopher_serv_addr, ssh_serv_addr, serv_addr, client_addr;
    struct sockaddr_in6 gopher_serv_addr6, ssh_serv_addr6, serv_addr6, client_addr6;
	int csockfd;
	int on = 1;
	int max_nodes = 4;
	int i;
	char str[INET6_ADDRSTRLEN];
	std::vector<struct node_t> nodes;
	std::vector<std::string> multiallowed;
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
	gopherport = inir.GetInteger("main", "gopher port", -1);
	datapath = inir.Get("paths", "data path", "data");
    ipv6 = inir.GetBoolean("main", "enable ipv6", false);

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

	std::ifstream multiallowf(datapath + "/multiallow.ip");
	while (std::getline(multiallowf, line)) {
		multiallowed.push_back(line);
	}
	multiallowf.close();

	for (i = 0; i < max_nodes; i++) {
		struct node_t n;
		n.pid = 0;
		n.ip = "";

		nodes.push_back(n);
	}

	// listen on telnet

	int telnetfd = socket(AF_INET, SOCK_STREAM, 0);
    int telnetfd6 = -1;

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



    // listen on telnet6

    if (ipv6) {
        memset(&serv_addr6, 0, sizeof(struct sockaddr_in6));

        serv_addr6.sin6_family = AF_INET6;
        serv_addr6.sin6_addr = in6addr_any;
        serv_addr6.sin6_port = htons(port);
        if (setsockopt(telnetfd6, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&on, sizeof(on)) < 0) {
            std::cerr << "Error setting IPV6_V6ONLY (Telnet - ipv6)" << std::endl;
            return -1;
        }
        if (setsockopt(telnetfd6, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
            std::cerr << "Error setting SO_REUSEADDR (Telnet - ipv6)" << std::endl;
            return -1;
        }
        if (setsockopt(telnetfd6, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) < 0) {
            std::cerr << "Error setting TCP_NODELAY (Telnet - ipv6)" << std::endl;
            return -1;
        }
        if (bind(telnetfd6, (struct sockaddr*)&serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
            std::cerr << "Error binding. (Telnet - ipv6)" << std::endl;
            return -1;
        }

        listen(telnetfd6, 5);
        std::cerr << "Listening on port " << port << "(TELNET - ipv6)" << std::endl;

    }

	listen(telnetfd, 5);
	std::cerr << "Listening on port " << port << "(TELNET)" << std::endl;

	int sshfd = -1;
    int sshfd6 = -1;

	if (sshport != -1) {
		sshfd = socket(AF_INET, SOCK_STREAM, 0);

		memset(&ssh_serv_addr, 0, sizeof(struct sockaddr_in));

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


        if (ipv6) {
            sshfd6 = socket(AF_INET6, SOCK_STREAM, 0);

            memset(&ssh_serv_addr6, 0, sizeof(struct sockaddr_in6));

            ssh_serv_addr6.sin6_family = AF_INET6;
            ssh_serv_addr6.sin6_addr = in6addr_any;
            ssh_serv_addr6.sin6_port = htons(sshport);
            if (setsockopt(sshfd6, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&on, sizeof(on)) < 0) {
                std::cerr << "Error setting IPV6_V6ONLY (SSH - ipv6)" << std::endl;
                return -1;
            }
            if (setsockopt(sshfd6, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
                std::cerr << "Error setting SO_REUSEADDR (SSH - ipv6)" << std::endl;
                return -1;
            }
            if (setsockopt(sshfd6, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) < 0) {
                std::cerr << "Error setting TCP_NODELAY (SSH - ipv6)" << std::endl;
                return -1;
            }
            if (bind(sshfd6, (struct sockaddr*)&ssh_serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
                std::cerr << "Error binding. (SSH - ipv6)" << std::endl;
                return -1;
            }

            listen(sshfd6, 5);
            std::cerr << "Listening on port " << sshport << "(SSH - ipv6)" << std::endl;
        }
		listen(sshfd, 5);
		std::cerr << "Listening on port " << sshport << "(SSH)" << std::endl;
	}

	int gopherfd = -1;
    int gopherfd6 = -1;

	if (gopherport != -1) {
		gopherfd = socket(AF_INET, SOCK_STREAM, 0);

		memset(&gopher_serv_addr, 0, sizeof(struct sockaddr_in));

		gopher_serv_addr.sin_family = AF_INET;
		gopher_serv_addr.sin_addr.s_addr = INADDR_ANY;
		gopher_serv_addr.sin_port = htons(gopherport);
		if (setsockopt(gopherfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
			std::cerr << "Error setting SO_REUSEADDR (Gopher)" << std::endl;
			return -1;
		}
		if (setsockopt(gopherfd, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) < 0) {
			std::cerr << "Error setting TCP_NODELAY (Gopher)" << std::endl;
			return -1;
		}
		if (bind(gopherfd, (struct sockaddr*)&gopher_serv_addr, sizeof(struct sockaddr_in)) < 0) {
			std::cerr << "Error binding. (Gopher)" << std::endl;
			return -1;
		}


        if (ipv6) {
            gopherfd6 = socket(AF_INET6, SOCK_STREAM, 0);

            memset(&gopher_serv_addr6, 0, sizeof(struct sockaddr_in6));

            gopher_serv_addr6.sin6_family = AF_INET6;
            gopher_serv_addr6.sin6_addr = in6addr_any;
            gopher_serv_addr6.sin6_port = htons(gopherport);

            if (setsockopt(gopherfd6, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&on, sizeof(on)) < 0) {
                std::cerr << "Error setting IPV6_V6ONLY (Gopher - ipv6)" << std::endl;
                return -1;
            }
            if (setsockopt(gopherfd6, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0) {
                std::cerr << "Error setting SO_REUSEADDR (Gopher - ipv6)" << std::endl;
                return -1;
            }
            if (setsockopt(gopherfd6, IPPROTO_TCP, TCP_NODELAY, (char*)&on, sizeof(on)) < 0) {
                std::cerr << "Error setting TCP_NODELAY (Gopher - ipv6)" << std::endl;
                return -1;
            }
            if (bind(gopherfd6, (struct sockaddr*)&gopher_serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
                std::cerr << "Error binding. (Gopher - ipv6)" << std::endl;
                return -1;
            }

            listen(gopherfd6, 5);
            std::cerr << "Listening on port " << gopherport << "(Gopher - ipv6)" << std::endl;
        }
        listen(gopherfd, 5);
		std::cerr << "Listening on port " << gopherport << "(Gopher)" << std::endl;
	}

	int nfds;
	int maxfd = telnetfd;
	fd_set server_fds;
	FD_ZERO(&server_fds);
	FD_SET(telnetfd, &server_fds);
	if (gopherport != -1) {
		FD_SET(gopherfd, &server_fds);
		if (gopherfd > maxfd) {
			maxfd = gopherfd;
		}
	}

	if (sshport != -1) {
		FD_SET(sshfd, &server_fds);

		if (sshfd > maxfd) {
			maxfd = sshfd;
		}
	}

	if (ipv6) {
        FD_SET(telnetfd6, &server_fds);
        if (telnetfd6 > maxfd) maxfd = telnetfd6;

        if (gopherport != -1) {
            FD_SET(gopherfd6, &server_fds);
            if (gopherfd6 > maxfd) {
                maxfd = gopherfd6;
            }
        }

        if (sshport != -1) {
            FD_SET(sshfd6, &server_fds);

            if (sshfd6 > maxfd) {
                maxfd = sshfd6;
            }
        }

    }

	nfds = maxfd;
	nfds++;

	while (1) {
		csockfd = -1;
		bool telnet = false;
        bool ipv6con = false;
		fd_set copy_fds = server_fds;
		
		int clen = sizeof(struct sockaddr_in);
		int clen6 = sizeof(struct sockaddr_in6);

		memset(&client_addr, 0, clen);

		if (select(nfds, &copy_fds, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				continue;
			}
			else {
				exit(-1);
			}
		}
	
		if (FD_ISSET(telnetfd, &copy_fds)) {
			csockfd = accept(telnetfd, (struct sockaddr*)&client_addr, (socklen_t*)&clen);
			telnet = true;
		}
		if (ipv6){
            if (FD_ISSET(telnetfd6, &copy_fds)) {
                csockfd = accept(telnetfd6, (struct sockaddr*)&client_addr6, (socklen_t*)&clen6);
                telnet = true;
                ipv6con = true;
            }
        }
		if (sshport != -1) {
			if (FD_ISSET(sshfd, &copy_fds)) {
				csockfd = accept(sshfd, (struct sockaddr*)&client_addr, (socklen_t*)&clen);
			}
            if (ipv6){
                if (FD_ISSET(sshfd6, &copy_fds)) {
                    csockfd = accept(sshfd6, (struct sockaddr*)&client_addr6, (socklen_t*)&clen6);
                    ipv6con = true;
                }
            }
		}
		if (gopherport != -1) {
			if (FD_ISSET(gopherfd, &copy_fds)) {
                csockfd = accept(gopherfd, (struct sockaddr*)&client_addr, (socklen_t*)&clen);
#ifdef _MSC_VER
				std::stringstream ss;
				ss.str("");
				ss << "\"gofer.exe\" " << csockfd;
				
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

				if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
					std::cerr << "Failed to create process!" << std::endl;
					free(cmd);
					closesocket(csockfd);
					continue;
				}
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				free(cmd);
				closesocket(csockfd);
#else
				pid_t pid = fork();
				if (pid == 0) {
					snprintf(sockstr, 10, "%d", csockfd);
					if (execlp("./gofer", "./gofer", sockstr, NULL) == -1) {
						perror("Execlp: ");
						exit(-1);
					}
				}
				else if (pid == -1) {
					std::cerr << "Failed to create process!" << std::endl;
					close(csockfd);
				}
				else {
					close(csockfd);
				}
#endif
				continue;
			}
			if (ipv6) {
                if (FD_ISSET(gopherfd6, &copy_fds)) {
                    csockfd = accept(gopherfd6, (struct sockaddr*)&client_addr6, (socklen_t*)&clen6);
    #ifdef _MSC_VER
                    std::stringstream ss;
                    ss.str("");
                    ss << "\"gofer.exe\" " << csockfd;

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

                    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                        std::cerr << "Failed to create process!" << std::endl;
                        free(cmd);
                        closesocket(csockfd);
                        continue;
                    }
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    free(cmd);
                    closesocket(csockfd);
    #else
                    pid_t pid = fork();
                    if (pid == 0) {
                        snprintf(sockstr, 10, "%d", csockfd);
                        if (execlp("./gofer", "./gofer", sockstr, NULL) == -1) {
                            perror("Execlp: ");
                            exit(-1);
                        }
                    }
                    else if (pid == -1) {
                        std::cerr << "Failed to create process!" << std::endl;
                        close(csockfd);
                    }
                    else {
                        close(csockfd);
                    }
    #endif
                    continue;
                }
            }
		}
		if (csockfd != -1) {
			std::string ipaddr;
            if (ipv6con) {
                ipaddr = std::string(inet_ntop(AF_INET, &((struct sockaddr_in*)&client_addr)->sin_addr, str, sizeof(str)));
            } else {
                ipaddr = std::string(inet_ntop(AF_INET6, &((struct sockaddr_in6*)&client_addr6)->sin6_addr, str, sizeof(str)));
            }
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
			for (size_t n = 0; n < nodes.size(); n++) {
				if (nodes.at(n).ip == ipaddr) {
					alreadyloggedin = true;
					break;
				}
			}

			if (alreadyloggedin && !in_multiallowed(&multiallowed, ipaddr)) {
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
					si.dwFlags = STARTF_USESHOWWINDOW;
					si.wShowWindow = SW_MINIMIZE;
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
#ifdef __APPLE__
                    char buffer[PROC_PIDPATHINFO_MAXSIZE];
                    if (proc_name(nodes.at(i).pid, buffer, sizeof(buffer)) == -1) {
                        std::cerr << "ERROR getting proc_name" << std::endl;
                    }
                                       
                    if (strncmp(buffer, "talisman", 8) == 0) {
                        continue;
                    }
#else
					char buffer[PATH_MAX];
					snprintf(buffer, sizeof buffer, "/proc/%lu/cmdline", nodes.at(i).pid);
					FILE* fptr = fopen(buffer, "r");

					if (fptr) {
						fgets(buffer, sizeof buffer, fptr);
						fclose(fptr);

						if (strncmp(buffer, "./talisman", 10) == 0) {
							continue;
						}
					}
#endif
					nodes.at(i).pid = 0;
					nodes.at(i).ip = "";
				}
			}
			bool alreadyloggedin = false;
			for (i = 0; i < nodes.size(); i++) {
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
						if(sshport == -1) {
							close(telnetfd);
						}
						else {
							close(telnetfd);
							close(sshfd);
						}

						snprintf(sockstr, 10, "%d", csockfd);
						snprintf(nodestr, 10, "%d", i + 1);
						if (telnet) {
							std::cerr << "Launching Talisman (Telnet)" << std::endl;
							if (execlp("./talisman", "./talisman", "-S", sockstr, "-N", nodestr, "-T", NULL) == -1) {
								perror("Execlp: ");
								exit(-1);
							}
						}
						else {
							std::cerr << "Launching Talisman (SSH)" << std::endl;
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
			if (i == max_nodes) {
				std::cerr << "All nodes busy." << std::endl;
				send(csockfd, "BUSY\r\n", 6, 0);
				close(csockfd);
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
