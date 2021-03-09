
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
#include "Script.h"
#include "Door.h"
#include "Editor.h"

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
	override_on = -1;
	override_width = 0;
	override_height = 0;
	sshc = nullptr;
	ssht = nullptr;
#ifdef _MSC_VER
	hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOutput, &dwMode);
	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(hOutput, dwMode);
#endif
	term_width = 80;
	term_height = 25;
}

Node::~Node() {
	if (clog != nullptr) {
		delete clog;
	}
	if (bulletins != nullptr) {
		delete bulletins;
	}
}

int Node::get_term_width() {
	if (u.get_uid() != 0) {
		if (override_on == -1) {
			override_on = u.get_attribute("screen_override", "false") == "true";
			override_width = stoi(u.get_attribute("screen_override_width", "80"));
			override_height = stoi(u.get_attribute("screen_override_height", "25"));
		}
		if (override_on) {
			return override_width;
		}
	}
	return term_width;
}

int Node::get_term_height() {
	if (u.get_uid() != 0) {
		if (override_on == -1) {
			override_on = u.get_attribute("screen_override", "false") == "true";
			override_width = stoi(u.get_attribute("screen_override_width", "80"));
			override_height = stoi(u.get_attribute("screen_override_height", "25"));
		}
		if (override_on) {
			return override_height;
		}
	}
	return term_height;
}

void Node::set_term_width(int w) {
	term_width = w;
}

void Node::set_term_height(int h) {
	term_height = h;
}

void Node::update_node_use(std::string usage) {
	std::filesystem::path nusep(config.tmp_path());
	nusep.append(std::to_string(node));
	std::filesystem::create_directories(nusep);
	nusep.append("node.use");

	

	FILE* fptr = fopen(nusep.u8string().c_str(), "w");

	if (fptr) {
		fprintf(fptr, "%s\n", u.get_username().c_str());
		fprintf(fptr, "%s\n", usage.c_str());
		fclose(fptr);
	}
}

void Node::pause() {
	if (hasANSI) {
		print_f("\x1b[s|14Press any key...|07");
		getch();
		print_f("\x1b[u\x1b[K");

	}
	else {
		print_f("|14Press any key...|07");
		getch();
		print_f("\r\n");
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

bool Node::compare_token(std::string field, std::string token) {
	if (field.substr(0, token.size()) == token) {
		for (size_t i = token.size(); i < field.substr(token.size()).size(); i++) {
			if (field[i] != '#') {
				return false;
			}
		}
		return true;
	}

	return false;
}

void Node::send_file(std::filesystem::path p, bool pause, bool script) {
	char lastc = 'x';
	bool gottag = false;
	std::stringstream ss;
	std::ifstream in(p);
	int lines = 1;
	bool stop = false;
	char c;
	if (in.is_open()) {
		while (in.get(c) && !stop) {
			if (c == 0x1a) break;
			if (c == '@' && gottag == false) {
				gottag = true;
				continue;
			}
			if (c == '@' && gottag == true) {
				//deal with tag
				if (compare_token(ss.str(), "MAILCONF")) {
					int mailconf = stoi(u.get_attribute("cur_msg_conf", "-1"));
					if (mailconf != -1) {
						print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, config.msgconfs.at(mailconf).get_name().c_str());
					}
					else {
						print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "None.");
					}
				}
				else if (compare_token(ss.str(), "MAILAREA")) {
					int mailconf = stoi(u.get_attribute("cur_msg_conf", "-1"));
					int mailarea = stoi(u.get_attribute("cur_msg_area", "-1"));
					if (mailconf != -1 && mailarea != -1) {
						print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, config.msgconfs.at(mailconf).areas.at(mailarea).get_name().c_str());
					}
					else {
						print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "None.");
					}
				}
				else if (compare_token(ss.str(), "FILEAREA")) {
					int fileconf = stoi(u.get_attribute("cur_file_conf", "-1"));
					int filearea = stoi(u.get_attribute("cur_file_area", "-1"));
					if (fileconf != -1 && filearea != -1) {
						print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, config.fileconfs.at(fileconf).areas.at(filearea).get_name().c_str());
					}
					else {
						print_f("%-.*s", ss.str().size() + 2, ss.str().size() + 2, "None.");
					}
				}
				else if (compare_token(ss.str(), "FILECONF")) {
					int fileconf = stoi(u.get_attribute("cur_file_conf", "-1"));
					if (fileconf != -1) {
						print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, config.fileconfs.at(fileconf).get_name().c_str());
					}
					else {
						print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "None.");
					}
				}
				else if (compare_token(ss.str(), "VERSION")) {
					std::stringstream ss2;
					ss2 << VERSION_MAJOR << "." << VERSION_MINOR << "-" << VERSION_STR;
					print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, ss2.str().c_str());
				}
				else if (compare_token(ss.str(), "TIMELEFT")) {
					std::stringstream ss2;
					ss2 << (timeleft / 60) << " mins";
					print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, ss2.str().c_str());
				}
				else if (ss.str().substr(0, 10) == "RUNSCRIPT:" && !script) {
					std::stringstream ss2;

					ss2 << config.script_path() << "/" << ss.str().substr(10) << ".lua";
					Script::exec(this, ss2.str());
				}
				else if (ss.str() == "NOPAUSE") {
					pause = false;
				}
				else if (compare_token(ss.str(), "SECLEVEL")) {
					struct sec_level_t *sl = config.get_sec_level_info(u.get_sec_level());
					print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, sl->name.c_str());
				}
				else if (compare_token(ss.str(), "ULOCATION")) {
					print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, u.get_attribute("location", "Somewhere, The World").c_str());
				}
				else if (compare_token(ss.str(), "UNAME")) {
					print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, u.get_username().c_str());
				}
				else if (compare_token(ss.str(), "UFULLNAME")) {
					print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, u.get_attribute("fullname", "Some User").c_str());
				}
				else if (ss.str() == "PAUSE") {
					if (hasANSI) {
						print_f("\x1b[s|14More (Y/N/C) ? |07");
					}
					else {
						print_f("|14More (Y/N/C) ? |07");
					}

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
					if (hasANSI) {
						print_f("\x1b[u\x1b[K");
					}
					else {
						print_f("\r\n");
					}
					lines = 0;
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
#ifdef _MSC_VER
				WriteConsoleA(hOutput, &c, 1, NULL, NULL);
#endif
				if (lines == get_term_height() - 2 && pause) {
					if (hasANSI) {
						print_f("\x1b[s|14More (Y/N/C) ? |07");
					}
					else {
						print_f("|14More (Y/N/C) ? |07");
					}

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
					if (hasANSI) {
						print_f("\x1b[u\x1b[K");
					}
					else {
						print_f("\r\n");
					}
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
	send_gfile(filename, pause, false);
}

void Node::send_gfile(std::string filename, bool pause, bool script) {
	std::vector<std::filesystem::path> gfiles;

	std::filesystem::path p(config.gfile_path());
	if (hasANSI) {
		p.append(filename + "." + std::to_string(get_term_width()) + "x" + std::to_string(get_term_height()) + ".ans");
		if (std::filesystem::exists(p)) {
			// got one ansi
			gfiles.push_back(p);
			p = config.gfile_path();
			p.append(filename + "." + std::to_string(get_term_width()) + "x" + std::to_string(get_term_height()) + ".ans.1");
			int count = 2;
			while (std::filesystem::exists(p)) {
				gfiles.push_back(p);
				p = config.gfile_path();
				p.append(filename + "." + std::to_string(get_term_width()) + "x" + std::to_string(get_term_height()) + ".ans." + std::to_string(count));
				count++;
			}

			send_file(gfiles.at(rand() % gfiles.size()), pause, script);
			print_f("\x1b[0m");
			return;
		}
		else {
			p.clear();
			p.assign(config.gfile_path());
			p.append(filename + ".ans");
			if (std::filesystem::exists(p)) {
				gfiles.push_back(p);
				p = config.gfile_path();
				p.append(filename + ".ans.1");
				int count = 2;
				while (std::filesystem::exists(p)) {
					gfiles.push_back(p);
					p = config.gfile_path();
					p.append(filename + ".ans." + std::to_string(count));
					count++;
				}

				send_file(gfiles.at(rand() % gfiles.size()), pause, script);
				print_f("\x1b[0m");
				return;
			}
		}
	}

	p.clear();
	p.assign(config.gfile_path());
	p.append(filename + "." + std::to_string(get_term_width()) + "x" + std::to_string(get_term_height()) + ".asc");
	if (std::filesystem::exists(p)) {
		gfiles.push_back(p);
		p = config.gfile_path();
		p.append(filename + "." + std::to_string(get_term_width()) + "x" + std::to_string(get_term_height()) + ".asc.1");
		int count = 2;
		while (std::filesystem::exists(p)) {
			gfiles.push_back(p);
			p = config.gfile_path();
			p.append(filename + "." + std::to_string(get_term_width()) + "x" + std::to_string(get_term_height()) + ".asc." + std::to_string(count));
			count++;
		}

		send_file(gfiles.at(rand() % gfiles.size()), pause, script);
	}
	else {
		p.clear();
		p.assign(config.gfile_path());

		p.append(filename + ".asc");
		if (std::filesystem::exists(p)) {
			gfiles.push_back(p);
			p = config.gfile_path();
			p.append(filename + ".asc.1");
			int count = 2;
			while (std::filesystem::exists(p)) {
				gfiles.push_back(p);
				p = config.gfile_path();
				p.append(filename + ".asc." + std::to_string(count));
				count++;
			}

			send_file(gfiles.at(rand() % gfiles.size()), pause, script);
		}
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
	unsigned char buffer[2048];
	int i = 0;
	struct timeval tv;

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
				else {
					if (stage == 0) {
						if ((unsigned char)ch == IAC && telnet) {
							stage = 1;
						}
						else if (ch != '\n' && ch != '\0') {
							if (!time_check()) {
								printf("|14You are out of time for today!\r\n");
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
						else if ((unsigned char)ch == 250) {
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
						if ((unsigned char)ch == 240) {
							if (buffer[0] == NAWS) {

								term_width = buffer[2];
								term_height = buffer[4];
							}
							stage = 0;
						}
						else {
							if (i < 2047) {
								buffer[i++] = (unsigned char)ch;
								buffer[i] = '\0';
							}
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
	return get_string(maxlen, masked, clear, "");
}

std::string Node::get_string(int maxlen, bool masked, bool clear, std::string def) {
	std::stringstream ss;

	ss << def;

	if (hasANSI && !clear) {
		print_f("%s%s\x1b[s", config.get_prompt_colour(), def.c_str());
		for (int i = def.size(); i < maxlen; i++) {
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
		else if (isprint((unsigned char)ch)) {
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
	WriteConsoleA(hOutput, str, strlen(str), NULL, NULL);
#endif
}

void Node::print_f_nc(const char* fmt, ...) {
	char buffer[2048];
	va_list args;
	va_start(args, fmt);

	vsnprintf(buffer, sizeof buffer, fmt, args);
	send_str(buffer);
	va_end(args);
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
			case 16:
				send_str("\x1b[40m");
				break;
			case 17:
				send_str("\x1b[44m");
				break;
			case 18:
				send_str("\x1b[42m");
				break;
			case 19:
				send_str("\x1b[46m");
				break;
			case 20:
				send_str("\x1b[41m");
				break;
			case 21:
				send_str("\x1b[45m");
				break;
			case 22:
				send_str("\x1b[43m");
				break;
			case 23:
				send_str("\x1b[47m");
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
			WriteConsoleA(hOutput, &buffer[i], 1, NULL, NULL);
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

void Node::tag_file(std::string filename, FileArea* fa)
{
	for (size_t i = 0; i < tagged_files.size(); i++) {
		if (tagged_files.at(i).filename == filename) {
			return;
		}
	}

	struct tagged_file_t t;
	t.filename = filename;
	t.fa = fa;
	tagged_files.push_back(t);
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

	pause();

	cls();
	send_gfile("system", true);
	pause();

	cls();
	send_gfile("login", true);
}

int Node::run() {
	return run(nullptr, nullptr);
}

bool Node::newuser() {
	log->log(LOG_INFO, "New user signing up on node %d", node);
	cls();
	send_gfile("newuser");
	print_f("|14Create a new account? (Y/N): |07");
	char ch = tolower(getche());
	if (ch == 'y') {
		std::string newusername = "";
		while (true) {
			print_f("\r\n       Desired username: ");
			newusername = get_string(16, false);
			trim(newusername);
			if (User::username_allowed(config, newusername) && User::check_fullname(config, newusername)) {
				break;
			}
			print_f("\r\n|12Sorry, username not allowed (Too short, inappropriate or already in use.)|07\r\n");
		}
		std::string password = "";
		while (true) {
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

			if (!User::check_fullname(config, firstname + " " + lastname) || !User::username_allowed(config, firstname + " " + lastname)) {
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
				u.set_attribute("seclevel", std::to_string(config.new_user_sec_level()));
				
				bool found = false;

				for (size_t msgconf = 0; msgconf < config.msgconfs.size(); msgconf++) {
					if (config.msgconfs.at(msgconf).get_sec_level() > u.get_sec_level()) continue;
					for (size_t msgarea = 0; msgarea < config.msgconfs.at(msgconf).areas.size(); msgarea++) {
						if (config.msgconfs.at(msgconf).areas.at(msgarea).get_r_sec_level() > u.get_sec_level()) continue;
						u.set_attribute("cur_msg_conf", std::to_string(msgconf));
						u.set_attribute("cur_msg_area", std::to_string(msgarea));
						found = true;
						break;
					}
					if (found) break;
				}

				found = false;

				for (size_t fileconf = 0; fileconf < config.fileconfs.size(); fileconf++) {
					if (config.fileconfs.at(fileconf).get_sec_level() > u.get_sec_level()) continue;
					for (size_t filearea = 0; filearea < config.fileconfs.at(fileconf).areas.size(); filearea++) {
						if (config.fileconfs.at(fileconf).areas.at(filearea).get_d_sec_level() > u.get_sec_level()) continue;
						u.set_attribute("cur_file_conf", std::to_string(fileconf));
						u.set_attribute("cur_file_area", std::to_string(filearea));
						found = true;
						break;
					}
					if (found) break;
				}

				if (config.new_user_feedback()) {
					cls();
					send_gfile("feedback");
					pause();

					std::string to = User::user_exists(&config, config.op_name());

					if (to.size() == 0) {
						print_f("|12No such user!|07\r\n");
					}
					else {
						print_f("\r\n|14Sending mail to |15%s\r\n", to.c_str());
						std::vector<std::string> newemail = Editor::enter_message(this, to, "New User Feedback", "E-Mail", true, nullptr);
						if (newemail.size() > 0) {
							Email::save_message(this, to, u.get_username(), "New User Feedback", newemail);
						}
					}
				}
				return true;
			}
			else {
				print_f("\r\n|12Sorry, an error occured!|07\r\n");
				return false;
			}
		}
	}
	return false;
}

int Node::run(std::string *sshusername, std::string *sshpassword) {

	unsigned char iac_echo[] = { IAC, IAC_WILL, IAC_ECHO, '\0' };
	unsigned char iac_sga[] = { IAC, IAC_WILL, IAC_SUPPRESS_GO_AHEAD, '\0' };
	unsigned char iac_naws[] = { IAC, IAC_DO, NAWS, '\0' };
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

	std::filesystem::path nmsgp(config.tmp_path());

	nmsgp.append(std::to_string(node));
	nmsgp.append("node.msg");

	std::filesystem::remove(nmsgp);

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

	if (telnet) {
		send(socket, (char*)iac_naws, 3, 0);
	}

	send_gfile("welcome");

	int tries = 0;

	bool login_pause = false;

	if (sshusername == nullptr || sshpassword == nullptr) {
		while (!logged_in) {
			print_f("\r\nEnter USERNAME or NEW\r\n");
			print_f("LOGIN: ");
			std::string login = get_string(16, false);
			if (strcasecmp(login.c_str(), "NEW") == 0) {
				logged_in = newuser();
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
	}
	else if (sshusername != nullptr && sshpassword != nullptr) {
		std::string login = *sshusername;
		std::string password = *sshpassword;
		if (strcasecmp(login.c_str(), "NEW") == 0) {
			print_f("\r\n|14Signing up as a new user...\r\n");
			pause();
			if (!newuser()) {
				return 0;
			}
		} else if (!u.load_user(login, password)) {
			return 0;
		}
		if (strcasecmp(login.c_str(), "NEW") != 0) {
			print_f("\r\n|14Welcome back |15%s!|07\r\n", login.c_str());
			login_pause = true;
		}
	}
	else {
		return 0;
	}
	log->log(LOG_INFO, "%s logged in on node %d", u.get_username().c_str(), node);
	srand(time(NULL));
	clog = new CallLog(&config);
	clog->log_on(u.get_username(), node);
	update_node_use("Logging in.");

	struct sec_level_t *sl = config.get_sec_level_info(u.get_sec_level());

	last_on = stol(u.get_attribute("last_on", "0"));
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

	if (login_pause) {
		pause();
	}

	bulletins = new Bulletins();
	bulletins->load(this);

	for (size_t i = 0; i < config.get_login_items()->size(); i++) {
		if (u.get_sec_level() >= config.get_login_items()->at(i).seclevel) {
			if (config.get_login_items()->at(i).clearscreen) {
				cls();
			}
			if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "QUICKLOGIN") == 0) {
				print_f("|14Quick Login? (Y/N) : |07");
				if (tolower(getche()) == 'y') {
					print_f("\r\n");
					break;
				}
				print_f("\r\n");
			} 
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "SENDGFILE") == 0) {
				send_gfile(config.get_login_items()->at(i).data, true);
			}
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "BULLETINS") == 0) {
				bulletins->display(this);
			}
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "EMAILCHECK") == 0) {
				int email_tot = Email::count_email(this);
				int email_unr = Email::unread_email(this);
				if (email_tot > 0) {
					if (email_unr > 0) {
						print_f("|14You have %d new, and %d old private email(s).\r\n", email_unr, email_tot);
						print_f("|14Read them now? (Y/N) : |07");
						if (tolower(getche()) == 'y') {
							Email::list_email(this);
							cls();
						}
						else {
							print_f("\r\n\r\n\r\n");
						}
					}
					else {
						print_f("|14You have %d old private email(s).|07\r\n\r\n", email_tot);
					}
				}
				else {
					print_f("|14You have no private email.|07\r\n\r\n");
				}
			}
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "MAILSCAN") == 0) {
				print_f("|14Scan for new messages? (Y/N) : |07");
				if (tolower(getche()) != 'n') {
					MsgConf::scan(this);
				}
			}
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "NEWFILES") == 0) {
				print_f("|14Scan for new files? (Y/N) : |07");
				if (tolower(getche()) != 'n') {
					print_f("\r\n\r\n");
					for (size_t i = 0; i < config.fileconfs.size(); i++) {
						if (config.fileconfs.at(i).get_sec_level() > u.get_sec_level()) continue;
						print_f("|14Scanning conference: |15%s|14...|07\r\n", config.fileconfs.at(i).get_name().c_str());
						for (size_t j = 0; j < config.fileconfs.at(i).areas.size(); j++) {
							if (config.fileconfs.at(i).areas.at(j).get_d_sec_level() > u.get_sec_level()) continue;
							print_f("|14... Scanning area: |15%s|14...|07\r\n", config.fileconfs.at(i).areas.at(j).get_name().c_str());
							config.fileconfs.at(i).areas.at(j).list_files(this, last_on);
						}
					}
				}
			}
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "LAST10") == 0) {
				CallLog::last10_callers(this);
			}
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "RUNSCRIPT") == 0) {
				std::stringstream ss;

				ss << config.script_path() << "/" << config.get_login_items()->at(i).data << ".lua";
				Script::exec(this, ss.str());
			} 
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "RUNDOOR") == 0) {
				std::vector<std::string> arguments;
				arguments.push_back(std::to_string(node));
#ifdef _MSC_VER
				arguments.push_back(std::to_string(socket));
#endif
				Door::createDropfiles(this);
				if (!Door::runExternal(this, config.get_login_items()->at(i).data, arguments, false)) {
#ifdef _MSC_VER
					closesocket(socket);
#else
					close(socket);
#endif
					disconnected();
				}
			}
			else if (strcasecmp(config.get_login_items()->at(i).command.c_str(), "MSGREADNEW") == 0) {
				bool subonly = false;
				char c;
				print_f("\r\n|14Read new messages? |15Y|08=|14Yes, All|08, |15S|08=|14Yes, Subbed only|08, |15N|08=|14No |08: |07");
				c = tolower(getch());
				if (c == 's') {
					subonly = true;
				} 
				if (c != 'n') {
					bool done = false;
					for (size_t msgconf = 0; msgconf < config.msgconfs.size(); msgconf++) {
						if (config.msgconfs.at(msgconf).get_sec_level() <= u.get_sec_level()) {
							print_f("\r\n|14Searching conference |15%s|14...\r\n", config.msgconfs.at(msgconf).get_name().c_str());
							for (size_t msgarea = 0; msgarea < config.msgconfs.at(msgconf).areas.size(); msgarea++) {
								if (config.msgconfs.at(msgconf).areas.at(msgarea).get_r_sec_level() <= u.get_sec_level()) {
									if (!subonly || u.is_subscribed(config.msgconfs.at(msgconf).areas.at(msgarea).get_file())) {
										int last_read = u.user_get_lastread(config.msgconfs.at(msgconf).areas.at(msgarea).get_file());
										if (last_read < config.msgconfs.at(msgconf).areas.at(msgarea).get_total_msgs()) {
											done = !config.msgconfs.at(msgconf).areas.at(msgarea).read_message(last_read + 1, false, true, false, NULL);
										}
										if (done) {
											break;
										}
									}
								}
							}
							if (done) {
								break;
							}
						}
					}
				}
			}
			if (config.get_login_items()->at(i).pauseafter) {
				print_f("\r\n");
				pause();
			}
		}
	}

	Menu m(this);

	m.load(config.menu_path() + "/" + config.main_menu() + ".toml");
	m.run();

	cls();
	send_gfile("goodbye");
	log->log(LOG_INFO, "Node %d logged off (graceful)", node);

	disconnected();

	return 0;
}

void Node::disconnected() {
	if (clog != nullptr) {
		clog->log_off();
	}

	if (sshc != nullptr) {
#ifdef _MSC_VER
		closesocket(socket);
#else
		close(socket);
#endif
		ssht->join();
	}
	else {
#ifdef _MSC_VER
		closesocket(socket);
#else
		close(socket);
#endif
	}
	std::filesystem::path nusep(config.tmp_path());

	nusep.append(std::to_string(node));
	nusep.append("node.use");

	std::filesystem::remove(nusep);

	log->log(LOG_INFO, "Node %d logged off (disconnected)", node);
 	exit(-1);
}

void Node::display_nodes() {
	cls();
	char buffer2[256];

	for (int i = 1; i <= config.max_nodes(); i++) {
		std::filesystem::path nusep(config.tmp_path());

		nusep.append(std::to_string(i));
		nusep.append("node.use");

		FILE* fptr = fopen(nusep.u8string().c_str(), "r");

		if (fptr) {
			fgets(buffer2, 256, fptr);
			std::string uname(buffer2);
			fgets(buffer2, 256, fptr);
			std::string action(buffer2);
			fclose(fptr);

			rtrim(uname);
			rtrim(action);

			print_f("|14Node %d|08: |15%s|08, |07%s\r\n", i, uname.c_str(), action.c_str());
		}
		else {
			print_f("|14Node %d|08: Waiting for call.\r\n", i);
		}
	}
}
