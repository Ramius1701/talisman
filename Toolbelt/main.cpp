#include <iostream>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "../Common/INIReader.h"
#include "User.h"
#include "Files.h"
#include "Nodelist.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}

// trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

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
				if (!User::set_attribute(inir.Get("paths", "data path", "data"), user, "seclevel", std::to_string(seclevel))) {
					std::cout << "Failed.";
				}
				else {
					std::cout << "Done." << std::endl;
				}
				return 0;
			}
		}
		else if (strcasecmp(argv[1], "uploadindex") == 0) {
			std::string uploaded_by = "Unknown";
			if (argc == 6) {
				uploaded_by = std::string(argv[5]);
			}

			if (argc >= 5) {
				std::ifstream infile;
				std::filesystem::path folder(argv[3]);
				std::string database = std::string(argv[4]);
				std::vector<std::string> descr;

				infile.open(argv[2]);
				std::string line;
				std::string file = "";
				Files files;

				while (std::getline(infile, line))
				{

					if (line[0] == ' ' || line[0] == '\t') {
						// description extended from last filename
						std::string d = line;
						trim(d);
						descr.push_back(d);
					}
					else {
						if (file != "") {
							// add file
							std::filesystem::path fpath = folder;
							fpath.append(file);
							if (std::filesystem::exists(fpath)) {
								files.insert_file(database, std::filesystem::absolute(fpath).u8string(), descr, uploaded_by);
								std::cout << "Added: " << file << std::endl;
							}
							else {
								std::cout << "Skipped: " << file << " (Missing) " << std::endl;
							}
						}
						file = line.substr(0, line.find(' '));
						std::string d = line.substr(line.find(' '));
						trim(d);
						descr.clear();
						descr.push_back(d);
					}
				}
				if (file != "") {
					// add file
					std::filesystem::path fpath = folder;
					fpath.append(file);
					if (std::filesystem::exists(fpath)) {
						files.insert_file(database, std::filesystem::absolute(fpath).u8string(), descr, uploaded_by);
						std::cout << "Added: " << file << std::endl;
					}
					else {
						std::cout << "Skipped: " << file << " (Missing) " << std::endl;
					}
				}
				infile.close();
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
				if (d.path().filename().u8string() != "files.bbs") {
					if (!files.add_file(inir.Get("paths", "temp path", "data"), database, std::filesystem::absolute(d.path()).u8string(), uploaded_by)) {
						std::cout << "Failed to add: " << std::filesystem::absolute(d.path()).u8string() << std::endl;
					}
					else {
						std::cout << "Successfully added: " << std::filesystem::absolute(d.path()).u8string() << std::endl;
					}
				}
				else {
					std::cout << "Skipping files.bbs" << std::endl;
				}
			}
		}
		}
		else if (strcasecmp(argv[1], "filetrim") == 0) {
		if (argc == 3) {
			std::string database = std::string(argv[2]);
			Files files;

			int trimmed = files.trim(database);

			std::cout << "Trimmed " << std::to_string(trimmed) << " missing files from database." << std::endl;
		}
		}
		else if (strcasecmp(argv[1], "nodelistp") == 0) {
		if (argc == 5) {
			std::string nodelist = std::string(argv[3]);
			std::string database = std::string(argv[4]);
			std::string domain = std::string(argv[2]);
			if (!Nodelist::parse(domain, nodelist, database)) {
				std::cout << "Failure" << std::endl;
			}
			else {
				std::cout << "Done" << std::endl;
			}
		}
		}
		else if (strcasecmp(argv[1], "allfiles") == 0) {
		if (argc == 4) {
			int seclevel;
			try {
				seclevel = stoi(std::string(argv[2]));
			}
			catch (std::invalid_argument) {
				std::cerr << "Invalid argument for sec level." << std::endl;
				return -1;
			}
			catch (std::out_of_range) {
				std::cerr << "Out of range for sec level." << std::endl;
				return -1;
			}

			Files files;

			int tot = files.all_files(inir.Get("paths", "data path", "data"), seclevel, 0, std::string(argv[3]));

			std::cout << "All files list " << argv[3] << " created listing " << tot << " total files." << std::endl;
		}
		}
		else if (strcasecmp(argv[1], "movefile") == 0) {
			if (argc == 6) {
				std::string srcfile(argv[2]);
				std::string destfile(argv[3]);
				std::string srcdb(argv[4]);
				std::string destdb(argv[5]);

				Files files;

				if (!files.move_file(inir.Get("paths", "data path", "data"), srcfile, destfile, srcdb, destdb)) {
					std::cout << "Failed to move file!" << std::endl;
				}
				else {
					std::cout << "Successfully moved file!" << std::endl;
				}
			}
		}
		else if (strcasecmp(argv[1], "newfiles") == 0) {
			if (argc == 5) {
				int seclevel;
				try {
					seclevel = stoi(std::string(argv[2]));
				}
				catch (std::invalid_argument) {
					std::cerr << "Invalid argument for sec level." << std::endl;
					return -1;
				}
				catch (std::out_of_range) {
					std::cerr << "Out of range for sec level." << std::endl;
					return -1;
				}

				if (strlen(argv[3]) != 10 || argv[3][4] != '.' || argv[3][7] != '.') {
					std::cerr << "Invalid date!" << std::endl;
					return -1;
				}

				Files files;

				struct tm dtm;

				dtm.tm_year = ((argv[3][0] - '0') * 1000 + (argv[3][1] - '0') * 100 + (argv[3][2] - '0') * 10 + (argv[3][3] - '0') - 1900);
				dtm.tm_mon = (argv[3][5] - '0') * 10 + (argv[3][6] - '0') - 1;
				dtm.tm_mday = (argv[3][8] - '0') * 10 + (argv[3][9] - '0');
				dtm.tm_hour = 0;
				dtm.tm_min = 0;
				dtm.tm_sec = 0;

				time_t then = mktime(&dtm);

				int tot = files.all_files(inir.Get("paths", "data path", "data"), seclevel, then, std::string(argv[4]));

				std::cout << "New files list " << argv[4] << " created listing " << tot << " total files." << std::endl;
			}
		} else if (strcasecmp(argv[1], "clearlrall") == 0) {
            if (!User::clear_lastread(inir.Get("paths", "data path", "data"), "", "")) {
                std::cout << "Error clearing last read." << std::endl;
            } else {
                std::cout << "Success clearing last read." << std::endl;
            }
        } else if (strcasecmp(argv[1], "clearlruser") == 0) {
            if (argc == 3) {
                if (!User::clear_lastread(inir.Get("paths", "data path", "data"), "", std::string(argv[2]))) {
                    std::cout << "Error clearing last read." << std::endl;
                } else {
                    std::cout << "Success clearing last read." << std::endl;
                }
            }
        } else if (strcasecmp(argv[1], "clearlrbase") == 0) {
            if (argc == 3) {
                if (!User::clear_lastread(inir.Get("paths", "data path", "data"), std::string(argv[2]), "")) {
                    std::cout << "Error clearing last read." << std::endl;
                } else {
                    std::cout << "Success clearing last read." << std::endl;
                }
            }
        } else if (strcasecmp(argv[1], "clearlrbaseuser") == 0) {
            if (argc == 4) {
                if (!User::clear_lastread(inir.Get("paths", "data path", "data"), std::string(argv[2]), std::string(argv[3]))) {
                    std::cout << "Error clearing last read." << std::endl;
                } else {
                    std::cout << "Success clearing last read." << std::endl;
                }
            }
        } else if (strcasecmp(argv[1], "deleteuser") == 0) {
            if (argc == 3) {
                if (!User::delete_user(inir.Get("paths", "data path", "data"), std::string(argv[2]))) {
                    std::cout << "Failed" << std::endl;
                }
            }
        }
	}
	else {
		std::cerr << "Usage: " << argv[0] << " command [args]" << std::endl;
		std::cerr << "   COMMAND password         ARGS username newpassword" << std::endl;
		std::cerr << "   COMMAND seclevel         ARGS username newlevel" << std::endl;
		std::cerr << "   COMMAND uploadindex      ARGS indexfile folder database [uploadedby]" << std::endl;
		std::cerr << "   COMMAND uploadbulk       ARGS folder database [uploadedby]" << std::endl;
		std::cerr << "   COMMAND filetrim         ARGS database" << std::endl;
		std::cerr << "   COMMAND movefile         ARGS srcfilename destdir srcdatabase destdatabase" << std::endl;
		std::cerr << "   COMMAND allfiles         ARGS sec_level outfile" << std::endl;
		std::cerr << "   COMMAND newfiles         ARGS sec_level yyyy.mm.dd outfile" << std::endl;
		std::cerr << "   COMMAND nodelistp        ARGS domain nodelist database" << std::endl;
        std::cerr << "   COMMAND clearlrall       ARGS (NONE)" << std::endl;
        std::cerr << "   COMMAND clearlruser      ARGS username" << std::endl;
        std::cerr << "   COMMAND clearlrbase      ARGS msgbasefile" << std::endl;
        std::cerr << "   COMMAND clearlrbaseuser  ARGS msgbasefile username" << std::endl;
        std::cerr << "   COMMAND deleteuser       ARGS username" << std::endl;
	}
}
