#include <iostream>
#include <cstring>
#include <filesystem>
#include "INIReader.h"
#include "User.h"
#include "Files.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

int main(int argc, char** argv) {
	INIReader inir("talisman.ini");

	if (inir.ParseError() != 0) {
		std::cerr << "Unable to parse talisman.ini!" << std::endl;
		return -1;
	}

	if (argc > 1) {
		if (strcasecmp(argv[1], "password") == 0) {
			if (argc == 4) {
				std::string user(argv[2]);
				std::string newpass(argv[3]);

				if (User::update_password(inir.Get("paths", "data path", "data"), user, newpass)) {
					std::cout << "Successfully updated password for " << user << "." << std::endl;
				}
				else {
					std::cerr << "Failed to update password for " << user << "." << std::endl;
				}
				return 0;
			}
		}
		else if (strcasecmp(argv[1], "seclevel") == 0) {
			if (argc == 4) {
				std::string user(argv[2]);
				int seclevel;
				try {
					seclevel = stoi(std::string(argv[3]));
				}
				catch (std::invalid_argument) {
					std::cerr << "Invalid argument for sec level." << std::endl;
					return -1;
				}
				catch (std::out_of_range) {
					std::cerr << "Out of range for sec level." << std::endl;
					return -1;
				}
				User::set_attribute(inir.Get("paths", "data path", "data"), user, "seclevel", std::to_string(seclevel));
				std::cout << "Done." << std::endl;
				return 0;
			}
		}
		else if (strcasecmp(argv[1], "uploadbulk") == 0) {
			std::string uploaded_by = "Unknown";
			if (argc == 5) {
				uploaded_by = std::string(argv[4]);
			}
			if (argc >= 4) {
				std::filesystem::path folder(argv[2]);
				std::string database = std::string(argv[3]);
				Files files;
				files.load_archivers(inir.Get("paths", "data path", "data"));

				for (auto& d : std::filesystem::directory_iterator(folder)) {
					if (!files.add_file(inir.Get("paths", "temp path", "data"), database, std::filesystem::absolute(d.path()).u8string(), uploaded_by)) {
						std::cout << "Failed to add: " << std::filesystem::absolute(d.path()).u8string() << std::endl;
					}
					else {
						std::cout << "Successfully added: " << std::filesystem::absolute(d.path()).u8string() << std::endl;
					}
				}
			}
		}
	}
	else {
		std::cerr << "Usage: " << argv[0] << " command [args]" << std::endl;
		std::cerr << "   COMMAND password ARGS username newpassword" << std::endl;
		std::cerr << "   COMMAND seclevel ARGS username newlevel" << std::endl;
		std::cerr << "   COMMAND uploadbulk ARGS folder database [uploadedby]" << std::endl;

	}
}