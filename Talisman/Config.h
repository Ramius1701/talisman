#pragma once
#include <vector>
#include <string>

#include "MsgConf.h"

class Config
{
public:
	Config();
	bool load(std::string filename);
	std::string gfile_path() {
		return _gfilepath;
	}
	std::string data_path() {
		return _datapath;
	}
	std::string menu_path() {
		return _menupath;
	}

	std::string main_menu() {
		return _mainmenu;
	}
	std::vector<MsgConf> msgconfs;

private:
	std::string _mainmenu;
	std::string _menupath;
	std::string _gfilepath;
	std::string _datapath;
	
};

