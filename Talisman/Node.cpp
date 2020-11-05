
#ifdef _MSC_VER
#define _WIN32_LEAN_AND_MEAN 1
#include <WinSock2.h>
#include <Windows.h>
#include <conio.h>

#define strcasecmp _stricmp

#else
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <sys/utsname.h>
#endif
#include <filesystem>
#include <sstream>
#include <iostream>
#include <ctime>
#include <fstream>
#include <algorithm>
#include "GenDefs.h"
#include "Node.h"
#include "Config.h"
#include "User.h"
#include "Menu.h"
#include "CallLog.h"
#include "Logger.h"
#include "Email.h"
#include "Bulletins.h"

static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

Node::Node(int node, int socket, bool telnet) {
	this->node = node;
	this->socket = socket;
	this->telnet = telnet;
	hasANSI = false;
	clog = nullptr;
	bulletins = nullptr;
	timeout = 0;
	stop_timeout = false;
	last_time_check = 0;
	timeleft = 120;
	log = new Logger();
}

Node::~Node() {
	if (clog != nullptr) {
		delete clog;
	}
	if (bulletins != nullptr) {
		delete bulletins;
	}
}

bool Node::detectANSI() {
	print_f("\x1b[6n");
	char buffer[1024];
	timeval t;
	time_t then = time(NULL);
	t.tv_sec = 1;
	t.tv_usec = 0;
	time_t now;
	int len;
	int gotnum = 0;
	int gotnum1 = 0;
	do {
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(socket, &fds);

		if (select(socket + 1, &fds, NULL, NULL, &t) < 0) {
			return false;
		}

		if (FD_ISSET(socket, &fds)) {
			len = recv(socket, buffer, 1024, 0);
			if (len == 0) {
				disconnected();
			}
			for (int i = 0; i < len; i++) {
				if (buffer[i] == '\x1b' && buffer[i + 1] == '[') {
					for (int j = i + 2; j < len; j++) {
						switch (buffer[j]) {
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
							gotnum = 1;
							break;
						case ';':
							gotnum1 = 1;
							gotnum = 0;
							break;
						case 'R':
							if (gotnum && gotnum1) {
								return true;
							}
							break;
						}
					}
				}
			}
		}

		now = time(NULL);
	} while (now - then < 5);

	return false;
}


void Node::send_file(std::filesystem::path p, bool pause) {
	char lastc = 'x';
	bool gottag = false;
	std::stringstream ss;
	std::ifstream in(p);
	int lines = 1;
	bool stop = false;
	char c;
	if (in.is_open()) {
		while (in.good() && !stop) {
			in.get(c);
			if (c == 0x1a) break;
			if (c == '@' && gottag == false) {
				gottag = true;
				continue;
			}
			if (c == '@' && gottag == true) {
				//deal with tag
				if (ss.str() == "MAILCONF") {
					int mailconf = stoi(u.get_attribute("cur_msg_conf", "-1"));
					if (socket) {
						if (mailconf != -1) {
							send(socket, config.msgconfs.at(mailconf).get_name().c_str(), config.msgconfs.at(mailconf).get_name().size(), 0);
						}
						else {
							send(socket, "None.", 5, 0);
						}
					}
					else {
						if (mailconf != -1) {
							std::cout << config.msgconfs.at(mailconf).get_name();
						}
						else {
							std::cout << "None.";
						}
					}
				}
				else if (ss.str() == "MAILAREA") {
					int mailconf = stoi(u.get_attribute("cur_msg_conf", "-1"));
					int mailarea = stoi(u.get_attribute("cur_msg_area", "-1"));

					if (socket) {
						if (mailconf != -1 && mailarea != -1) {
							send(socket, config.msgconfs.at(mailconf).areas.at(mailarea).get_name().c_str(), config.msgconfs.at(mailconf).areas.at(mailarea).get_name().size(), 0);
						}
						else {
							send(socket, "None.", 5, 0);
						}
					}
					else {
						if (mailconf != 1 && mailarea != -1) {
							std::cout << config.msgconfs.at(mailconf).areas.at(mailarea).get_name();
						}
						else {
							std::cout << "None.";
						}
					}
				}
				else if (ss.str() == "VERSION") {
					print_f("%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);
				}
				else if (ss.str() == "TIMELEFT") {
					print_f("%d mins", timeleft / 60);
				}
				else {
					if (socket) {
						send(socket, "@", 1, 0);
						send(socket, ss.str().c_str(), ss.str().size(), 0);
						send(socket, "@", 1, 0);
					}
					else {
						printf("@%s@", ss.str().c_str());
					}
				}
				ss.str("");
				gottag = false;
				continue;
			}
			if (gottag == true) {
				if (c == '\r' || c == '\n') {
					if (socket) {
						send(socket, "@", 1, 0);
						send(socket, ss.str().c_str(), ss.str().size(), 0);
					}
					else {
						printf("@%s", ss.str().c_str());
					}
					lastc = ss.str().at(ss.str().size() - 1);
					ss.str("");
					gottag = false;
				}
				else {
					ss << c;
					continue;
				}
			}

			if (socket) {

				if (c == '\n') {
					if (lastc != '\r') {
						send(socket, "\r", 1, 0);
					}
					lines++;
				}
				lastc = c;
				send(socket, &c, 1, 0);
				if (lines == 23 && pause) {
					print_f("|14More (Y/N/C) ? ");

					switch (tolower(getche())) {
					case 'n':
						stop = true;
						break;
					case 'c':
						pause = false;
						break;
					default:
						break;
					}
					print_f("|07\r\n");
					lines = 0;
				}
			}
			else {
				putchar(c);
			}
		}
		in.close();
	}
}

void Node::send_gfile(std::string filename) {
	send_gfile(filename, false);
}

void Node::send_gfile(std::string filename, bool pause) {

	std::filesystem::path p(config.gfile_path());
	if (hasANSI) {
		p.append(filename + ".ans");
		if (std::filesystem::exists(p)) {
			send_file(p, pause);
			print_f("\x1b[0m");
			return;
		}
	}

	p.clear();
	p.assign(config.gfile_path());
	p.append(filename + ".asc");
	if (std::filesystem::exists(p)) {
		send_file(p, pause);
	}
}

void Node::putch(const char c) {
	if (socket) {
		send(socket, &c, 1, 0);
	}
#ifdef _MSC_VER
	std::cout << c;
#endif
}

char Node::getche() {
	char c = getch();
	putch(c);
	return c;
}

bool Node::time_check() {
	time_t now = time(NULL);

	if (last_time_check == 0) {
		last_time_check = now;
	}


	if (now - last_time_check >= 60) {
		timeleft -= (now - last_time_check);
		if (u.get_uid() != 0) {
			u.set_attribute("time_left", std::to_string((int)(timeleft / 60)));
		}
		last_time_check = now;
	}

	if (timeleft <= 0) {
		return false;
	}
	return true;
}

char Node::getch() {
	char ch;
	int len;
	int stage = 0;
	char order = 0;
	char buffer[2048];
	int i = 0;
	struct timeval tv;
	struct sec_level_t* sl;

	if (socket != 0) {
		while (true) {
			fd_set rfd;
			FD_ZERO(&rfd);
			FD_SET(socket, &rfd);

			tv.tv_sec = 60;
			tv.tv_usec = 0;

			int rs = select(socket + 1, &rfd, NULL, NULL, &tv);
			if (rs == 0) {
				// one minute has elapsed
				if (!stop_timeout) {
					timeout++;
					if (timeout == timeoutmax - 1) {
						print_f("|14You are about to time out!\r\n");
					}
					else if (timeout == timeoutmax) {
						print_f("|12You have timed out, call back when you're there!\r\n");
#ifdef _MSC_VER
						closesocket(socket);
#else
						close(socket);
#endif
						disconnected();
					}
				}
				if (!time_check()) {
					print_f("|14You are out of time for today!\r\n");
#ifdef _MSC_VER
					closesocket(socket);
#else
					close(socket);
#endif
					disconnected();
				}
			}
			else if (rs == -1 && errno != EINTR) {
				disconnected();
			}
			else if (FD_ISSET(socket, &rfd)) {
				len = recv(socket, &ch, 1, 0);
				if (len == 0) {
					disconnected();
				}
				else if (len == -1) {
#ifdef _MSC_VER
					int err = WSAGetLastError();
					if (err == WSAENOTCONN) {
						disconnected();
					}
					else {
						closesocket(socket);
						disconnected();
					}
#else
					if (errno != EINTR) {
						close(socket);
						disconnected();
					}
#endif
					continue;
				}
				if (stage == 0) {
					if ((unsigned char)ch == IAC) {
						stage = 1;
					}
					else if (ch != '\n' && ch != '\0') {
						if (!time_check()) {
							print_f("|14You are out of time for today!\r\n");
#ifdef _MSC_VER
							closesocket(socket);
#else
							close(socket);
#endif
							disconnected();
						}
						return ch;
					}
				}
				else if (stage == 1) {
					if ((unsigned char)ch == IAC) {
						return ch;
					}
					else if ((unsigned char)ch == 240) {
						stage = 3;
					}
					else {
						order = ch;
						stage = 2;
					}
				}
				else if (stage == 2) {
					// handle iac
					stage = 0;
				}
				else if (stage == 3) {
					if ((unsigned char)ch == 250) {
						stage = 0;
					}
					else {
						if (i < 2047) {
							buffer[i++] = ch;
							buffer[i] = '\0';
						}
					}
				}
			}
		}
	}
	else {
		do {
#ifdef _MSC_VER
			ch = _getch();
#else
			ch = getchar();
#endif
		} while (ch == '\n');
	}
	if (!time_check()) {
		print_f("|14You are out of time for today!\r\n");
#ifdef _MSC_VER
		closesocket(socket);
#else
		close(socket);
#endif
		disconnected();
	}
	return ch;
}

std::string Node::get_string(int maxlen, bool masked, bool clear) {
	std::stringstream ss;
	if (hasANSI && !clear) {
		print_f("\x1b[s\x1b[1;37;41m");
		for (int i = 0; i < maxlen; i++) {
			print_f(" ");
		}
		print_f("\x1b[u");
	}
	
	do {
		char ch = getch();
		if (ch == '\r') {
			break;
		}
		else if (ch == 127 || ch == '\b') {
			if (ss.str().size() > 0) {
				std::string tempstr = ss.str().substr(0, ss.str().size() - 1);
				print_f("\x1b[D \x1b[D");
				ss.str("");
				ss << tempstr;
			}
		}
		else {
			if (masked) {
				putch('*');
			}
			else {
				putch(ch);
			}
			ss << ch;
		}

	} while (ss.str().length() < maxlen);

	if (hasANSI && !clear) {
		print_f("\x1b[0m");
	}

	return ss.str();
}

std::string Node::get_string(int maxlen, bool masked)
{
	return get_string(maxlen, masked, false);
}

void Node::cls() {
	if (hasANSI) {
		print_f("\x1b[2J\x1b[1;1H");
	}
	else {
		print_f("\r\n");
	}
}

void Node::send_str(const char* str) {
	if (socket != 0) {
		send(socket, str, strlen(str), 0);
	}
#ifdef _MSC_VER
	std::cout << str;
#endif
}

void Node::print_f(const char* fmt, ...)
{
	char buffer[2048];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buffer, sizeof buffer, fmt, args);

	for (size_t i = 0; i < strlen(buffer); i++) {
		if (i + 2 < strlen(buffer) && buffer[i] == '|' && buffer[i + 1] >= '0' && buffer[i + 1] <= '9' && buffer[i + 2] >= '0' && buffer[i + 2] <= '9') {
			int pipecolor = (buffer[i + 1] - '0') * 10 + (buffer[i + 2] - '0');
			
			switch (pipecolor) {
			case 0:
				send_str("\x1b[0;30m");
				break;
			case 1:
				send_str("\x1b[0;34m");
				break;
			case 2:
				send_str("\x1b[0;32m");
				break;
			case 3:
				send_str("\x1b[0;36m");
				break;
			case 4:
				send_str("\x1b[0;31m");
				break;
			case 5:
				send_str("\x1b[0;35m");
				break;
			case 6:
				send_str("\x1b[0;33m");
				break;
			case 7:
				send_str("\x1b[0;37m");
				break;
			case 8:
				send_str("\x1b[1;30m");
				break;
			case 9:
				send_str("\x1b[1;34m");
				break;
			case 10:
				send_str("\x1b[1;32m");
				break;
			case 11:
				send_str("\x1b[1;36m");
				break;
			case 12:
				send_str("\x1b[1;31m");
				break;
			case 13:
				send_str("\x1b[1;35m");
				break;
			case 14:
				send_str("\x1b[1;33m");
				break;
			case 15:
				send_str("\x1b[1;37m");
				break;
			}
			
			
			i += 2;
			continue;
		}
		else {
			if (socket != 0) {
				send(socket, &buffer[i], 1, 0);
			}
#ifdef _MSC_VER
			std::cout << buffer[i];
#endif
		}
	}

	va_end(args);
}

std::string Node::operating_system() {
#ifdef _MSC_VER
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	switch (si.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_AMD64:
		return std::string("Windows/x64");
	case PROCESSOR_ARCHITECTURE_INTEL:
		return std::string("Windows/x86");
	case PROCESSOR_ARCHITECTURE_ARM:
		return std::string("Windows/ARM");
	case PROCESSOR_ARCHITECTURE_ARM64:
		return std::string("Windows/ARM64");
	case PROCESSOR_ARCHITECTURE_IA64:
		return std::string("Windows/Itanium");
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
		return std::string("Windows/Unknown");
	}
#else
	struct utsname sys;
	std::stringstream ss;
	ss.str("");
	uname(&sys);

	ss << sys.sysname << "/" << sys.machine;

	return ss.str();
#endif

	return std::string("Unknown");
}

void Node::system_info() {
	cls();
	send_gfile("sysinfo");
	print_f("|15Talisman BBS v%d.%d-%s\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);
	print_f("Copyright (C) 2020, Andrew Pamment\r\n");
	print_f("All rights reserved.\r\n\r\n");

	print_f("|15System Name: |14%s\r\n", config.sys_name().c_str());
	print_f("|15 Sysop Name: |14%s\r\n", config.op_name().c_str());
	print_f("|15         OS: |14%s\r\n", operating_system().c_str());
	print_f("|15       Node: |14%d\r\n\r\n", node);

	print_f("|14Press any key...|07");
	getch();

	cls();
	send_gfile("system");
	print_f("|14Press any key...|07");
	getch();

	cls();
	send_gfile("login");
}

int Node::run() {

	unsigned char iac_echo[] = { IAC, IAC_WILL, IAC_ECHO, '\0' };
	unsigned char iac_sga[] = { IAC, IAC_WILL, IAC_SUPPRESS_GO_AHEAD, '\0' };
	bool logged_in = false;

	if (socket != 0) {
#ifdef _MSC_VER
		WSADATA wsaData;

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			std::cerr << "Error initializing winsock!" << std::endl;
			return -1;
		}
#endif
		if (telnet) {
			send(socket, (char *)iac_echo, 3, 0);
			send(socket, (char *)iac_sga, 3, 0);
		}
	}

	print_f("Talisman v%d.%d-%s; Copyright (c) 2020; Andrew Pamment\r\n", VERSION_MAJOR, VERSION_MINOR, VERSION_STR);

	/* Load configuration */
	if (!config.load(this, "talisman.ini")) {
		print_f("Unable to load config! (Exiting)\r\n");
		return -1;
	}

	log->load(config.get_logpath() + "/talisman.log");

	u.set_config(config);

	if (socket) {
		print_f("Detecting ANSI Graphics... ");
		hasANSI = detectANSI();
		if (hasANSI) {
			print_f("DETECTED\r\n");
		}
		else {
			print_f("NOT DETECTED\r\n");
		}
	}
	else {
		hasANSI = true;
	}

	send_gfile("welcome");

	int tries = 0;

	while (!logged_in) {
		print_f("\r\nEnter USERNAME or NEW\r\n");
		print_f("LOGIN: ");
		std::string login = get_string(16, false);
		if (strcasecmp(login.c_str(), "NEW") == 0) {
			log->log(LOG_INFO, "New user signing up on node %d", node);
			cls();
			send_gfile("newuser");
			print_f("|14Create a new account? (Y/N): |07");
			char ch = tolower(getche());
			if (ch == 'y') {
				std::string newusername = "";
				while(true) {
					print_f("\r\n       Desired username: ");
					newusername = get_string(16, false);
					trim(newusername);
					if (User::username_allowed(config, newusername)) {
						break;
					}
					print_f("\r\n|12Sorry, username not allowed (Too short, inappropriate or already in use.)|07\r\n");
				}
				std::string password = "";
				while(true) {
					print_f("\r\n       Desired password: ");
					password = get_string(16, true);
					if (password.size() < 6) {
						print_f("\r\n|12Password too short..|07\r\n");
						continue;
					}
					print_f("\r\n        Repeat password: ");
					std::string password_r = get_string(16, true);

					if (password != password_r) {
						print_f("\r\n|12Passwords don't match..|07\r\n");
						continue;
					}
					break;
				}

				std::string firstname = "";
				std::string lastname = "";

				while (true) {
					print_f("\r\n        Your first name: ");
					firstname = get_string(26, false);
					if (firstname.find(' ') != std::string::npos) {
						print_f("\r\n|12First name can not contain a space!.\r\n|07");
						continue;
					}
					print_f("\r\n         Your last name: ");
					lastname = get_string(26, false);
					trim(lastname);
					if (firstname.size() < 2 || lastname.size() < 2) {
						print_f("\r\n|12First name and last name must both be at least 2 characters long.\r\n|07");
						continue;
					}

					if (!User::check_fullname(config, firstname + " " + lastname)) {
						print_f("\r\n|12Someone with that name is already registered, sorry.\r\n|07");
						continue;
					}
					break;
				}
				std::string location;
				while (true) {
					print_f("\r\n   Approximate location: ");
					location = get_string(26, false);
					trim(location);
					if (location.size() < 2) {
						print_f("\r\n|12Too short. Come on, don't be shy!\r\n|07");
						continue;
					}

					break;
				}
				std::string email;
				print_f("\r\n Contact E-Mail address: ");
				email = get_string(32, false);
				trim(email);
				print_f("\r\n|14Thankyou. Have you entered everything correctly? (Y/N): |07");
				if (tolower(getche() == 'y')) {
					print_f("\r\n|10Great! Saving your account, and logging you in!\r\n|07");
					if (u.inst_user(newusername, password, firstname, lastname, location, email)) {
						logged_in = true;
					}
					else {
						print_f("\r\n|12Sorry, an error occured!|07\r\n");
						return 0;
					}
				}
			}
		}
		else {
			print_f("\r\nPASSW: ");
			std::string password = get_string(16, true);

			if (u.load_user(login, password)) {
				logged_in = true;
			}
			else {
				log->log(LOG_INFO, "%s failed to login on node %d (wrong password)", login.c_str(), node);
				tries++;
			}
		}
		if (tries == 3) {
			return 0;
		}
	}

	log->log(LOG_INFO, "%s logged in on node %d", u.get_username().c_str(), node);

	clog = new CallLog(&config);
	clog->log_on(u.get_username(), node);


	struct sec_level_t *sl = config.get_sec_level_info(u.get_sec_level());

	time_t last_on = stoi(u.get_attribute("last_on", "0"));
	time_t now = time(NULL);
	struct tm last_on_tm;
	struct tm now_tm;
#ifdef _MSC_VER
	localtime_s(&last_on_tm, &last_on);
	localtime_s(&now_tm, &now);
#else
	localtime_r(&last_on, &last_on_tm);
	localtime_r(&now, &now_tm);
#endif

	if (last_on_tm.tm_year != now_tm.tm_year || last_on_tm.tm_yday != now_tm.tm_yday) {
		if (sl != NULL) {
			timeleft = sl->time_online;
		}
		else {
			timeleft = 20;
		}
		u.set_attribute("time_left", std::to_string(timeleft));
	}
	else {
		if (sl != NULL) {
			timeleft = stoi(u.get_attribute("time_left", std::to_string(sl->time_online)));
		}
		else {
			timeleft = stoi(u.get_attribute("time_left", "20"));
		}
	}

	timeleft *= 60;

	if (sl != NULL) {
		timeoutmax = sl->timeout;
	}
	else {
		timeoutmax = 10;
	}

	u.set_attribute("last_on", std::to_string(time(NULL)));

	cls();
	
	send_gfile("login");

	print_f("|14Press any key...|07");
	getch();

	bulletins = new Bulletins();
	if (bulletins->load(this)) {
		bulletins->display(this);
	}

	cls();
	int email_tot = Email::count_email(this);
	int email_unr = Email::unread_email(this);
	if (email_tot > 0) {
		if (email_unr > 0) {
			print_f("|14You have %d new, and %d old private email(s).\r\n", email_unr, email_tot);
			print_f("|14Read them now? (Y/N) : ");
			if (tolower(getche()) == 'y') {
				Email::list_email(this);
				cls();
			}
			else {
				print_f("\r\n\r\n\r\n");
			}
		}
		else {
			print_f("|14You have %d old private email(s).\r\n\r\n", email_tot);
		}
	}
	else {
		print_f("|14You have no private email.\r\n\r\n");
	}
	print_f("|14Scan for new messages? (Y/N) : |07");
	if (tolower(getche()) != 'n') {
		MsgConf::scan(this);
	}

	cls();
	CallLog::last10_callers(this);

	print_f("|14Press any key...|07");
	getch();


	Menu m(this);

	m.load(config.menu_path() + "/" + config.main_menu() + ".toml");
	m.run();

	cls();
	send_gfile("goodbye");
	log->log(LOG_INFO, "Node %d logged off (graceful)", node);
	clog->log_off();
	return 0;
}

void Node::disconnected() {
	if (clog != nullptr) {
		clog->log_off();
	}
	log->log(LOG_INFO, "Node %d logged off (disconnected)", node);
 	exit(-1);
}
