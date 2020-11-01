#include <string>
#include <ctime>
#include <vector>
#include "IPBlockItem.h"

IPBlockItem::IPBlockItem(std::string ipaddress, std::string datapath, bool block, bool pass) {
	blocklist = block;
	passlist = pass;
	ipaddr = ipaddress;
	first_try = time(NULL);
	times = 0;
	data_path = datapath;
}

IPBlockItem::~IPBlockItem() {

}

bool IPBlockItem::should_pass() {
	time_t curtime = time(NULL);

	if (blocklist) {
		return false;
	}

	if (passlist) {
		return true;
	}

	if (curtime > first_try + 300) {
		first_try = curtime;
		return true;
	} else {
		times++;
		if (times > 5) {
			blocklist = true;
			FILE *fptr = fopen(std::string(data_path + "/blocklist.ip").c_str(), "a");
			if (fptr) {
				fprintf(fptr, "%s\n", ipaddr.c_str());
				fclose(fptr);
			}
			return false;
		}
	}

	return true;
}

std::string IPBlockItem::getip() {
	return ipaddr;
}