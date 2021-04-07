#include <iostream>
#include <cstring>

#include "Scanner.h"
#include "Tosser.h"
#include "Config.h"
#include "SubReq.h"

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
	else if (strcasecmp(argv[1], "add") == 0) {
		if (argc < 5) {
			std::cout << "Usage: " << argv[0] << " add NETNAME HOSTID SUBTYPE" << std::endl;
			return -1;
		}
		SubReq r;
		r.add(argv[2], atoi(argv[3]), argv[4]);
	}
	else if (strcasecmp(argv[1], "drop") == 0) {
		if (argc < 5) {
			std::cout << "Usage: " << argv[0] << " drop NETNAME HOSTID SUBTYPE" << std::endl;
			return -1;
		}
		SubReq r;
		r.drop(argv[2], atoi(argv[3]), argv[4]);
	}
	else {
		std::cout << "Usage: " << argv[0] << " [scan|toss|add|drop]" << std::endl;
		return -1;
	}
	return 0;
}