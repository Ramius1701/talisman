#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/endian.h>
#else
#include <endian.h>
#endif
#endif
#include "../Common/INIReader.h"
#include "../Common/Logger.h"
#include "../Common/toml.hpp"
#include "../Common/tendian.h"
#include "Archiver.h"
#include "Config.h"
#include "Dupe.h"
#include "PacketStructs.h"
#include "Scanner.h"
#include "Tosser.h"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

extern void sig_handler(int signal);

static inline uint16_t host2le_s(uint16_t s) {
#if defined(__LITTLE_ENDIAN__)
  return s;
#else
  return (((s >> 8) & 0xffu) | ((s & 0xffu) << 8));
#endif
}

static inline uint32_t host2le_l(uint32_t s) {
#if defined(__LITTLE_ENDIAN__)
  return s;
#else
  return (((s & 0xff000000u) >> 24) | ((s & 0x00ff0000u) >> 8) | ((s & 0x0000ff00u) << 8) | ((s & 0x000000ffu) << 24));
#endif
}

static inline void convert_phdr(struct packet_t *pkt) {
#if defined(__LITTLE_ENDIAN__)
  return;
#else
  pkt->orignode = host2le_s(pkt->orignode);
  pkt->destnode = host2le_s(pkt->destnode);
  pkt->year = host2le_s(pkt->year);
  pkt->month = host2le_s(pkt->month);
  pkt->day = host2le_s(pkt->day);
  pkt->hour = host2le_s(pkt->hour);
  pkt->minute = host2le_s(pkt->minute);
  pkt->second = host2le_s(pkt->second);
  pkt->baud = host2le_s(pkt->baud);
  pkt->version = host2le_s(pkt->version);
  pkt->origNet = host2le_s(pkt->origNet);
  pkt->destNet = host2le_s(pkt->destNet);
  pkt->origZone = host2le_s(pkt->origZone);
  pkt->destZone = host2le_s(pkt->destZone);
  pkt->auxNet = host2le_s(pkt->auxNet);
  pkt->capValid = host2le_s(pkt->capValid);
  pkt->capWord = host2le_s(pkt->capWord);
  pkt->origZone2 = host2le_s(pkt->origZone2);
  pkt->destZone2 = host2le_s(pkt->destZone2);
  pkt->origPoint = host2le_s(pkt->origPoint);
  pkt->destPoint = host2le_s(pkt->destPoint);
  pkt->prodData = host2le_l(pkt->prodData);
#endif
}

static inline void convert_pmsghdr(struct packed_message_t *msg) {
#if defined(__LITTLE_ENDIAN__)
  return;
#else
  msg->message_type = host2le_s(msg->message_type);
  msg->orig_node = host2le_s(msg->orig_node);
  msg->dest_node = host2le_s(msg->dest_node);
  msg->orig_net = host2le_s(msg->orig_net);
  msg->dest_net = host2le_s(msg->dest_net);
  msg->attribute = host2le_s(msg->attribute);
  msg->cost = host2le_s(msg->cost);
#endif
}

bool Tosser::update(std::string tag, std::string links, bool filearea) {
  auto config = toml::parse_file(_datapath + "/postie.toml");

  auto areaitems = config.get_as<toml::array>((filearea ? "filearea" : "area"));
  if (areaitems != nullptr) {

    for (size_t k = 0; k < areaitems->size(); k++) {
      auto itemtable = areaitems->get(k)->as_table();

      auto areatag = itemtable->get("tag");

      std::string mytag;

      if (areatag != nullptr) {
        mytag = areatag->as_string()->value_or("");
      } else {
        mytag = "";
      }

      if (mytag == tag) {
        // std::stringstream ss2;

        itemtable->insert_or_assign("links", links);

        std::ofstream file(_datapath + "/postie.toml", std::ios_base::trunc);

        file << config;
        file.close();

        return true;
      }
    }
  }

  return false;
}

void Tosser::areafix(Config *c, sq_msg_t *msg) {
  link_conf_t *link = NULL;
  bool showhelp = false;

  for (size_t i = 0; i < c->links.size(); i++) {
    if (c->links.at(i).aka->zone == msg->xmsg.orig.zone && c->links.at(i).aka->net == msg->xmsg.orig.net && c->links.at(i).aka->node == msg->xmsg.orig.node &&
        c->links.at(i).aka->point == msg->xmsg.orig.point) {
      link = &c->links.at(i);
      break;
    }
  }

  if (link == NULL) {
    log.log(LOG_INFO, "Got areafix message from someone not defined in links! (%d:%d/%d.%d)", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node,
            msg->xmsg.orig.point);
    return;
  }

  // first check password
  if (link->areafixpwd != "" && strncasecmp(link->areafixpwd.c_str(), msg->xmsg.subject, link->areafixpwd.size()) == 0) {
    // password is good.
    std::vector<std::string> smsg;
    std::stringstream ss;
    for (size_t i = 0; i < (size_t)msg->msg_len; i++) {
      if (msg->msg[i] == '\r') {
        smsg.push_back(ss.str());
        ss.str("");
      } else if (msg->msg[i] != '\n') {
        ss << msg->msg[i];
      }
    }
    if (ss.str().size() > 0) {
      smsg.push_back(ss.str());
    }

    std::vector<std::string> msgout;

    for (std::string line : smsg) {
      if (line.size() > 0) {
        if (line.at(0) == '-') {
          // remove area
          bool success = false;

          for (size_t i = 0; i < c->areas.size(); i++) {
            if (strcasecmp(c->areas.at(i).areatag.c_str(), line.substr(1).c_str()) == 0 &&
                link->allowedgroups.find(c->areas.at(i).group) != std::string::npos) {
              std::stringstream ss2;
              for (size_t j = 0; j < c->areas.at(i).links.size(); j++) {
                if (c->areas.at(i).links.at(j) == link) {
                  success = true;
                } else {
                  ss2 << c->areas.at(i).links.at(j)->aka->zone << ":" << c->areas.at(i).links.at(j)->aka->net << "/" << c->areas.at(i).links.at(j)->aka->node
                      << "." << c->areas.at(i).links.at(j)->aka->point << ",";
                }
              }

              if (success == true) {
                if (ss2.str().size() > 1) {
                  success = update(c->areas.at(i).areatag, ss2.str().substr(0, ss2.str().size() - 1), false);
                } else {
                  success = update(c->areas.at(i).areatag, ss2.str(), false);
                }
              }
              if (success) {
                msgout.push_back("You have been removed from " + c->areas.at(i).areatag + " successfully.");
                log.log(LOG_INFO, "Successfully removed %d:%d/%d.%d from %s", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node,
                        msg->xmsg.orig.point, line.substr(1).c_str());
              } else {
                msgout.push_back("You have NOT been removed from " + c->areas.at(i).areatag);
              }
              break;
            }
          }

        } else if (line.at(0) == '%') {
          if (strcasecmp(line.substr(1).c_str(), "LIST") == 0) {
            msgout.push_back("Areas you have access to:");
            msgout.push_back("");
            for (size_t i = 0; i < c->areas.size(); i++) {
              if (link->allowedgroups.find(c->areas.at(i).group) != std::string::npos) {
                msgout.push_back(c->areas.at(i).areatag);
              }
            }
          } else if (strcasecmp(line.substr(1).c_str(), "HELP") == 0) {
            showhelp = true;
          } else if (strncasecmp(line.substr(1).c_str(), "RESCAN", 6) == 0) {
            if (line.size() > 8) {
              std::string area = line.substr(8);
              bool success = false;

              for (size_t i = 0; i < c->areas.size(); i++) {
                if (strcasecmp(c->areas.at(i).areatag.c_str(), area.c_str()) == 0 && link->allowedgroups.find(c->areas.at(i).group) != std::string::npos) {
                  sq_msg_base_t *mb;
                  int count = 0;
                  mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c->areas.at(i).file).c_str());
                  if (mb != NULL) {
                    sq_dword start = 1;

                    if (mb->basehdr.num_msg > 100) {
                      start = mb->basehdr.num_msg - 100;
                    }

                    for (sq_dword m = start; m < mb->basehdr.num_msg; m++) {
                      sq_msg_t *msg = SquishReadMsg(mb, m);
                      if (msg != NULL) {
                        if (link->fptr == NULL) {
                          Scanner::initialize_packet(link, std::string(_tmppath + "/postie-" + std::to_string(pid)), link->ouraka);
                        }
                        // write message
                        if (msg->xmsg.attr & MSGLOCAL) {
                          Scanner::write_msg_to_pkt(&c->areas.at(i), link, msg, true);
                        } else {
                          Scanner::write_msg_to_pkt(&c->areas.at(i), link, msg, false);
                        }
                        count++;
                        SquishFreeMsg(msg);
                      }
                    }
                    SquishCloseMsgBase(mb);
                    msgout.push_back("RESCAN " + area + ": Sent " + std::to_string(count) + " Messages");
                    success = true;
                  }
                  break;
                }
              }
              if (!success) {
                msgout.push_back("RESCAN " + area + ": Not successful!");
              }
            } else {
              msgout.push_back("RESCAN requires an area as an argument!");
            }
          }
        } else {
          // add area
          bool success = false;

          for (size_t i = 0; i < c->areas.size(); i++) {
            if (line.at(0) == '+') {
              line = line.substr(1);
            }
            if (strcasecmp(c->areas.at(i).areatag.c_str(), line.c_str()) == 0 && link->allowedgroups.find(c->areas.at(i).group) != std::string::npos) {
              std::stringstream ss2;
              for (size_t j = 0; j < c->areas.at(i).links.size(); j++) {
                if (c->areas.at(i).links.at(j) == link) {
                  success = false;
                  break;
                } else {
                  ss2 << c->areas.at(i).links.at(j)->aka->zone << ":" << c->areas.at(i).links.at(j)->aka->net << "/" << c->areas.at(i).links.at(j)->aka->node
                      << "." << c->areas.at(i).links.at(j)->aka->point << ",";
                }
              }

              if (ss2.str().size() > 1 && success == true) {
                ss2 << "," << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
                success = update(c->areas.at(i).areatag, ss2.str(), false);
              } else {
                ss2 << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
                success = update(c->areas.at(i).areatag, ss2.str(), false);
              }
              if (success) {
                msgout.push_back("You have been added to " + c->areas.at(i).areatag + " successfully.");
                log.log(LOG_INFO, "Successfully added %d:%d/%d.%d to %s", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point,
                        line.c_str());
              } else {
                msgout.push_back("You have NOT been added to " + c->areas.at(i).areatag);
              }
              break;
            }
          }
        }
      }
    }

    sq_msg_t sqmsg;

    memset(&sqmsg, 0, sizeof(sq_msg_t));

    sqmsg.xmsg.orig.zone = link->ouraka->zone;
    sqmsg.xmsg.orig.net = link->ouraka->net;
    sqmsg.xmsg.orig.point = link->ouraka->point;
    sqmsg.xmsg.orig.node = link->ouraka->node;

    sqmsg.xmsg.dest.zone = link->aka->zone;
    sqmsg.xmsg.dest.net = link->aka->net;
    sqmsg.xmsg.dest.point = link->aka->point;
    sqmsg.xmsg.dest.node = link->aka->node;

    strncpy(sqmsg.xmsg.subject, "AREAFIX Response", 71);
    strncpy(sqmsg.xmsg.to, msg->xmsg.from, 35);
    strncpy(sqmsg.xmsg.from, "AREAFIX", 35);

    std::tm at;

    time_t now = time(NULL);
#ifdef _MSC_VER
    localtime_s(&at, &now);
#else
    localtime_r(&now, &at);
#endif
    sqmsg.xmsg.date_written.date |= (((sq_word)at.tm_mday) & 31);
    sqmsg.xmsg.date_written.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
    sqmsg.xmsg.date_written.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

    sqmsg.xmsg.date_written.time |= (((sq_word)at.tm_sec) & 31);
    sqmsg.xmsg.date_written.time |= (((sq_word)at.tm_min) & 63) << 5;
    sqmsg.xmsg.date_written.time |= (((sq_word)at.tm_hour) & 31) << 11;

    sqmsg.xmsg.attr = MSGUID | MSGPRIVATE;

    std::stringstream ctrlstr;

    ctrlstr << "\x01INTL " << link->aka->zone << ":" << link->aka->net << "/" << link->aka->node << " " << link->ouraka->zone << ":" << link->ouraka->net << "/"
            << link->ouraka->node << "\r";
    if (link->aka->point > 0) {
      ctrlstr << "\x01TOPT " << link->aka->point << "\r";
    }
    if (link->ouraka->point > 0) {
      ctrlstr << "\001FMPT " << link->ouraka->point << "\r";
    }

    sqmsg.ctrl = (char *)malloc(ctrlstr.str().size());
    if (!sqmsg.ctrl) {
      return;
    }
    memcpy(sqmsg.ctrl, ctrlstr.str().c_str(), ctrlstr.str().size());
    sqmsg.ctrl_len = ctrlstr.str().size();

    std::stringstream msgstr;

    if (msgout.size() > 0) {
      // send msgout

      for (size_t i = 0; i < msgout.size(); i++) {
        msgstr << msgout.at(i) << "\r";
      }
    }
    if (showhelp) {
      // send help text
      msgstr << "\r\r----------------------------------------------------------\r";
      msgstr << "AREAFIX HELP!\r";
      msgstr << "----------------------------------------------------------\r";
      msgstr << "+SOMEAREA\r\r";
      msgstr << "This will add you to SOMEAREA\r\r";
      msgstr << "-SOMEAREA\r\r";
      msgstr << "This will remove you from SOMEAREA\r\r";
      msgstr << "%LIST\r\r";
      msgstr << "This will give you a list of everything available\r\r";
      msgstr << "%RESCAN AREA_TAG\r\r";
      msgstr << "(Re)send up to the last 100 messages in this base.\r\r";
      msgstr << "%HELP\r\r";
      msgstr << "This will show you this help\r\r";
      msgstr << "----------------------------------------------------------\r";
    }

    sqmsg.msg = (char *)malloc(msgstr.str().size());
    if (!sqmsg.msg) {
      free(sqmsg.ctrl);
      return;
    }
    memcpy(sqmsg.msg, msgstr.str().c_str(), msgstr.str().size());
    sqmsg.msg_len = msgstr.str().size();

    if (link->fptr == NULL) {
      Scanner::initialize_packet(link, tempdir.u8string(), link->ouraka);
    }
    Scanner::write_netmail_to_pkt(link->ouraka, link->aka, &sqmsg, false, link->fptr, link->flavour);
  } else {
    log.log(LOG_ERROR, "Incorrect password for %d:%d/%d.%d", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point);
  }
}

void Tosser::filefix(Config *c, sq_msg_t *msg) {
  link_conf_t *link = NULL;
  bool showhelp = false;

  for (size_t i = 0; i < c->links.size(); i++) {
    if (c->links.at(i).aka->zone == msg->xmsg.orig.zone && c->links.at(i).aka->net == msg->xmsg.orig.net && c->links.at(i).aka->node == msg->xmsg.orig.node &&
        c->links.at(i).aka->point == msg->xmsg.orig.point) {
      link = &c->links.at(i);
      break;
    }
  }

  if (link == NULL) {
    log.log(LOG_INFO, "Got filefix message from someone not defined in links! (%d:%d/%d.%d)", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node,
            msg->xmsg.orig.point);
    return;
  }

  // first check password
  if (link->areafixpwd != "" && strncasecmp(link->areafixpwd.c_str(), msg->xmsg.subject, link->areafixpwd.size()) == 0) {
    // password is good.
    std::vector<std::string> smsg;
    std::stringstream ss;
    for (size_t i = 0; i < (size_t)msg->msg_len; i++) {
      if (msg->msg[i] == '\r') {
        smsg.push_back(ss.str());
        ss.str("");
      } else if (msg->msg[i] != '\n') {
        ss << msg->msg[i];
      }
    }
    if (ss.str().size() > 0) {
      smsg.push_back(ss.str());
    }

    std::vector<std::string> msgout;

    for (std::string line : smsg) {
      if (line.size() > 0) {
        if (line.at(0) == '-') {
          // remove area
          bool success = false;

          for (size_t i = 0; i < c->fileareas.size(); i++) {
            if (strcasecmp(c->fileareas.at(i).areatag.c_str(), line.substr(1).c_str()) == 0 &&
                link->allowedgroups.find(c->fileareas.at(i).group) != std::string::npos) {
              std::stringstream ss2;
              for (size_t j = 0; j < c->fileareas.at(i).links.size(); j++) {
                if (c->fileareas.at(i).links.at(j).link == link) {
                  success = true;
                } else {
                  if (c->fileareas.at(i).links.at(j).forward_allowed == false && c->fileareas.at(i).links.at(j).process_allowed == false) {
                    ss2 << "!";
                  } else if (c->fileareas.at(i).links.at(j).forward_allowed == true && c->fileareas.at(i).links.at(j).process_allowed == true) {
                    ss2 << "&";
                  } else if (c->fileareas.at(i).links.at(j).forward_allowed == true && c->fileareas.at(i).links.at(j).process_allowed == false) {
                    ss2 << "=";
                  }
                  ss2 << c->fileareas.at(i).links.at(j).link->aka->zone << ":" << c->fileareas.at(i).links.at(j).link->aka->net << "/"
                      << c->fileareas.at(i).links.at(j).link->aka->node << "." << c->fileareas.at(i).links.at(j).link->aka->point << ",";
                }
              }

              if (success == true) {
                if (ss2.str().size() > 1) {
                  success = update(c->fileareas.at(i).areatag, ss2.str().substr(0, ss2.str().size() - 1), true);
                } else {
                  success = update(c->fileareas.at(i).areatag, ss2.str(), true);
                }
              }
              if (success) {
                msgout.push_back("You have been removed from " + c->fileareas.at(i).areatag + " successfully.");
                log.log(LOG_INFO, "Successfully removed %d:%d/%d.%d from %s", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node,
                        msg->xmsg.orig.point, line.substr(1).c_str());
              } else {
                msgout.push_back("You have NOT been removed from " + c->fileareas.at(i).areatag);
              }
              break;
            }
          }

        } else if (line.at(0) == '%') {
          if (strcasecmp(line.substr(1).c_str(), "LIST") == 0) {
            msgout.push_back("File areas you have access to:");
            msgout.push_back("");
            for (size_t i = 0; i < c->fileareas.size(); i++) {
              if (link->allowedgroups.find(c->fileareas.at(i).group) != std::string::npos) {
                msgout.push_back(c->fileareas.at(i).areatag);
              }
            }
          } else if (strcasecmp(line.substr(1).c_str(), "HELP") == 0) {
            showhelp = true;
          }
        } else {
          // add area
          bool success = false;

          for (size_t i = 0; i < c->fileareas.size(); i++) {
            if (line.at(0) == '+') {
              line = line.substr(1);
            }
            if (strcasecmp(c->fileareas.at(i).areatag.c_str(), line.c_str()) == 0 && link->allowedgroups.find(c->fileareas.at(i).group) != std::string::npos) {
              std::stringstream ss2;
              for (size_t j = 0; j < c->fileareas.at(i).links.size(); j++) {
                if (c->fileareas.at(i).links.at(j).link == link) {
                  success = false;
                  break;
                } else {
                  if (c->fileareas.at(i).links.at(j).forward_allowed == false && c->fileareas.at(i).links.at(j).process_allowed == false) {
                    ss2 << "!";
                  } else if (c->fileareas.at(i).links.at(j).forward_allowed == true && c->fileareas.at(i).links.at(j).process_allowed == true) {
                    ss2 << "&";
                  } else if (c->fileareas.at(i).links.at(j).forward_allowed == true && c->fileareas.at(i).links.at(j).process_allowed == false) {
                    ss2 << "=";
                  }
                  ss2 << c->fileareas.at(i).links.at(j).link->aka->zone << ":" << c->fileareas.at(i).links.at(j).link->aka->net << "/"
                      << c->fileareas.at(i).links.at(j).link->aka->node << "." << c->fileareas.at(i).links.at(j).link->aka->point << ",";
                }
              }

              if (ss2.str().size() > 1 && success == true) {
                ss2 << ",!" << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
                success = update(c->fileareas.at(i).areatag, ss2.str(), true);
              } else {
                ss2 << "!" << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
                success = update(c->fileareas.at(i).areatag, ss2.str(), true);
              }
              if (success) {
                msgout.push_back("You have been added to " + c->fileareas.at(i).areatag + " successfully.");
                log.log(LOG_INFO, "Successfully added %d:%d/%d.%d to %s", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point,
                        line.c_str());
              } else {
                msgout.push_back("You have NOT been added to " + c->fileareas.at(i).areatag);
              }
              break;
            }
          }
        }
      }
    }

    sq_msg_t sqmsg;

    memset(&sqmsg, 0, sizeof(sq_msg_t));

    sqmsg.xmsg.orig.zone = link->ouraka->zone;
    sqmsg.xmsg.orig.net = link->ouraka->net;
    sqmsg.xmsg.orig.point = link->ouraka->point;
    sqmsg.xmsg.orig.node = link->ouraka->node;

    sqmsg.xmsg.dest.zone = link->aka->zone;
    sqmsg.xmsg.dest.net = link->aka->net;
    sqmsg.xmsg.dest.point = link->aka->point;
    sqmsg.xmsg.dest.node = link->aka->node;

    strncpy(sqmsg.xmsg.subject, "FILEFIX Response", 71);
    strncpy(sqmsg.xmsg.to, msg->xmsg.from, 35);
    strncpy(sqmsg.xmsg.from, "FILEFIX", 35);

    std::tm at;

    time_t now = time(NULL);
#ifdef _MSC_VER
    localtime_s(&at, &now);
#else
    localtime_r(&now, &at);
#endif
    sqmsg.xmsg.date_written.date |= (((sq_word)at.tm_mday) & 31);
    sqmsg.xmsg.date_written.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
    sqmsg.xmsg.date_written.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

    sqmsg.xmsg.date_written.time |= (((sq_word)at.tm_sec) & 31);
    sqmsg.xmsg.date_written.time |= (((sq_word)at.tm_min) & 63) << 5;
    sqmsg.xmsg.date_written.time |= (((sq_word)at.tm_hour) & 31) << 11;

    sqmsg.xmsg.attr = MSGUID | MSGPRIVATE;

    std::stringstream ctrlstr;

    ctrlstr << "\x01INTL " << link->aka->zone << ":" << link->aka->net << "/" << link->aka->node << " " << link->ouraka->zone << ":" << link->ouraka->net << "/"
            << link->ouraka->node << "\r";
    if (link->aka->point > 0) {
      ctrlstr << "\x01TOPT " << link->aka->point << "\r";
    }
    if (link->ouraka->point > 0) {
      ctrlstr << "\001FMPT " << link->ouraka->point << "\r";
    }

    sqmsg.ctrl = (char *)malloc(ctrlstr.str().size());
    if (!sqmsg.ctrl) {
      return;
    }
    memcpy(sqmsg.ctrl, ctrlstr.str().c_str(), ctrlstr.str().size());
    sqmsg.ctrl_len = ctrlstr.str().size();

    std::stringstream msgstr;

    if (msgout.size() > 0) {
      // send msgout

      for (size_t i = 0; i < msgout.size(); i++) {
        msgstr << msgout.at(i) << "\r";
      }
    }
    if (showhelp) {
      // send help text
      msgstr << "\r\r----------------------------------------------------------\r";
      msgstr << "FILEFIX HELP!\r";
      msgstr << "----------------------------------------------------------\r";
      msgstr << "+SOMEAREA\r\r";
      msgstr << "This will add you to SOMEAREA\r\r";
      msgstr << "-SOMEAREA\r\r";
      msgstr << "This will remove you from SOMEAREA\r\r";
      msgstr << "%LIST\r\r";
      msgstr << "This will give you a list of everything available\r\r";
      msgstr << "%HELP\r\r";
      msgstr << "This will show you this help\r\r";
      msgstr << "----------------------------------------------------------\r";
    }

    sqmsg.msg = (char *)malloc(msgstr.str().size());
    if (!sqmsg.msg) {
      free(sqmsg.ctrl);
      return;
    }
    memcpy(sqmsg.msg, msgstr.str().c_str(), msgstr.str().size());
    sqmsg.msg_len = msgstr.str().size();

    if (link->fptr == NULL) {
      Scanner::initialize_packet(link, tempdir.u8string(), link->ouraka);
    }
    Scanner::write_netmail_to_pkt(link->ouraka, link->aka, &sqmsg, false, link->fptr, link->flavour);
  } else {
    log.log(LOG_ERROR, "Incorrect password for %d:%d/%d.%d", msg->xmsg.orig.zone, msg->xmsg.orig.net, msg->xmsg.orig.node, msg->xmsg.orig.point);
  }
}

std::string Tosser::get_msgid(std::string ctrlbody) {
  std::stringstream kludge;

  for (size_t z = 0; z < ctrlbody.size(); z++) {
    if (ctrlbody.at(z) == '\001') {
      if (kludge.str().size() > 0) {
        if (kludge.str().find("MSGID: ") == 0) {
          int start = 7;

          std::string msgid = kludge.str().substr(start);

          return msgid;
        }
      }
      kludge.str("");
      continue;
    }
    kludge << ctrlbody.at(z);
  }
  if (kludge.str().size() > 0) {
    if (kludge.str().find("MSGID: ") == 0) {
      int start = 7;

      std::string msgid = kludge.str().substr(start);

      return msgid;
    }
  }
  return "";
}

void Tosser::bad_packet(Config *c, std::string filename) {

  std::filesystem::path badpkt(c->packetdir());
  size_t ext = 0;
  badpkt.append(std::filesystem::path(filename).filename().u8string() + ".bad");
  while (std::filesystem::exists(badpkt)) {
    badpkt = c->packetdir();
    badpkt.append(std::filesystem::path(filename).filename().u8string() + ".bad." + std::to_string(ext));
    ext++;
  }

  std::filesystem::rename(filename, badpkt);
  log.log(LOG_ERROR, "Encountered Bad Packet: %s", badpkt.u8string().c_str());
}

UMSGID Tosser::get_reply_msg(sq_msg_base_t *mb, sq_msg_t *msg) {
  // get reply id from CTRL
  std::string reply = "";
  std::stringstream kludge;

  for (size_t z = 0; z < msg->ctrl_len; z++) {
    if (msg->ctrl[z] == '\001') {
      if (kludge.str().size() > 0) {
        if (kludge.str().find("REPLY: ") == 0) {
          reply = kludge.str().substr(7);
          break;
        }
      }
      kludge.str("");
      continue;
    }
    kludge << msg->ctrl[z];
  }

  if (kludge.str().size() > 0) {
    if (kludge.str().find("REPLY: ") == 0) {
      reply = kludge.str().substr(7);
    }
  }

  if (reply == "") {
    return 0;
  }

  for (size_t i = 1; i <= mb->basehdr.num_msg; i++) {
    sq_msg_t *rmsg = SquishReadMsg(mb, i);
    std::string msgid = "";

    if (!rmsg) {
      continue;
    }

    kludge.str("");
    // get msg id from CTRL
    for (size_t z = 0; z < rmsg->ctrl_len; z++) {
      if (rmsg->ctrl[z] == '\001') {
        if (kludge.str().size() > 0) {
          if (kludge.str().find("MSGID: ") == 0) {
            msgid = kludge.str().substr(7);
            break;
          }
        }
        kludge.str("");
        continue;
      }
      kludge << rmsg->ctrl[z];
    }
    if (kludge.str().size() > 0) {
      if (kludge.str().find("MSGID: ") == 0) {
        msgid = kludge.str().substr(7);
      }
    }
    if (msgid == "") {
      SquishFreeMsg(rmsg);
      continue;
    }
    if (msgid == reply) {
      UMSGID ret = rmsg->xmsg.umsgid;

      for (int k = 0; k < 9; k++) {
        if (rmsg->xmsg.replies[k] == 0) {
          rmsg->xmsg.replies[k] = msg->xmsg.umsgid;

          SquishLockMsgBase(mb);
          SquishUpdateHdr(mb, rmsg);
          SquishUnlockMsgBase(mb);
          SquishFreeMsg(rmsg);
          return ret;
        }
      }
    }
    SquishFreeMsg(rmsg);
  }
  return 0;
}

NETADDR *Tosser::get_echomail_addr(std::string ctrlbody, std::string msgbody) {
  // first try getting address from origin line
  std::stringstream ss(msgbody);
  std::string line;
  NETADDR *ftnaddr = NULL;
  std::vector<std::string> lines;

  while (getline(ss, line, '\r')) {
    lines.push_back(line);
  }

  for (int z = lines.size() - 1; z >= 0; z--) {
    if (lines.at(z).find(" * Origin: ") == 0) {
      // found origin line.

      int start = lines.at(z).rfind("(") + 1;
      int size = lines.at(z).substr(start).find(")");

      std::string addr = lines.at(z).substr(start, size);
      if (ftnaddr != NULL) {
        free(ftnaddr);
      }

      ftnaddr = parse_fido_addr(addr.c_str());
    }
  }

  if (ftnaddr != NULL) {

    return ftnaddr;
  }

  // next try MSGID
  std::stringstream kludge;

  for (size_t z = 0; z < ctrlbody.size(); z++) {
    if (ctrlbody.at(z) == '\001') {
      if (kludge.str().size() > 0) {
        if (kludge.str().find("MSGID: ") == 0) {
          size_t start = 7;
          size_t size = kludge.str().substr(start).find(" ");

          if (size != std::string::npos) {
            std::string addr = kludge.str().substr(start, size - 1);
            ftnaddr = parse_fido_addr(addr.c_str());

            if (ftnaddr != NULL) {
              return ftnaddr;
            } else {
              start = kludge.str().find("@");
              if (start != std::string::npos) {
                size = kludge.str().substr(start).find(" ");
                if (size != std::string::npos) {
                  std::string addr = kludge.str().substr(start, size - 1);
                  ftnaddr = parse_fido_addr(addr.c_str());

                  if (ftnaddr != NULL) {
                    return ftnaddr;
                  }
                }
              }
            }
          }
        }
      }
      kludge.str("");
      continue;
    }
    kludge << ctrlbody.at(z);
  }
  // next fail
  return NULL;
}

#ifndef bswap16
static uint16_t bswap16(uint16_t arg) {
  uint16_t hibyte = (arg & 0xff00) >> 8;
  uint16_t lobyte = (arg & 0xff);
  return lobyte << 8 | hibyte;
}
#endif

bool sort_by_last_write(std::filesystem::path p1, std::filesystem::path p2) {
  return (std::filesystem::last_write_time(p1) < std::filesystem::last_write_time(p2));
}

bool sort_by_filename(std::filesystem::path p1, std::filesystem::path p2) {
  time_t t1 = 0;
  time_t t2 = 0;

  try {
    t1 = std::stoi(p1.stem(), nullptr, 16);
    t2 = std::stoi(p2.stem(), nullptr, 16);
  } catch (std::exception const &) {
    return false;
  }
  return (t1 < t2);
}

bool Tosser::run(bool protinbound) {
  INIReader inir("talisman.ini");

  Config c;

  if (inir.ParseError()) {
    return false;
  }

  _datapath = inir.Get("Paths", "Data Path", "data");
  _msgpath = inir.Get("Paths", "Message Path", "msgs");
  _logpath = inir.Get("Paths", "Log Path", "logs");
  _tmppath = inir.Get("Paths", "Temp Path", "temp");

  log.load(_logpath + "/postie.log");

#ifdef _MSC_VER
  pid = GetCurrentProcessId();
#else
  pid = getpid();
#endif

  std::filesystem::path pidfile(_datapath + "/postie.pid");
  int tries = 0;
  while (std::filesystem::exists(pidfile)) {
    if (tries == 10) {
      log.log(LOG_ERROR, "Timeout waiting for pid file...");
      return false;
    }
#ifdef _MSC_VER
    Sleep(1000);
#else
    sleep(1);
#endif
    tries++;
  }

  FILE *fptr = fopen(pidfile.u8string().c_str(), "wx");
  if (!fptr) {
    log.log(LOG_ERROR, "Failed to open pid file...");
    return false;
  }
  fprintf(fptr, "%lu\r\n", pid);
  fclose(fptr);
  signal(SIGTERM, sig_handler);
#ifndef _MSC_VER
  signal(SIGHUP, sig_handler);
  signal(SIGINT, sig_handler);
#endif

  log.log(LOG_DEBUG, "Starting Tosser");

  if (!c.load(_datapath, &log)) {
    std::filesystem::remove(pidfile);
    return false;
  }

  if (!c.load_archivers(_datapath, &log)) {
    std::filesystem::remove(pidfile);
    return false;
  }

  static const char *fileext1 = "SMTWFsmtwf";
  static const char *fileext2 = "UOEHRAuoehra";
  static const char *fileext3 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

  // toss each file one at a time
  std::filesystem::path inbound((protinbound ? c.protinbound() : c.inbound()));
  tempdir = _tmppath + "/postie-" + std::to_string(pid);

  std::vector<std::filesystem::path> totoss;

  for (auto &p : std::filesystem::directory_iterator(inbound)) {
    if (std::filesystem::is_regular_file(p.path())) {
      totoss.push_back(p.path());
    }
  }

  std::sort(totoss.begin(), totoss.end(), sort_by_last_write);

  std::filesystem::remove_all(tempdir);
  std::filesystem::create_directories(tempdir);

  for (const std::filesystem::path &packetpth : totoss) {
    std::filesystem::path temp_name(packetpth);
    std::filesystem::path e(packetpth.extension().u8string() + ".toss");

    temp_name.replace_extension(e);

    if (strcasecmp(packetpth.extension().u8string().c_str(), ".pkt") == 0) {
      std::filesystem::rename(packetpth, temp_name);
      std::filesystem::copy(temp_name, std::filesystem::path(tempdir.u8string() + "/" + packetpth.filename().u8string()));
    } else {
      if (!strchr(fileext1, packetpth.extension().u8string().substr(1, 1).c_str()[0])) {
        continue;
      }

      if (!strchr(fileext2, packetpth.extension().u8string().substr(2, 1).c_str()[0])) {
        continue;
      }

      if (!strchr(fileext3, packetpth.extension().u8string().substr(3, 1).c_str()[0])) {
        continue;
      }
      std::filesystem::rename(packetpth, temp_name);

      bool unarced = false;

      // unarchive packet
      for (size_t i = 0; i < c.archivers.size(); i++) {
        FILE *fptr = fopen(temp_name.u8string().c_str(), "rb");
        if (c.archivers.at(i)->offset >= 0) {
          fseek(fptr, c.archivers.at(i)->offset, SEEK_SET);
        } else {
          fseek(fptr, c.archivers.at(i)->offset, SEEK_END);
        }

        uint8_t byte;
        bool match = true;
        for (int z = 0; z < c.archivers.at(i)->bytelen; z++) {
          fread(&byte, 1, 1, fptr);

          if (byte != c.archivers.at(i)->bytes[z]) {
            match = false;
            break;
          }
        }
        fclose(fptr);
        if (match == false)
          continue;
        c.archivers.at(i)->extract(temp_name.u8string(), tempdir.u8string());
        unarced = true;
        break;
      }
      if (unarced == false) {
        log.log(LOG_ERROR, "Unable to find archiver for bundle!");
        continue;
      }
    }

    std::vector<std::filesystem::path> packet_files;

    for (auto &pkt : std::filesystem::directory_iterator{tempdir}) {
      if (std::filesystem::is_directory(pkt.path()))
        continue;
      else {
        packet_files.push_back(pkt.path());
      }
    }

    std::sort(packet_files.begin(), packet_files.end(), sort_by_filename);

    for (std::filesystem::path pkt : packet_files) {
      if (std::filesystem::file_size(pkt) < 58) {
        // move bad packet to .bad
        log.log(LOG_ERROR, "Packet size < 58 bytes");
        bad_packet(&c, pkt.u8string());
        continue;
      }
      FILE *fptr = fopen(pkt.u8string().c_str(), "rb");

      if (!fptr) {
        log.log(LOG_ERROR, "Unable to open packet! %s", pkt.u8string().c_str());
        continue;
      }

      // read packet header
      struct packet_t phdr;
      struct packed_message_t pmsg;

      fread(&phdr, sizeof(struct packet_t), 1, fptr);
      convert_phdr(&phdr);

      bool type2plus = false;

      if (phdr.version != 2) {
        fclose(fptr);
        log.log(LOG_ERROR, "Packet version != 2");
        // move bad packet to .bad
        bad_packet(&c, pkt.u8string());
        continue;
      }

      if (phdr.capWord == bswap16(phdr.capValid) && phdr.capWord & 1) {
        type2plus = true;
      }

      NETADDR pktorig;
      NETADDR pktdest;

      if (type2plus) {
        pktorig.zone = phdr.origZone2;
        pktorig.node = phdr.orignode;
        pktdest.zone = phdr.destZone2;
        pktdest.node = phdr.destnode;

        if (phdr.origNet == 0xffff) {
          pktorig.net = phdr.auxNet;
          pktorig.point = phdr.origPoint;
        } else {
          pktorig.net = phdr.origNet;
          pktorig.point = 0;
        }
        pktdest.net = phdr.destNet;
        pktdest.point = phdr.destPoint;
      } else {
        pktorig.zone = phdr.origZone;
        pktorig.net = phdr.origNet;
        pktorig.node = phdr.orignode;
        pktorig.point = 0;

        pktdest.zone = phdr.destZone;
        pktdest.net = phdr.destNet;
        pktdest.node = phdr.destnode;
        pktdest.point = 0;
      }
      bool is_bad_packet = false;
      bool link_found = false;
      for (size_t i = 0; i < c.links.size(); i++) {
        if (c.links.at(i).ouraka->point != 0) {
          if (c.links.at(i).aka->zone == pktdest.zone && c.links.at(i).aka->net == pktdest.net && c.links.at(i).aka->node == pktdest.node) {
            if (strncasecmp(c.links.at(i).packetpwd.c_str(), phdr.password, 8) != 0) {
              log.log(LOG_ERROR, "Incorrect Packet Password!");
              is_bad_packet = true;
              break;
            } else {
              link_found = true;
              break;
            }
          }
        } else {
          if (c.links.at(i).aka->zone == pktorig.zone && c.links.at(i).aka->net == pktorig.net && c.links.at(i).aka->node == pktorig.node) {
            if (strncasecmp(c.links.at(i).packetpwd.c_str(), phdr.password, 8) != 0) {
              log.log(LOG_ERROR, "Incorrect Packet Password!");
              is_bad_packet = true;
              break;
            } else {
              link_found = true;
              break;
            }
          }
        }
      }
      if (protinbound && !link_found) {
        log.log(LOG_ERROR, "Packet from unknown link in secure inbound..");
        is_bad_packet = true;
      }

      if (is_bad_packet) {

        fclose(fptr);
        // move bad packet to .bad
        bad_packet(&c, pkt.u8string());
        continue;
      }

      while (fread(&pmsg, sizeof(struct packed_message_t), 1, fptr) == 1) {
        convert_pmsghdr(&pmsg);
        if (pmsg.message_type != 2) {
          is_bad_packet = true;
          break;
        }
        char ch;

        std::stringstream datestr;

        datestr.str("");
        while ((ch = fgetc(fptr)) != '\0') {
          if (feof(fptr))
            break;
          datestr << ch;
        }

        std::stringstream tostr;

        tostr.str("");
        while ((ch = fgetc(fptr)) != '\0') {
          if (feof(fptr))
            break;
          tostr << ch;
        }
        std::stringstream fromstr;

        fromstr.str("");
        while ((ch = fgetc(fptr)) != '\0') {
          if (feof(fptr))
            break;
          fromstr << ch;
        }
        std::stringstream subjstr;

        subjstr.str("");
        while ((ch = fgetc(fptr)) != '\0') {
          if (feof(fptr))
            break;
          subjstr << ch;
        }

        std::stringstream areastr;
        std::stringstream bodystr;
        std::stringstream msgstr;
        std::stringstream ctrlstr;
        std::string line;
        std::string areatag;

        bool pastarea = false;
        bool pastorigin = false;
        bodystr.str("");

        while ((ch = fgetc(fptr)) != '\0') {
          if (feof(fptr))
            break;
          // onto body
          bodystr << ch;
        }

        while (getline(bodystr, line, '\r')) {
          if (line.find("AREA:") == 0 && !pastarea) {
            areatag = line.substr(5);
            pastarea = true;
            continue;
          } else if (!pastarea) {
            areatag = "";
            pastarea = true;
          }
          if (line.find("--- ") == 0 || line == "---") {
            pastorigin = true;
          }
          if (line[0] == '\001' && !pastorigin) {
            ctrlstr << line << "\r";
          } else {
            msgstr << line << "\r";
          }
        }

        bool msgprocessed = false;

        if (areatag != "" && protinbound) {

          // it's an echomail
          sq_msg_t sqmsg;

          memset(&sqmsg, 0, sizeof(sq_msg_t));

          sqmsg.ctrl = (char *)malloc(ctrlstr.str().size());
          if (!sqmsg.ctrl) {
            continue;
          }
          memcpy(sqmsg.ctrl, ctrlstr.str().c_str(), ctrlstr.str().size());
          sqmsg.ctrl_len = ctrlstr.str().size();

          sqmsg.msg = (char *)malloc(msgstr.str().size());
          if (!sqmsg.msg) {
            free(sqmsg.ctrl);
            continue;
          }
          memcpy(sqmsg.msg, msgstr.str().c_str(), msgstr.str().size());
          sqmsg.msg_len = msgstr.str().size();

          NETADDR *emaddr = get_echomail_addr(ctrlstr.str(), msgstr.str());

          if (emaddr != NULL) {
            sqmsg.xmsg.orig.zone = emaddr->zone;
            sqmsg.xmsg.orig.node = emaddr->node;
            sqmsg.xmsg.orig.net = emaddr->net;
            sqmsg.xmsg.orig.point = emaddr->point;
            free(emaddr);
          }

          strncpy(sqmsg.xmsg.subject, subjstr.str().c_str(), 71);
          strncpy(sqmsg.xmsg.to, tostr.str().c_str(), 35);
          strncpy(sqmsg.xmsg.from, fromstr.str().c_str(), 35);

          std::tm lt;

          datestr >> std::get_time(&lt, "%d %b %y  %H:%M:%S");
          if (lt.tm_year < 68) {
            lt.tm_year += 100;
          }

          sqmsg.xmsg.date_written.date |= (((sq_word)lt.tm_mday) & 31);
          sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_mon + 1)) & 15) << 5;
          sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_year - 80)) & 127) << 9;

          sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_sec) & 31);
          sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_min) & 63) << 5;
          sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_hour) & 31) << 11;

          std::tm at;

          time_t now = time(NULL);
#ifdef _MSC_VER
          localtime_s(&at, &now);
#else
          localtime_r(&now, &at);
#endif

          sqmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
          sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
          sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

          strncpy(sqmsg.xmsg.__ftsc_date, datestr.str().c_str(), sizeof(sqmsg.xmsg.__ftsc_date));

          sqmsg.xmsg.attr = MSGUID;

          // check if dupe
          std::string msgid = get_msgid(ctrlstr.str());

          if (Dupe::is_dupe(_datapath + "/dupehist_v3.dat", msgid, &sqmsg)) {

            if (c.dupebase() == "") {
              log.log(LOG_INFO, "Found duplicate, discarding as no dupe base is configured.");
            } else {
              sq_msg_base_t *mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.dupebase()).c_str());

              if (mb != NULL) {
                SquishLockMsgBase(mb);
                SquishWriteMsg(mb, &sqmsg);
                SquishUnlockMsgBase(mb);
                SquishCloseMsgBase(mb);
                log.log(LOG_INFO, "Found duplicate, saved in dupe base.");
              } else {
                log.log(LOG_INFO, "Found duplicate, discarding because an error occured opening the dupe base.");
              }
            }
            free(sqmsg.msg);
            free(sqmsg.ctrl);
            continue;
          }

          for (size_t a = 0; a < c.areas.size(); a++) {
            if (strcasecmp(areatag.c_str(), c.areas.at(a).areatag.c_str()) == 0) {
              // process message
              // send to downlinks
              std::vector<struct seenby_t> seenbys = Scanner::parse_seenbys(msgstr.str());

              for (size_t l = 0; l < c.areas.at(a).links.size(); l++) {
                // if it's to a point
                if (c.areas.at(a).links.at(l)->aka->point != 0) {
                  if (type2plus && phdr.origNet == 0xffff) {
                    if (c.areas.at(a).links.at(l)->aka->zone == phdr.origZone && c.areas.at(a).links.at(l)->aka->net == phdr.auxNet &&
                        c.areas.at(a).links.at(l)->aka->node == phdr.orignode && c.areas.at(a).links.at(l)->aka->point == phdr.origPoint) {
                      continue;
                    }
                  } else {
                    if (c.areas.at(a).links.at(l)->aka->zone == phdr.origZone && c.areas.at(a).links.at(l)->aka->net == phdr.origNet &&
                        c.areas.at(a).links.at(l)->aka->node == phdr.orignode) {
                      continue;
                    }
                  }
                } else {

                  // check seenbys

                  if (Scanner::check_seenby(&seenbys, c.areas.at(a).links.at(l)->aka)) {
                    continue;
                  }
                }

                if (c.areas.at(a).links.at(l)->fptr == NULL) {
                  if (c.areas.at(a).links.at(l)->aka->point != 0) {
                    Scanner::initialize_packet(c.areas.at(a).links.at(l), tempdir.u8string(), &pktorig);
                  } else {
                    Scanner::initialize_packet(c.areas.at(a).links.at(l), tempdir.u8string(), c.areas.at(a).links.at(l)->ouraka);
                  }
                }
                Scanner::write_msg_to_pkt(&c.areas.at(a), c.areas.at(a).links.at(l), &sqmsg, false);
              }
              // save to base

              sq_msg_base_t *mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.areas.at(a).file).c_str());

              if (mb != NULL) {
                SquishLockMsgBase(mb);
                SquishWriteMsg(mb, &sqmsg);
                SquishUnlockMsgBase(mb);

                UMSGID reply;

                if ((reply = get_reply_msg(mb, &sqmsg)) != 0) {
                  sqmsg.xmsg.replyto = reply;
                  SquishLockMsgBase(mb);
                  SquishUpdateHdr(mb, &sqmsg);
                  SquishUnlockMsgBase(mb);
                }

                SquishCloseMsgBase(mb);
                log.log(LOG_INFO, "Added echomail \"%s\" to area \"%s\"", sqmsg.xmsg.subject, c.areas.at(a).areatag.c_str());

                if (c.areas.at(a).thook != "") {
                  std::stringstream hss;

                  hss << c.areas.at(a).thook << " " << c.areas.at(a).areatag << " \\\"";

                  for (size_t i = 0; i < strlen(sqmsg.xmsg.subject); i++) {
                    if (sqmsg.xmsg.subject[i] == '\"') {
                      hss << "'";
                    } else {
                      hss << sqmsg.xmsg.subject[i];
                    }
                  }
                  hss << "\\\" \\\"";

                  for (size_t i = 0; i < strlen(sqmsg.xmsg.from); i++) {
                    if (sqmsg.xmsg.from[i] == '\"') {
                      hss << "'";
                    } else {
                      hss << sqmsg.xmsg.from[i];
                    }
                  }
                  hss << "\\\"";
                  log.log(LOG_INFO, "Running hook: %s", hss.str().c_str());
                  Archiver::runexec(hss.str());
                }
              } else {
                log.log(LOG_ERROR, "Unable to open message base! : %s", std::string(_msgpath + "/" + c.areas.at(a).file).c_str());
              }

              msgprocessed = true;
              break;
            }
          }
          free(sqmsg.msg);
          free(sqmsg.ctrl);
          if (!msgprocessed) {
            log.log(LOG_ERROR, "Message for area %s not processed!", areatag.c_str());
          }
        } else if (areatag == "") {
          // it's a netmail...
          // delete empty mail
          if (bodystr.str().size() == 0) {
            log.log(LOG_INFO, "Got an empty netmail, discarding...");
            continue;
          }
          if (fromstr.str() == "ARCmail") {
            log.log(LOG_INFO, "Got a message from ARCmail, discarding...");
            continue;
          }
          // is it for us...
          // look for intl kludge & topt kludge
          std::stringstream kludge;

          NETADDR *intldest = NULL;
          NETADDR *intlorig = NULL;
          int intlpoint = 0;
          int intlfpoint = 0;

          for (size_t z = 0; z < ctrlstr.str().size(); z++) {
            if (ctrlstr.str().at(z) == '\001') {
              if (kludge.str().size() > 0) {
                if (kludge.str().find("INTL ") == 0) {
                  size_t addrsize = kludge.str().substr(5).find(" ");

                  if (intldest != NULL) {
                    free(intldest); // incase there is more than one intl line :O
                  }

                  std::string intl = kludge.str();
                  std::string intld = kludge.str().substr(5, addrsize);
                  std::string intlo = kludge.str().substr(5 + addrsize + 1);

                  intldest = parse_fido_addr(intld.c_str());
                  intlorig = parse_fido_addr(intlo.c_str());
                } else if (kludge.str().find("TOPT ") == 0) {
                  try {
                    intlpoint = stoi(kludge.str().substr(5));
                  } catch (std::invalid_argument const &) {

                  } catch (std::out_of_range const &) {
                  }
                } else if (kludge.str().find("FMPT ") == 0) {
                  try {
                    intlfpoint = stoi(kludge.str().substr(5));
                  } catch (std::invalid_argument const &) {

                  } catch (std::out_of_range const &) {
                  }
                }
              }
              kludge.str("");
              continue;
            }
            kludge << ctrlstr.str().at(z);
          }

          if (kludge.str().size() > 0) {
            if (kludge.str().find("INTL ") == 0) {
              size_t addrsize = kludge.str().substr(5).find(" ");

              if (intldest != NULL) {
                free(intldest); // incase there is more than one intl line :O
              }

              std::string intl = kludge.str();
              std::string intld = kludge.str().substr(5, addrsize);
              std::string intlo = kludge.str().substr(5 + addrsize + 1);

              intldest = parse_fido_addr(intld.c_str());
              intlorig = parse_fido_addr(intlo.c_str());
            } else if (kludge.str().find("TOPT ") == 0) {
              try {
                intlpoint = stoi(kludge.str().substr(5));
              } catch (std::invalid_argument const &) {

              } catch (std::out_of_range const &) {
              }
            } else if (kludge.str().find("FMPT ") == 0) {
              try {
                intlfpoint = stoi(kludge.str().substr(5));
              } catch (std::invalid_argument const &) {

              } catch (std::out_of_range const &) {
              }
            }
          }

          if (intldest != NULL) {
            intldest->point = intlpoint;
            if (intlorig != NULL) {
              intlorig->point = intlfpoint;
            }
          } else {
            // assume it's based on net / node
            intldest = (NETADDR *)malloc(sizeof(NETADDR));
            if (!intldest) {
              // panic!
              // oom?
              continue;
            }
            intldest->zone = phdr.destZone;
            intldest->net = pmsg.dest_net;
            intldest->node = pmsg.dest_node;
            intldest->point = intlpoint;
            if (intlorig == NULL) {
              intlorig = (NETADDR *)malloc(sizeof(NETADDR));
              if (!intlorig) {
                // panic!
                // oom?
                free(intldest);
                continue;
              }
              intlorig->zone = phdr.origZone;
              intlorig->net = pmsg.orig_net;
              intlorig->node = pmsg.orig_node;
              intlorig->point = intlfpoint;
            }
          }

          // if it is, import it
          // to me?
          bool found = false;
          size_t nmarea = 0;

          for (size_t a = 0; a < c.addresses.size(); a++) {
            if (intldest->zone == c.addresses.at(a).aka->zone && intldest->net == c.addresses.at(a).aka->net && intldest->node == c.addresses.at(a).aka->node &&
                intldest->point == c.addresses.at(a).aka->point) {
              // it's for us!
              found = true;
              for (size_t ar = 0; ar < c.netmailareas.size(); ar++) {
                if (intldest->zone == c.netmailareas.at(ar).aka->zone && intldest->net == c.netmailareas.at(ar).aka->net &&
                    intldest->node == c.netmailareas.at(ar).aka->node && intldest->point == c.netmailareas.at(ar).aka->point) {
                  // put it in this netmail area
                  nmarea = ar;
                  break;
                }
              }
              break;
            }
          }
          sq_msg_t sqmsg;

          memset(&sqmsg, 0, sizeof(sq_msg_t));

          sqmsg.ctrl = (char *)malloc(ctrlstr.str().size());
          if (!sqmsg.ctrl) {
            free(intlorig);
            free(intldest);
            continue;
          }
          memcpy(sqmsg.ctrl, ctrlstr.str().c_str(), ctrlstr.str().size());
          sqmsg.ctrl_len = ctrlstr.str().size();

          sqmsg.msg = (char *)malloc(msgstr.str().size());
          if (!sqmsg.msg) {
            free(intlorig);
            free(intldest);
            free(sqmsg.ctrl);
            continue;
          }
          memcpy(sqmsg.msg, msgstr.str().c_str(), msgstr.str().size());
          sqmsg.msg_len = msgstr.str().size();

          sqmsg.xmsg.orig.zone = intlorig->zone;
          sqmsg.xmsg.orig.net = intlorig->net;
          sqmsg.xmsg.orig.point = intlorig->point;
          sqmsg.xmsg.orig.node = intlorig->node;

          sqmsg.xmsg.dest.zone = intldest->zone;
          sqmsg.xmsg.dest.net = intldest->net;
          sqmsg.xmsg.dest.point = intldest->point;
          sqmsg.xmsg.dest.node = intldest->node;

          strncpy(sqmsg.xmsg.subject, subjstr.str().c_str(), 71);
          strncpy(sqmsg.xmsg.to, tostr.str().c_str(), 35);
          strncpy(sqmsg.xmsg.from, fromstr.str().c_str(), 35);

          free(intlorig);

          std::tm lt;

          memset(&lt, 0, sizeof(std::tm));

          datestr >> std::get_time(&lt, "%d %b %y  %H:%M:%S");
          if (lt.tm_year < 68) {
            lt.tm_year += 100;
          }

          sqmsg.xmsg.date_written.date |= (((sq_word)lt.tm_mday) & 31);
          sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_mon + 1)) & 15) << 5;
          sqmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_year - 80)) & 127) << 9;

          sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_sec) & 31);
          sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_min) & 63) << 5;
          sqmsg.xmsg.date_written.time |= (((sq_word)lt.tm_hour) & 31) << 11;

          std::tm at;

          time_t now = time(NULL);
#ifdef _MSC_VER
          localtime_s(&at, &now);
#else
          localtime_r(&now, &at);
#endif

          sqmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
          sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
          sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

          strncpy(sqmsg.xmsg.__ftsc_date, datestr.str().c_str(), sizeof(sqmsg.xmsg.__ftsc_date));

          sqmsg.xmsg.attr = MSGUID | MSGPRIVATE;

          if (found) {
            if (strcasecmp(sqmsg.xmsg.to, "areafix") == 0 && protinbound) {
              log.log(LOG_INFO, "It's to areafix!");
              areafix(&c, &sqmsg);
            } else if (strcasecmp(sqmsg.xmsg.to, "filefix") == 0 && protinbound) {
              log.log(LOG_INFO, "It's to filefix!");
              filefix(&c, &sqmsg);
            } else {

              sq_msg_base_t *mb = SquishOpenMsgBase(std::string(_msgpath + "/" + c.netmailareas.at(nmarea).file).c_str());

              if (mb != NULL) {
                SquishLockMsgBase(mb);
                SquishWriteMsg(mb, &sqmsg);
                SquishUnlockMsgBase(mb);
                SquishCloseMsgBase(mb);
                log.log(LOG_INFO, "Added netmail \"%s\" to netmail area", sqmsg.xmsg.subject);
              } else {
                log.log(LOG_ERROR, "Unable to open message base! %s", std::string(_msgpath + "/" + c.netmailareas.at(nmarea).file).c_str());
              }
            }

          } else if (protinbound) {
            // if not send it on
            log.log(LOG_INFO, "Netmail not to us... forwarding.");

            bool matchedroute = false;
            for (size_t r = 0; r < c.routes.size(); r++) {
              if (Scanner::matchroute(c.routes.at(r).route, intldest)) {
                // found route
                matchedroute = true;
                log.log(LOG_INFO, "Netmail not to us... matched route.");
                // find link relating to route
                for (size_t l = 0; l < c.links.size(); l++) {
                  if (c.links.at(l).aka->zone == c.routes.at(r).aka->zone && c.links.at(l).aka->node == c.routes.at(r).aka->node &&
                      c.links.at(l).aka->net == c.routes.at(r).aka->net && c.links.at(l).aka->point == c.routes.at(r).aka->point) {
                    if (c.links.at(l).fptr == NULL) {
                      Scanner::initialize_packet(&c.links.at(l), tempdir.u8string(), c.links.at(1).ouraka);
                    }
                    Scanner::write_netmail_to_pkt(c.links.at(l).ouraka, c.links.at(l).aka, &sqmsg, false, c.links.at(l).fptr, c.links.at(l).flavour);
                    log.log(LOG_INFO, "Netmail not to us... wrote packet.");

                    break;
                  }
                }
                break;
              }
            }

            if (!matchedroute) {
              log.log(LOG_ERROR, "Got netmail with no matching routes..");
            }
          } else {
            log.log(LOG_ERROR, "Got netmail not for me in unprotected inbound! Discarding...");
          }
          free(intldest);
          free(sqmsg.msg);
          free(sqmsg.ctrl);
        } else if (areatag != "" && !protinbound) {
          log.log(LOG_ERROR, "Got echomail in unprotected inbound! Discarding...");
        }
      }
      if (is_bad_packet) {
        // move bad packet to .bad
        log.log(LOG_ERROR, "Packet message version != 2");
        fclose(fptr);
        bad_packet(&c, pkt.u8string());
        continue;
      }
      fclose(fptr);
      std::filesystem::remove(pkt);
    }

    std::filesystem::remove(temp_name);
  }

  for (size_t lid = 0; lid < c.links.size(); lid++) {
    if (c.links.at(lid).fptr != NULL) {
      char null[2];
      memset(null, 0, 2);

      fwrite(null, 2, 1, c.links.at(lid).fptr);

      fclose(c.links.at(lid).fptr);
      c.links.at(lid).fptr = NULL;
      // create bundle
      std::string bundlename =
          c.packetdir() + "/" + Scanner::get_bundle_name(c.links.at(lid).ouraka, c.links.at(lid).aka, c.packetdir(), c.bundlename_ts(), _datapath);
      if (bundlename == "") {
        log.log(LOG_ERROR, "Unable to get bundle name");
        continue;
      }

      for (size_t arc = 0; arc < c.archivers.size(); arc++) {
        if (strcasecmp(c.archivers.at(arc)->name.c_str(), c.links.at(lid).archiver.c_str()) == 0) {
          std::vector<std::string> files;

          files.push_back(c.links.at(lid).packetpath.u8string());

          c.archivers.at(arc)->compress(bundlename, files);
          Scanner::append_flo_file(&c.links.at(lid), &c, bundlename, "ref");
          break;
        }
      }
    }
  }
  std::filesystem::path temppath(_tmppath + "/postie-" + std::to_string(pid));

  std::filesystem::remove_all(temppath);
  std::filesystem::remove(pidfile);
  return true;
}
