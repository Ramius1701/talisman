#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include "Config.h"
#include "toml.hpp"
#include "Archiver.h"

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

bool Config::load_archivers(std::string datapath)
{
	try {
		auto data3 = toml::parse_file(datapath + "/archivers.toml");

		auto arcitems = data3.get_as<toml::array>("archiver");

		for (size_t i = 0; i < arcitems->size(); i++) {
			auto itemtable = arcitems->get(i)->as_table();
			std::string mysig;
			int myoffset;
			std::string myname;
			std::string myext;
			std::string myunarc;
			std::string myarc;

			auto name = itemtable->get("name");
			if (name != nullptr) {
				myname = name->as_string()->value_or("Invalid Name");
			}
			else {
				myname = "Unknown";
			}

			auto ext = itemtable->get("extension");
			if (ext != nullptr) {
				myext = ext->as_string()->value_or("");
			}
			else {
				myext = "";
			}

			auto unarc = itemtable->get("unarc");
			if (unarc != nullptr) {
				myunarc = unarc->as_string()->value_or("");
			}
			else {
				myunarc = "";
			}
			auto arc = itemtable->get("arc");
			if (arc != nullptr) {
				myarc = arc->as_string()->value_or("");
			}
			else {
				myarc = "";
			}

			auto sig = itemtable->get("signature");

			if (sig != nullptr) {
				mysig = sig->as_string()->value_or("");
			}
			else {
				mysig = "";
			}

			auto offset = itemtable->get("offset");

			if (offset != nullptr) {
				myoffset = offset->as_integer()->value_or(0);
			}
			else {
				myoffset = 0;
			}

			uint8_t *signature;

			if (mysig == "") {
				signature = NULL;
			}
			else {
				signature = (uint8_t *)malloc(mysig.size() / 2);

				if (!signature) {
					continue;
				}

				for (size_t i = 0; i < mysig.size() / 2; i++) {
					if (mysig.at(i * 2) >= '0' && mysig.at(i * 2) <= '9') {
						signature[i] = (mysig.at(i * 2) - '0') << 4;
					}
					else {
						signature[i] = (toupper(mysig.at(i * 2)) - 'A' + 10) << 4;
					}


					if (mysig.at(i * 2 + 1) >= '0' && mysig.at(i * 2 + 1) <= '9') {
						signature[i] = (signature[i] & 0xf0) | (mysig.at(i * 2 + 1) - '0');
					}
					else {
						signature[i] = (signature[i] & 0xf0) | (toupper(mysig.at(i * 2 + 1)) - 'A' + 10);
					}
				}
			}
			Archiver* a = new Archiver(myname, myext, myunarc, myarc, myoffset, signature, mysig.size() / 2);
			archivers.push_back(a);
		}
	}

	catch (toml::parse_error) {
		std::cerr << "Error parsing " << datapath << "/archivers.toml" << std::endl;
		return false;
	}
	return true;
}

bool Config::load(std::string datapath) {
	try {
		auto data = toml::parse_file(datapath + "/postie.toml");

		auto _inbound = data["postie"]["inbound"].as_string();

		if (_inbound == nullptr) {
			__inbound = "";
		}
		else {
			__inbound = _inbound->value_or("");
		}

		auto _protinbound = data["postie"]["protinbound"].as_string();

		if (_protinbound == nullptr) {
			__protinbound = "";
		}
		else {
			__protinbound = _protinbound->value_or("");
		}

		auto _outbound = data["postie"]["outbound"].as_string();

		if (_outbound == nullptr) {
			__outbound = "";
		}
		else {
			__outbound = _outbound->value_or("");
		}

		auto _packetdir = data["postie"]["packetdir"].as_string();

		if (_packetdir == nullptr) {
			__packetdir = "";
		}
		else {
			__packetdir = _packetdir->value_or("");
		}

		auto _msgbasedir = data["postie"]["msgbasedir"].as_string();

		if (_msgbasedir == nullptr) {
			__msgbasedir = "";
		}
		else {
			__msgbasedir = _msgbasedir->value_or("");
		}
		auto addressitems = data.get_as<toml::array>("address");

		for (size_t i = 0; i < addressitems->size(); i++) {
			auto itemtable = addressitems->get(i)->as_table();
			std::string myaka;

			auto addr = itemtable->get("aka");
			if (addr != nullptr) {
				myaka = addr->as_string()->value_or("");
				
				struct address_conf_t naddr;
				naddr.aka = parse_fido_addr(myaka.c_str());
				if (naddr.aka != NULL) {
					addresses.push_back(naddr);
				}
			}
		}
		auto linkitems = data.get_as<toml::array>("link");

		for (size_t i = 0; i < linkitems->size(); i++) {
			auto itemtable = linkitems->get(i)->as_table();
			NETADDR *myaka;
			NETADDR *myouraka;
			std::string myflavour;
			std::string myarchiver;
			std::string mypacketpwd;

			auto addr = itemtable->get("aka");
			if (addr != nullptr) {
				std::string aka = addr->as_string()->value_or("");
				myaka = parse_fido_addr(aka.c_str());
				if (!myaka) {
					continue;
				}
			}
			else {
				continue;
			}

			auto flavour = itemtable->get("flavour");
			if (flavour != nullptr) {
				myflavour = flavour->as_string()->value_or("normal");
			}
			else {
				myflavour = "normal";
			}

			auto ouraddr = itemtable->get("ouraka");
			if (ouraddr != nullptr) {
				std::string aka = ouraddr->as_string()->value_or("");
				myouraka = parse_fido_addr(aka.c_str());
				if (!myouraka) {
					free(myaka);
					continue;
				}
			}
			else {
				continue;
			}

			auto archiver = itemtable->get("archiver");
			if (archiver != nullptr) {
				myarchiver = archiver->as_string()->value_or("");
			}
			else {
				myarchiver = "";
			}

			auto packetpwd = itemtable->get("packetpwd");
			if (packetpwd != nullptr) {
				mypacketpwd = packetpwd->as_string()->value_or("");
			}
			else {
				mypacketpwd = "";
			}

			struct link_conf_t newlink;

			newlink.aka = myaka;
			newlink.ouraka = myouraka;
			newlink.archiver = myarchiver;
			newlink.packetpwd = mypacketpwd;
			newlink.flavour = myflavour;
			newlink.fptr = NULL;
			links.push_back(newlink);
		}
		auto areaitems = data.get_as<toml::array>("area");

		for (size_t i = 0; i < areaitems->size(); i++) {
			auto itemtable = areaitems->get(i)->as_table();

			NETADDR* myaka;
			std::string myfile;
			std::string mytag;
			std::string mylinklist;
			auto addr = itemtable->get("aka");
			if (addr != nullptr) {
				std::string aka = addr->as_string()->value_or("");
				myaka = parse_fido_addr(aka.c_str());
				if (!myaka) {
					continue;
				}
			}
			else {
				continue;
			}
			auto file = itemtable->get("file");
			if (file != nullptr) {
				myfile = file->as_string()->value_or("");
			}
			else {
				myfile = "";
			}
			auto areatag = itemtable->get("tag");
			if (areatag != nullptr) {
				mytag = areatag->as_string()->value_or("");
			}
			else {
				mytag = "";
			}
			auto linklist = itemtable->get("links");
			if (linklist != nullptr) {
				mylinklist = linklist->as_string()->value_or("");
			}
			else {
				mylinklist = "";
			}

			if (mytag == "" || myfile == "") {
				free(myaka);
				continue;
			}

			struct area_conf_t aconf;

			std::stringstream ss(mylinklist);
			std::string buff;

			while (getline(ss, buff, ',')) {
				trim(buff);
				NETADDR* laddr = parse_fido_addr(buff.c_str());
				if (laddr) {
					for (size_t y = 0; y < links.size(); y++) {
						if (laddr->zone == links.at(y).aka->zone && laddr->net == links.at(y).aka->net && laddr->node == links.at(y).aka->node && laddr->point == links.at(y).aka->point) {
							aconf.links.push_back(&links.at(y));
							break;
						} 
					}

					free(laddr);
				}
			}

			aconf.aka = myaka;
			aconf.areatag = mytag;
			aconf.file = myfile;
			areas.push_back(aconf);
		}
	}
	catch (toml::parse_error) {
		return false;
	}
	return true;
}