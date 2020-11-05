#include <sstream>
#include "Protocol.h"
#include "Door.h"
#ifdef _MSC_VER
#include <windows.h>
#include <direct.h>
#define PATH_MAX MAX_PATH
#else
#include <limits.h>
#include <unistd.h>
#endif

Protocol::Protocol(std::string name, std::string dl_cmd, std::string ul_cmd, bool batch, bool prompt)
{
	this->name = name;
	download_cmd = dl_cmd;
	upload_cmd = ul_cmd;
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
	iss.str(upload_cmd);


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
		std::string fname = n->get_string(32, false);

		ss << fname;
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
		else {
			args.push_back(s);
		}
	}
	Door::runExternal(n, cmd, args, true);

	chdir(buffer);
}

void Protocol::download(Node* n, int socket, std::vector<std::filesystem::path> *files)
{
	std::vector<std::string> args;
	std::istringstream iss;
	bool gotcmd = false;
	std::string cmd;

	iss.str(download_cmd);

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
				for (int i = 0; i < files->size(); i++) {
					args.push_back(files->at(i).u8string());
				}
			}
			else {
				args.push_back(s);
			}
		}
		Door::runExternal(n, cmd, args, true);
	}
	else {
		for (int i = 0; i < files->size(); i++) {
			args.clear();
			n->print_f("Sending %s with %s\r\n", files->at(i).c_str(), name.c_str());
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
				}
				else {
					args.push_back(s);
				}
			}
			Door::runExternal(n, cmd, args, true);
		}
	}
}
