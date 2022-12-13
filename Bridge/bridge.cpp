#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include "../Common/Squish.h"
#include "../Common/Logger.h"
#include "bridge.h"

std::string remove_seenby_path(std::string msgbuf) {
  std::stringstream ss(msgbuf);
  std::stringstream ss2;
  std::string buff;
  bool gotseenby = false;
  bool pastseenby = false;
  std::vector<std::string> lines;
  std::vector<std::string> noseenby;
  while (getline(ss, buff, '\r')) {
    lines.push_back(buff);
  }

  for (int i = lines.size() - 1; i >= 0; i--) {
    if (lines.at(i).find("SEEN-BY: ") == 0 && !pastseenby) {
      gotseenby = true;
    } else {
      if (gotseenby == true) {
        pastseenby = true;
      }
      noseenby.insert(noseenby.begin(), lines.at(i));
    }
  }

  for (size_t i = 0; i < noseenby.size(); i++) {
    if (noseenby.at(i).find("\001PATH: ") == 0) {
      continue;
    }
    ss2 << noseenby.at(i) << "\r";
  }

  return ss2.str();
}

void Bridge::do_bridge(std::string msg_path, struct brlink *linkdesc, Logger *log) {
    FILE *lastread1_fptr;
    FILE *lastread2_fptr;

    uint32_t lr1 = 0;
    uint32_t lr2 = 0;

    lastread1_fptr = fopen(std::string(msg_path + "/" + linkdesc->msgfile1 + ".bl_" + linkdesc->linkname).c_str(), "rb");
    if (lastread1_fptr) {
        fread(&lr1, sizeof(uint32_t), 1 , lastread1_fptr);
        fclose(lastread1_fptr);
    }
    
    lastread2_fptr = fopen(std::string(msg_path + "/" + linkdesc->msgfile2 + ".bl_" + linkdesc->linkname).c_str(), "rb");
    if (lastread2_fptr) {
        fread(&lr2, sizeof(uint32_t), 1 , lastread2_fptr);
        fclose(lastread2_fptr);
    }

    std::vector<sq_msg_t *> new_msgs1;
    std::vector<sq_msg_t *> new_msgs2;

    sq_msg_base_t *mb1 = SquishOpenMsgBase(std::string(msg_path + "/" + linkdesc->msgfile1).c_str());

    if (!mb1) {
        fprintf(stderr, "Failed to open %s", std::string(msg_path + "/" + linkdesc->msgfile1).c_str());
        return;
    }

    sq_msg_base_t *mb2 = SquishOpenMsgBase(std::string(msg_path + "/" + linkdesc->msgfile2).c_str());
    if (!mb2) {
        fprintf(stderr, "Failed to open %s", std::string(msg_path + "/" + linkdesc->msgfile2).c_str());
        SquishCloseMsgBase(mb1);
        return;
    }

    if (!SquishLockMsgBase(mb1)) {
        fprintf(stderr, "Failed to lock %s", std::string(msg_path + "/" + linkdesc->msgfile1).c_str());
        SquishCloseMsgBase(mb1);
        SquishCloseMsgBase(mb2);
        return;
    }
    if (!SquishLockMsgBase(mb2)) {
        fprintf(stderr, "Failed to lock %s", std::string(msg_path + "/" + linkdesc->msgfile2).c_str());
        SquishUnlockMsgBase(mb1);
        SquishCloseMsgBase(mb1);
        SquishCloseMsgBase(mb2);
        return;
    }

    for (uint32_t i = SquishUMSGID2Offset(mb1, lr1 + 1, 1); i <= mb1->basehdr.num_msg; i++) {
        sq_msg_t *msg = SquishReadMsg(mb1, i);

        if (!msg)
            break;

        msg->xmsg.attr &= ~(MSGSENT);
        msg->xmsg.attr |= MSGLOCAL;

        lr1 = msg->xmsg.umsgid;

        // strip seenby
        std::string msgcontent = std::string(msg->msg, msg->msg_len);
        msgcontent = remove_seenby_path(msgcontent);

        if (msgcontent.rfind("\r--- ") != std::string::npos) {
            size_t pos = msgcontent.rfind("\r--- ");
            msgcontent[pos + 1] = '=';
            msgcontent[pos + 2] = '=';
            msgcontent[pos + 3] = '=';
        } else if (msgcontent.rfind("\r---\r") != std::string::npos) {
            size_t pos = msgcontent.rfind("\r---\r");
            msgcontent[pos + 1] = '=';
            msgcontent[pos + 2] = '=';
            msgcontent[pos + 3] = '=';        
        }

        std::stringstream ss;

        if (linkdesc->msgbase_type2 == 3) {
            ss.str("");
            // QWKIE will append the tagline
        } else {
            ss << "\r--- Talisman Bridge/" << VERSION <<"\r * Origin: " << linkdesc->tagline2;
            if (linkdesc->msgbase_type2 > 0) {    
                ss << " (";
                if (linkdesc->msgbase_type2 == 1) {
                    ss << linkdesc->address2->zone;
                    ss << ":";
                    ss << linkdesc->address2->net;
                    ss << "/";
                    ss << linkdesc->address2->node;

                    if (linkdesc->address2->point != 0) {
                        ss << ".";
                        ss << linkdesc->address2->point;
                    }
                } else if (linkdesc->msgbase_type2 == 2) {
                    ss << "@" << linkdesc->address2->node;
                }
                ss << ")";
            }
            ss << "\r";
        }
        msgcontent.append(ss.str());

        free(msg->msg);
        msg->msg = (char *)malloc(msgcontent.size());
        if (!msg->msg) {
            goto clean_up;
        }

        memcpy(msg->msg, msgcontent.c_str(), msgcontent.size());
        msg->msg_len = msgcontent.size();

        // change origin

        memcpy(&msg->xmsg.orig, linkdesc->address2, sizeof(NETADDR));

        new_msgs1.push_back(msg);
    }

    for (uint32_t i = SquishUMSGID2Offset(mb2, lr2 + 1, 1); i <= mb2->basehdr.num_msg; i++) {
        sq_msg_t *msg = SquishReadMsg(mb2, i);

        if (!msg)
            break;

        msg->xmsg.attr &= ~(MSGSENT);
        msg->xmsg.attr |= MSGLOCAL;

        lr2 = msg->xmsg.umsgid;

        // strip seenby
        std::string msgcontent = std::string(msg->msg, msg->msg_len);
        msgcontent = remove_seenby_path(msgcontent);

        if (msgcontent.rfind("\r--- ") != std::string::npos) {
            size_t pos = msgcontent.rfind("\r--- ");
            msgcontent[pos + 1] = '=';
            msgcontent[pos + 2] = '=';
            msgcontent[pos + 3] = '=';
        } else if (msgcontent.rfind("\r---\r") != std::string::npos) {
            size_t pos = msgcontent.rfind("\r---\r");
            msgcontent[pos + 1] = '=';
            msgcontent[pos + 2] = '=';
            msgcontent[pos + 3] = '=';        
        }

        std::stringstream ss;

        if (linkdesc->msgbase_type1 == 3) {
            // QWKIE will append the tagline
            ss.str("");
        } else {
            ss << "\r--- Talisman Bridge/" << VERSION <<"\r * Origin: " << linkdesc->tagline1;
            if (linkdesc->msgbase_type1 > 0) {    
                ss << " (";
                if (linkdesc->msgbase_type1 == 1) {
                    ss << linkdesc->address1->zone;
                    ss << ":";
                    ss << linkdesc->address1->net;
                    ss << "/";
                    ss << linkdesc->address1->node;

                    if (linkdesc->address1->point != 0) {
                        ss << ".";
                        ss << linkdesc->address1->point;
                    }
                } else if (linkdesc->msgbase_type1 == 2) {
                    ss << "@" << linkdesc->address1->node;
                }
                ss << ")";
            }
            ss << "\r";
        }

        msgcontent.append(ss.str());

        free(msg->msg);
        msg->msg = (char *)malloc(msgcontent.size());
        if (!msg->msg) {
            goto clean_up;
        }

        memcpy(msg->msg, msgcontent.c_str(), msgcontent.size());
        msg->msg_len = msgcontent.size();

        // change origin

        memcpy(&msg->xmsg.orig, linkdesc->address1, sizeof(NETADDR));

        new_msgs2.push_back(msg);
    }


    if (new_msgs1.size() > 0) {
        for (size_t i = 0; i < new_msgs1.size(); i++) {
            // add to mb2
            SquishWriteMsg(mb2, new_msgs1.at(i));
            lr2 = new_msgs1.at(i)->xmsg.umsgid;
        }
    }

    if (new_msgs2.size() > 0) {
        for (size_t i = 0; i < new_msgs2.size(); i++) {
            // add to mb1
            SquishWriteMsg(mb1, new_msgs2.at(i));
            lr1 = new_msgs2.at(i)->xmsg.umsgid;
        }
    }
    
    if (lr1 != 0) {
        lastread1_fptr = fopen(std::string(msg_path + "/" + linkdesc->msgfile1 + ".bl_" + linkdesc->linkname).c_str(), "wb");
        if (lastread1_fptr) {
            fwrite(&lr1, sizeof(uint32_t), 1, lastread1_fptr);
            fclose(lastread1_fptr);
        }
    }

    if (lr2 != 0) {
        lastread2_fptr = fopen(std::string(msg_path + "/" + linkdesc->msgfile2 + ".bl_" + linkdesc->linkname).c_str(), "wb");
        if (lastread2_fptr) {
            fwrite(&lr2, sizeof(uint32_t), 1, lastread2_fptr);
            fclose(lastread2_fptr);
        }
    }
    log->log(LOG_INFO, "Bridge run for %s", linkdesc->linkname.c_str());
    log->log(LOG_INFO, "Bridged %d messages to %s lr %d", new_msgs1.size(), linkdesc->msgfile2.c_str(), lr2);
    log->log(LOG_INFO, "Bridged %d messages to %s lr %d", new_msgs2.size(), linkdesc->msgfile1.c_str(), lr1);

clean_up:

    SquishUnlockMsgBase(mb1);
    SquishUnlockMsgBase(mb2);

    SquishCloseMsgBase(mb1);
    SquishCloseMsgBase(mb2);

    for (sq_msg_t *m : new_msgs1) {
        SquishFreeMsg(m);
    }

    for (sq_msg_t *m : new_msgs2) {
        SquishFreeMsg(m);
    }

}