#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

#include "Server.h"

#include <iostream>

int main(int argc, char **argv)
{
	if (argc < 3) {
		std::cerr << "Usage: binki -P addr" << std::endl;
		return -1;
	}

	if (strcasecmp(argv[1], "-S") == 0) {
		Server s;
		return s.run(strtol(argv[2], NULL, 10));
	}
	else if (strcasecmp(argv[1], "-P") == 0) {
		// poll
	}
	else {
		std::cerr << "Usage: binki -P addr" << std::endl;
		return -1;
	}
}

