#pragma once
#include <vector>
#include <string>

#include "MsgConf.h"
#include "FileConf.h"

class Node;
class Protocol;
class Archiver;

struct sec_level_t {
	std::string name;
	int level;
	int time_online;
	int timeout;
};

struct login_item_t {
	std::string command;
	std::string data;
	bool clearscreen;
	bool pauseafter;
	int seclevel;
};

class Config
{
public:
	Config();
	bool load(Node *n, std::string filename);

	struct sec_level_t* get_sec_level_info(int seclvl);

	int max_nodes() {
		return _max_nodes;
	}

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
	std::string script_path() {
		return _scriptpath;
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

	std::string external_editor() {
		return _externaleditor;
	}

	std::string get_logpath() {
		return _logpath;
	}

	std::string qwk_id() {
		return _qwk_id;
	}

	std::string get_location() {
		return _location;
	}

	std::string get_hostname() {
		return _hostname;
	}

	int get_gopher_port() {
		return _gopherport;
	}

	std::vector<struct login_item_t>* get_login_items() {
		return &loginitems;
	}

	int new_user_sec_level() {
		return _new_user_sec_level;
	}

	bool new_user_feedback() {
		return _new_user_feedback;
	}

	const char* get_prompt_colour();

	std::vector<MsgConf> msgconfs;
	std::vector<FileConf> fileconfs;
	std::vector<Archiver*> archivers;
	Protocol* select_protocol(Node* n);
	int select_archiver(Node* n);
	

private:
	int _max_nodes;
	std::string _bg_colour;
	std::string _fg_colour;
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
	std::string _externaleditor;
	std::string _logpath;
	std::string _scriptpath;
	std::string _location;
	std::string _qwk_id;
	std::string _hostname;
	bool _new_user_feedback;
	std::vector<struct sec_level_t> seclevels;
	std::vector<Protocol*> protocols;
	std::vector<struct login_item_t> loginitems;
	std::string prompt_background_ansi;
	int _new_user_sec_level;
	int _gopherport;
};

