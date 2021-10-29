#ifdef _MSC_VER
#include <Windows.h>
#endif
#include <sstream>
#include <algorithm>
#include "Archiver.h"

static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
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
			for (size_t i = 0; i < filelist.size(); i++) {
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
			for (size_t i = 0; i < filelist.size(); i++) {
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
    rtrim(cmd);

#ifdef _MSC_VER
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	char* cmd_cstr = strdup(cmd.c_str());
	if (!cmd_cstr) return;

	if (!CreateProcessA(nullptr, cmd_cstr, nullptr, nullptr, true, 0, nullptr, NULL, &si, &pi)) {
		return;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	free(cmd_cstr);
#else
	std::stringstream ss;

	ss << "/bin/sh -c \"" << cmd << "\"";

	system(ss.str().c_str());
#endif
}
