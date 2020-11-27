#pragma once
#include "httplib.h"

class Config;

class Webbie
{
public:
	int run();
private:
	static std::string prepare_page(Config* c, std::string content);
	static void index(const httplib::Request& req, httplib::Response& res);
};

