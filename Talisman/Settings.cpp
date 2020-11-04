#include "Settings.h"
#include "Node.h"

void Settings::do_settings(Node* n) {
	const char* yesnoask[] = { "ASK", "YES", "NO" };
	while (true) {

		n->cls();
		n->print_f(" |14Settings for |15%s |08(|15%s|08)\r\n", n->get_user().get_attribute("fullname", "UNKNOWN").c_str(), n->get_user().get_username().c_str());
		n->print_f("|08------------------------------------------------------------------------------|07\r\n");
		n->print_f(" |15L |14Change your location |08(|15%s|08)\r\n", n->get_user().get_attribute("location", "Somewhere, The World").c_str());
		n->print_f(" |15E |14Change your email |08(|15%s|08)\r\n", n->get_user().get_attribute("email", "").c_str());
		n->print_f(" |15P |14Change your password |08(|15NOT SHOWN|08)\r\n");
		n->print_f(" |15F |14Use full screen editor |08(|15%s|08)\r\n", yesnoask[stoi(n->get_user().get_attribute("fullscreeneditor", "0"))]);
		n->print_f(" |15K |14Show Message Kludge Lines |08(|15%s|08)\r\n", (n->get_user().get_attribute("viewkludges", "false") == "false" ? "NO" : "YES"));
		n->print_f("\r\n");
		n->print_f(" |15Q |14Quit\r\n");
		n->print_f("|08------------------------------------------------------------------------------|07\r\n");
		n->print_f("|14Command |08[|15L|08,|15E|08,|15P|08,|15F|08,|15Q|08]: |07");
		std::string cmd = n->get_string(1, false);
		n->print_f("\r\n\r\n");
		if (cmd.size() > 0) {
			switch (tolower(cmd[0])) {
			case 'l':
			{
				std::string ret = n->get_string(26, false);
				if (ret.size() >= 2) {
					n->get_user().set_attribute("location", ret);
				}
			}
				break;
			case 'e':
			{
				std::string ret = n->get_string(32, false);
				n->get_user().set_attribute("email", ret);
			}
				break;
			case 'p':
			{
				n->print_f("Your current password: ");
				std::string curpass = n->get_string(16, true);
				if (n->get_user().check_password(curpass)) {
					n->print_f("\r\n         New password: ");
					std::string newpass = n->get_string(16, true);
					if (newpass.size() < 6) {
						n->print_f("\r\n|12Password too short!|07\r\n");
					}
					else {
						n->print_f("\r\n      Repeat password: ");
						std::string reppass = n->get_string(16, true);
						if (newpass == reppass) {
							if (n->get_user().update_password(newpass)) {
								n->print_f("\r\n|10SUCCESS!\r\n");
							}
							else {
								n->print_f("\r\n|12FAILURE!\r\n");
							}
						}
						else {
							n->print_f("\r\n|12Passwords don't match!\r\n");
						}
					}
				}
				else {
					n->print_f("\r\n|12Sorry, that is incorrect.\r\n");
				}
			}
			n->print_f("|12Press any key...|07");
			n->getch();
				break;
			case 'f':
			{
				int cur = stoi(n->get_user().get_attribute("fullscreeneditor", "0"));
				cur++;
				if (cur == 3) cur = 0;
				n->get_user().set_attribute("fullscreeneditor", std::to_string(cur));

			}
				break;
			case 'k':
			{
				bool viewkludges = n->get_user().get_attribute("viewkludges", "false") == "false";
				n->get_user().set_attribute("viewkludges", (viewkludges ? "true" : "false"));
			}
				break;
			case 'q':
				return;
			}
		}
	}
}