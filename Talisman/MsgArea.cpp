#include <string>
#include "MsgArea.h"

MsgArea::MsgArea(std::string name, std::string filename, int r, int w)
{
	this->name = name;
	this->file = filename;
	this->read_sec_level = r;
	this->write_sec_level = w;
}
