#include <fstream>
#include <sstream>
#include "MessageReader.h"
#include "Node.h"
#include "Nodelist.h"
#include "Script.h"

bool MessageReader::print_header(Node *n, struct msg_reader_msg_t *msg, int tot_msgs) {
  std::ifstream in;
  char c;
  bool gottag = false;
  std::stringstream ss;
  int lastc = 'x';

  if (msg->msg_type == 0) {
    // local mail
    in.open(n->get_config()->gfile_path() + "/fsr_header_local.ans");
  } else if (msg->msg_type == 1) {
    // echomail
    in.open(n->get_config()->gfile_path() + "/fsr_header_echo.ans");
  } else if (msg->msg_type == 2) {
    // netmail
    in.open(n->get_config()->gfile_path() + "/fsr_header_net.ans");
    if (!in.is_open()) {
      in.open(n->get_config()->gfile_path() + "/fsr_header_echo.ans");
    }
  } else if (msg->msg_type == 3) {
    // email
    in.open(n->get_config()->gfile_path() + "/fsr_header_email.ans");
  } else {
    in.open(n->get_config()->gfile_path() + "/fsr_header_file.ans");
  }

  if (!in.is_open()) {
    return false;
  }
  while (in.get(c)) {
    if (c == 0x1a) {
      break;
    }
    if (c == '@' && gottag == false) {
      gottag = true;
      continue;
    } else if (c == '@' && gottag == true) {
      // end of tag

      if (n->compare_token(ss.str(), "MSGAREA")) {
        if (msg->area != NULL) {
          n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, msg->area->get_name().c_str());
        } else {
          if (msg->msg_type == 3) {
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "Private Email!");
          } else {
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, " ");
          }
        }
      } else if (n->compare_token(ss.str(), "MSGCONF")) {
        if (msg->area != NULL) {
          n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, msg->area->get_conf()->get_name().c_str());
        } else {
          if (msg->msg_type == 3) {
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "Local");
          } else {
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, " ");
          }
        }
      } else if (n->compare_token(ss.str(), "MSGSUBJ")) {
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, msg->subject.c_str());
      } else if (n->compare_token(ss.str(), "MSGFROM")) {
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, msg->from.c_str());
      } else if (n->compare_token(ss.str(), "MSGTO")) {
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, msg->to.c_str());
      } else if (n->compare_token(ss.str(), "FROMBBS")) {
        if (msg->area->get_wwivnode() == 0) {
          if (msg->origaddr->point == 0) {
            std::string node = Nodelist::lookup_bbsname(n, std::to_string(msg->origaddr->zone) + ":" + std::to_string(msg->origaddr->net) + "/" +
                                                               std::to_string(msg->origaddr->node));
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, node.c_str());
          } else {
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "A Point System");
          }
        } else {
          n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "A WWIVnet System");
        }
      } else if (n->compare_token(ss.str(), "FROMADDR")) {
        std::stringstream ss2;
        if (msg->area->get_wwivnode() == 0) {
          ss2 << msg->origaddr->zone << ":" << msg->origaddr->net << "/" << msg->origaddr->node << "." << msg->origaddr->point;
        } else {
          ss2 << "@" << msg->origaddr->node;
        }
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, ss2.str().c_str());
      } else if (n->compare_token(ss.str(), "TOBBS")) {
        if (msg->area->get_wwivnode() == 0) {
          if (msg->destaddr->point == 0) {
            std::string node = Nodelist::lookup_bbsname(n, std::to_string(msg->destaddr->zone) + ":" + std::to_string(msg->destaddr->net) + "/" +
                                                               std::to_string(msg->destaddr->node));
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, node.c_str());
          } else {
            n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "A Point System");
          }
        } else {
          n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, "A WWIVnet System");
        }
      } else if (n->compare_token(ss.str(), "TOADDR")) {
        std::stringstream ss2;
        if (msg->area->get_wwivnode() == 0) {
          ss2 << msg->destaddr->zone << ":" << msg->destaddr->net << "/" << msg->destaddr->node << "." << msg->destaddr->point;
        } else {
          ss2 << "@" << msg->destaddr->node;
        }
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, ss2.str().c_str());
      } else if (n->compare_token(ss.str(), "MSGDATE")) {
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, msg->date.c_str());
      } else if (n->compare_token(ss.str(), "MSGN")) {
        std::stringstream ss2;
        ss2 << msg->msg_no;
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, ss2.str().c_str());
      } else if (n->compare_token(ss.str(), "TOTN")) {
        std::stringstream ss2;
        ss2 << tot_msgs;
        n->print_f("%-*.*s", ss.str().size() + 2, ss.str().size() + 2, ss2.str().c_str());
      } else if (ss.str().substr(0, 10) == "RUNSCRIPT:") {
        std::stringstream ss2;
        ss2 << n->get_config()->script_path() << "/" << ss.str().substr(10) << ".lua";

        Script::msgheader(n, ss2.str(), msg->area->get_file().substr(n->get_config()->msg_path().size() + 1), msg->msg_serial, msg->from, msg->to,
                          msg->subject);
      }
      ss.str("");
      gottag = false;
      continue;
    }
    if (gottag == true) {
      if (c == '\r' || c == '\n') {
        n->print_f("@%s", ss.str().c_str());
        lastc = ss.str().at(ss.str().size() - 1);
        ss.str("");
        gottag = false;
      } else {
        ss << c;
        continue;
      }
    }
    if (c == '\n') {
      if (lastc != '\r') {
        n->putch('\r');
      }
    }
    lastc = c;
    n->putch(c);
  }
  in.close();
  return true;
}

int MessageReader::read_message_fsr(Node *n, struct msg_reader_msg_t *msg, int tot_msgs, uint32_t flags) {
  int top = 0;
  std::vector<std::string> linesv2;

  for (size_t i = 0; i < msg->body->size(); i++) {
    if (msg->body->at(i).type == 0) {
      linesv2.push_back(msg->body->at(i).line);
    } else if (msg->body->at(i).type == 1) {
      linesv2.push_back("\x1b[1;36m" + msg->body->at(i).line + "\x1b[0m");
    } else if (msg->body->at(i).type == 2 && msg->showkluges) {
      if (msg->ansi) {
        if (msg->body->at(i).line[0] == '\x01') {
          linesv2.push_back("\x1b[1;30m@" + msg->body->at(i).line.substr(1) + "\r\n");
        } else {
          linesv2.push_back("\x1b[1;30m" + msg->body->at(i).line + "\r\n");
        }
      } else {
        if (msg->body->at(i).line[0] == '\x01') {
          linesv2.push_back("\x1b[1;30m@" + msg->body->at(i).line.substr(1) + "\x1b[0m");
        } else {
          linesv2.push_back("\x1b[1;30m" + msg->body->at(i).line + "\x1b[0m");
        }
      }
    } else if (msg->body->at(i).type == 3) {
      if (msg->ansi) {
        linesv2.push_back("\x1b[1;35m" + msg->body->at(i).line + "\r\n");
      } else {
        linesv2.push_back("\x1b[1;35m" + msg->body->at(i).line + "\x1b[0m");
      }
    }
  }
  // n->print_f("\x1b[6;1H%s\x1b[K\x1b[0;40;37m", n->get_config()->get_prompt_colour());
  n->print_f("\x1b[%d;1H%s ? For help\x1b[K\x1b[0;40;37m", n->get_term_height() - 1, n->get_config()->get_prompt_colour());

  bool done = false;
  while (!done) {

    if (top + n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8) < linesv2.size()) {
      n->print_f("\x1b[%d;%dH%sMORE\x1b[0;40;37m", n->get_term_height() - 1, n->get_term_width() - 5, n->get_config()->get_prompt_colour());
    } else {
      n->print_f("\x1b[%d;%dH%s END\x1b[0;40;37m", n->get_term_height() - 1, n->get_term_width() - 5, n->get_config()->get_prompt_colour());
    }

    for (size_t i = 0; i < n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8); i++) {
      if (i + top < linesv2.size()) {
        if (msg->ansi) {
          n->print_f("\x1b[%d;1H", i + (flags & DISABLE_HEADER ? 1 : 7));
          for (size_t z = 0; z < linesv2.at(top + i).size(); z++) {
            if (linesv2.at(top + i).at(z) == '\r') {
              n->print_f("\x1b[0m\x1b[K");
              break;
            } else {
              n->print_f("%c", linesv2.at(top + i).at(z));
            }
          }

        } else {
          n->print_f("\x1b[%d;1H%s\x1b[K", i + (flags & DISABLE_HEADER ? 1 : 7), linesv2.at(i + top).c_str());
        }
      } else {
        n->print_f("\x1b[%d;1H\x1b[K", i + (flags & DISABLE_HEADER ? 1 : 7));
      }
    }

    while (true) {
      char c = n->getch();

      if (c == '\x1b') {
        c = n->getch();
        if (c == '[') {
          c = n->getch();
          if (c == 'A') {
            // up
            if (top > 0) {
              top--;
              break;
            }
          } else if (c == 'B') {
            // down
            if (top + n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8) < linesv2.size()) {
              top++;
              break;
            }
          } else if (c == 'C') {
            // right
            if (!(flags & DISABLE_NEXT)) {
              return 5;
            }
          } else if (c == 'D') {
            // left
            if (!(flags & DISABLE_PREV)) {
              return 6;
            }
          } else if (c == 'K') {
            // end
            top = linesv2.size() - (n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8));
            if (top < 0) {
              top = 0;
            }
            break;
          } else if (c == 'H') {
            // home
            top = 0;
            break;
          } else if (c == 'V' || c == '5') {
            // page up
            if (c == '5') {
              n->getch();
            }
            top = top - (n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8));
            if (top < 0) {
              top = 0;
            }
            break;
          } else if (c == 'U' || c == '6') {
            // page down
            if (c == '6') {
              n->getch();
            }
            top = top + (n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8));
            if (top > (int)linesv2.size() - (int)(n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8))) {
              top = (int)linesv2.size() - (int)(n->get_term_height() - (flags & DISABLE_HEADER ? 2 : 8));
              if (top < 0) {
                top = 0;
              }
            }
            break;
          }
        }
      }
      if (tolower(c) == 'k') {
        if (!(flags & DISABLE_KLUDGE)) {
          return 7;
        }
      }
      if (c == '\r') {
        if (!(flags & DISABLE_NEXT)) {
          return 5;
        }
        return 0;
      }
      if (tolower(c) == 'q') {
        return 0;
      }
      if (tolower(c) == 'c') {
        if (!(flags & (DISABLE_UNREAD | DISABLE_SEARCH))) {
          return 1;
        }
      }
      if (tolower(c) == 'd') {
        if (!(flags & DISABLE_DELETE)) {
          return 2;
        }
      }
      if (tolower(c) == 'r') {
        if (!(flags & DISABLE_REPLY)) {
          n->cls();
          return 3;
        }
      } else if (tolower(c) == 'o') {
        return 8;
      }
      if (c == '?') {
        int row = 1;
        n->print_f("\x1b[%d;20H\x1b[0;30;47m+-----------[HELP]-----------+", (n->get_term_height() - 9) / 2 + 4);
        n->print_f("\x1b[%d;20H|                            |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        n->print_f("\x1b[%d;20H|    (UP/DOWN) Scroll        |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        if (!(flags & (DISABLE_NEXT | DISABLE_PREV))) {
          n->print_f("\x1b[%d;20H| (LEFT/RIGHT) Prev/Next Msg |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        }
        if (!(flags & DISABLE_UNREAD)) {
          n->print_f("\x1b[%d;20H|  (C) Continue to Next Area |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        } else if (!(flags & DISABLE_SEARCH)) {
          n->print_f("\x1b[%d;20H|  (C) Continue Search       |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        }

        if (!(flags & DISABLE_DOWNLOAD)) {
          n->print_f("\x1b[%d;20H|  (O) Download Message      |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        }

        if (!(flags & DISABLE_DELETE)) {
          n->print_f("\x1b[%d;20H|  (D) Delete Message        |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        }
        n->print_f("\x1b[%d;20H|  (Q) Quit                  |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        n->print_f("\x1b[%d;20H|                            |", ((n->get_term_height() - 9) / 2 + 4) + row++);
        n->print_f("\x1b[%d;20H+----------------------------+\x1b[0m", ((n->get_term_height() - 9) / 2 + 4) + row++);
        n->getch();
        break;
      }
    }
  }
  return 0;
}

int MessageReader::read_message_classic(Node *n, struct msg_reader_msg_t *msg, int tot_msgs, uint32_t flags) {
  int lines = 0;

  if (!(flags & DISABLE_HEADER)) {
    n->print_f("|08------------------------------------------------------------------------------\r\n");
    lines = 6;
  }
  for (size_t lno = 0; lno < msg->body->size(); lno++) {
    if (msg->body->at(lno).type == 0) {
      if (msg->ansi) {
        n->print_f("%s", msg->body->at(lno).line.c_str());
      } else {
        n->print_f("|07%s\r\n", msg->body->at(lno).line.c_str());
      }
      lines++;
    } else if (msg->body->at(lno).type == 1) {
      n->print_f("|10%s\r\n", msg->body->at(lno).line.c_str());
      lines++;
    } else if (msg->body->at(lno).type == 2) {
      if (msg->showkluges) {
        if (msg->body->at(lno).line[0] == '\x01') {
          n->print_f("|08@%s\r\n", msg->body->at(lno).line.substr(1).c_str());
        } else {
          n->print_f("|08%s\r\n", msg->body->at(lno).line.c_str());
        }

        lines++;
      }
    } else if (msg->body->at(lno).type == 3) {
      n->print_f("|13%s\r\n", msg->body->at(lno).line.c_str());
      lines++;
    }
    if (lines == n->get_term_height() - 2) {
      if (n->hasANSI) {
        n->print_f("\x1b[s|14Continue (Y/N) : |07");
      } else {
        n->print_f("|14Continue (Y/N) : |07");
      }
      if (tolower(n->getche()) == 'n') {
        if (n->hasANSI) {
          n->print_f("\x1b[u\x1b[K");
        } else {
          n->print_f("\r\n");
        }
        break;
      }
      if (n->hasANSI) {
        n->print_f("\x1b[u\x1b[K");
      } else {
        n->print_f("\r\n");
      }
      lines = 0;
    }
  }
  n->print_f("\r\n");

  std::stringstream ss;

  if (!(flags & DISABLE_REPLY)) {
    ss << "|15R|08=|14Reply|08, ";
  }
  ss << "|15A|08=|14Again|08, ";
  if (!(flags & DISABLE_PREV)) {
    ss << "|15P|08=|14Prev|08, ";
  }
  if (!(flags & DISABLE_NEXT)) {
    ss << "|15N|08=|14Next|08, ";
  }
  if (!(flags & DISABLE_SEARCH)) {
    ss << "|15C|08=|14Continue Search|08, ";
  }
  if (!(flags & DISABLE_UNREAD)) {
    ss << "|15C|08=|14Continue to Next Area|08, ";
  }
  ss << "|15Q|08=|14Quit |08: |07";

  n->print_f("%s", ss.str().c_str());

  std::string res = n->get_string(1, false);
  if (res.size() == 0) {
    if (!(flags & DISABLE_SEARCH)) {
      return 1;
    } else {
      return 5;
    }
  } else {
    switch (tolower(res[0])) {
    case 'd':
      if (!(flags & DISABLE_DELETE)) {
        return 2;
      }
      break;
    case 'r':
      if (!(flags & DISABLE_REPLY)) {
        return 3;
      }
      break;
    case 'a':
      return 4;
    case 'n':
      if (!(flags & DISABLE_NEXT)) {
        return 5;
      }
      break;
    case 'p':
      if (!(flags & DISABLE_PREV)) {
        return 6;
      }
      break;
    case 'k':
      if (!(flags & DISABLE_KLUDGE)) {
        return 7;
      }
      break;
    case 'o':
      if (!(flags & DISABLE_DOWNLOAD)) {
        return 8;
      }
    case 'q':
      return 0;
    case 'c':
      if (!(flags & (DISABLE_SEARCH | DISABLE_UNREAD))) {
        return 9;
      }
      break;
    }
  }
  return 4;
}

int MessageReader::read_message(Node *n, struct msg_reader_msg_t *msg, int tot_msgs, uint32_t flags) {
  bool fsr = (n->get_user().get_attribute("fullscreenreader", "true") == "true" && n->hasANSI);

  n->cls();

  if (!(flags & DISABLE_HEADER)) { 

    if (!print_header(n, msg, tot_msgs)) {
      if (msg->area != NULL) {
        n->print_f("|14   Area: |15%-46.46s\r\n", msg->area->get_name().c_str());
      } else {
        n->print_f("|14   Area: |15%-46.46s\r\n", "Personal E-Mail");
      }
      n->print_f("|14Subject: |15%-65.65s\r\n", msg->subject.c_str());

      if (msg->area == NULL || (!msg->area->is_echomail() && !msg->area->is_netmail())) {
        n->print_f("|14   From: |15%-41.41s\r\n", msg->from.c_str());
        n->print_f("|14     To: |15%-36.36s\r\n", msg->to.c_str());
      } else if (msg->area != NULL) {
        if (msg->area->get_wwivnode() == 0) {
          n->print_f("|14   From: |15%-32.32s |14Addr: |15%d:%d/%d.%d\r\n", msg->from.c_str(), msg->origaddr->zone, msg->origaddr->net, msg->origaddr->node,
                    msg->origaddr->point);

          std::string node = Nodelist::lookup_bbsname(n, std::to_string(msg->origaddr->zone) + ":" + std::to_string(msg->origaddr->net) + "/" +
                                                            std::to_string(msg->origaddr->node));

          if (msg->origaddr->point == 0) {
            n->print_f("|14     To: |15%-32.32s |14Host: |15%-30.30s\r\n", msg->to.c_str(), node.c_str());
          } else {
            n->print_f("|14     To: |15%-32.32s |14Host: |15A Point System\r\n", msg->to.c_str());
          }
        } else {
          n->print_f("|14   From: |15%-32.32s |14Addr: |15%d\r\n", msg->from.c_str(), msg->origaddr->node);
          n->print_f("|14     To: |15%-36.36s\r\n", msg->to.c_str());
        }
      }
      n->print_f("|14   Date: |15%-16.16s                 |14Msg#: |15%6d of %6d\r\n", msg->date.c_str(), msg->msg_no, tot_msgs);
    }
  }

  if (fsr) {
    return read_message_fsr(n, msg, tot_msgs, flags);
  } else {
    return read_message_classic(n, msg, tot_msgs, flags);
  }
}