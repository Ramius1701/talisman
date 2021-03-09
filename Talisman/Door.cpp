#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#if defined (__OpenBSD__) || defined (__NetBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <libgen.h>
#include <termios.h>
#if defined(__FreeBSD__)
#include <libutil.h>
#else
#include <util.h>
#endif
#include <sys/ioctl.h>
#else
#include <pty.h>
#endif
#include <signal.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include "GenDefs.h"
#include "Door.h"
#include "Config.h"
#include "Node.h"
#include "User.h"
#include "CallLog.h"

#ifndef _MSC_VER
#define _stricmp strcasecmp
#define LINE_END "\r\n"
#else
#define LINE_END "\n"
#endif

#ifndef _MSC_VER

int running_door;


void doorchld_handler(int s) {
	while (waitpid(-1, NULL, WNOHANG) > 0);

	running_door = 0;
}


int ttySetRaw(int fd, struct termios *prevTermios) {
	struct termios t;

	if (tcgetattr(fd, &t) == -1)
		return -1;

	if (prevTermios != NULL)
		*prevTermios = t;

	t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
	t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);
	t.c_oflag &= ~OPOST;
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSAFLUSH, &t) == -1)
		return -1;

	return 0;
}


#endif

void Door::createDropfiles(Node *n) {
	std::filesystem::path fpath;
	fpath.append(n->get_config()->tmp_path());
	fpath.append(std::to_string(n->getnodenum()));
	if (!std::filesystem::exists(fpath)) {
		std::filesystem::create_directories(fpath);
	}

	std::filesystem::path chaintxt(fpath);
	chaintxt.append("chain.txt");

	std::ofstream f3(chaintxt);

	f3 << n->get_user().get_uid() << LINE_END;
	f3 << n->get_user().get_username() << LINE_END;
	f3 << n->get_user().get_attribute("fullname", "UNKNOWN") << LINE_END;
	f3 << "NONE" << LINE_END;
	f3 << "21" << LINE_END;
	f3 << "M" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "01/01/71" << LINE_END;
	f3 << "80" << LINE_END;
	f3 << "25" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "1" << LINE_END;
	f3 << "1" << LINE_END;
	f3 << std::to_string(n->get_timeleft()) << LINE_END;
	f3 << n->get_config()->gfile_path() << LINE_END;
	f3 << n->get_config()->tmp_path() << LINE_END;
	f3 << "NOLOG" << LINE_END;
	f3 << "115200" << LINE_END;
	f3 << "1" << LINE_END;
	f3 << n->get_config()->sys_name() << LINE_END;
	f3 << n->get_config()->op_name() << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "0" << LINE_END;
	f3 << "8N1" << LINE_END;
	f3 << "115200" << LINE_END;
	f3 << "0" << LINE_END;			
	f3.close();

	std::filesystem::path d32path(fpath);

	d32path.append("door32.sys");

	std::ofstream f(d32path);

	f << "2" << LINE_END;
	f << n->get_socket() << LINE_END;
	f << "38400" << LINE_END;
	f << "Talisman v" << VERSION_MAJOR << "." << VERSION_MINOR << "-" << VERSION_STR << LINE_END; // TODO: Add version
	f << n->get_user().get_uid() << LINE_END;
	f << n->get_user().get_attribute("fullname", "UNKNOWN") << LINE_END;
	f << n->get_user().get_username() << LINE_END;
	f << n->get_user().get_sec_level() << LINE_END;
	f << std::to_string(n->get_timeleft() / 60) << LINE_END; // TODO: Time left
	f << (n->hasANSI ? "1" : "0") << LINE_END;
	f << n->getnodenum() << LINE_END;

	f.close();

	std::filesystem::path doorsyspath(fpath);
	doorsyspath.append("door.sys");

	std::ofstream f2(doorsyspath);

	f2 << "COM1:" << LINE_END;
	f2 << "38400" << LINE_END;
	f2 << "8" << LINE_END;
	f2 << n->getnodenum() << LINE_END;
	f2 << "38400" << LINE_END;
	f2 << "Y" << LINE_END;
	f2 << "N" << LINE_END;
	f2 << "Y" << LINE_END;
	f2 << "Y" << LINE_END;
	f2 << n->get_user().get_attribute("fullname", "UNKNOWN") << LINE_END;
	f2 << n->get_user().get_attribute("location", "Somewhere, The World") << LINE_END;
	f2 << "00-0000-0000" << LINE_END;
	f2 << "00-0000-0000" << LINE_END;
	f2 << "SECRET" << LINE_END;
	f2 << n->get_user().get_sec_level() << LINE_END; // TODO: Security Level
	f2 << n->clog->total_calls(n->get_user().get_username()) << LINE_END;
	f2 << "01-01-1971" << LINE_END;
	f2 << std::to_string(n->get_timeleft()) << LINE_END;
	f2 << "999" << LINE_END;
	f2 << "GR" << LINE_END;
	f2 << "25" << LINE_END;
	f2 << "N" << LINE_END;
	f2 << LINE_END;
	f2 << LINE_END;
	f2 << LINE_END;
	f2 << n->get_user().get_uid() << LINE_END;
	f2 << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "99999" << LINE_END;
	f2 << "01-01-1971" << LINE_END;
	f2 << LINE_END;
	f2 << LINE_END;
	f2 << n->get_config()->op_name() << LINE_END;
	f2 << n->get_user().get_username() << LINE_END;
	f2 << "none" << LINE_END;
	f2 << "Y" << LINE_END;
	f2 << "N" << LINE_END;
	f2 << "Y" << LINE_END;
	f2 << "7" << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "01-01-1971" << LINE_END;
	f2 << "00:00" << LINE_END;
	f2 << "00:00" << LINE_END;
	f2 << "32768" << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "None." << LINE_END;
	f2 << "0" << LINE_END;
	f2 << "0" << LINE_END;

	f2.close();
}

bool telnet_bin_mode;

bool Door::runExternal(Node *n, std::string command, std::vector<std::string> args, bool raw) {
	n->stop_timeout = true;
#ifdef _MSC_VER
	std::stringstream ss;
	u_long mode = 0;
	ss.str("");
	ss << "\"" << command << "\"";
	for (int i = 0;i < args.size();i++) {
		ss << " " << args.at(i);
	}

	char *cmd = strdup(ss.str().c_str());

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
		n->print_f("\r\nFailed to run door\r\n");
		free(cmd);
		n->stop_timeout = false;
		return true;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	free(cmd);
	ioctlsocket(n->get_socket(), FIONBIO, &mode);

	return true;
#else
	// TODO unix door
	pid_t pid;
	char **argv;
	int door_in;
	int door_out;
	struct winsize ws;
	struct sigaction sa;
	int t;
	fd_set fdset;
	int master;
	int slave;
	struct timeval thetimeout;
	int ret;
	int len;
	unsigned char inbuf[256];
	unsigned char outbuf[512];
	int gotiac;
	int g;
	int h;
	unsigned char c;
	struct termios oldit2;
	int iac;

	unsigned char iac_binary_will[] = {IAC, IAC_WILL, IAC_TRANSMIT_BINARY, '\0'};
	unsigned char iac_binary_wont[] = {IAC, IAC_WONT, IAC_TRANSMIT_BINARY, '\0'};
	unsigned char iac_binary_do[] = {IAC, IAC_DO, IAC_TRANSMIT_BINARY, '\0'};
	unsigned char iac_binary_dont[] = {IAC, IAC_DONT, IAC_TRANSMIT_BINARY, '\0'};

	door_in = n->get_socket();
	door_out = n->get_socket();

	argv = (char **)malloc(sizeof(char *) * (args.size() + 2));
	if (!argv) {
		n->stop_timeout = false;
		return true;
	}

	argv[0] = strdup(command.c_str());
	for (int i=0;i<args.size();i++) {
		argv[i+1] = strdup(args.at(i).c_str());
	}
	argv[args.size() + 1] = NULL;

	ws.ws_row = 24;
	ws.ws_col = 80;
	running_door = 1;

	if (openpty(&master, &slave, NULL, NULL, &ws) == 0) {
		sa.sa_handler = doorchld_handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESTART | SA_SIGINFO;
		if (sigaction(SIGCHLD, &sa, NULL) == -1) {
			perror("sigaction");
			n->stop_timeout = false;
			return true;
		}
		
		ttySetRaw(master, &oldit2);
	    ttySetRaw(slave, &oldit2);

		pid = fork();

		if (pid < 0) {
			n->print_f("\r\nFailed to run door\r\n");
			n->stop_timeout = false;
			return true;
		} else if (pid == 0) {
			close(master);
			dup2(slave, 0);
			dup2(slave, 1);

			close(slave);
			setsid();
			ioctl(0, TIOCSCTTY, 1);
			execvp(command.c_str(), argv);
			exit(0);
		} else {
			gotiac = 0;
			while(running_door) {
				FD_ZERO(&fdset);
				FD_SET(master, &fdset);
				FD_SET(door_in, &fdset);

				if (master > door_in) {
					t = master + 1;
				} else {
					t = door_in + 1;
				}

				thetimeout.tv_sec = 5;
				thetimeout.tv_usec = 0;

				ret = select(t, &fdset, NULL, NULL, &thetimeout);
				if (ret > 0) {
					if (FD_ISSET(door_in, &fdset)) {
						len = read(door_in, inbuf, 256);
						if (len == 0) {
							close(master);
							for (int i=0;i<args.size() + 1;i++) {
								free(argv[i]);
							}
							free(argv);
							return false;
						}
						g = 0;
						for (h=0;h<len;h++) {
							c = inbuf[h];
							if (!raw) {
								if (c == '\n' || c == '\0') {
									continue;
								}
							}

							if (c == 255 && n->is_telnet()) {
								if (gotiac == 1) {
									outbuf[g++] = c;
									gotiac = 0;
								} else {
									gotiac = 1;
								}
							} else {
								if (gotiac == 1) {
									if (c == 254 || c == 253 || c == 252 || c == 251) {
										iac = c;
										gotiac = 2;
									} else if (c == 250) {
										gotiac = 3;
									} else {
										gotiac = 0;
									}
								} else if (gotiac == 2) {
									if (c == IAC_TRANSMIT_BINARY) {
										if (iac == IAC_DO) {
											if (!telnet_bin_mode) {
												write(master, iac_binary_will, 3);
												telnet_bin_mode = true;
											}
										} else if (iac == IAC_DONT) {
											if (telnet_bin_mode) {
												write(master, iac_binary_wont, 3);
												telnet_bin_mode = false;
											}
										} else if (iac == IAC_WILL) {
											if (!telnet_bin_mode) {
												write(master, iac_binary_do, 3);
												telnet_bin_mode = true;
											}
										} else if (iac == IAC_WONT) {
											if (telnet_bin_mode) {
												write(master, iac_binary_dont, 3);
												telnet_bin_mode = false;
											}
										}
									}
									gotiac = 0;
								} else if (gotiac == 3) {
									if (c == 240) {
										gotiac = 0;
									}
								} else {
									outbuf[g++] = c;
								}
							}
							
						}

						write(master, outbuf, g);
					} else if (FD_ISSET(master, &fdset)) {
						len = read(master, inbuf, 256);
						if (len == 0) {
							close(master);
							break;
						}

						g = 0;
						for (h = 0; h < len; h++) {
							c = inbuf[h];
							if (c == 255 && n->is_telnet()) {
								outbuf[g++] = c;
							}
							outbuf[g++] = c;
						}
						write(door_out, outbuf, g);
					}
				} else {
					if (ret == -1) {
						if (errno != EINTR) {
							n->disconnected();
						}
					}
				}
			}
		}
	}

	for (int i=0;i<args.size() + 1;i++) {
		free(argv[i]);
	}
	free(argv);
#endif
	n->stop_timeout = false;
	return true;
}

Door::Door()
{
}


Door::~Door()
{
}
