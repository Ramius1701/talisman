#pragma once
#include <string>
#include <vector>
class Archiver
{
public:
	Archiver(std::string name, std::string extension, std::string unarc, std::string arc) {
		this->name = name;
		this->extension = extension;
		this->unarc = unarc;
		this->arc = arc;
	}

	std::string name;
	std::string extension;
	std::string unarc;
	std::string arc;

	void extract(std::string archive, std::string outdir);
	void extract(std::string archive, std::vector<std::string> filelist, std::string outdir);
	void compress(std::string archive, std::vector<std::string> filelist);
private:
	static void runexec(std::string cmd);

};

