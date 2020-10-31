#include "INIReader.h"
#include "Config.h"

Config::Config() {

}

bool Config::load(std::string filename) {
	INIReader inir(filename);

	if (inir.ParseError() != 0) {
		return false;
	}

	_gfilepath = inir.Get("Paths", "GFile Path", "gfiles");
	_datapath = inir.Get("Paths", "Data Path", "data");
	_menupath = inir.Get("Paths", "Menu Path", "menus");
	_mainmenu = inir.Get("Main", "Root Menu", "main");
	return true;
}