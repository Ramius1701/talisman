#include <iostream>
#include <cstring>

#include "Scanner.h"
#include "Tosser.h"
#include "Config.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " [scan|toss]" << std::endl;
		return -1;
	}

	if (strcasecmp(argv[1], "scan") == 0) {
		Scanner s;
		s.run();
	}
	else if (strcasecmp(argv[1], "toss") == 0) {
		Tosser t;
		t.run();
	}
	else {
		std::cout << "Usage: " << argv[0] << " [scan|toss]" << std::endl;
		return -1;
	}
	return 0;
}