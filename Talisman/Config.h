#pragma once
#include <vector>
#include <string>

#include "MsgConf.h"

class Node;

class Config
{
public:
	Config();
	bool load(Node *n, std::string filename);
	std::string gfile_path() {
		return _gfilepath;
	}
	std::string data_path() {
		return _datapath;
	}
	std::string menu_path() {
		return _menupath;
	}
	std::string msg_path() {
		return _msgpath;
	}
	std::string tmp_path() {
		return _tmppath;
	}
	std::string sys_name() {
		return _sysname;
	}

	std::string op_name() {
		return _opname;
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
	std::string _msgpath;
	std::string _tmppath;
	std::string _sysname;
	std::string _opname;
};

