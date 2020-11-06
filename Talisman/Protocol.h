#pragma once
#include <string>
#include "Node.h"

class Protocol
{
public:
	Protocol(std::string name, std::string dl_cmd, std::string ul_cmd, bool batch, bool prompt);
	void upload(Node* n, int socket, std::string uploadpath);
	void download(Node* n, int socket, std::vector<std::filesystem::path> *files);
	std::string get_name() {
		return name;
	}
private:
	std::string name;
	std::string download_cmd;
	std::string upload_cmd;

	bool batch;
	bool prompt;
};

