#pragma once
#include <string>
#include <vector>
#include <ctime>

class IPBlockItem {
	public:
		IPBlockItem(std::string ipaddress, bool block, bool pass);
		~IPBlockItem();
		bool should_pass();
		std::string getip();
	private:
		std::string ipaddr;
		bool passlist;
		bool blocklist;
		int times;
		time_t first_try;
};