#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include "../Common/Squish.h"
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

void Bridge::do_bridge(std::string msgbase1, std::string origin1, std::string msgbase2, std::string origin2) {
    FILE *lastread1_fptr;
    FILE *lastread2_fptr;

    uint32_t lr1 = 0;
    uint32_t lr2 = 0;

    NETADDR *o1 = parse_fido_addr(origin1.c_str());
    NETADDR *o2 = parse_fido_addr(origin2.c_str());

    if (!o1 || !o2) {
        return;
    }

    lastread1_fptr = fopen(std::string(msgbase1 + ".blr").c_str(), "rb");
    if (lastread1_fptr) {
        fread(&lr1, sizeof(uint32_t), 1 , lastread1_fptr);
        fclose(lastread1_fptr);
    }
    
    lastread2_fptr = fopen(std::string(msgbase2 + ".blr").c_str(), "rb");
    if (lastread2_fptr) {
        fread(&lr2, sizeof(uint32_t), 1 , lastread2_fptr);
        fclose(lastread2_fptr);
    }

    lr1++;
    lr2++;

    std::vector<sq_msg_t *> new_msgs1;
    std::vector<sq_msg_t *> new_msgs2;

    sq_msg_base_t *mb1 = SquishOpenMsgBase(msgbase1.c_str());

    if (!mb1) {
        fprintf(stderr, "Failed to open %s", msgbase1.c_str());
        return;
    }

    sq_msg_base_t *mb2 = SquishOpenMsgBase(msgbase2.c_str());
    if (!mb2) {
        fprintf(stderr, "Failed to open %s", msgbase2.c_str());
        SquishCloseMsgBase(mb1);
        return;
    }

    if (!SquishLockMsgBase(mb1)) {
        fprintf(stderr, "Failed to lock %s", msgbase1.c_str());
        SquishCloseMsgBase(mb1);
        SquishCloseMsgBase(mb2);
        return;
    }
    if (!SquishLockMsgBase(mb2)) {
        fprintf(stderr, "Failed to lock %s", msgbase2.c_str());
        SquishUnlockMsgBase(mb1);
        SquishCloseMsgBase(mb1);
        SquishCloseMsgBase(mb2);
        return;
    }

    for (uint32_t i = SquishUMSGID2Offset(mb1, lr1, 1); i <= mb1->basehdr.num_msg; i++) {
        sq_msg_t *msg = SquishReadMsg(mb1, i);

        if (!msg)
            break;

        msg->xmsg.attr &= ~(MSGSENT);
        msg->xmsg.attr |= MSGLOCAL;

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

        ss << "\r---\r Talisman Bridge (";
        ss << o2->zone;
        ss << ":";
        ss << o2->net;
        ss << "/";
        ss << o2->node;

        if (o2->point != 0) {
            ss << ".";
            ss << o2->point;
        }

        ss << ")\r";

        msgcontent.append(ss.str());

        free(msg->msg);
        msg->msg = (char *)malloc(msgcontent.size());
        if (!msg->msg) {
            goto clean_up;
        }

        memcpy(msg->msg, msgcontent.c_str(), msgcontent.size());
        msg->msg_len = msgcontent.size();

        // change origin

        memcpy(&msg->xmsg.orig, o2, sizeof(NETADDR));

        new_msgs1.push_back(msg);
    }

    for (uint32_t i = SquishUMSGID2Offset(mb2, lr2, 1); i <= mb2->basehdr.num_msg; i++) {
        sq_msg_t *msg = SquishReadMsg(mb2, i);

        if (!msg)
            break;

        msg->xmsg.attr &= ~(MSGSENT);
        msg->xmsg.attr |= MSGLOCAL;


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

        ss << "\r---\r Talisman Bridge (";
        ss << o1->zone;
        ss << ":";
        ss << o1->net;
        ss << "/";
        ss << o1->node;

        if (o1->point != 0) {
            ss << ".";
            ss << o1->point;
        }

        ss << ")\r";

        msgcontent.append(ss.str());

        free(msg->msg);
        msg->msg = (char *)malloc(msgcontent.size());
        if (!msg->msg) {
            goto clean_up;
        }

        memcpy(msg->msg, msgcontent.c_str(), msgcontent.size());
        msg->msg_len = msgcontent.size();

        // change origin

        memcpy(&msg->xmsg.orig, o1, sizeof(NETADDR));

        new_msgs2.push_back(msg);
    }


    for (size_t i = 0; i < new_msgs1.size(); i++) {
        // add to mb2
        SquishWriteMsg(mb2, new_msgs1.at(i));
        lr2 = new_msgs1.at(i)->xmsg.umsgid;
    }

    for (size_t i = 0; i < new_msgs2.size(); i++) {
        // add to mb1
        SquishWriteMsg(mb1, new_msgs2.at(i));
        lr1 = new_msgs2.at(i)->xmsg.umsgid;
    }

    

    lastread1_fptr = fopen(std::string(msgbase1 + ".blr").c_str(), "wb");
    if (lastread1_fptr) {
        fwrite(&lr1, sizeof(uint32_t), 1 , lastread1_fptr);
        fclose(lastread1_fptr);
    }
    
    lastread2_fptr = fopen(std::string(msgbase2 + ".blr").c_str(), "wb");
    if (lastread2_fptr) {
        fwrite(&lr2, sizeof(uint32_t), 1 , lastread2_fptr);
        fclose(lastread2_fptr);
    }

    fprintf(stderr, "Bridged %d messages to %s\n", new_msgs1.size(), msgbase2.c_str());
    fprintf(stderr, "Bridged %d messages to %s\n", new_msgs2.size(), msgbase1.c_str());

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