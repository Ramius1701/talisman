#pragma once
class MsgArea
{
public:
	MsgArea(std::string name, std::string filename, int r, int w);
	int get_r_sec_level() {
		return read_sec_level;
	}
	std::string get_name() {
		return name;
	}
private:
	std::string name;
	std::string file;
	int read_sec_level;
	int write_sec_level;
};

