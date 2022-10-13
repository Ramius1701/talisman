#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#define strcasecmp stricmp
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <string>
#include <iostream>
#include <sstream>
#include <ctime>
#include "Request.h"
#include "MsgGroup.h"
#include "../Common/INIReader.h"
#include "../Common/Logger.h"
#include "../Common/toml.hpp"
#include "../Common/Squish.h"

static std::string find_kludge(sq_msg_t *msg, std::string kludge) {
  std::stringstream ret;

  for (size_t z = 0; z < msg->ctrl_len; z++) {
    if (msg->ctrl[z] == '\001') {
      if (ret.str().size() > 0) {
        if (ret.str().find(kludge + ": ") == 0) {
          return ret.str().substr(kludge.size() + 2);
        }
      }
      ret.str("");
      continue;
    }
    ret << msg->ctrl[z];
  }
  if (ret.str().size() > 0) {
    if (ret.str().find(kludge + ": ") == 0) {
      return ret.str().substr(kludge.size() + 2);
    }
  }
  return "";
}

Request::Request() {
  authenticated = false;
  log = new Logger();
  selected_group = -1;
  selected_article = -1;
}

bool Request::loadconfig() {
  INIReader inir("talisman.ini");

  if (inir.ParseError() != 0) {
    std::cerr << "Unable to parse talisman.ini!" << std::endl;
    return false;
  }

  std::string datapath = inir.Get("Paths", "Data Path", "data");
  msgpath = inir.Get("Paths", "Message Path", "msgs");
  std::string logpath = inir.Get("Paths", "Log Path", "logs");
  hostname = inir.Get("main", "hostname", "localhost");
  log->load(logpath + "/newssrv.log");

  try {
    auto data = toml::parse_file(datapath + "/newssrv.toml");

    auto groupitems = data.get_as<toml::array>("msggroup");

    for (size_t i = 0; i < groupitems->size(); i++) {
      auto itemtable = groupitems->get(i)->as_table();

      std::string mynntpname;
      std::string mysquishfile;
      std::string mydesc;
      bool myreadonly;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        mynntpname = name->as_string()->value_or("");
      } else {
        mynntpname = "";
      }
      auto file = itemtable->get("file");
      if (file != nullptr) {
        mysquishfile = file->as_string()->value_or("");
      } else {
        mysquishfile = "";
      }

      auto readonly = itemtable->get("readonly");
      if (readonly != nullptr) {
        myreadonly = readonly->as_boolean()->value_or(false);
      } else {
        myreadonly = false;
      }

      auto desc = itemtable->get("description");
      if (desc != nullptr) {
        mydesc = desc->as_string()->value_or(mynntpname);
      } else {
        mydesc = mynntpname;
      }

      if (mynntpname != "" && mysquishfile != "") {

        MsgGroup *g = new MsgGroup(mynntpname, mysquishfile, mydesc, myreadonly);

        groups.push_back(g);
      }
    }


  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/newssrv.toml, Line %d, Column %d", datapath.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    return false;
  }
  return true;
}

bool Request::dorequest(int socket, std::string request) {
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

  this->socket = socket;

  log->log(LOG_INFO, "Got request \"%s\"", request.c_str());



  if (request == "QUIT") {
    std::string quit_response = "205 Goodbye\r\n";
    send(socket, quit_response.c_str(), quit_response.size(), 0);
    return false;
  } else if (request.substr(0, 4) == "LIST") {
    if (request.size() > 5) {
      dolist(request.substr(5));
    } else {
      dolist("");
    }
    return true;
  } else if (request.substr(0, 5) == "XOVER") {
      int start = -1;
      int end = -1;
      std::string eol = ".\r\n";

      if (request.size() > 6) {
        if (request.substr(6).find("-") != std::string::npos) {
          if (request.substr(request.find("-") + 1).size() == 0) {
            end = -1;
          } else {
            try {
              end = std::stoi(request.substr(request.find("-") + 1));
            } catch (std::invalid_argument const &) {

            } catch (std::out_of_range const &) {

            }
          }

          try {
            start = std::stoi(request.substr(6, request.find("-") - 6));
          } catch (std::invalid_argument const &) {

          } catch (std::out_of_range const &) {

          }
        } else {
          try {
            start = std::stoi(request.substr(6));
            end = start;
          } catch (std::invalid_argument const &) {

          } catch (std::out_of_range const &) {

          }
        }
      } else {
        start = selected_article;
        end = selected_article;
      }

      sq_msg_base_t *mb = SquishOpenMsgBase(std::string(msgpath + "/" + groups.at(selected_group)->msgbase).c_str());

      if (!mb) {
        return true;
      }

      std::string resp = "224 Overview information follows\r\n";
      send(socket, resp.c_str(), resp.size(), 0);


      for (size_t i = 1; i <= mb->basehdr.num_msg; i++) {
        sq_msg_t *msg = SquishReadMsg(mb, i);
        if (!msg) {
          continue;
        }
        bool showmsg = false;
        if (end != -1) {
          if (msg->xmsg.umsgid >= start && msg->xmsg.umsgid <= end) {
            showmsg = true;
          }
        } else {
          if (msg->xmsg.umsgid >= start) {
            showmsg = true;
          }
        }

        if (showmsg) {
          struct tm msg_tm;
          memset(&msg_tm, 0, sizeof(struct tm));
          msg_tm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 80;
          msg_tm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
          msg_tm.tm_mday = msg->xmsg.date_written.date & 31;
          msg_tm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
          msg_tm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
          msg_tm.tm_sec = ((msg->xmsg.date_written.time) & 31) * 2;
          msg_tm.tm_isdst = -1;


          mktime(&msg_tm);

          char datestr[36];
          std::string tzutc = find_kludge(msg, "TZUTC");

          if (tzutc.size() == 4 && tzutc.at(0) != '-') {
            std::stringstream ss;
            ss << "+" << tzutc.at(0) << tzutc.at(1) << tzutc.at(2) << tzutc.at(3);
            tzutc = ss.str();

          } else if (tzutc.size() == 5) {
            std::stringstream ss;
            ss << "-" << tzutc.at(1) << tzutc.at(2) << tzutc.at(3) << tzutc.at(4);
            tzutc = ss.str();
          } else {
            tzutc = "+0000";
          }
          snprintf(datestr, 36, "%s, %d %s %d %02d:%02d:%02d %s", days[msg_tm.tm_wday], msg_tm.tm_mday, months[msg_tm.tm_mon], msg_tm.tm_year + 1900, msg_tm.tm_hour, msg_tm.tm_min, msg_tm.tm_sec, tzutc.c_str());


          std::stringstream ss;
          ss << msg->xmsg.umsgid << "\t" << msg->xmsg.subject << "\t" << msg->xmsg.from << "\t" << datestr << "\t" << msgid(msg->xmsg.umsgid, groups.at(selected_group)->newsgrp) << "\t" << (msg->xmsg.replyto == 0 ? "" : msgid(msg->xmsg.replyto, groups.at(selected_group)->newsgrp)) << "\t\t\t\r\n";

          send(socket, ss.str().c_str(), ss.str().size(), 0);

        }
        SquishFreeMsg(msg);
      }
      SquishCloseMsgBase(mb);
      send(socket, eol.c_str(), eol.size(), 0);
      return true;
  } else if (request.substr(0, 7) == "ARTICLE") {
      if (request.size() > 8) {
        if (request.at(8) == '<') {
          // msgid
          if (request.substr(9).find(".") != std::string::npos && request.substr(9).find("@") != std::string::npos) {
            try {
              int article = std::stoi(request.substr(9, request.substr(9).find(".")));
              std::string group = request.substr(request.substr(9).find(".") + 1, request.substr(9).find("@"));
              for (size_t i = 0; i < groups.size(); i++) {
                if (groups.at(i)->newsgrp == group) {
                  doarticle(i, article, true);
                  return true;
                }
              }
            } catch (std::invalid_argument const &) {

            } catch (std::out_of_range const &) {

            }
          }
        } else {
          try {
            int article = std::stoi(request.substr(8));

            doarticle(selected_group, article, false);

            return true;
          } catch (std::invalid_argument const &) {

          } catch (std::out_of_range const &) {

          }
        }
      } else {
        doarticle(selected_group, selected_article, false);
        return true;
      }
  } else if (request.substr(0, 4) == "HEAD") {
      if (request.size() > 5) {
        if (request.at(5) == '<') {
          // msgid
          if (request.substr(6).find(".") != std::string::npos && request.substr(6).find("@") != std::string::npos) {
            try {
              int article = std::stoi(request.substr(6, request.substr(6).find(".")));
              std::string group = request.substr(request.substr(6).find(".") + 1, request.substr(6).find("@"));
              for (size_t i = 0; i < groups.size(); i++) {
                if (groups.at(i)->newsgrp == group) {
                  dohead(i, article, true);
                  return true;
                }
              }
            } catch (std::invalid_argument const &) {

            } catch (std::out_of_range const &) {

            }
          }
        } else {
          try {
            int article = std::stoi(request.substr(5));

            dohead(selected_group, article, false);

            return true;
          } catch (std::invalid_argument const &) {

          } catch (std::out_of_range const &) {

          }
        }
      } else {
        dohead(selected_group, selected_article, false);
        return true;
      }
  } else if (request.substr(0, 5) == "GROUP") {
    if (request.size() > 6) {
      for(size_t i = 0; i < groups.size();i++) {
        if (groups.at(i)->newsgrp == request.substr(6)) {
          selected_group = i;

          std::stringstream ss;
          sq_msg_base_t *mb = SquishOpenMsgBase(std::string(msgpath + "/" + groups.at(selected_group)->msgbase).c_str());

          if (!mb) {
            continue;
          }

          sq_msg_t *msg;
          int low = 1;
          int high = 0;
          int tot = mb->basehdr.num_msg;
          msg = SquishReadMsg(mb, 1);

          if (msg) {
            low = msg->xmsg.umsgid;
            SquishFreeMsg(msg);
          }

          msg = SquishReadMsg(mb, mb->basehdr.num_msg);
          if (msg) {
            high = msg->xmsg.umsgid;
            SquishFreeMsg(msg);
          }
          SquishCloseMsgBase(mb);
          ss << "211 " << tot << " " << low << " " << high << " " << groups.at(selected_group)->newsgrp << "\r\n";
          send(socket, ss.str().c_str(), ss.str().size(), 0);
          return true;
        }
      }
      // group not found
      std::string err = "411 No such newsgroup\r\n";
      send(socket, err.c_str(), err.size(), 0);

      return true;
    } else {
      std::string err = "501 No group specified\r\n";
      send(socket, err.c_str(), err.size(), 0);

      return true;
    }
  }


  std::string err_response = "500 Unknown Command\r\n";

  send(socket, err_response.c_str(), err_response.size(), 0);

  return true;
}

void Request::dolist(std::string argument) {
  std::string eol = ".\r\n";
  std::stringstream line;

  bool listactive = false;

  if (argument != "") {
    if (strcasecmp(argument.c_str(), "active") == 0) {
      listactive = true;
    } else if (strcasecmp(argument.substr(0, 10).c_str(), "newsgroups") == 0) {
      if (argument.size() > 10) {
        line.str("501 Patterns not supported for LIST NEWSGROUPS\r\n");
        send(socket, line.str().c_str(), line.str().size(), 0);
        return;
      } else {
        line.str("215 List of newsgroups follows\r\n");
        send(socket, line.str().c_str(), line.str().size(), 0);
        for (size_t i = 0; i < groups.size(); i++) {
          line.str("");
          line << groups.at(i)->newsgrp << "\t" << groups.at(i)->desc << "\r\n";
          send(socket, line.str().c_str(), line.str().size(), 0);
        }
      }
    } else if (strcasecmp(argument.c_str(), "overview.fmt") == 0) {
      line.str("215 List of fields in XOVER result\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
      line.str("Subject:\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
      line.str("From:\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
      line.str("Date:\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
      line.str("Message-ID:\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
      line.str("References:\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
      line.str("Bytes:\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
      line.str("Lines:\r\n");
      send(socket, line.str().c_str(), line.str().size(), 0);
    }
  } else {
    listactive = true;
  }

  if (listactive) {
    line.str("215 List of newsgroups follows\r\n");
    send(socket, line.str().c_str(), line.str().size(), 0);
    for (size_t i = 0; i < groups.size(); i++) {
      line.str("");

      sq_msg_base_t *mb = SquishOpenMsgBase(std::string(msgpath + "/" + groups.at(i)->msgbase).c_str());

      if (!mb) {
        continue;
      }

      sq_msg_t *msg;
      int low = 1;
      int high = 0;

      msg = SquishReadMsg(mb, 1);

      if (msg) {
        low = msg->xmsg.umsgid;
        SquishFreeMsg(msg);
      }

      msg = SquishReadMsg(mb, mb->basehdr.num_msg);
      if (msg) {
        high = msg->xmsg.umsgid;
        SquishFreeMsg(msg);
      }
      SquishCloseMsgBase(mb);

      if (groups.at(i)->readonly || !authenticated) {
        line << groups.at(i)->newsgrp << " " << high << " " << low << " " <<  "n\r\n";
      } else {
        line << groups.at(i)->newsgrp << " " << high << " " << low << " " << "y\r\n";
      }
      send(socket, line.str().c_str(), line.str().size(), 0);
    }
  }

  send(socket, eol.c_str(), eol.size(), 0);
}

std::string Request::msgid(int mid, std::string group) {
  std::stringstream ss;

  ss << "<" << mid << "." << group << "@" << hostname << ">";

  return ss.str();
}

void Request::dohead(int grp, int article, bool byid) {
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  sq_msg_base_t *mb = SquishOpenMsgBase(std::string(msgpath + "/" + groups.at(grp)->msgbase).c_str());

  if (!mb) {
    return;
  }

  sq_msg_t *msg = SquishReadMsg(mb, article);

  if (!msg) {
    SquishCloseMsgBase(mb);
    return;
  }

  std::stringstream ss;

  ss << "221 " << (byid ? 0 : article) << " " << msgid(article, groups.at(grp)->newsgrp) << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "Path: Talisman!not-for-mail\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "From: " << msg->xmsg.from << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "X-Comment-To: " << msg->xmsg.to << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "To: " << msg->xmsg.to << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "Newsgroups: " << groups.at(grp)->newsgrp << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "Subject: " << msg->xmsg.subject << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);


  struct tm msg_tm;
  memset(&msg_tm, 0, sizeof(struct tm));
  msg_tm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 80;
  msg_tm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
  msg_tm.tm_mday = msg->xmsg.date_written.date & 31;
  msg_tm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
  msg_tm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
  msg_tm.tm_sec = ((msg->xmsg.date_written.time) & 31) * 2;
  msg_tm.tm_isdst = -1;

  mktime(&msg_tm);

  char datestr[36];

  std::string tzutc = find_kludge(msg, "TZUTC");

  if (tzutc.size() == 4 && tzutc.at(0) != '-') {
    std::stringstream ss;
    ss << "+" << tzutc.at(0) << tzutc.at(1) << tzutc.at(2) << tzutc.at(3);
    tzutc = ss.str();

  } else if (tzutc.size() == 5) {
    std::stringstream ss;
    ss << "-" << tzutc.at(1) << tzutc.at(2) << tzutc.at(3) << tzutc.at(4);
    tzutc = ss.str();
  } else {
    tzutc = "+0000";
  }

  snprintf(datestr, 36, "%s, %d %s %d %02d:%02d:%02d %s", days[msg_tm.tm_wday], msg_tm.tm_mday, months[msg_tm.tm_mon], msg_tm.tm_year + 1900, msg_tm.tm_hour, msg_tm.tm_min, msg_tm.tm_sec, tzutc.c_str());

  ss.str("");
  ss << "Date: " << datestr << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);
  ss.str("");
  ss << "Message-ID: " << msgid(article, groups.at(grp)->newsgrp) << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << ".\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  SquishFreeMsg(msg);
  SquishCloseMsgBase(mb);
}

void Request::doarticle(int grp, int article, bool byid) {
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  sq_msg_base_t *mb = SquishOpenMsgBase(std::string(msgpath + "/" + groups.at(grp)->msgbase).c_str());

  if (!mb) {
    return;
  }

  sq_msg_t *msg = SquishReadMsg(mb, article);

  if (!msg) {
    SquishCloseMsgBase(mb);
    return;
  }

  std::stringstream ss;

  ss << "220 " << (byid ? 0 : article) << " " << msgid(article, groups.at(grp)->newsgrp) << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "Path: Talisman!not-for-mail\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "From: " << msg->xmsg.from << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "X-Comment-To: " << msg->xmsg.to << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "To: " << msg->xmsg.to << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "Newsgroups: " << groups.at(grp)->newsgrp << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "Subject: " << msg->xmsg.subject << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);


  struct tm msg_tm;
  memset(&msg_tm, 0, sizeof(struct tm));
  msg_tm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 80;
  msg_tm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
  msg_tm.tm_mday = msg->xmsg.date_written.date & 31;
  msg_tm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
  msg_tm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
  msg_tm.tm_sec = ((msg->xmsg.date_written.time) & 31) * 2;
  msg_tm.tm_isdst = -1;
  mktime(&msg_tm);

  char datestr[36];

  std::string tzutc = find_kludge(msg, "TZUTC");

  if (tzutc.size() == 4 && tzutc.at(0) != '-') {
    std::stringstream ss;
    ss << "+" << tzutc.at(0) << tzutc.at(1) << tzutc.at(2) << tzutc.at(3);
    tzutc = ss.str();
  } else if (tzutc.size() == 5) {
    std::stringstream ss;
    ss << "-" << tzutc.at(1) << tzutc.at(2) << tzutc.at(3) << tzutc.at(4);
    tzutc = ss.str();
  } else {
    tzutc = "+0000";
  }

  snprintf(datestr, 36, "%s, %d %s %d %02d:%02d:%02d %s", days[msg_tm.tm_wday], msg_tm.tm_mday, months[msg_tm.tm_mon], msg_tm.tm_year + 1900, msg_tm.tm_hour, msg_tm.tm_min, msg_tm.tm_sec, tzutc.c_str());

  ss.str("");
  ss << "Date: " << datestr << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);
  ss.str("");
  ss << "Message-ID: " << msgid(article, groups.at(grp)->newsgrp) << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");
  ss << "\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  ss.str("");

  std::vector<std::string> lines;

  for (int i = 0; i < msg->msg_len; i++) {
    if (msg->msg[i] == '\r') {
      if (ss.str() == ".") {
        ss << ".";
      }
      lines.push_back(ss.str());
      ss.str("");
    } else {
      ss << msg->msg[i];
    }
  }

  if (ss.str().size() > 0) {
      if (ss.str() == ".") {
        ss << ".";
      }
      lines.push_back(ss.str());
      ss.str("");
  }

  size_t end = lines.size();

  for (size_t i = lines.size() - 1; i >= 0; i--) {
    if (lines.at(i).substr(0, 9) == "SEEN-BY: ") {
      end--;
    } else if (lines.at(i).substr(0, 7) == "\001PATH: ") {
      end--;
    } else {
      break;
    }
  }

  for (size_t i = 0; i < end; i++) {
    send(socket, std::string(lines.at(i) + "\r\n").c_str(), lines.at(i).size() + 2, 0);
  }

  ss.str("");
  ss << ".\r\n";
  send(socket, ss.str().c_str(), ss.str().size(), 0);

  SquishFreeMsg(msg);
  SquishCloseMsgBase(mb);
}
