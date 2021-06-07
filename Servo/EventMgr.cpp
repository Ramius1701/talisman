#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#ifdef _MSC_VER
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include "../Common/toml.hpp"
#include "EventMgr.h"

extern std::string ts();

bool EventMgr::load_config(std::string datapath)
{
    try {
		auto data = toml::parse_file(datapath + "/events.toml");

		auto eventitems = data.get_as<toml::array>("event");

		for (size_t i = 0; i < eventitems->size(); i++) {
            std::string myname;
            int myinterval;
			std::string myexec;
            std::string myfiletowatch;

            auto itemtable = eventitems->get(i)->as_table();

			auto name = itemtable->get("name");
			if (name != nullptr) {
				myname = name->as_string()->value_or("Unnamed Event");
			}
			else {
				myname = "Unnamed Event";
			}

			auto interval = itemtable->get("interval");
			if (interval != nullptr) {
				myinterval = interval->as_integer()->value_or(0);
			}
			else {
				myinterval = 0;
			}

			auto filetowatch = itemtable->get("watchfile");
            if (filetowatch != nullptr) {
                myfiletowatch = filetowatch->as_string()->value_or("");
            } else {
                myfiletowatch = "";
            }

			auto exe = itemtable->get("exec");
			if (exe != nullptr) {
				myexec = exe->as_string()->value_or("");
			}
			else {
				myexec = "";
			}

			if ((myinterval == 0 && myfiletowatch == "") || myexec == "" || (myinterval != 0 && myfiletowatch != "")) {
                std::cerr << "\x1b[1;31m" << ts() << "EventManager: Invalid event config for " << myname << "\x1b[0m" << std::endl;
                continue;
            }

            struct event_t ev;

            ev.name = myname;
            ev.execute = myexec;
            ev.interval = myinterval;

            if (myfiletowatch != "") {
                ev.file_to_watch = myfiletowatch;

                if (std::filesystem::exists(myfiletowatch)) {
                    ev.file_exists = true;
                    ev.modified_time = std::filesystem::last_write_time(myfiletowatch);
                } else {
                    ev.file_exists = false;
                    ev.modified_time = std::filesystem::file_time_type::clock::now();
                }
            }

            if (ev.interval != 0) {
                ev.nextrun = time(NULL) + (myinterval * 60);
            }
            events.push_back(ev);
        }
    } catch (toml::parse_error) {
        return false;
    }

    std::cout << "\x1b[1;37m" << ts() << "EventManager: Loaded " << events.size() << " events..." << "\x1b[0m" << std::endl;

    return true;
}

void EventMgr::executor(EventMgr *ev)
{
    while(!ev->shutdown) {
#ifdef _MSC_VER
        Sleep(60000);
#else
        sleep(60);
#endif
        time_t now = time(NULL);

        for (size_t i = 0; i < ev->events.size(); i++) {
            bool shouldrun = false;
            std::string reason = "";
            if (ev->events.at(i).interval > 0) {
                if (ev->events.at(i).nextrun <= now) {
                    ev->events.at(i).nextrun = now + (ev->events.at(i).interval * 60);
                    reason = "Timed";
                    shouldrun = true;
                }
            } else {
                std::filesystem::path fspath(ev->events.at(i).file_to_watch);

                if (ev->events.at(i).file_exists != std::filesystem::exists(fspath)) {
                    shouldrun = true;
                    ev->events.at(i).file_exists = std::filesystem::exists(fspath);
                    if (!ev->events.at(i).file_exists) {
                        reason = "File Deleted";
                    } else {
                        reason = "File Created";
                        ev->events.at(i).modified_time = std::filesystem::last_write_time(fspath);
                    }
                } else if (std::filesystem::exists(fspath) && ev->events.at(i).modified_time != std::filesystem::last_write_time(fspath)) {
                    shouldrun = true;
                    ev->events.at(i).modified_time = std::filesystem::last_write_time(fspath);
                    reason = "File Modified";
                }
            }

            if (shouldrun) {
                std::cout << "\x1b[1;37m" << ts() << "EventManager: Running " << ev->events.at(i).name << " (Reason: " << reason << ")" << "\x1b[0m" << std::endl;

#ifdef _MSC_VER
				char* cmd = strdup(ev->events.at(i).execute.c_str());

				STARTUPINFOA si;
				PROCESS_INFORMATION pi;

				ZeroMemory(&si, sizeof(si));
				si.cb = sizeof(si);
				si.dwFlags = STARTF_USESHOWWINDOW;
				si.wShowWindow = SW_MINIMIZE;

				ZeroMemory(&pi, sizeof(pi));

				if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
					std::cerr << "\x1b[1;31m" << ts() << "EventManager: Failed to create process!" << "\x1b[0m" << std::endl;
					free(cmd);
					continue;
				}
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
				free(cmd);
#else

				pid_t pid = fork();
				if (pid == 0) {
                    std::stringstream ss;
                    execl("/bin/sh", "sh", "-c", ev->events.at(i).execute.c_str(), (char *) NULL);
                    exit(0);
				}
				else if (pid == -1) {
					std::cerr << "\x1b[1;31m" << ts() << "EventManager: Failed to create process!" << "\x1b[0m" << std::endl;
				}
#endif
            }
        }
    }
}


void EventMgr::run(std::string datapath)
{
    // load events
    if (!load_config(datapath)) {
        std::cerr << "\x1b[1;31m" << ts() << "EventManager: Error loading event manager config" << "\x1b[0m" << std::endl;
        return;
    }

    shutdown = false;
    // run event thread
    std::thread th(executor, this);
    th.detach();
    // return
}
