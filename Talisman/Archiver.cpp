#ifdef _MSC_VER
#include <Windows.h>
#include <direct.h>
#define strcasecmp _stricmp
#endif
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include "Archiver.h"
#include "Door.h"
#include "Node.h"
#include "Config.h"


void Archiver::extract(Node* n, std::string archive, std::string outdir) {
	std::filesystem::path p(archive);

	for (size_t i = 0; i < n->get_config()->archivers.size(); i++) {
		if (strcasecmp(p.extension().u8string().c_str(), n->get_config()->archivers.at(i)->extension.c_str()) == 0) {
			n->get_config()->archivers.at(i)->extract(archive, outdir);
			return;
		}
	}
}

void Archiver::extract(Node* n, std::string archive, std::vector<std::string> filelist, std::string outdir) {
	std::filesystem::path p(archive);

	for (size_t i = 0; i < n->get_config()->archivers.size(); i++) {
		if (strcasecmp(p.extension().u8string().c_str(), n->get_config()->archivers.at(i)->extension.c_str()) == 0) {
			n->get_config()->archivers.at(i)->extract(archive, filelist, outdir);
			return;
		}
	}
}

void Archiver::extract(std::string archive, std::string outdir)
{
	std::stringstream ss;
	std::istringstream iss(unarc);

	for (std::string s; iss >> s; ) {
		if (s == "@FILELIST@") {
			// all files
		}
		else if (s == "@OUTDIR@") {
			ss << outdir << " ";
		}
		else if (s == "@ARCHIVE@") {
			ss << archive << " ";
		}
		else {
			ss << s << " ";
		}
	}

	runexec(ss.str());

}

void Archiver::extract(std::string archive, std::vector<std::string> filelist, std::string outdir)
{
	std::stringstream ss;
	std::istringstream iss(unarc);

	for (std::string s; iss >> s; ) {
		if (s == "@FILELIST@") {
			for (int i = 0; i < filelist.size(); i++) {
				ss << filelist.at(i) << " ";
			}
		}
		else if (s == "@OUTDIR@") {
			ss << outdir << " ";
		}
		else if (s == "@ARCHIVE@") {
			ss << archive << " ";
		}
		else {
			ss << s << " ";
		}
	}

	runexec(ss.str());
}
void Archiver::compress(std::string archive, std::vector<std::string> filelist)
{
	std::stringstream ss;
	std::istringstream iss(arc);

	for (std::string s; iss >> s; ) {
		if (s == "@FILELIST@") {
			for (int i = 0; i < filelist.size(); i++) {
				ss << filelist.at(i) << " ";
			}
		}
		else if (s == "@ARCHIVE@") {
			ss << archive << " ";
		}
		else {
			ss << s << " ";
		}
	}

	runexec(ss.str());
}

void Archiver::runexec(std::string cmd) {
#ifdef _MSC_VER
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	char* cmd_cstr = strdup(cmd.c_str());
	if (cmd_cstr) return;

	if (!CreateProcessA(nullptr, cmd_cstr, nullptr, nullptr, true, 0, nullptr, NULL, &si, &pi)) {
		return;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	free(cmd_cstr);
#else
	std::stringstream ss;

	ss << "/bin/sh -c " << cmd;

	system(ss.str().c_str());
#endif
}