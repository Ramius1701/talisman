#include "Config.h"
#include "INIReader.h"

bool Config::parse() {
	INIReader inir("talisman.ini");

	if (inir.ParseError()) {
		return false;
	}

	_datapath = inir.Get("Paths", "Data Path", "data");
	_msgpath = inir.Get("Paths", "Message Path", "msgs");
	_logpath = inir.Get("Paths", "Log Path", "logs");
	_opname = inir.Get("Main", "Sysop Name", "Sysop");
	_sysname = inir.Get("Main", "System Name", "Talisman");
	_location = inir.Get("Main", "Location", "Somewhere, The World");
	_wwwpath = inir.Get("Main", "WWW Path", "www");

	return true;
}