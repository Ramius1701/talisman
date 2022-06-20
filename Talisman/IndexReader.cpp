#include "../Common/Squish.h"
#include "CallLog.h"
#include "Editor.h"
#include "IndexReader.h"
#include "Node.h"
#include "Nodelist.h"
#include <cstring>
#include <iostream>
#include <vector>
#ifdef _MSC_VER
#define strcasecmp stricmp
#endif

struct area_details_t {
  MsgArea *ma;
  UMSGID lr;
  size_t total;
  size_t unread;
  size_t unread_personal;
  bool tagged;
  bool subbed;
  time_t last_post;
  UMSGID top_msgid;
};

struct conf_details_t {
  MsgConf *mc;
  std::vector<struct area_details_t> area;
};

void IndexReader::run(Node *n) {
  int selected_conf = 0;
  int selected_area = 0;

  int start_conf = 0;
  int start_area = 0;

  int x = 0;

  while (true) {
    std::vector<struct conf_details_t> conf;

    for (size_t i = 0; i < n->get_config()->msgconfs.size(); i++) {
      if (n->get_config()->msgconfs.at(i)->get_sec_level() > n->get_user().get_sec_level())
        continue;

      struct conf_details_t newconf;

      newconf.mc = n->get_config()->msgconfs.at(i);
      for (size_t j = 0; j < newconf.mc->areas.size(); j++) {
        if (newconf.mc->areas.at(j)->get_r_sec_level() > n->get_user().get_sec_level())
          continue;

        struct area_details_t newarea;

        newarea.ma = newconf.mc->areas.at(j);
        newarea.lr = n->get_user().user_get_lastread(newarea.ma->get_file());
        newarea.total = newarea.ma->get_total_msgs();
        newarea.unread = newconf.mc->areas.at(j)->get_new_msgs(newarea.lr);
        newarea.unread_personal = 0;
        newarea.tagged = false;
        newarea.last_post = 0;
        newarea.subbed = n->get_user().is_subscribed(newarea.ma->get_file());
        newarea.top_msgid = 0;
        sq_msg_base_t *mb = SquishOpenMsgBase(newarea.ma->get_file().c_str());
        if (mb != NULL) {
          if (mb->basehdr.num_msg > 0) {
            sq_msg_t *msg;
            msg = SquishReadMsg(mb, mb->basehdr.num_msg);
            if (msg) {
              struct tm localtm;
              memset(&localtm, 0, sizeof(struct tm));
              localtm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980 - 1900;
              localtm.tm_mday = msg->xmsg.date_written.date & 31;
              localtm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
              localtm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
              localtm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
              localtm.tm_sec = msg->xmsg.date_written.time & 31;
              localtm.tm_isdst = -1;
              newarea.top_msgid = msg->xmsg.umsgid;
              newarea.last_post = mktime(&localtm);

              SquishFreeMsg(msg);
            }
          }
          SquishCloseMsgBase(mb);
        }
        newconf.area.push_back(newarea);
      }

      if (newconf.area.size() > 0) {
        conf.push_back(newconf);
      }
    }

    n->cls();
    n->print_f("\x1b[1;1H%s      Message Area                      Total Unread Last Msg\x1b[K", n->get_config()->get_prompt_colour());
    n->print_f("\x1b[%d;1H%sUp / Down - Scroll, Enter - Select, ? - Help (More Commands)\x1b[K", n->get_term_height() - 1,
               n->get_config()->get_prompt_colour());
    n->print_f("|16");
    while (true) {
      int cur_conf = start_conf;
      int cur_area = start_area;
      for (size_t i = 0; i < n->get_term_height() - 3; i++) {
        n->print_f("\x1b[%d;1H", i + 2);
        if (cur_area == 0) {
          n->print_f("|14      %s\x1b[K", conf.at(cur_conf).mc->get_name().c_str());
          i++;
          if (i == n->get_term_height() - 3)
            break;
          n->print_f("\x1b[%d;1H", i + 2);
        }
        struct tm ltm;
#ifdef _MSC_VER
        localtime_s(&ltm, &conf.at(cur_conf).area.at(cur_area).last_post);
#else
        localtime_r(&conf.at(cur_conf).area.at(cur_area).last_post, &ltm);
#endif

        if (conf.at(cur_conf).area.at(cur_area).unread > 0) {
          n->print_f(" |10NEW |15");
        } else if (conf.at(cur_conf).area.at(cur_area).total == 0) {
          n->print_f(" |08    ");
        } else {
          n->print_f(" |07    ");
        }

        if (selected_conf == cur_conf && selected_area == cur_area) {
          n->print_f("%s", n->get_config()->get_prompt_colour());
          x = i;
          if (conf.at(cur_conf).area.at(cur_area).total == 0) {
            n->print_f(" %-32.32s %6d %6d No Messages %s %s\x1b[K", conf.at(cur_conf).area.at(cur_area).ma->get_name().c_str(),
                       conf.at(cur_conf).area.at(cur_area).total, conf.at(cur_conf).area.at(cur_area).unread,
                       conf.at(cur_conf).area.at(cur_area).subbed ? "Sub" : "   ", conf.at(cur_conf).area.at(cur_area).tagged ? "Tag" : "   ");
          } else {
            n->print_f(" %-32.32s %6d %6d %04d/%02d/%02d  %s %s\x1b[K", conf.at(cur_conf).area.at(cur_area).ma->get_name().c_str(),
                       conf.at(cur_conf).area.at(cur_area).total, conf.at(cur_conf).area.at(cur_area).unread, ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
                       conf.at(cur_conf).area.at(cur_area).subbed ? "Sub" : "   ", conf.at(cur_conf).area.at(cur_area).tagged ? "Tag" : "   ");
          }
          n->print_f("|16");
        } else {
          n->print_f("|16");
          if (conf.at(cur_conf).area.at(cur_area).total == 0) {
            n->print_f(" %-32.32s %6d %6d No Messages %s %s\x1b[K", conf.at(cur_conf).area.at(cur_area).ma->get_name().c_str(),
                       conf.at(cur_conf).area.at(cur_area).total, conf.at(cur_conf).area.at(cur_area).unread,
                       conf.at(cur_conf).area.at(cur_area).subbed ? "|09Sub" : "   ", conf.at(cur_conf).area.at(cur_area).tagged ? "|12Tag" : "   ");
          } else {
            n->print_f(" %-32.32s %6d %6d %04d/%02d/%02d  %s %s\x1b[K", conf.at(cur_conf).area.at(cur_area).ma->get_name().c_str(),
                       conf.at(cur_conf).area.at(cur_area).total, conf.at(cur_conf).area.at(cur_area).unread, ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
                       conf.at(cur_conf).area.at(cur_area).subbed ? "|09Sub" : "   ", conf.at(cur_conf).area.at(cur_area).tagged ? "|12Tag" : "   ");
          }
        }

        cur_area++;

        if (cur_area >= (int)conf.at(cur_conf).area.size()) {
          cur_conf++;
          i++;
          cur_area = 0;
          if (i == n->get_term_height() - 3)
            break;
          n->print_f("\x1b[%d;1H\x1b[K", i + 2);
        }

        if (cur_conf >= (int)conf.size()) {
          break;
        }
      }

      char c = n->getch();

      if (c == '\r' || c == '\n') {
        if (conf.at(selected_conf).area.at(selected_area).unread > 0) {

          int msgno = conf.at(selected_conf).area.at(selected_area).ma->umsgid_to_offset(conf.at(selected_conf).area.at(selected_area).lr + 1);
          while (true) {
            msgno = conf.at(selected_conf).area.at(selected_area).ma->list_messages(msgno);
            if (msgno > 0 && msgno <= conf.at(selected_conf).area.at(selected_area).ma->get_total_msgs()) {
              int last;
              conf.at(selected_conf).area.at(selected_area).ma->read_message(msgno, &last);
              msgno = last;
            } else {
              break;
            }
          }

          break;
        } else {
          int msgno = 1;
          while (true) {
            msgno = conf.at(selected_conf).area.at(selected_area).ma->list_messages(msgno);
            if (msgno > 0 && msgno <= conf.at(selected_conf).area.at(selected_area).ma->get_total_msgs()) {
              int last;
              conf.at(selected_conf).area.at(selected_area).ma->read_message(msgno, &last);
              msgno = last;
            } else {
              break;
            }
          }
          break;
        }
      } else if (c == ' ') {
        conf.at(selected_conf).area.at(selected_area).tagged = !conf.at(selected_conf).area.at(selected_area).tagged;
      } else if (c == 'S' || c == 's') {
        bool nothingtagged = true;
        for (size_t i = 0; i < conf.size(); i++) {
          for (size_t j = 0; j < conf.at(i).area.size(); j++) {
            if (conf.at(i).area.at(j).tagged) {
              n->get_user().set_subscribed(conf.at(i).area.at(j).ma->get_file(), true);
              conf.at(i).area.at(j).subbed = true;
              nothingtagged = false;
            }
          }
        }
        if (nothingtagged) {
          n->get_user().set_subscribed(conf.at(selected_conf).area.at(selected_area).ma->get_file(), true);
          conf.at(selected_conf).area.at(selected_area).subbed = true;
        }
      } else if (c == 'U' || c == 'u') {
        bool nothingtagged = true;
        for (size_t i = 0; i < conf.size(); i++) {
          for (size_t j = 0; j < conf.at(i).area.size(); j++) {
            if (conf.at(i).area.at(j).tagged) {
              n->get_user().set_subscribed(conf.at(i).area.at(j).ma->get_file(), false);
              conf.at(i).area.at(j).subbed = false;
              nothingtagged = false;
            }
          }
        }

        if (nothingtagged) {
          n->get_user().set_subscribed(conf.at(selected_conf).area.at(selected_area).ma->get_file(), false);
          conf.at(selected_conf).area.at(selected_area).subbed = false;
        }
      } else if (c == 'R' || c == 'r') {
        bool nothingtagged = true;
        for (size_t i = 0; i < conf.size(); i++) {
          for (size_t j = 0; j < conf.at(i).area.size(); j++) {
            if (conf.at(i).area.at(j).tagged) {
              n->get_user().user_set_lastread(conf.at(i).area.at(j).ma->get_file(), conf.at(i).area.at(j).total);
              conf.at(i).area.at(j).lr = conf.at(i).area.at(j).total;
              conf.at(i).area.at(j).unread = 0;
              nothingtagged = false;
            }
          }
        }
        if (nothingtagged) {
          n->get_user().user_set_lastread(conf.at(selected_conf).area.at(selected_area).ma->get_file(),
                                          conf.at(selected_conf).area.at(selected_area).top_msgid);
          conf.at(selected_conf).area.at(selected_area).lr = conf.at(selected_conf).area.at(selected_area).top_msgid;
          conf.at(selected_conf).area.at(selected_area).unread = 0;
        }
      } else if (c == 'C' || c == 'c') {
        for (size_t i = 0; i < conf.size(); i++) {
          for (size_t j = 0; j < conf.at(i).area.size(); j++) {
            conf.at(i).area.at(j).tagged = false;
          }
        }
      }

      else if (c == 'q' || c == 'Q') {
        return;
      } else if (c == 'p' || c == 'P') {
        n->cls();
        int tot_areas = 0;
        bool selected_is_tagged = true;
        bool tagged_netmail = false;
        for (size_t i = 0; i < conf.size(); i++) {
          for (size_t j = 0; j < conf.at(i).area.size(); j++) {
            if (conf.at(i).area.at(j).tagged) {
              tot_areas++;
              if (conf.at(i).area.at(j).ma->is_netmail()) {
                tagged_netmail = true;
              }
            }
          }
        }

        if (tot_areas > 0) {
          if (n->get_config()->get_sec_level_info(n->get_user().get_sec_level())->bulk_msg_allowed) {
            n->print_f("|12Warning! |15You have tagged %d areas to send this message on!\r\n", tot_areas);
            n->print_f("         Do you want to do that? (Y/N) : ");
            char ch = n->getch();
            if (ch == 'y' || ch == 'Y') {
              selected_is_tagged = false;
            }
          } else {
            selected_is_tagged = true;
          }
        }

        if (!selected_is_tagged && tagged_netmail) {
          n->print_f("\r\nSorry! One of your tagged bases is a netmail area. Aborting!\r\n");
          n->pause();
          break;
        }

        bool doabort = false;
        n->print_f("\r\n     |07To: ");
        std::string to = n->get_string(35, false);
        n->print_f("\r\nSubject: ");
        std::string subject = n->get_string(60, false);
        std::string netaddr;
        if (selected_is_tagged && conf.at(selected_conf).area.at(selected_area).ma->is_netmail()) {
          n->print_f("\r\nAddress: ");
          netaddr = n->get_string(16, false);
          if (conf.at(selected_conf).area.at(selected_area).ma->get_wwivnode() == 0) {
            NETADDR *na = parse_fido_addr(netaddr.c_str());

            if (na == NULL) {
              doabort = true;
            } else {
              if (na->point == 0) {
                n->print_f("\r\n\r\n|14 Sending to.. |15%d:%d/%d.%d (%s)", na->zone, na->net, na->node, na->point,
                           Nodelist::lookup_bbsname(n, std::to_string(na->zone) + ":" + std::to_string(na->net) + "/" + std::to_string(na->node)).c_str());
              } else {
                n->print_f("\r\n\r\n|14 Sending to.. |15%d:%d/%d.%d (A Point System)", na->zone, na->net, na->node, na->point);
              }
              free(na);
            }
          } else {
            try {
              int nn = stoi(netaddr);
              if (nn <= 0 || nn > 0xffff) {
                doabort = true;
              } else {
                n->print_f("\r\n\r\n|14 Sending to.. |15@%d", nn);
              }
            } catch (std::out_of_range const &) {
              doabort = true;
            } catch (std::invalid_argument const &) {
              doabort = true;
            }
          }
          if (to.size() == 0 || strcasecmp(to.c_str(), "ALL") == 0) {
            doabort = true;
          }
        } else {
          if (to.size() == 0) {
            to = "All";
          }
          netaddr = "";
        }

        if (doabort || subject.size() == 0) {
          n->print_f("\r\n|14Aborted!\r\n");
        } else {
          if (selected_is_tagged) {
            std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, conf.at(selected_conf).area.at(selected_area).ma->get_name(),
                                                                  conf.at(selected_conf).area.at(selected_area).ma->is_netmail(), nullptr);
            if (nmsg.size() > 0) {
              if (n->get_user().get_attribute("signature_enabled", "false") == "true") {
                MsgArea::attach_sig(&nmsg, n->get_user().get_attribute("signature", ""));
              }
              if (conf.at(selected_conf).area.at(selected_area).ma->get_real_names()) {
                conf.at(selected_conf)
                    .area.at(selected_area)
                    .ma->save_message(to, n->get_user().get_attribute("fullname", n->get_user().get_username()), subject, nmsg, netaddr, 0);
              } else {
                conf.at(selected_conf).area.at(selected_area).ma->save_message(to, n->get_user().get_username(), subject, nmsg, netaddr, 0);
              }
              n->clog->post_msg();
              n->get_user().inc_attrib("msgs_posted");
            }
          } else {
            std::vector<std::string> nmsg =
                Editor::enter_message(n, to, subject, "Multiple Areas", conf.at(selected_conf).area.at(selected_area).ma->is_netmail(), nullptr);
            if (nmsg.size() > 0) {
              if (n->get_user().get_attribute("signature_enabled", "false") == "true") {
                MsgArea::attach_sig(&nmsg, n->get_user().get_attribute("signature", ""));
              }
              for (size_t i = 0; i < conf.size(); i++) {
                for (size_t j = 0; j < conf.at(i).area.size(); j++) {
                  if (conf.at(i).area.at(j).tagged) {
                    if (conf.at(i).area.at(j).ma->get_real_names()) {
                      conf.at(i).area.at(j).ma->save_message(to, n->get_user().get_attribute("fullname", n->get_user().get_username()), subject, nmsg, netaddr,
                                                             0);
                    } else {
                      conf.at(i).area.at(j).ma->save_message(to, n->get_user().get_username(), subject, nmsg, netaddr, 0);
                    }
                    n->clog->post_msg();
                    n->get_user().inc_attrib("msgs_posted");
                  }
                }
              }
            }
          }
        }
        break;
      } else if (c == '?') {
        n->print_f("\x1b[%d;20H\x1b[0;30;47m+-----------[HELP]-----------+", (n->get_term_height() - 16) / 2);
        n->print_f("\x1b[%d;20H|                            |", ((n->get_term_height() - 16) / 2) + 1);
        n->print_f("\x1b[%d;20H|     UP/DOWN  Scroll Areas  |", ((n->get_term_height() - 16) / 2) + 2);
        n->print_f("\x1b[%d;20H|     ENTER    Read Area     |", ((n->get_term_height() - 16) / 2) + 3);
        n->print_f("\x1b[%d;20H|     SPACE    Tag Area      |", ((n->get_term_height() - 16) / 2) + 4);
        n->print_f("\x1b[%d;20H|     P        Post Message  |", ((n->get_term_height() - 16) / 2) + 5);
        n->print_f("\x1b[%d;20H|     Q        Quit          |", ((n->get_term_height() - 16) / 2) + 6);
        n->print_f("\x1b[%d;20H|     ?        Help          |", ((n->get_term_height() - 16) / 2) + 7);
        n->print_f("\x1b[%d;20H|                            |", ((n->get_term_height() - 16) / 2) + 8);
        n->print_f("\x1b[%d;20H|  With Tagged Areas:        |", ((n->get_term_height() - 16) / 2) + 9);
        n->print_f("\x1b[%d;20H|     C        Clear Tagged  |", ((n->get_term_height() - 16) / 2) + 10);
        n->print_f("\x1b[%d;20H|     S        Subscribe     |", ((n->get_term_height() - 16) / 2) + 11);
        n->print_f("\x1b[%d;20H|     U        Unsubscribe   |", ((n->get_term_height() - 16) / 2) + 12);
        n->print_f("\x1b[%d;20H|     R        Mark as Read  |", ((n->get_term_height() - 16) / 2) + 13);
        n->print_f("\x1b[%d;20H|                            |", ((n->get_term_height() - 16) / 2) + 14);
        n->print_f("\x1b[%d;20H+----------------------------+\x1b[0m", ((n->get_term_height() - 16) / 2) + 15);
        n->getch();
      } else if (c == '\x1b') {
        c = n->getch();
        if (c == '[') {
          c = n->getch();
          if (c == 'A') {
            // up
            if (selected_area > 0) {
              selected_area--;
              x--;
            } else {
              if (selected_conf > 0) {
                selected_conf--;
                selected_area = conf.at(selected_conf).area.size() - 1;
                x -= 3;
              }
            }
          } else if (c == 'B') {
            // down
            if (selected_area < (int)conf.at(selected_conf).area.size() - 1) {
              selected_area++;
              x++;
            } else {
              if (selected_conf < (int)conf.size() - 1) {
                selected_conf++;
                selected_area = 0;
                x += 3;
              }
            }
          } else if (c == 'V' || c == '5') {
            if (c == '5') {
              n->getch();
            }
            // page up
            for (size_t i = 0; i < n->get_term_height() - 4;) {
              if (selected_area > 0) {
                selected_area--;
                i++;
              } else {
                if (selected_conf > 0 && (int)i + 3 < (int)n->get_term_height() - 4) {
                  selected_conf--;
                  selected_area = conf.at(selected_conf).area.size() - 1;
                  i += 3;
                } else {
                  break;
                }
              }
            }
            start_area = selected_area;
            start_conf = selected_conf;
            continue;
          } else if (c == 'U' || c == '6') {
            if (c == '6') {
              n->getch();
            }
            // page down
            size_t i = 0;
            for (; i < n->get_term_height() - 4;) {
              if (selected_area < (int)conf.at(selected_conf).area.size() - 1) {
                selected_area++;
                i++;
              } else {
                if (selected_conf < (int)conf.size() - 1 && (int)i + 3 < (int)n->get_term_height() - 4) {
                  selected_conf++;
                  selected_area = 0;
                  i += 3;
                } else {
                  break;
                }
              }
            }

            start_area = selected_area;
            start_conf = selected_conf;
            // adjust start_area if less than a page to display

            int s_conf = selected_conf;
            int s_area = selected_area;

            int counter = 0;

            for (; s_conf < (int)conf.size(); s_conf++) {
              for (; s_area < (int)conf.at(s_conf).area.size(); s_area++) {
                counter++;
              }
              if (s_conf < (int)conf.size() - 1) {
                counter += 3;
              }
              s_area = 0;
            }

            if (counter < (int)n->get_term_height() - 3) {
              for (size_t j = 0; j < n->get_term_height() - 3 - counter;) {
                if (start_area > 0) {
                  start_area--;
                  j++;
                } else {
                  if (start_conf > 0) {
                    start_conf--;
                    start_area = conf.at(start_conf).area.size() - 1;
                    j += 3;
                  } else {
                    break;
                  }
                }
              }
            }
            continue;
          }
        }
      }

      // recalculate the start
      while (x < 0) {
        // scroll up
        if (start_area > 0) {
          start_area--;
          x++;
        } else {
          if (start_conf > 0) {
            start_conf--;
            start_area = conf.at(start_conf).area.size() - 1;
            x += 3;
          }
        }
      }

      while (x >= (int)n->get_term_height() - 3) {
        // scroll down
        if (start_area < (int)conf.at(start_conf).area.size() - 1) {
          start_area++;
          x--;
        } else {
          if (start_conf < (int)conf.size() - 1) {
            start_conf++;
            start_area = 0;
            x -= 3;
          }
        }
      }
    }
  }
}
