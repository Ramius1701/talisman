#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinSock2.h>
#define strcasecmp stricmp
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <iostream>
#include <filesystem>
#include <optional>
#include <fstream>
#include "../Common/INIReader.h"
#include "Request.h"
#include "../Common/Logger.h"

std::optional<std::filesystem::path> MakeAbsolute(const std::filesystem::path& root, const std::filesystem::path& userPath)
{
	auto finalPath = (root / userPath).lexically_normal();

	auto [rootEnd, nothing] = std::mismatch(root.begin(), root.end(), finalPath.begin());

	if (rootEnd != root.end())
		return std::nullopt;

	return finalPath;
}

bool Request::compare_ext(std::filesystem::path path, std::string ext) {
	if (strcasecmp(path.extension().u8string().c_str(), ext.c_str()) == 0) {
		return true;
	}
	return false;
}

void Request::dodirlist(int socket, std::filesystem::path base, std::filesystem::path dir) {
	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
		std::stringstream ss;
		const auto filenameStr = entry.path().filename().u8string();

		if (filenameStr == "gophermap") continue;

		try {
			if (entry.is_directory()) {
				ss << "1" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
			}
			else if (entry.is_regular_file()) {
				if (compare_ext(entry.path(), ".txt") || compare_ext(entry.path(), ".md") || compare_ext(entry.path(), ".markdown")) {
					ss << "0" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".gif")) {
					ss << "g" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".htm") || compare_ext(entry.path(), ".html")) {
					ss << "h" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".jpg") || compare_ext(entry.path(), ".jpeg") || compare_ext(entry.path(), ".png") ||
					compare_ext(entry.path(), ".bmp") || compare_ext(entry.path(), ".pcx") || compare_ext(entry.path(), ".ico") ||
					compare_ext(entry.path(), ".tif") || compare_ext(entry.path(), ".tiff") || compare_ext(entry.path(), ".svg") ||
					compare_ext(entry.path(), ".eps")) {
					ss << "I" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".mp3") || compare_ext(entry.path(), ".mp2") || compare_ext(entry.path(), ".wav")
					|| compare_ext(entry.path(), ".mid") || compare_ext(entry.path(), ".wma") || compare_ext(entry.path(), ".flac")
					|| compare_ext(entry.path(), ".mpc") || compare_ext(entry.path(), ".aiff") || compare_ext(entry.path(), ".aac")) {
					ss << "s" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else if (compare_ext(entry.path(), ".pdf")) {
					ss << "P" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
				else {
					ss << "9" << filenameStr << "\t" << std::filesystem::relative(dir, base).generic_u8string() << "/" << filenameStr << "\t" << hostname << "\t" << port << "\r\n";
				}
			}
		}
		catch (std::exception) {
		}
		send(socket, ss.str().c_str(), ss.str().size(), 0);
	}
}

void Request::dorequest(int socket, std::string request) {
	INIReader inir("talisman.ini");

	Logger log;

	if (inir.ParseError() != 0) {
		std::cerr << "Unable to parse talisman.ini!" << std::endl;
		return;
	}

	hostname = inir.Get("main", "hostname", "localhost");
	port = inir.GetInteger("main", "gopher port", 7070);

	log.load(inir.Get("paths", "log path", "logs") + "/gofer.log");

	log.log(LOG_INFO, "Request: %s", request.c_str());

	if (request.size() > 0 && request.substr(0, 1) == "/") {
		request = request.substr(1);
	}

	std::filesystem::path fpath = std::filesystem::absolute(inir.Get("paths", "gopher root", "gopher"));
	try {
		std::filesystem::path respath = MakeAbsolute(fpath, std::filesystem::path(request)).value();
		if (std::filesystem::exists(respath)) {
			if (std::filesystem::is_directory(respath)) {
				std::filesystem::path dir(respath);
				respath.append("gophermap");
				if (!std::filesystem::exists(respath)) {
					dodirlist(socket, fpath, dir);
					return;
				}
			}

			std::ifstream file(respath);
			if (file.is_open()) {
				while (!file.eof()) {
					std::string line;
					std::getline(file, line);

					std::stringstream ss;
					if (respath.filename().u8string() == "gophermap") {
						if (line.size() > 0 && line.at(0) == '#') continue;
						
						if (line == "%FILES%") {
							dodirlist(socket, fpath, respath.parent_path());
							continue;
						}


						std::istringstream iss(line);
						std::string token;
						std::vector<std::string> tokens;
						while (std::getline(iss, token, '\t'))
							tokens.push_back(token);

						if (tokens.size() == 1) {
							ss << tokens.at(0) << "\t" << "fake" << "\t" << hostname << "\t" << port << "\r\n";
						}
						else if (tokens.size() > 1) {
							std::string respath2;
							if (tokens.at(1).size() > 0 && tokens.at(1).at(0) != '/') {
								respath2 = request + "/" + tokens.at(1);
							}
							else {
								respath2 = tokens.at(1);
							}

							if (tokens.size() == 2) {
								ss << tokens.at(0) << "\t" << respath2 << "\t" << hostname << "\t" << port << "\r\n";
							}
							else if (tokens.size() == 3) {
								ss << tokens.at(0) << "\t" << respath2 << "\t" << tokens.at(2) << "\t" << port << "\r\n";
							}
							else {
								ss << tokens.at(0) << "\t" << respath2 << "\t" << tokens.at(2) << "\t" << tokens.at(3) << "\r\n";
							}
						}
					}
					else {
						ss << line << "\r\n";
					}
					send(socket, ss.str().c_str(), ss.str().size(), 0);
				}
				file.close();
				return;
			}
		}
	} catch (const std::bad_optional_access& e) {
		log.log(LOG_ERROR, "Bad Request!");
	}

	std::stringstream errormsg;

	errormsg << "3'" << request << "' Does not exist!\terror.host\t1" << std::endl;
	send(socket, errormsg.str().c_str(), errormsg.str().size(), 0);
}