#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#else
#include <unistd.h>
#endif
#include <fstream>
#include "INIReader.h"
#include "Config.h"
#include "TicProc.h"
#include "Logger.h"

bool TicProc::run() {
	INIReader inir("talisman.ini");
	Config c;
	unsigned long pid;

	if (inir.ParseError()) {
		return false;
	}

	_datapath = inir.Get("Paths", "Data Path", "data");
	_logpath = inir.Get("Paths", "Log Path", "logs");
	_tmppath = inir.Get("Paths", "Temp Path", "temp");

	Logger log;

	log.load(_logpath + "/postie.log");

	if (!c.load(_datapath)) {
		return false;
	}

	if (!c.load_archivers(_datapath)) {
		return false;
	}

#ifdef _MSC_VER
	pid = GetCurrentProcessId();
#else
	pid = getpid();
#endif

	// foreach tic file
	for (auto& p : std::filesystem::directory_iterator(c.inbound())) {
		std::filesystem::path filepth = p.path();

		if (strcasecmp(filepth.extension().u8string().c_str(), ".tic") == 0) {
			//    process tic file
			struct ticfile_t tic;

			std::ifstream ifs(filepth);
			std::string line;
			while (std::getline(ifs, line))
			{
				if (line.find("area ") == 0) {
					tic.area = line.substr(5);
				}
				if (line.find("file") == 0) {
					tic.file = line.substr(5);
				}
				if (line.find("lfile") == 0) {
					tic.lname = line.substr(6);
				}
				if (line.find("fullname") == 0) {
					tic.lname = line.substr(9);
				}
				if (line.find("desc") == 0) {
					tic.shortdesc = line.substr(6);
				}
				if (line.find("ldesc") == 0) {
					tic.desc.push_back(line.substr(7));
				}
				if (line.find("replaces") == 0) {
					tic.replaces = line.substr(9);
				}
				if (line.find("crc") == 0) {
					tic.crc = strtol(line.substr(4).c_str(), NULL, 16);
				}
				if (line.find("pw") == 0) {
					tic.password = line.substr(3);
				}
			}
			//    add file to area
			//    forward any files to downlinks
		}
}