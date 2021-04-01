#include <sstream>
#include "Protocol.h"
#include "Door.h"
#include "CallLog.h"
#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#define PATH_MAX MAX_PATH
#else
#include <limits.h>
#include <unistd.h>
#endif

Protocol::Protocol(std::string name, std::string dl_cmd, std::string ssh_dl_cmd, std::string ul_cmd, std::string ssh_ul_cmd, bool batch, bool prompt)
{
	this->name = name;
	download_cmd = dl_cmd;
	upload_cmd = ul_cmd;
	ssh_upload_cmd = ssh_ul_cmd;
	ssh_download_cmd = ssh_dl_cmd;
	this->batch = batch;
	this->prompt = prompt;
}

void Protocol::upload(Node* n, int socket, std::string uploadpath)
{
	std::vector<std::string> args;
	std::istringstream iss;
	std::stringstream ss;
	std::string cmd;
	bool gotcmd = false;
	char buffer[PATH_MAX];
	if (n->is_telnet()) {
		iss.str(upload_cmd);
	}
	else {
		iss.str(ssh_upload_cmd);
	}
	std::string fname = "";

	ss.str("");
	ss << uploadpath;

#ifdef _MSC_VER
	if (uploadpath[uploadpath.length() - 1] != '\\') {
		ss << "\\";
	}
#else
	if (uploadpath[uploadpath.length() - 1] != '/') {
		ss << "/";
	}
#endif

	getcwd(buffer, PATH_MAX);
	chdir(uploadpath.c_str());

	if (prompt) {
		n->print_f("Please enter the name of the file you are uploading: ");
		fname = n->get_string(32, false);
		if (fname == "") {
			return;
		}
	}

	for (std::string s; iss >> s; ) {
		if (!gotcmd) {
			cmd = s;
			gotcmd = true;
			continue;
		}
		if (s == "@SOCKET@") {
			args.push_back(std::to_string(socket));
		}
		else if (s == "@UPPATH@") {
			args.push_back(ss.str());
		}
		else if (s == "@FILENAME@" && fname != "") {
			args.push_back(fname);
		}
		else {
			args.push_back(s);
		}
	}
	int ret = Door::runExternal(n, cmd, args, true);

	chdir(buffer);
	if (!ret) {
		n->disconnected();
	}
}

void Protocol::download(Node* n, int socket, std::vector<std::filesystem::path> *files)
{
	std::vector<std::string> args;
	std::istringstream iss;
	bool gotcmd = false;
	std::string cmd;

	if (n->is_telnet()) {
		iss.str(download_cmd);
	}
	else {
		iss.str(ssh_download_cmd);
	}

	if (batch) {
		for (std::string s; iss >> s; ) {
			if (!gotcmd) {
				cmd = s;
				gotcmd = true;
				continue;
			}
			if (s == "@SOCKET@") {
				args.push_back(std::to_string(socket));
			}
			else if (s == "@FILELIST@") {
				for (size_t i = 0; i < files->size(); i++) {
					args.push_back(files->at(i).u8string());
					n->clog->down_bytes((uint32_t)std::filesystem::file_size(files->at(i)));
				}
			}
			else {
				args.push_back(s);
			}
		}
		if (!Door::runExternal(n, cmd, args, true)) {
			n->disconnected();
		}	
	}
	else {
		for (size_t i = 0; i < files->size(); i++) {
			args.clear();
			n->print_f("Sending %s with %s\r\n", files->at(i).filename().u8string().c_str(), name.c_str());
			gotcmd = false;
			for (std::string s; iss >> s; ) {
				if (!gotcmd) {
					cmd = s;
					gotcmd = true;
					continue;
				}
				if (s == "@SOCKET@") {
					args.push_back(std::to_string(socket));
				}
				else if (s == "@FILENAME@") {
					args.push_back(files->at(i).u8string());
					n->clog->down_bytes((uint32_t)std::filesystem::file_size(files->at(i)));
				}
				else {
					args.push_back(s);
				}
			}
			if (!Door::runExternal(n, cmd, args, true)) {
				n->disconnected();
			}
		}
	}
}
