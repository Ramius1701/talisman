#include "Archiver.h"
#include "Editor.h"
#include "Node.h"
#include "Settings.h"
#include <sstream>

void Settings::do_settings(Node *n) {
  const char *yesnoask[] = {"ASK", "YES", "NO"};
  while (true) {

    n->cls();
    n->print_f(" |14Settings for |15%s |08(|15%s|08)\r\n", n->get_user().get_attribute("fullname", "UNKNOWN").c_str(), n->get_user().get_username().c_str());
    n->print_f("|08------------------------------------------------------------------------------|07\r\n");
    n->print_f(" |15L |14Change your location |08(|15%s|08)\r\n", n->get_user().get_attribute("location", "Somewhere, The World").c_str());
    n->print_f(" |15E |14Change your email |08(|15%s|08)\r\n", n->get_user().get_attribute("email", "").c_str());
    n->print_f(" |15P |14Change your password |08(|15NOT SHOWN|08)\r\n");
    n->print_f(" |15F |14Use full screen editor |08(|15%s|08)\r\n", yesnoask[stoi(n->get_user().get_attribute("fullscreeneditor", "0"))]);
    n->print_f(" |15R |14Use full screen reader |08(|15%s|08)\r\n", (n->get_user().get_attribute("fullscreenreader", "true") == "false" ? "NO" : "YES"));
    n->print_f(" |15K |14Show Message Kludge Lines |08(|15%s|08)\r\n", (n->get_user().get_attribute("viewkludges", "false") == "false" ? "NO" : "YES"));
    n->print_f(" |15N |14Allow Node Messages |08(|15%s|08)\r\n", (n->get_user().get_attribute("nodemsgs", "true") == "false" ? "NO" : "YES"));
    int myarc = stoi(n->get_user().get_attribute("archiver", "-1"));
    if (myarc >= (int)n->get_config()->archivers.size()) {
      myarc = -1;
    }
    n->print_f(" |15A |14Default Archiver for QWK |08(|15%s|08)\r\n", (myarc == -1 ? "NONE" : n->get_config()->archivers.at(myarc)->name.c_str()));
    n->print_f(" |15S |14Toggle your signature |08(|15%s|08)\r\n", (n->get_user().get_attribute("signature_enabled", "false") == "false" ? "OFF" : "ON"));
    n->print_f(" |15O |14Override Screen Dimensions |08(|15%s|08)\r\n", (n->get_user().get_attribute("screen_override", "false") == "false" ? "NO" : "YES"));
    n->print_f(" |15W |14Override Screen Width |08(|15%s|08)\r\n", n->get_user().get_attribute("screen_override_width", "80").c_str());
    n->print_f(" |15H |14Override Screen Height |08(|15%s|08)\r\n", n->get_user().get_attribute("screen_override_height", "24").c_str());
    n->print_f(" |15T |14Set Default Theme |08(|15%s|08)\r\n", n->get_config()->selected_theme_name().c_str());
    n->print_f("\r\n");
    n->print_f(" |15Q |14Quit\r\n");
    n->print_f("|08------------------------------------------------------------------------------|07\r\n");
    n->print_f("|14Command |08[|15L|08,|15E|08,|15P|08,|15F|08,|15R|08,|15K|08,|15N|08,|15A|08,|15S|08,|15O|08,|15W|08,|15H|08,|15Q|08]: |07");
    std::string cmd = n->get_string(1, false);
    n->print_f("\r\n\r\n");
    if (cmd.size() > 0) {
      switch (tolower(cmd[0])) {
      case 'l': {
        std::string ret = n->get_string(26, false);
        if (ret.size() >= 2) {
          n->get_user().set_attribute("location", ret);
        }
      } break;
      case 'e': {
        std::string ret = n->get_string(32, false);
        n->get_user().set_attribute("email", ret);
      } break;
      case 'p': {
        n->print_f("Your current password: ");
        std::string curpass = n->get_string(16, true);
        if (n->get_user().check_password(curpass)) {
          n->print_f("\r\n         New password: ");
          std::string newpass = n->get_string(16, true);
          if (newpass.size() < 6) {
            n->print_f("\r\n|12Password too short!|07\r\n");
          } else {
            n->print_f("\r\n      Repeat password: ");
            std::string reppass = n->get_string(16, true);
            if (newpass == reppass) {
              if (n->get_user().update_password(newpass)) {
                n->print_f("\r\n|10SUCCESS!\r\n");
              } else {
                n->print_f("\r\n|12FAILURE!\r\n");
              }
            } else {
              n->print_f("\r\n|12Passwords don't match!\r\n");
            }
          }
        } else {
          n->print_f("\r\n|12Sorry, that is incorrect.\r\n");
        }
      }
        n->pause();
        break;
      case 'f': {
        int cur = stoi(n->get_user().get_attribute("fullscreeneditor", "0"));
        cur++;
        if (cur == 3)
          cur = 0;
        n->get_user().set_attribute("fullscreeneditor", std::to_string(cur));

      } break;
      case 'k': {
        bool viewkludges = n->get_user().get_attribute("viewkludges", "false") == "false";
        n->get_user().set_attribute("viewkludges", (viewkludges ? "true" : "false"));
      } break;
      case 'r': {
        bool fsr = n->get_user().get_attribute("fullscreenreader", "true") == "false";
        n->get_user().set_attribute("fullscreenreader", (fsr ? "true" : "false"));
      } break;
      case 'o': {
        bool screen_override = n->get_user().get_attribute("screen_override", "false") == "false";
        n->get_user().set_attribute("screen_override", (screen_override ? "true" : "false"));
        if (screen_override) {
          n->override_on = 1;
        } else {
          n->override_on = 0;
        }
      } break;
      case 'n': {
        bool nodemsgs = n->get_user().get_attribute("nodemsgs", "true") == "false";
        n->get_user().set_attribute("nodemsgs", (nodemsgs ? "true" : "false"));
      } break;
      case 'a': {
        int arc = n->get_config()->select_archiver(n);
        if (arc != -1) {
          n->get_user().set_attribute("archiver", std::to_string(arc));
        }
      } break;
      case 's': {
        if (n->get_user().get_attribute("signature", "").size() == 0) {
          n->get_user().set_attribute("signature", "");
        }
        bool enabled = n->get_user().get_attribute("signature_enabled", "false") == "true";
        enabled = !enabled;
        n->get_user().set_attribute("signature_enabled", (enabled ? "true" : "false"));
      } break;
      case 'w': {
        std::string newwidth = n->get_string(3, false);
        bool stringok = true;
        for (size_t i = 0; i < newwidth.size(); i++) {
          if (newwidth.at(i) < '0' || newwidth.at(i) > '9') {
            stringok = false;
            break;
          }
        }
        if (stringok && newwidth.size() > 0) {
          n->get_user().set_attribute("screen_override_width", newwidth);
          n->override_width = stoi(newwidth);
        } else {
          n->print_f("\r\n\r\n|12Invalid Argument!\r\n");
        }
      } break;
      case 'h': {
        std::string newheight = n->get_string(3, false);
        bool stringok = true;
        for (size_t i = 0; i < newheight.size(); i++) {
          if (newheight.at(i) < '0' || newheight.at(i) > '9') {
            stringok = false;
            break;
          }
        }
        if (stringok && newheight.size() > 0) {
          n->get_user().set_attribute("screen_override_height", newheight);
          n->override_height = stoi(newheight);
        } else {
          n->print_f("\r\n\r\n|12Invalid Argument!\r\n");
        }
      } break;
      case 't': {
        if (n->get_config()->select_theme(n, false) != -1) {
          n->print_f("\r\n\r\n|10Your selected theme will be activated next login!\r\n\r\n");
          n->pause();
        }

      } break;
      case 'q':
        return;
      }
    }
  }
}
