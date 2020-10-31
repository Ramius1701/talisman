#ifdef _MSC_VER
#include <Windows.h>

#define strcasecmp _stricmp
#else
#include <unistd.h>
#include <cstring>
#endif
#include <iostream>
#include "Config.h"
#include "Node.h"


int main(int argc, char** argv) {
	int node = 0;
	int socket = 0;
	bool telnet = false;
	int ret;

#ifdef _MSC_VER
	HANDLE hInput;
	DWORD prev_mode;

	hInput = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(hInput, &prev_mode);
	SetConsoleMode(hInput, (prev_mode & ~(ENABLE_QUICK_EDIT_MODE | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)) | ENABLE_EXTENDED_FLAGS);
#endif
	for (int i = 1; i < argc; i++) {
		if (strcasecmp(argv[i], "-N") == 0) {
			node = (int)strtoul(argv[i + 1], NULL, 10);
			i++;
			continue;
		}
		else if (strcasecmp(argv[i], "-S") == 0) {
			socket = (int)strtoul(argv[i + 1], NULL, 10);
			i++;
			continue;
		}
		else if (strcasecmp(argv[i], "-T") == 0) {
			telnet = true;
		}
	}

	std::cerr << "Socket : " << socket << " Telnet : " << telnet << "Node : " << node << std::endl;

	Node n(node, socket, telnet);
	ret = n.run();
#ifdef _MSC_VER
	SetConsoleMode(hInput, prev_mode);
	closesocket(socket);
#else
	close(socket);
#endif
	
	return ret;
}