#include <iostream>
#include <cstring>
#include <csignal>
#include "Scanner.h"
#include "Tosser.h"
#include "TicProc.h"
#include "../Common/INIReader.h"

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

void sig_handler(int signal) {
    INIReader inir("talisman.ini");

    if (inir.ParseError()) {
		exit(3);
	}

	std::string _datapath = inir.Get("Paths", "Data Path", "data");

    std::filesystem::path pidfile = (_datapath + "/postie.pid");

    if (std::filesystem::exists(pidfile)) {
        std::filesystem::remove(pidfile);
    }
    exit(3);
}


int main(int argc, char** argv) {


	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " [scan|toss|ticproc|tichatch]" << std::endl;
		return -1;
	}

	if (strcasecmp(argv[1], "scan") == 0) {
		Scanner s;
		s.run();
	}
	else if (strcasecmp(argv[1], "toss") == 0) {
		Tosser t;
		t.run(true);
		t.run(false);
	}
	else if (strcasecmp(argv[1], "ticproc") == 0) {
		TicProc tp;
		tp.run();
	} else if (strcasecmp(argv[1], "tichatch") == 0) {
        TicProc tp;
        if (argc < 6) {
            std::cout << "Usage: " << argv[0] << " tichatch \"file\" \"areatag\" \"replaces\" \"desc\"" << std::endl;
            return -1;
        }
        tp.hatch(argv[2], argv[3], argv[4], argv[5]);
    }
	else {
		std::cout << "Usage: " << argv[0] << " [scan|toss|ticproc|tichatch]" << std::endl;
		return -1;
	}
	return 0;
}
