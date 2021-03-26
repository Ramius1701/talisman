#pragma once

#include <string>
#include <filesystem>

class Request
{
public:
	void dorequest(int socket, std::string request);
private:
	std::string hostname;
	int port;
	void dodirlist(int socket, std::filesystem::path base, std::filesystem::path dir);
	bool compare_ext(std::filesystem::path path, std::string ext);
};

