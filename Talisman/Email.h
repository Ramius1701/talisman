#pragma once

#include "MsgArea.h"
class Node;

class Email
{
public:
	std::string sender;
	std::string subject;
	std::vector<std::string> msg;
	bool seen;
	time_t date;
	int id;
	static int count_email(Node* n);
	static int unread_email(Node* n);
	static int view_email(Node* n, Email e);
	static void list_email(Node* n);
	static bool save_message(Node* n, std::string to, std::string from, std::string subject, std::vector< std::string> msg);
	static int qwk_scan(Node* n, FILE* msgs_dat_fptr, FILE* pers_ndx_fptr, FILE* conf_ndx_fptr, int tot);
	static void delete_email(Node* n, int id);
	static void set_all_seen(Node* n);
private:
	static bool open_database(std::string filename, sqlite3** db);

};

