#include <filesystem>
#include <fstream>
#include <sstream>
#include "Webbie.h"
#include "Config.h"

std::string Webbie::prepare_page(Config *c, std::string content) {
	std::filesystem::path www(c->wwwpath());
	std::filesystem::path hdr(www);
	std::filesystem::path ftr(www);


	hdr.append("header.tpl");
	ftr.append("footer.tpl");

	if (std::filesystem::exists(hdr) && std::filesystem::exists(ftr)) {
		std::ifstream infile;
		std::stringstream ss;
		infile.open(hdr, std::ios::in);
		std::string str;
		while (std::getline(infile, str)) {
			ss << str << std::endl;
		}

		infile.close();
		ss << content << std::endl;

		infile.open(ftr, std::ios::in);
		while (std::getline(infile, str)) {
			ss << str << std::endl;
		}
		infile.close();
		return ss.str();
	}
	else {
		return content;
	}
}

void Webbie::index(const httplib::Request& req, httplib::Response& res) {
	Config c;
	c.parse();

	std::filesystem::path www(c.wwwpath());
	std::filesystem::path idx(www);
	std::ifstream infile;
	std::string str;
	std::stringstream ss;


	idx.append("index.tpl");

	if (std::filesystem::exists(idx)) {
		infile.open(idx, std::ios::in);
		while (std::getline(infile, str)) {
			ss << str << std::endl;
		}
		infile.close();
	}
	else {
		ss << "No Index..." << std::endl;
	}

	res.set_content(prepare_page(&c, ss.str()), "text/html");
}

int Webbie::run() {

	httplib::Server srv;

	srv.Get("/",  index);

	srv.listen("0.0.0.0", 8080);
	return 0;
}