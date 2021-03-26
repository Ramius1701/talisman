#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#ifdef _MSC_VER
#include <WinSock2.h>
#else
#include <unistd.h>
#endif
#include "Editor.h"
#include "Node.h"
#include "Door.h"
#include "FullScreenEditor.h"

std::vector<std::string> Editor::enter_message_ex(Node *n, std::string to, std::string subject, std::string areaname, bool priv, std::vector<std::string>* quotebuffer)
{
	std::vector<std::string> msg;
	if (quotebuffer != nullptr) {
		FILE* q_fptr = fopen(std::string(n->get_config()->tmp_path() + "/" + std::to_string(n->getnodenum()) + "/MSGTMP").c_str(), "wb");
		if (q_fptr) {
			for (size_t i = 0; i < quotebuffer->size(); i++) {
				fprintf(q_fptr, "%s\r\n", quotebuffer->at(i).c_str());
			}

			fclose(q_fptr);
		}
	}

	FILE* fptr = fopen(std::string(n->get_config()->tmp_path() + "/" + std::to_string(n->getnodenum()) + "/MSGINF").c_str(), "wb");
	if (fptr) {
		fprintf(fptr, "%s\r\n", n->get_user().get_username().c_str());
		fprintf(fptr, "%s\r\n", to.c_str());
		fprintf(fptr, "%s\r\n", subject.c_str());
		fprintf(fptr, "0\r\n");
		fprintf(fptr, "%s\r\n", areaname.c_str());
		if (priv) {
			fprintf(fptr, "YES\r\n");
		}
		else {
			fprintf(fptr, "NO\r\n");
		}

		fclose(fptr);

		std::vector<std::string> args;

		args.push_back(std::to_string(n->getnodenum()));
#ifdef _MSC_VER
		args.push_back(std::to_string(n->get_socket()));
#endif
		Door::createDropfiles(n);
		if (!Door::runExternal(n, n->get_config()->external_editor(), args, false)) {
#ifdef _MSC_VER
			closesocket(n->get_socket());
#else
			close(n->get_socket());
#endif
			n->disconnected();
		}
		
		std::string line;
		std::ifstream infile(std::string(n->get_config()->tmp_path() + "/" + std::to_string(n->getnodenum()) + "/MSGTMP"));
		while (std::getline(infile, line))
		{
			std::istringstream iss(line);

			if (line.at(line.size() - 1) == '\r') {
				line = line.substr(0, line.size() - 1);
			}

			msg.push_back(line);
		}
		infile.close();
		if (msg.size() > 0) {
			return msg;
		}
	}
	return msg;
}
std::vector<std::string> Editor::enter_message(Node* n, std::string to, std::string subject, std::string areaname, bool priv, std::vector<std::string>* quotebuffer, std::vector<std::string>* body) {
	int fse = stoi(n->get_user().get_attribute("fullscreeneditor", "0"));

	if (fse == 0) {
		if (n->get_config()->external_editor() != "" && n->hasANSI && body == nullptr) {
			n->print_f("\r\n\r\n|14Use external editor (Y/N) : |07");
			if (tolower(n->getch()) == 'n') {
				return enter_message_in(n, to, subject, areaname, priv, quotebuffer, nullptr);
			}
			else {
				return enter_message_ex(n, to, subject, areaname, priv, quotebuffer);
			}
		}
		else if (n->hasANSI) {
			n->print_f("\r\n\r\n|14Use fullscreen editor (Y/N) : |07");
			if (tolower(n->getch()) == 'n') {
				return enter_message_in(n, to, subject, areaname, priv, quotebuffer, body);
			}
			else {
				FullScreenEditor fseditor(n, to, subject, quotebuffer, body);
				return fseditor.edit();
			}
		} else {
			return enter_message_in(n, to, subject, areaname, priv, quotebuffer, body);
		}
	}
	else if (fse == 1 && n->hasANSI) {
		if (n->get_config()->external_editor() != "" && body == nullptr) {
			return enter_message_ex(n, to, subject, areaname, priv, quotebuffer);
		}
		else {
			FullScreenEditor fseditor(n, to, subject, quotebuffer, body);
			return fseditor.edit();
		}
	}
	else {
		return enter_message_in(n, to, subject, areaname, priv, quotebuffer, body);
	}
}

std::vector<std::string> Editor::enter_message_in(Node *n, std::string to, std::string subject, std::string areaname, bool priv, std::vector<std::string>* quotebuffer, std::vector<std::string> *body)
{
	std::vector<std::string> lines;
	bool done = false;
	std::string cur_line;

	if (body != nullptr) {
		for (size_t i = 0; i < body->size(); i++) {
			lines.push_back(body->at(i));
		}
	}

	n->print_f("\r\n|08---------------------------------------------------------------------");
	n->print_f("\r\n|14 Commands on a new line: /? for HELP /S to SAVE, /A to ABORT");
	n->print_f("\r\n|08---------------------------------------------------------------------");
	while (!done) {
		n->print_f("\r\n|08[|15%.4d|08]: |07", lines.size());
		cur_line = n->get_string(70, false, true);
		if (cur_line == "/S" || cur_line == "/s") {
			return lines;
		}
		else if (cur_line == "/A" || cur_line == "/a") {
			lines.clear();
			return lines;
		}
		else if ((cur_line == "/Q" || cur_line == "/q") && quotebuffer != nullptr) {
			n->print_f("\r\n\r\n");
			int qlinec = 0;
			for (int i = 0; i < quotebuffer->size(); i++) {
				n->print_f("[%.4d]: %s\r\n", i, quotebuffer->at(i).c_str());
				qlinec++;
				if (qlinec == 23) {
					n->print_f("|14Continue (Y/N) : |07");
					if (tolower(n->getche()) == 'n') {
						break;
					}
					qlinec = 0;
				}
			}

			try {
				n->print_f("\r\n|15Quote From Line: |07");
				int qfrom = std::stoi(n->get_string(5, false));
				n->print_f("\r\n  |15Quote To Line: |07");
				int qto = std::stoi(n->get_string(5, false));
				if (!(qfrom > qto || qfrom < 0 || qto >= quotebuffer->size())) {
					for (int i = qfrom; i <= qto; i++) {
						lines.push_back(quotebuffer->at(i));
					}
				}
			}
			catch (std::out_of_range&) {
				n->print_f("\r\n|14Value out of range!|07\r\n");
			}
			catch (std::invalid_argument&) {
				n->print_f("\r\n|14Invalid Argument!|07\r\n");
			}
		}
		else if ((cur_line == "/D" || cur_line == "/d") && lines.size() > 0) {
			try {
				n->print_f("\r\n|14Delete From Line: |07");
				int dfrom = std::stoi(n->get_string(5, false));
				n->print_f("\r\n  |14Delete To Line: |07");
				int dto = std::stoi(n->get_string(5, false));
				if (!(dfrom > dto || dfrom < 0 || dto >= lines.size())) {
					for (int i = dto; i >= dfrom; i--) {
						lines.erase(lines.begin() + i);
					}
				}
			}
			catch (std::out_of_range&) {
				n->print_f("\r\n|14Value out of range!|07\r\n");
			}
			catch (std::invalid_argument&) {
				n->print_f("\r\n|14Invalid Argument!|07\r\n");
			}
		}
		else if (cur_line == "/L" || cur_line == "/l") {
			for (int i = 0; i < lines.size(); i++) {
				n->print_f("\r\n|08[|14%.4d|08]: |07%s", i, lines.at(i).c_str());
			}
			n->print_f("\r\n");
		}
		else if (cur_line == "/?") {
			n->print_f("\r\n|12>>>> |15HELP |12<<<<|07\r\n");
			n->print_f("/S Save Message\r\n");
			n->print_f("/A Abort Message\r\n");
			n->print_f("/Q Quote Message\r\n");
			n->print_f("/L List Message\r\n");
			n->print_f("/D Delete Lines\r\n\00170");
		}
		else {
			lines.push_back(cur_line);
		}
	}
	return lines;
}