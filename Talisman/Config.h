#pragma once
#include <vector>
#include <string>

#include "MsgConf.h"

class Node;

struct sec_level_t {
	std::string name;
	int level;
	int time_online;
	int timeout;
};

class Config
{
public:
	Config();
	bool load(Node *n, std::string filename);

	struct sec_level_t* get_sec_level_info(int seclvl);

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

	std::string netmail_sem() {
		return _netmailsem;
	}

	std::string echomail_sem() {
		return _echomailsem;
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
	std::string _netmailsem;
	std::string _echomailsem;
	std::vector<struct sec_level_t> seclevels;
};

