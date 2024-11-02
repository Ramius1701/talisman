#include <cstring>
#ifdef _MSC_VER
#include <Windows.h>
#include <shlwapi.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#include "../Common/Squish.h"
#include "CallLog.h"
#include "Door.h"
#include "Editor.h"
#include "GenDefs.h"
#include "MsgArea.h"
#include "MsgConf.h"
#include "Node.h"
#include "Nodelist.h"
#include "Qwk.h"
#include "Script.h"
#include "Config.h"
#include "Protocol.h"
#include "MessageReader.h"

MsgArea::MsgArea(MsgConf *mc, Node *n, std::string name, std::string filename, int r, int w, int d, int down, std::string oaddr, bool netmail,
                 std::string tagline, int qwk, bool rn, int wwivnode, bool subbed_by_default) {
  this->myconf = mc;
  this->name = Config::convert_cp437(name);
  this->file = filename;
  this->read_sec_level = r;
  this->write_sec_level = w;
  this->delete_sec_level = d;
  this->delete_own_sec_level = down;
  this->n = n;
  this->orig_addr = oaddr;
  this->_is_netmail = netmail;
  this->tagline = Config::convert_cp437(tagline);
  this->qwk_base_no = qwk;
  this->real_names = rn;
  this->wwivnode = wwivnode;
  this->subbed_by_default = subbed_by_default;
}

void MsgArea::delete_message(sq_msg_base_t *mb, sq_msg_t *msg) {
  bool candelete = false;

  if (delete_sec_level > 0 && n->get_user().get_sec_level() >= delete_sec_level) {
    candelete = true;
  } else if (delete_own_sec_level > 0 && is_from_me(n, msg) && n->get_user().get_sec_level() >= delete_own_sec_level) {
    candelete = true;
  }

  if (!candelete) {
    if (n->get_config()->get_sec_level_info(n->get_user().get_sec_level())->can_delete_msgs) {
      candelete = true;
    } else if (is_from_me(n, msg) && n->get_config()->get_sec_level_info(n->get_user().get_sec_level())->can_delete_own_msgs) {
      candelete = true;
    }
  }

  if (candelete) {
    if (SquishLockMsgBase(mb)) {
      SquishDeleteMsg(mb, msg);
      SquishUnlockMsgBase(mb);
    }
  }
}

uint32_t MsgArea::umsgid_to_offset(UMSGID lr) {
  sq_msg_base_t *mb;
  uint32_t tot = 0;
  mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    return 0;
  }
  tot = SquishUMSGID2Offset(mb, lr, 1);

  SquishCloseMsgBase(mb);
  return tot;
}

int MsgArea::get_new_msgs(UMSGID lr) {
  sq_msg_base_t *mb;
  unsigned int tot = 0;
  mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    return 0;
  }

  tot = mb->basehdr.num_msg - umsgid_to_offset(lr);

  SquishCloseMsgBase(mb);
  return tot;
}

int MsgArea::get_total_msgs() {
  sq_msg_base_t *mb;
  unsigned int tot = 0;
  mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    return 0;
  }
  tot = mb->basehdr.num_msg;
  SquishCloseMsgBase(mb);
  return tot;
}

std::vector<std::string> MsgArea::word_wrap(std::string str, size_t len) {
  std::vector<std::string> strvec;
  size_t line_start = 0;
  size_t last_space = 0;

  for (size_t i = 0; i < str.size(); i++) {
    if (str[i] == ' ') {
      last_space = i;
    }
    if (i - line_start == len) {
      if (last_space == line_start) {
        strvec.push_back(str.substr(line_start, i - line_start));
        line_start = i;
        last_space = i;
      } else {
        strvec.push_back(str.substr(line_start, last_space - line_start));
        line_start = last_space + 1;
        i = line_start;
        last_space = line_start;
      }
    }
  }

  if (line_start < str.size()) {
    strvec.push_back(str.substr(line_start));
  }
  return strvec;
}

bool MsgArea::save_message(std::string to, std::string from, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to) {
  return save_message(to, from, subject, text, netaddr, inreply_to, 0);
}

bool MsgArea::save_message(std::string to, std::string from, std::string subject, std::vector<std::string> text, std::string netaddr, unsigned int inreply_to,
                           time_t date) {
  sq_msg_base_t *mb = SquishOpenMsgBase(file.c_str());

  if (!mb) {
    return false;
  }

  char replyidbuffer[256];
  char charsbuffer[] = "\001CHRS: CP437 2";
  char tzutcbuffer[256];
  char toptbuffer[256];
  char fmptbuffer[256];
  char intlbuffer[256];
  char msgidbuffer[256];
  char *msg;
  FILE *fptr;
  uint32_t msgid;
  time_t thetime;

  int at;
  struct tm lt;
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int ret;
  uint32_t repmsgid = 0;
  sq_msg_t *rep_msg = NULL;
  std::stringstream ss;

  if (date == 0) {
    thetime = time(NULL);
  } else {
    thetime = date;
  }

  for (size_t x = 0; x < text.size(); x++) {
    ss << text.at(x) << "\r";
  }

  msg = (char *)malloc(ss.str().size() + 1);
  if (!msg) {
    return false;
  }

  memset(msg, 0, ss.str().size() + 1);

  strncpy(msg, ss.str().c_str(), ss.str().size());

  std::stringstream originline;

  if (orig_addr != "" || wwivnode != 0) {
    originline << "\r--- Talisman v" << VERSION_MAJOR << "." << VERSION_MINOR << "-" << VERSION_STR << " (" << n->operating_system() << ")\r * Origin: ";
    if (tagline != "") {
      originline << tagline;
    } else {
      originline << "A Mysterious BBS";
    }
    if (orig_addr != "") {
      originline << " (" << orig_addr << ")\r";
    }
  }

  sq_msg_t newmsg;

  memset(&newmsg, 0, sizeof(newmsg));

  if (wwivnode == 0) {
#ifdef _MSC_VER
    TIME_ZONE_INFORMATION tz;
    GetTimeZoneInformation(&tz);
    int bias = tz.Bias;
    if (bias > 0) {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: -%02d%02d", abs(bias / 60), abs(bias % 60));
    } else {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: %02d%02d", abs(bias / 60), abs(bias % 60));
    }
#else
    time_t gmt, rawtime = time(NULL);
    struct tm *ptm;

    struct tm gbuf;
    ptm = gmtime_r(&rawtime, &gbuf);
    // Request that mktime() looksup dst in timezone database
    ptm->tm_isdst = -1;
    gmt = mktime(ptm);

    int bias = (int)difftime(rawtime, gmt);
    bias /= 60;
    if (bias < 0) {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: -%02d%02d", abs(bias / 60), abs(bias % 60));
    } else {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: %02d%02d", abs(bias / 60), abs(bias % 60));
    }
#endif

    memset(replyidbuffer, 0, 256);

    if (inreply_to > 0) {
      rep_msg = SquishReadMsg(mb, umsgid_to_offset(inreply_to));
      if (rep_msg != NULL && rep_msg->xmsg.attr & MSGUID) {
        repmsgid = rep_msg->xmsg.umsgid;
        for (int i = 0; i < rep_msg->ctrl_len - 8; i++) {
          if (strncmp(&rep_msg->ctrl[i], "\x01MSGID: ", 8) == 0) {
            int h = 8;
            snprintf(replyidbuffer, sizeof replyidbuffer, "\001REPLY: ");
            for (int j = i + 8; j < rep_msg->ctrl_len && rep_msg->ctrl[j] != '\x01'; j++) {
              replyidbuffer[h++] = rep_msg->ctrl[j];
            }
            break;
          }
        }
      }
    }

    memset(msgidbuffer, 0, 256);

    fptr = fopen(std::string(n->get_config()->data_path() + "/msgserial.dat").c_str(), "rb");

    if (!fptr) {
      msgid = (uint32_t)thetime;
    } else {
      fread(&msgid, sizeof(uint32_t), 1, fptr);
      fclose(fptr);

      if (thetime > msgid) {
        msgid = (uint32_t)thetime;
      } else {
        msgid++;
      }
    }

    fptr = fopen(std::string(n->get_config()->data_path() + "/msgserial.dat").c_str(), "wb");
    if (fptr) {
      fwrite(&msgid, sizeof(uint32_t), 1, fptr);
      fclose(fptr);
    }
    if (orig_addr != "") {
      snprintf(msgidbuffer, sizeof msgidbuffer, "\x01MSGID: %s %08X", orig_addr.c_str(), msgid);
    } else {
      std::stringstream sanitizefile;

      for (size_t s = 0; s < file.size(); s++) {
        if (file[s] == '/' || file[s] == '\\') {
          sanitizefile << '_';
        } else {
          sanitizefile << file[s];
        }
      }

      snprintf(msgidbuffer, sizeof msgidbuffer, "\x01MSGID: <%x.%u.%s@%s>", msgid, mb->basehdr.uid, sanitizefile.str().c_str(),
               n->get_config()->get_hostname().c_str());
    }

    // are we a netmail
    newmsg.ctrl_len = strlen(msgidbuffer) + strlen(tzutcbuffer) + strlen(replyidbuffer) + strlen(charsbuffer);

    if (orig_addr != "") {
      NETADDR *orig = parse_fido_addr(orig_addr.c_str());
      if (orig != NULL) {
        newmsg.xmsg.orig.zone = orig->zone;
        newmsg.xmsg.orig.net = orig->net;
        newmsg.xmsg.orig.node = orig->node;
        newmsg.xmsg.orig.point = orig->point;
        free(orig);
      } else {
        newmsg.xmsg.orig.zone = 0;
        newmsg.xmsg.orig.net = 0;
        newmsg.xmsg.orig.node = 0;
        newmsg.xmsg.orig.point = 0;
      }
    } else {
      newmsg.xmsg.orig.zone = 0;
      newmsg.xmsg.orig.net = 0;
      newmsg.xmsg.orig.node = 0;
      newmsg.xmsg.orig.point = 0;
    }

    if (netaddr != "") {
      NETADDR *dest = parse_fido_addr(netaddr.c_str());
      if (dest != NULL) {
        newmsg.xmsg.dest.zone = dest->zone;
        newmsg.xmsg.dest.net = dest->net;
        newmsg.xmsg.dest.node = dest->node;
        newmsg.xmsg.dest.point = dest->point;
        free(dest);
        snprintf(intlbuffer, sizeof intlbuffer, "\x01INTL %d:%d/%d %d:%d/%d", newmsg.xmsg.dest.zone, newmsg.xmsg.dest.net, newmsg.xmsg.dest.node,
                 newmsg.xmsg.orig.zone, newmsg.xmsg.orig.net, newmsg.xmsg.orig.node);
        newmsg.ctrl_len += strlen(intlbuffer);
        if (newmsg.xmsg.dest.point > 0) {
          snprintf(toptbuffer, sizeof toptbuffer, "\x01TOPT %d", newmsg.xmsg.dest.point);
          newmsg.ctrl_len += strlen(toptbuffer);
        }
        if (newmsg.xmsg.orig.point > 0) {
          snprintf(fmptbuffer, sizeof fmptbuffer, "\001FMPT %d", newmsg.xmsg.orig.point);
          newmsg.ctrl_len += strlen(fmptbuffer);
        }
      } else {
        newmsg.xmsg.dest.zone = 0;
        newmsg.xmsg.dest.net = 0;
        newmsg.xmsg.dest.node = 0;
        newmsg.xmsg.dest.point = 0;
      }
    }
    newmsg.ctrl = (char *)malloc(newmsg.ctrl_len);
    if (!newmsg.ctrl) {
      free(msg);
      return false;
    }
    at = 0;
    memcpy(newmsg.ctrl, tzutcbuffer, strlen(tzutcbuffer));
    at += strlen(tzutcbuffer);
    memcpy(&newmsg.ctrl[at], charsbuffer, strlen(charsbuffer));
    at += strlen(charsbuffer);
    if (orig_addr != "" || wwivnode == 0) {
      memcpy(&newmsg.ctrl[at], msgidbuffer, strlen(msgidbuffer));
      at += strlen(msgidbuffer);
    }
    if (inreply_to > 0) {
      memcpy(&newmsg.ctrl[at], replyidbuffer, strlen(replyidbuffer));
      at += strlen(replyidbuffer);
    }
    if (newmsg.xmsg.dest.zone != 0) {
      if (newmsg.xmsg.dest.point != 0) {
        memcpy(&newmsg.ctrl[at], toptbuffer, strlen(toptbuffer));
        at += strlen(toptbuffer);
      }
      if (newmsg.xmsg.orig.point != 0) {
        memcpy(&newmsg.ctrl[at], fmptbuffer, strlen(fmptbuffer));
        at += strlen(fmptbuffer);
      }
      memcpy(&newmsg.ctrl[at], intlbuffer, strlen(intlbuffer));
      // at += strlen(intlbuffer);
    }

  } else {
    newmsg.ctrl = NULL;
    newmsg.ctrl_len = 0;

    newmsg.xmsg.orig.zone = 20000;
    newmsg.xmsg.orig.net = 20000;
    newmsg.xmsg.orig.node = wwivnode;
    newmsg.xmsg.orig.point = 0;

    if (netaddr != "") {
      newmsg.xmsg.dest.zone = 20000;
      newmsg.xmsg.dest.net = 20000;
      newmsg.xmsg.dest.node = stoi(netaddr);
      newmsg.xmsg.dest.point = 0;
    }
  }

  if ((orig_addr != "" || wwivnode != 0) && !is_netmail()) {
    newmsg.msg_len = strlen(msg) + originline.str().size();
    newmsg.msg = (char *)malloc(strlen(msg) + originline.str().size());
    if (!newmsg.msg) {
      free(newmsg.ctrl);
      free(msg);
      return false;
    }
    memcpy(newmsg.msg, msg, strlen(msg));
    memcpy(&newmsg.msg[strlen(msg)], originline.str().c_str(), originline.str().size());
  } else {
    newmsg.msg_len = strlen(msg);
    newmsg.msg = (char *)malloc(strlen(msg));
    if (!newmsg.msg) {
      free(newmsg.ctrl);
      free(msg);
      return false;
    }
    memcpy(newmsg.msg, msg, strlen(msg));
  }

  strncpy(newmsg.xmsg.subject, subject.c_str(), 71);
  strncpy(newmsg.xmsg.from, from.c_str(), 35);
  strncpy(newmsg.xmsg.to, to.c_str(), 35);

  newmsg.xmsg.replyto = repmsgid;
  newmsg.xmsg.attr = MSGLOCAL | MSGUID;

  if (netaddr != "") {
    newmsg.xmsg.attr |= MSGPRIVATE;
  }
#if _MSC_VER
  localtime_s(&lt, &thetime);
#else
  localtime_r(&thetime, &lt);
#endif
  newmsg.xmsg.date_written.date |= (((sq_word)lt.tm_mday) & 31);
  newmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_mon + 1)) & 15) << 5;
  newmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_year - 80)) & 127) << 9;

  newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_sec) & 31);
  newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_min) & 63) << 5;
  newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_hour) & 31) << 11;

  newmsg.xmsg.date_arrived.date = newmsg.xmsg.date_written.date;
  newmsg.xmsg.date_arrived.time = newmsg.xmsg.date_written.time;

  snprintf(newmsg.xmsg.__ftsc_date, 20, "%02d %s %02d  %02d:%02d:%02d", lt.tm_mday, months[lt.tm_mon], lt.tm_year - 100, lt.tm_hour, lt.tm_min, lt.tm_sec);

  SquishLockMsgBase(mb);
  ret = SquishWriteMsg(mb, &newmsg);
  if (rep_msg != NULL) {
    for (int i = 0; i < 9; i++) {
      if (rep_msg->xmsg.replies[i] == 0) {
        rep_msg->xmsg.replies[i] = newmsg.xmsg.umsgid;
        SquishUpdateHdr(mb, rep_msg);
        break;
      }
    }
    SquishFreeMsg(rep_msg);
  }
  SquishUnlockMsgBase(mb);
  SquishCloseMsgBase(mb);

  free(msg);
  free(newmsg.msg);
  free(newmsg.ctrl);

  if (netaddr != "") {
    do_semaphore(n->get_config()->netmail_sem());
  } else if (orig_addr != "") {
    do_semaphore(n->get_config()->echomail_sem());
  } else if (wwivnode != 0) {
    do_semaphore(n->get_config()->echomail_sem());
  }

  return (ret == 1);
}

void MsgArea::do_semaphore(std::string sem) {
  time_t thetime = time(NULL);

  FILE *fptr = fopen(sem.c_str(), "w");
  if (fptr) {
    fwrite(&thetime, sizeof(time_t), 1, fptr);
    fclose(fptr);
  }
}

std::vector<std::string> MsgArea::strip_ansi(const char *msg, size_t len) {
  std::stringstream output;
  std::vector<std::string> ansi_msg = demangle_ansi(n, msg, len);
  std::vector<std::string> new_msg;

  for (size_t i = 0; i < ansi_msg.size(); i++) {
    for (size_t j = 0; j < ansi_msg.at(i).size(); j++) {
      if (ansi_msg.at(i).at(j) == '\x1b') {
        while (j < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", ansi_msg.at(i).at(j)) == NULL)
          j++;
      } else if (ansi_msg.at(i).at(j) != '\n' && ansi_msg.at(i).at(j) != '\r') {
        output << ansi_msg.at(i).at(j);
      }
    }
    new_msg.push_back(output.str());
    output.str("");
  }

  return new_msg;
}

struct character_t {
  char c;
  int fg_color;
  int bg_color;
  bool bold;
};

std::vector<std::string> MsgArea::demangle_ansi(Node *n, const char *msg, size_t len) {
  std::vector<std::string> new_msg;
  int lines = 0;
  int line_at = 0;
  int col_at = 0;
  int params[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  int param_count = 0;
  int fg_color = 7;
  int bg_color = 0;
  bool bold = false;
  int save_col = 0;
  int save_row = 0;

  if (msg == NULL || len == 0) {
    return new_msg;
  }

  for (size_t i = 0; i < len; i++) {
    if (msg[i] == '\r' || (i >= 1 && msg[i] == '\n' && msg[i - 1] != '\r')) {
      line_at++;
      if (line_at > lines) {
        lines = line_at;
      }
      col_at = 0;
    } else if (msg[i] == '\x1b') {
      i++;
      if (msg[i] != '[') {
        i--;
        continue;
      } else {
        param_count = 0;
        while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", msg[i]) == NULL) {
          if (msg[i] == ';') {
            param_count++;
          } else if (msg[i] >= '0' && msg[i] <= '9') {
            if (param_count == 0) {
              param_count = 1;
              for (int j = 0; j < 9; j++) {
                params[j] = 0;
              }
            }
            params[param_count - 1] = params[param_count - 1] * 10 + (msg[i] - '0');
          }
          i++;
        }
        switch (msg[i]) {
        case 'A':
          if (param_count > 0) {
            line_at -= params[0];
          } else {
            line_at--;
          }
          if (line_at < 0)
            line_at = 0;
          break;
        case 'B':
          if (param_count > 0) {
            line_at += params[0];
          } else {
            line_at++;
          }
          if (line_at > lines) {
            lines = line_at;
          }
          break;
        case 'C':
          if (param_count > 0) {
            col_at += params[0];
          } else {
            col_at++;
          }
          if (col_at > (int)n->get_term_width()) {
            col_at = n->get_term_width();
          }
          break;
        case 'D':
          if (param_count > 0) {
            col_at -= params[0];
          } else {
            col_at--;
          }
          if (col_at < 0)
            col_at = 0;
          break;
        case 'H':
        case 'f':
          if (param_count > 1) {
            params[0]--;
            params[1]--;
          }
          line_at = params[0];
          col_at = params[1];

          if (line_at > lines) {
            lines = line_at;
          }
          if (col_at > (int)n->get_term_width()) {
            col_at = n->get_term_width();
          }
          if (line_at < 0)
            line_at = 0;
          if (col_at < 0)
            col_at = 0;
          break;
        case 'u':
          col_at = save_col;
          line_at = save_row;
          break;
        case 's':
          save_col = col_at;
          save_row = line_at;
          break;
        case 'm':
          break;
        default:

          break;
        }
      }
    } else if (msg[i] != '\n') {
      col_at++;

      if (col_at >= (int)n->get_term_width()) {
        col_at = 0;
        line_at++;
        if (line_at > lines) {
          lines = line_at;
        }
      }
    }
  }

  struct character_t **fakescreen = (struct character_t **)malloc(sizeof(struct character_t *) * (lines + 1));

  if (!fakescreen) {
    return new_msg;
  }

  for (int i = 0; i <= lines; i++) {
    fakescreen[i] = (struct character_t *)malloc(sizeof(struct character_t) * (n->get_term_width() + 1));
    if (!fakescreen[i]) {
      for (int j = i - 1; j >= 0; j--) {
        free(fakescreen[j]);
      }
      free(fakescreen);
      return new_msg;
    }
    for (size_t x = 0; x <= n->get_term_width(); x++) {
      fakescreen[i][x].c = ' ';
      fakescreen[i][x].fg_color = 7;
      fakescreen[i][x].bg_color = 0;
    }
  }
  line_at = 0;
  col_at = 0;
  save_row = 0;
  save_col = 0;
  for (size_t i = 0; i < len; i++) {
    if (msg[i] == '\r' || (i >= 1 && msg[i] == '\n' && msg[i - 1] != '\r')) {
      line_at++;
      col_at = 0;
    } else if (msg[i] == '\x1b') {
      i++;
      if (msg[i] != '[') {
        i--;
        continue;
      } else {
        param_count = 0;
        while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", msg[i]) == NULL) {
          if (msg[i] == ';') {
            param_count++;
          } else if (msg[i] >= '0' && msg[i] <= '9') {
            if (param_count == 0) {
              param_count = 1;
              for (int j = 0; j < 9; j++) {
                params[j] = 0;
              }
            }
            params[param_count - 1] = params[param_count - 1] * 10 + (msg[i] - '0');
          }
          i++;
        }
        switch (msg[i]) {
        case 'A':
          if (param_count > 0) {
            line_at -= params[0];
          } else {
            line_at--;
          }
          if (line_at < 0)
            line_at = 0;
          break;
        case 'B':
          if (param_count > 0) {
            line_at += params[0];
          } else {
            line_at++;
          }
          break;
        case 'C':
          if (param_count > 0) {
            col_at += params[0];
          } else {
            col_at++;
          }
          if (col_at > (int)n->get_term_width()) {
            col_at = n->get_term_width();
          }
          break;
        case 'D':
          if (param_count > 0) {
            col_at -= params[0];
          } else {
            col_at--;
          }
          if (col_at < 0)
            col_at = 0;
          break;
        case 'H':
        case 'f':
          if (param_count > 1) {
            params[0]--;
            params[1]--;
          }
          line_at = params[0];
          col_at = params[1];
          if (line_at < 0)
            line_at = 0;
          if (col_at < 0)
            col_at = 0;
          if (col_at > (int)n->get_term_width())
            col_at = n->get_term_width();
          break;
        case 'm':
          for (int z = 0; z < param_count; z++) {
            if (params[z] == 0) {
              bold = false;
              fg_color = 7;
              bg_color = 0;
            } else if (params[z] == 1) {
              bold = true;
            } else if (params[z] == 2) {
              bold = false;
            }

            else if (params[z] >= 30 && params[z] <= 37) {
              fg_color = params[z] - 30;
            } else if (params[z] >= 40 && params[z] <= 47) {
              bg_color = params[z] - 40;
            }
          }
          break;
        case 'u':
          col_at = save_col;
          line_at = save_row;
          break;
        case 's':
          save_col = col_at;
          save_row = line_at;
          break;
        }
      }
    } else if (msg[i] != '\n') {
      fakescreen[line_at][col_at].c = msg[i];
      fakescreen[line_at][col_at].bold = bold;
      fakescreen[line_at][col_at].fg_color = fg_color;
      fakescreen[line_at][col_at].bg_color = bg_color;
      col_at++;
      if (col_at >= (int)n->get_term_width()) {
        line_at++;
        col_at = 0;
      }
    }
  }

  for (int i = 0; i < lines; i++) {
    for (int j = n->get_term_width() - 1; j >= 0; j--) {
      if (fakescreen[i][j].c == ' ') {
        fakescreen[i][j].c = '\0';
      } else {
        break;
      }
    }
  }

  std::stringstream ss;

  fg_color = 7;
  bg_color = 0;
  bold = false;
  bool got_tearline = false;
  for (int i = 0; i < lines; i++) {
    ss.str("");
    size_t j;

    if ((fakescreen[i][0].c == '-' && fakescreen[i][1].c == '-' && fakescreen[i][2].c == '-') && (fakescreen[i][3].c == 0 || fakescreen[i][3].c == ' ')) {
      got_tearline = true;
    }

    if (!got_tearline) {
      if (fakescreen[i][0].c != '\001') {
        if (bold) {
          ss << "\x1b[1m";
        } else {
          ss << "\x1b[0m";
        }

        ss << "\x1b[" << std::to_string(fg_color + 30) << "m";
        ss << "\x1b[" << std::to_string(bg_color + 40) << "m";
      }
    }
    for (j = 0; j < n->get_term_width(); j++) {
      if (fakescreen[i][j].c == '\0') {
        break;
      }
      if (!got_tearline) {
        bool reset = false;
        if (fakescreen[i][j].bold != bold) {
          bold = fakescreen[i][j].bold;
          if (bold) {
            ss << "\x1b[1m";
          } else {
            ss << "\x1b[0m";
            reset = true;
          }
        }

        if (fakescreen[i][j].fg_color != fg_color || reset) {
          fg_color = fakescreen[i][j].fg_color;
          ss << "\x1b[" << std::to_string(fg_color + 30) << "m";
        }
        if (fakescreen[i][j].bg_color != bg_color) {
          bg_color = fakescreen[i][j].bg_color;
          ss << "\x1b[" << std::to_string(bg_color + 40) << "m";
        }
      }
      ss << fakescreen[i][j].c;
    }
    if (j < n->get_term_width()) {
      if (!got_tearline) {
        ss << "\r\n";
      }
    }
    new_msg.push_back(ss.str());
  }

  for (int i = 0; i <= lines; i++) {
    free(fakescreen[i]);
  }
  free(fakescreen);

  return new_msg;
}

bool MsgArea::prepare_msg(sq_msg_t *msg, std::vector<struct line_t> *linesv, std::vector<std::string> *quotebuffer, bool manual_kludge = false) {
  std::stringstream ss;

  ss.str("");
  for (int i = 0; i < msg->ctrl_len; i++) {
    if (msg->ctrl[i] == '\x01' && ss.str().size() > 0) {
      if (ss.str().size() > n->get_term_width() - 1) {
        int type = 2;
        std::vector<std::string> newvec = word_wrap("\x01" + ss.str(), n->get_term_width() - 1);

        for (size_t z = 0; z < newvec.size(); z++) {
          struct line_t nline;
          nline.line = newvec.at(z);
          nline.type = type;
          linesv->push_back(nline);
        }
      } else if (ss.str().size() > 0) {
        int type = 2;
        struct line_t nline;
        nline.line = "\x01" + ss.str();
        nline.type = type;
        linesv->push_back(nline);
      }
      ss.str("");
    } else if (msg->ctrl[i] != '\x01' && msg->ctrl[i] != '\r') {
      ss << msg->ctrl[i];
    }
  }
  if (ss.str().size() > 0) {
    int type = 2;
    struct line_t nline;
    nline.line = "\x01" + ss.str();
    nline.type = type;
    linesv->push_back(nline);
  }
  ss.str("");

  bool ansimsg = false;

  for (int i = 0; i < msg->msg_len; i++) {
    if (msg->msg[i] == '\x1b') {
      ansimsg = true;
      break;
    }
  }

  bool got_tearline = false;

  if (ansimsg && n->hasANSI) {
    std::vector<std::string> new_msg = demangle_ansi(n, msg->msg, msg->msg_len);
    for (size_t i = 0; i < new_msg.size(); i++) {
      int type = 0;
      if (!got_tearline) {
        if (new_msg.at(i).find('>') < 5) {
          type = 1;
        } else if (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01') {
          type = 2;
        } else if (new_msg.at(i) == "---" || new_msg.at(i).find("--- ") == 0) {
          got_tearline = true;
          type = 3;
        }
      } else {
        if (new_msg.at(i).find("SEEN-BY: ") == 0 || (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01')) {
          type = 2;
        } else {
          type = 3;
        }
      }

      struct line_t nline;
      nline.line = new_msg.at(i);
      nline.type = type;
      linesv->push_back(nline);
    }
  } else {
    std::vector<std::string> new_msg;
    if (ansimsg) {
      new_msg = strip_ansi(msg->msg, msg->msg_len);
    } else {
      std::stringstream ss;
      for (size_t i = 0; i < (size_t)msg->msg_len; i++) {
        if (msg->msg[i] == '\r') {
          new_msg.push_back(ss.str());
          ss.str("");
        }

        else if (msg->msg[i] != '\n') {
          ss << msg->msg[i];
        }
      }
      new_msg.push_back(ss.str());
    }
    for (size_t i = 0; i < new_msg.size(); i++) {
      if (new_msg.at(i).size() > n->get_term_width() - 1) {
        int type = 0;
        if (!got_tearline) {
          if (new_msg.at(i).find('>') < 5) {
            type = 1;
          } else if (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01') {
            type = 2;
          } else if (new_msg.at(i) == "---" || new_msg.at(i).find("--- ") == 0) {
            got_tearline = true;
            type = 3;
          } else {
            type = 0;
          }
        } else {
          if (new_msg.at(i).find("SEEN-BY: ") == 0 || (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01')) {
            type = 2;
          } else {
            type = 3;
          }
        }

        std::vector<std::string> newvec = word_wrap(new_msg.at(i), n->get_term_width() - 1);

        for (size_t z = 0; z < newvec.size(); z++) {
          struct line_t nline;
          nline.line = newvec.at(z);
          nline.type = type;
          linesv->push_back(nline);
        }
      } else {
        int type = 0;
        if (!got_tearline) {
          if (new_msg.at(i).find('>') < 5) {
            type = 1;
          } else if (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01') {
            type = 2;
          } else if (new_msg.at(i) == "---" || new_msg.at(i).find("--- ") == 0) {
            got_tearline = true;
            type = 3;
          } else {
            type = 0;
          }
        } else {
          if (new_msg.at(i).find("SEEN-BY: ") == 0 || (new_msg.at(i).size() > 0 && new_msg.at(i).at(0) == '\x01')) {
            type = 2;
          } else {
            type = 3;
          }
        }
        struct line_t nline;
        nline.line = new_msg.at(i);
        nline.type = type;
        linesv->push_back(nline);
      }
    }
  }
  quotebuffer->clear();
  ss.str("");

  if (n->get_user().get_attribute("viewkludges", "false") == "true" || manual_kludge) {
    for (int i = 0; i < msg->ctrl_len; i++) {
      if (msg->ctrl[i] == '\x01' && ss.str().size() > 0) {
        if (ss.str().size() > 69) {
          std::vector<std::string> newvec = word_wrap("@" + ss.str(), 70);

          for (size_t z = 0; z < newvec.size(); z++) {
            quotebuffer->push_back(" > " + newvec.at(z));
          }
        } else {
          quotebuffer->push_back(" > @" + ss.str());
        }
        ss.str("");
      } else if (msg->ctrl[i] != '\x01') {
        ss << msg->ctrl[i];
      }
    }
    if (ss.str().size() > 0) {
      quotebuffer->push_back(" > @" + ss.str());
    }
  }

  std::vector<std::string> q_msg;

  if (ansimsg) {
    q_msg = strip_ansi(msg->msg, msg->msg_len);
  } else {
    std::stringstream ss;
    for (size_t m = 0; m < (size_t)msg->msg_len; m++) {
      if (msg->msg[m] == '\r') {
        q_msg.push_back(ss.str());
        ss.str("");
      } else if (msg->msg[m] != '\n') {
        ss << msg->msg[m];
      }
    }
    if (ss.str().size() > 0) {
      q_msg.push_back(ss.str());
    }
  }

  for (size_t i = 0; i < q_msg.size(); i++) {
    if (!manual_kludge && q_msg.at(i).size() > 0 && n->get_user().get_attribute("viewkludges", "false") == "false" &&
        (q_msg.at(i).at(0) == '\x01' || q_msg.at(i).find("SEEN-BY: ") == 0)) {
      continue;
    } else {
      if (q_msg.at(i).size() > 0 && q_msg.at(i).at(0) == '\x01') {
        q_msg.at(i).at(0) = '@';
      }
      if (q_msg.at(i).size() > 70) {
        std::vector<std::string> newvec = word_wrap(q_msg.at(i), 70);

        for (size_t z = 0; z < newvec.size(); z++) {
          std::stringstream ss2;
          ss2 << " > " << newvec.at(z);
          quotebuffer->push_back(ss2.str());
        }
      } else {
        std::stringstream ss2;
        ss2 << " > " << q_msg.at(i);
        quotebuffer->push_back(ss2.str());
      }
    }
  }
  return ansimsg;
}

void MsgArea::attach_sig(std::vector<std::string> *msg, std::string sig) {
  std::stringstream ss(sig);
  std::string line;

  if (sig.size() > 0) {
    while (getline(ss, line, '\r')) {
      msg->push_back(line);
    }
  }
}

void MsgArea::reply_to_msg(sq_msg_t *msg, std::vector<std::string> *quotebuffer) {
  if ((orig_addr != "" || wwivnode != 0) && !_is_netmail) {
    for (size_t i = 0; i < myconf->areas.size(); i++) {
      if (myconf->areas.at(i)->is_netmail() && myconf->areas.at(i)->get_w_sec_level() <= n->get_user().get_sec_level()) {
        n->print_f("\r\n|14Reply via Netmail ? (Y/[N]) |07");
        char rep = tolower(n->getch());
        if (rep == 'y') {
          std::stringstream netaddr;
          if (wwivnode == 0) {
            netaddr << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
          } else {
            netaddr << msg->xmsg.orig.node;
          }
          bool doabort = false;
          n->print_f("\r\n     To: ");
          std::string to = n->get_string(35, false, false, std::string(msg->xmsg.from));
          n->print_f("\r\nSubject: ");
          std::string subject = n->get_string(60, false, false, std::string(msg->xmsg.subject));
          n->print_f("\r\nAddress: ");
          std::string nnetaddr = n->get_string(16, false, false, netaddr.str());
          if (to.size() == 0 || strcasecmp(to.c_str(), "ALL") == 0) {
            doabort = true; // don't send netmail to "ALL"
          }
          if (wwivnode == 0) {
            NETADDR *na = parse_fido_addr(nnetaddr.c_str());
            if (!na) {
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
              int nn = stoi(nnetaddr);
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
          if (subject.size() > 0 && !doabort) {
            std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, name, true, quotebuffer);
            if (nmsg.size() > 0) {
              // attach signature
              if (n->get_user().get_attribute("signature_enabled", "false") == "true") {
                attach_sig(&nmsg, n->get_user().get_attribute("signature", ""));
              }

              if (myconf->areas.at(i)->get_real_names()) {
                myconf->areas.at(i)->save_message(to, n->get_user().get_attribute("fullname", n->get_user().get_username()), subject, nmsg, nnetaddr,
                                                  msg->xmsg.umsgid);
              } else {
                myconf->areas.at(i)->save_message(to, n->get_user().get_username(), subject, nmsg, nnetaddr, msg->xmsg.umsgid);
              }
              n->clog->post_msg();
              n->get_user().inc_attrib("msgs_posted");
            }
          }
          return;
        } else {
          break;
        }
      }
    }
  }

  if (write_sec_level > n->get_user().get_sec_level()) {
    n->print_f("\r\n|12Sorry, you don't have access to post in this area!|07\r\n");
    n->pause();
    return;
  }
  if (_is_netmail) {
    std::stringstream netaddr;
    if (wwivnode == 0) {
      netaddr << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
    } else {
      netaddr << msg->xmsg.orig.node;
    }

    n->print_f("\r\n     To: ");
    std::string to = n->get_string(35, false, false, std::string(msg->xmsg.from));
    n->print_f("\r\nSubject: ");
    std::string subject = n->get_string(60, false, false, std::string(msg->xmsg.subject));
    n->print_f("\r\nAddress: ");
    std::string nnetaddr = n->get_string(16, false, false, netaddr.str());

    bool doabort = false;

    if (to.size() == 0 || strcasecmp(to.c_str(), "ALL") == 0) {
      doabort = true; // don't send netmail to "ALL"
    }

    if (wwivnode == 0) {
      NETADDR *na = parse_fido_addr(nnetaddr.c_str());
      if (!na) {
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
        int nn = stoi(nnetaddr);
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
    if (subject.size() > 0 && !doabort) {
      std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, name, true, quotebuffer);
      if (nmsg.size() > 0) {
        // attach signature
        if (n->get_user().get_attribute("signature_enabled", "false") == "true") {
          attach_sig(&nmsg, n->get_user().get_attribute("signature", ""));
        }

        if (real_names) {
          save_message(to, n->get_user().get_attribute("fullname", n->get_user().get_username()), subject, nmsg, nnetaddr, msg->xmsg.umsgid);
        } else {
          save_message(to, n->get_user().get_username(), subject, nmsg, nnetaddr, msg->xmsg.umsgid);
        }
        n->clog->post_msg();
        n->get_user().inc_attrib("msgs_posted");
      }
    }
  } else {
    n->print_f("\r\n     To: ");
    std::string to = n->get_string(35, false, false, std::string(msg->xmsg.from));
    n->print_f("\r\nSubject: ");
    std::string subject = n->get_string(60, false, false, std::string(msg->xmsg.subject));

    if (to.size() == 0) {
      to = "All";
    }

    if (subject.size() > 0) {
      std::vector<std::string> nmsg = Editor::enter_message(n, to, subject, name, false, quotebuffer);
      if (nmsg.size() > 0) {
        if (n->get_user().get_attribute("signature_enabled", "false") == "true") {
          attach_sig(&nmsg, n->get_user().get_attribute("signature", ""));
        }
        if (real_names) {
          save_message(to, n->get_user().get_attribute("fullname", n->get_user().get_username()), subject, nmsg, "", msg->xmsg.umsgid);
        } else {
          save_message(to, n->get_user().get_username(), subject, nmsg, "", msg->xmsg.umsgid);
        }
        n->clog->post_msg();
        n->get_user().inc_attrib("msgs_posted");
      }
    }
  }
}

void MsgArea::read_message(int start, int *last) { read_message(start, false, false, true, last); }

bool MsgArea::read_message(int start, bool search, bool unread, bool set_last_read, int *last) {
  return read_message(start, search, unread, set_last_read, last, false);
}

bool MsgArea::read_message(int start, bool search, bool unread, bool set_last_read, int *last, bool personal) {
  sq_msg_base_t *mb;
  bool manual_kludge = false;
  bool fsr = (n->get_user().get_attribute("fullscreenreader", "true") == "true" && n->hasANSI);
  mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    n->print_f("|14Unable to open message base!|07\r\n");
    return false;
  }
  if (start > (int)mb->basehdr.num_msg) {
    n->print_f("|14Empty message base!|07\r\n");
    SquishCloseMsgBase(mb);
    return false;
  }
  int total_msgs = mb->basehdr.num_msg;
  int msg_to_read = start;
  int direction = 1;

  std::vector<struct line_t> linesv;
  std::vector<std::string> quotebuffer;

  while (true) {
    sq_msg_t *msg = SquishReadMsg(mb, msg_to_read);
    if (msg == NULL) {
      SquishCloseMsgBase(mb);
      if (search || unread || personal) {
        return true;
      }
      return false;
    }
    if ((msg->xmsg.attr & MSGPRIVATE && !is_to_me(n, msg) && !is_from_me(n, msg)) || (personal && !is_to_me(n, msg))) {
      if (direction == 1) {
        msg_to_read++;
      } else {
        msg_to_read--;
      }
      continue;
    }

    if (last != NULL) {
      *last = msg_to_read;
    }

    if (set_last_read) {
      if (n->get_user().user_get_lastread(file) < msg->xmsg.umsgid) {
        n->get_user().user_set_lastread(file, msg->xmsg.umsgid);
      }
    }

    linesv.clear();
    quotebuffer.clear();

    bool ansimsg = prepare_msg(msg, &linesv, &quotebuffer, manual_kludge);
    bool isutf8 = false;

    for (size_t i = 0; i < linesv.size(); i++) {
      if (linesv.at(i).type == 2) {
        if (linesv.at(i).line.find("CHRS: UTF-8 4") != std::string::npos) {
          isutf8 = true;
          break;
        }
      }
    }

    if (isutf8) {
      for (size_t i = 0; i < linesv.size(); i++) {
        if (linesv.at(i).type != 2) {
          linesv.at(i).line = Config::convert_cp437(linesv.at(i).line);
        }
      }
    }

    struct msg_reader_msg_t msg_r_msg;

    msg_r_msg.area = this;
    msg_r_msg.body = &linesv;
    msg_r_msg.from = std::string(msg->xmsg.from);
    msg_r_msg.to = std::string(msg->xmsg.to);
    msg_r_msg.subject = std::string(msg->xmsg.subject);
    msg_r_msg.destaddr = &msg->xmsg.dest;
    msg_r_msg.origaddr = &msg->xmsg.orig;
    if (is_netmail()) {
      msg_r_msg.msg_type = 2;
    } else if (is_echomail()) {
      msg_r_msg.msg_type = 1;
    } else {
      msg_r_msg.msg_type = 0;
    }
    char datebuf[17];
    snprintf(datebuf, 17, "%04d-%02d-%02d %02d:%02d", ((msg->xmsg.date_written.date >> 9) & 127) + 1980, (msg->xmsg.date_written.date >> 5) & 15,
             msg->xmsg.date_written.date & 31, (msg->xmsg.date_written.time >> 11) & 31, (msg->xmsg.date_written.time >> 5) & 63);
    msg_r_msg.date = std::string(datebuf);
    msg_r_msg.msg_serial = msg->xmsg.umsgid;
    msg_r_msg.msg_no = msg_to_read;
    msg_r_msg.ansi = ansimsg;
    msg_r_msg.showkluges = n->get_user().get_attribute("viewkludges", "false") == "true" || manual_kludge;

    uint32_t flags = 0;
    if (!unread && !personal) {
      flags |= DISABLE_UNREAD;
    }
    if (!search) {
      flags |= DISABLE_SEARCH;
    }

    int ret = MessageReader::read_message(n, &msg_r_msg, total_msgs, flags);

    switch (ret) {
    case 0: // Close and stop search
      SquishCloseMsgBase(mb);
      return false;
    case 1: // close and continue search
      SquishCloseMsgBase(mb);
      return true;
    case 2: // delete message
      delete_message(mb, msg);
      SquishCloseMsgBase(mb);
      return false;
    case 3: // reply to message
      reply_to_msg(msg, &quotebuffer);
      break;
    case 4: // reshow message
      direction = 1;
      break;
    case 5: // next message
      direction = 1;
      msg_to_read++;
      break;
    case 6: // previous message
      direction = 0;
      msg_to_read--;
      break;
    case 7: // toggle kludges
      manual_kludge = !manual_kludge;
      break;
    case 8: // download message
      download(n, msg);
      break;
    case 9:
      if (search || unread || personal) {
        SquishCloseMsgBase(mb);
        return true;
      }
      break;
    }
  }
}

bool MsgArea::is_from_me(Node *n, sq_msg_t *msg) {
  if (strcasecmp(msg->xmsg.from, n->get_user().get_username().c_str()) != 0 &&
      strcasecmp(msg->xmsg.from, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
    // printf("Not to me %s\n", msg->xmsg.to);
    return false;
  }
  if (_is_netmail) {
    if (wwivnode == 0) {
      NETADDR *myaddr = parse_fido_addr(orig_addr.c_str());
      if (!myaddr) {
        // printf("Failed to parse %s\n", orig_addr.c_str());
        return false;
      }

      if (myaddr->zone != msg->xmsg.orig.zone || myaddr->net != msg->xmsg.orig.net || myaddr->node != msg->xmsg.orig.node ||
          myaddr->point != msg->xmsg.orig.point) {
        free(myaddr);

        return false;
      }
      free(myaddr);

    } else {
      if (wwivnode != msg->xmsg.orig.node) {
        return false;
      }
    }
  }

  return true;
}

bool MsgArea::is_to_me(Node *n, sq_msg_t *msg) {
  if (strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 &&
      strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
    // printf("Not to me %s\n", msg->xmsg.to);
    return false;
  }
  if (_is_netmail) {
    if (wwivnode == 0) {
      NETADDR *myaddr = parse_fido_addr(orig_addr.c_str());
      if (!myaddr) {
        // printf("Failed to parse %s\n", orig_addr.c_str());
        return false;
      }

      if (myaddr->zone != msg->xmsg.dest.zone || myaddr->net != msg->xmsg.dest.net || myaddr->node != msg->xmsg.dest.node ||
          myaddr->point != msg->xmsg.dest.point) {
        // printf("Not equal %s, %d:%d/%d.%d %d:%d/%d.%d\n", orig_addr.c_str(), myaddr->zone, myaddr->net, myaddr->node, myaddr->point, msg->xmsg.dest.zone,
        // msg->xmsg.dest.net, msg->xmsg.dest.node, msg->xmsg.dest.point);
        free(myaddr);

        return false;
      }
      free(myaddr);

    } else {
      if (wwivnode != msg->xmsg.dest.node) {
        return false;
      }
    }
  }

  return true;
}

int MsgArea::list_messages(int start) {
  bool fsr = n->get_user().get_attribute("fullscreenreader", "true") == "true";

  if (fsr == false || !n->hasANSI) {
    return list_messages_old(start);
  } else if (fsr == true) {
    return list_messages_full(start);
  }
  return 0;
}

struct msg_list_t {
  int msgno;
  std::string subject;
  std::string from;
  std::string to;
  UMSGID umsgid;
};

int MsgArea::list_messages_full(size_t start) {
  bool redraw = true;
  int pos = 0;
  int selected = 0;

  std::vector<struct msg_list_t> msgs;

  UMSGID lr = n->get_user().user_get_lastread(file);

  sq_msg_base_t *mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    n->print_f("|14Unable to open message base!|07\r\n");
    return 0;
  }
  if (start > mb->basehdr.num_msg) {
    n->print_f("|14Empty message base!|07\r\n");
    SquishCloseMsgBase(mb);
    return 0;
  }

  for (size_t i = 1; i <= mb->basehdr.num_msg; i++) {
    struct msg_list_t mli;
    sq_msg_t *msg = SquishReadMsg(mb, i);
    if (msg == NULL) {
      continue;
    }
    if (msg->xmsg.attr & MSGPRIVATE && !is_to_me(n, msg) && !is_from_me(n, msg)) {
      SquishFreeMsg(msg);
      continue;
    }

    if (i == start) {
      pos = msgs.size();
      selected = msgs.size();
    }

    mli.msgno = i;
    mli.subject = std::string(msg->xmsg.subject);
    mli.from = std::string(msg->xmsg.from);
    mli.to = std::string(msg->xmsg.to);
    mli.umsgid = msg->xmsg.umsgid;
    msgs.push_back(mli);
    SquishFreeMsg(msg);
  }
  SquishCloseMsgBase(mb);

  if (msgs.size() == 0) {
    return 0;
  }

  if (selected >= (int)msgs.size()) {
    selected = msgs.size() - 1;
  } else if (selected < 0) {
    selected = 0;
  }

  if (msgs.size() - selected < n->get_term_height() - 3) {
    pos = msgs.size() - (n->get_term_height() - 3);
  }

  if (pos >= (int)msgs.size()) {
    pos = msgs.size() - 1;
  }

  if (pos < 0) {
    pos = 0;
  }

  while (true) {
    if (redraw) {
      n->cls();
      n->print_f("\x1b[1;1H%s Msg#    Subject                          From             To\x1b[K\x1b[0;40;37m", n->get_config()->get_prompt_colour());

      for (size_t i = pos; i - pos < n->get_term_height() - 3 && i < msgs.size(); i++) {
        if (msgs.at(i).umsgid <= lr) {
          if ((int)i == selected) {
            n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[0;47;30m%6d\x1b[1;40;30m] \x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K", (i - pos) + 2,
                       msgs.at(i).msgno, msgs.at(i).subject.c_str(), msgs.at(i).from.c_str(), msgs.at(i).to.c_str());
          } else {
            n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[1;37m%6d\x1b[1;30m] \x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K", (i - pos) + 2,
                       msgs.at(i).msgno, msgs.at(i).subject.c_str(), msgs.at(i).from.c_str(), msgs.at(i).to.c_str());
          }
        } else {
          if ((int)i == selected) {
            n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[0;47;30m%6d\x1b[1;40;30m]\x1b[1;31m*\x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K",
                       (i - pos) + 2, msgs.at(i).msgno, msgs.at(i).subject.c_str(), msgs.at(i).from.c_str(), msgs.at(i).to.c_str());
          } else {
            n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[1;37m%6d\x1b[1;30m]\x1b[1;31m*\x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K", (i - pos) + 2,
                       msgs.at(i).msgno, msgs.at(i).subject.c_str(), msgs.at(i).from.c_str(), msgs.at(i).to.c_str());
          }
        }
      }

      n->print_f("\x1b[%d;1H%sUp/Down to Move, Enter to Select, Q to Quit\x1b[K\x1b[0;40;37m", n->get_term_height() - 1, n->get_config()->get_prompt_colour());
      redraw = false;
    }

    n->print_f("\x1b[%d;7H", selected - pos + 2);

    char c = n->getch();

    if (tolower(c) == 'q') {
      return 0;
    }

    if (c == '\r') {
      return msgs.at(selected).msgno;
    }

    if (tolower(c) == 'j') {
      n->print_f("\x1b[%d;%dH%sJUMP: ", n->get_term_height() - 1, n->get_term_width() - 13, n->get_config()->get_prompt_colour());
      int jump = 0;
      try {
        jump = std::stoi(n->get_string(6, false));
      } catch (std::invalid_argument const &) {

      } catch (std::out_of_range const &) {
      }
      n->print_f("\x1b[%d;%dH%s\x1b[K\x1b[0m", n->get_term_height() - 1, n->get_term_width() - 13, n->get_config()->get_prompt_colour());
      if (jump > 0) {
        bool found = false;
        for (int i = 0; i < msgs.size(); i++) {
          if (msgs.at(i).umsgid == jump) {
            selected = i;
            pos = selected - (n->get_term_height() - 4);
            if (pos < 0) {
              pos = 0;
            }
            redraw = true;
            found = true;
            break;
          }
        }
        if (!found) {
          n->print_f("\x1b[%d;%dH%sNo Such Msg!\x1b[K\x1b[0m", n->get_term_height() - 1, n->get_term_width() - 13, n->get_config()->get_prompt_colour());
        }
      }
    }

    if (c == '\x1b') {
      c = n->getch();
      if (c == '[') {
        c = n->getch();
        if (c == 'A') {
          // up
          if (selected > 0) {
            selected--;
            if (selected - pos < 0) {
              pos -= n->get_term_height() - 3;
              if (pos < 0) {
                pos = 0;
              }
              redraw = true;
            } else {
              if (msgs.at(selected).umsgid <= lr) {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[0;47;30m%6d\x1b[1;40;30m] \x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K",
                           (selected - pos) + 2, msgs.at(selected).msgno, msgs.at(selected).subject.c_str(), msgs.at(selected).from.c_str(),
                           msgs.at(selected).to.c_str());
              } else {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[0;47;30m%6d\x1b[1;40;30m]\x1b[1;31m*\x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K",
                           (selected - pos) + 2, msgs.at(selected).msgno, msgs.at(selected).subject.c_str(), msgs.at(selected).from.c_str(),
                           msgs.at(selected).to.c_str());
              }
              if (msgs.at(selected + 1).umsgid <= lr) {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[1;37m%6d\x1b[1;30m] \x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K", (selected - pos) + 3,
                           msgs.at(selected + 1).msgno, msgs.at(selected + 1).subject.c_str(), msgs.at(selected + 1).from.c_str(),
                           msgs.at(selected + 1).to.c_str());
              } else {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[1;37m%6d\x1b[1;30m]\x1b[1;31m*\x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K",
                           (selected - pos) + 3, msgs.at(selected + 1).msgno, msgs.at(selected + 1).subject.c_str(), msgs.at(selected + 1).from.c_str(),
                           msgs.at(selected + 1).to.c_str());
              }
            }
          }
        } else if (c == 'B') {
          // down
          if (selected < (int)msgs.size() - 1) {
            selected++;

            if (selected - pos >= (int)n->get_term_height() - 3 && pos + n->get_term_height() - 3 < msgs.size()) {
              pos += n->get_term_height() - 3;
              if (msgs.size() - selected < n->get_term_height() - 3) {
                pos = (int)msgs.size() - (n->get_term_height() - 3);
              }
              if (pos < 0)
                pos = 0;
              redraw = true;
            } else {
              if (msgs.at(selected).umsgid <= lr) {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[0;47;30m%6d\x1b[1;40;30m] \x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K",
                           (selected - pos) + 2, msgs.at(selected).msgno, msgs.at(selected).subject.c_str(), msgs.at(selected).from.c_str(),
                           msgs.at(selected).to.c_str());
              } else {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[0;47;30m%6d\x1b[1;40;30m]\x1b[1;31m*\x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K",
                           (selected - pos) + 2, msgs.at(selected).msgno, msgs.at(selected).subject.c_str(), msgs.at(selected).from.c_str(),
                           msgs.at(selected).to.c_str());
              }
              if (msgs.at(selected - 1).umsgid <= lr) {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[1;37m%6d\x1b[1;30m] \x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K", (selected - pos) + 1,
                           msgs.at(selected - 1).msgno, msgs.at(selected - 1).subject.c_str(), msgs.at(selected - 1).from.c_str(),
                           msgs.at(selected - 1).to.c_str());
              } else {
                n->print_f("\x1b[%d;1H\x1b[1;30m[\x1b[1;37m%6d\x1b[1;30m]\x1b[1;31m*\x1b[1;33m%-32.32s \x1b[1;35m%-16.16s \x1b[1;36m%-16.16s\x1b[K",
                           (selected - pos) + 1, msgs.at(selected - 1).msgno, msgs.at(selected - 1).subject.c_str(), msgs.at(selected - 1).from.c_str(),
                           msgs.at(selected - 1).to.c_str());
              }
            }
          }
        } else if (c == 'K') {
          // end
          pos = msgs.size() - (n->get_term_height() - 3);
          if (pos < 0) {
            pos = 0;
          }
          selected = msgs.size() - 1;
          redraw = true;
        } else if (c == 'H') {
          // home
          pos = 0;
          selected = pos;
          redraw = true;
        } else if (c == 'V' || c == '5') {
          // page up
          if (c == '5') {
            n->getch();
          }
          pos = pos - (n->get_term_height() - 3);
          if (pos < 0) {
            pos = 0;
          }
          selected = pos;
          redraw = true;
        } else if (c == 'U' || c == '6') {
          // page down
          if (c == '6') {
            n->getch();
          }
          pos = pos + (n->get_term_height() - 3);
          if (pos >= (int)msgs.size()) {
            pos = (int)msgs.size() - (int)(n->get_term_height() - 3);
            if (pos < 0) {
              pos = 0;
            }
          }
          selected = pos;
          if (pos > 0) {
            if (msgs.size() - selected < n->get_term_height() - 3) {
              pos = (int)msgs.size() - (n->get_term_height() - 3);
            }
          }
          redraw = true;
        }
      }
    }
  }
}

int MsgArea::list_messages_old(int start) {
  sq_msg_base_t *mb;
  UMSGID lr = n->get_user().user_get_lastread(file);
  mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    n->print_f("|14Unable to open message base!|07\r\n");
    return 0;
  }
  if (start > (int)mb->basehdr.num_msg) {
    n->print_f("|14Empty message base!|07\r\n");
    SquishCloseMsgBase(mb);
    return 0;
  }
  size_t lines = 1;
  n->cls();
  n->print_f("|09 Msg#    Subject                          From             To              |07\r\n");
  for (size_t i = start; i <= mb->basehdr.num_msg; i++) {
    sq_msg_t *msg = SquishReadMsg(mb, i);

    if (msg == NULL) {
      continue;
    }

    if (msg->xmsg.attr & MSGPRIVATE && !is_to_me(n, msg) && !is_from_me(n, msg)) {
      SquishFreeMsg(msg);
      continue;
    } else {
      if (msg->xmsg.umsgid <= lr) {
        n->print_f("|08[|15%6d|08] |14%-32.32s |13%-16.16s |11%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
      } else {
        n->print_f("|08[|15%6d|08]|12*|14%-32.32s |13%-16.16s |11%-16.16s\r\n", i, msg->xmsg.subject, msg->xmsg.from, msg->xmsg.to);
      }
    }
    lines++;
    if (lines == n->get_term_height() - 2) {
      n->print_f("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", start, mb->basehdr.num_msg);

      std::string res = n->get_string(6, false);

      if (res.size() == 0) {
        lines = 1;
        n->cls();
        n->print_f("|09 Msg#    Subject                          From             To              |07\r\n");
        continue;
      } else if (tolower(res[0]) == 'q') {
        SquishCloseMsgBase(mb);
        return 0;
      } else {
        SquishCloseMsgBase(mb);
        try {
          return std::stoi(res);
        } catch (std::invalid_argument const &) {
          return 0;
        } catch (std::out_of_range const &) {
          return 0;
        }
      }
    }
  }

  n->print_f("|14Select |08[|15%d|08-|15%d|08], |15ENTER|08=|14Quit |07", start, mb->basehdr.num_msg);
  std::string res = n->get_string(6, false);

  SquishCloseMsgBase(mb);

  if (res.size() == 0) {
    return 0;
  } else {
    try {
      return std::stoi(res);
    } catch (std::invalid_argument const &) {
      return 0;
    } catch (std::out_of_range const &) {
      return 0;
    }
  }
}

void MsgArea::update_lr(time_t date) {
  sq_msg_base_t *mb;
  UMSGID prev_msg = 0;

  mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    return;
  }

  size_t totmsgs = mb->basehdr.num_msg;

  for (size_t i = 1; i <= totmsgs; i++) {
    sq_msg_t *msg = SquishReadMsg(mb, i);
    if (!msg) {
      continue;
    }
    struct tm msg_tm;
    memset(&msg_tm, 0, sizeof(struct tm));
    msg_tm.tm_year = ((msg->xmsg.date_written.date >> 9) & 127) + 80;
    msg_tm.tm_mon = ((msg->xmsg.date_written.date >> 5) & 15) - 1;
    msg_tm.tm_mday = msg->xmsg.date_written.date & 31;
    msg_tm.tm_hour = (msg->xmsg.date_written.time >> 11) & 31;
    msg_tm.tm_min = (msg->xmsg.date_written.time >> 5) & 63;
    msg_tm.tm_isdst = -1;
    SquishFreeMsg(msg);
    time_t msgtime = mktime(&msg_tm);

    if (msgtime > date) {
      n->get_user().user_set_lastread(file, prev_msg);
      SquishCloseMsgBase(mb);
      return;
    }
    prev_msg = msg->xmsg.umsgid;
  }
  n->get_user().user_set_lastread(file, prev_msg);
  SquishCloseMsgBase(mb);
  return;
}

bool MsgArea::search(std::vector<std::string> keywords, int type, bool newonly) {
  sq_msg_base_t *mb;
  bool foundmsg = false;
  mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    return true;
  }

  UMSGID lastr;

  if (newonly) {
    lastr = n->get_user().user_get_lastread(file) + 1;
  } else {
    lastr = 1;
  }

  n->print_f("|14Scanning |15%s\r\n|07", name.c_str());

  for (size_t i = 1; i <= mb->basehdr.num_msg; i++) {
    foundmsg = false;
    sq_msg_t *msg = SquishReadMsg(mb, i);

    if (msg == NULL) {
      continue;
    }

    if (msg->xmsg.attr & MSGPRIVATE && !is_to_me(n, msg) && !is_from_me(n, msg)) {
      SquishFreeMsg(msg);
      continue;
    }

    if (msg->xmsg.umsgid < lastr) {
      SquishFreeMsg(msg);
      continue;
    }

    switch (type) {
    case MSGSEARCH_BODY: {

      char *body = (char *)malloc(msg->msg_len + 1);
      if (!body) {
        SquishFreeMsg(msg);
        SquishCloseMsgBase(mb);
        return true;
      }
      memcpy(body, msg->msg, msg->msg_len);
      body[msg->msg_len] = '\0';
      std::string bodystr(body);
      free(body);
      for (size_t k = 0; k < keywords.size(); k++) {
        if (bodystr.find(" " + keywords.at(k) + " ") != std::string::npos) {
          foundmsg = true;
          break;
        }
      }
    } break;
    case MSGSEARCH_SUBJ: {
      for (size_t k = 0; k < keywords.size(); k++) {
#ifdef _MSC_VER
        if (StrStrIA(msg->xmsg.subject, keywords.at(k).c_str()) != NULL) {
#else
        if (strcasestr(msg->xmsg.subject, keywords.at(k).c_str()) != NULL) {
#endif
          foundmsg = true;
          break;
        }
      }
    } break;
    case MSGSEARCH_USER: {
      for (size_t k = 0; k < keywords.size(); k++) {
        if (strncasecmp(msg->xmsg.from, keywords.at(k).c_str(), 36) == 0 || strncasecmp(msg->xmsg.to, keywords.at(k).c_str(), 36) == 0) {
          foundmsg = true;
          break;
        }
      }

    } break;
    }
    SquishFreeMsg(msg);
    if (foundmsg) {
      if (read_message(i, true, false, true, NULL) == false) {
        SquishCloseMsgBase(mb);
        return false;
      }
    }
  }

  SquishCloseMsgBase(mb);
  return true;
}

static int ieee_to_msbin(float *src4, float *dest4) {
  unsigned char *ieee = (unsigned char *)src4;
  unsigned char *msbin = (unsigned char *)dest4;
  unsigned char sign = 0x00;
  unsigned char msbin_exp = 0x00;
  int i;
  /* See _fmsbintoieee() for details of formats   */
  sign = ieee[3] & 0x80;
  msbin_exp |= ieee[3] << 1;
  msbin_exp |= ieee[2] >> 7;
  /* An ieee exponent of 0xfe overflows in MBF    */
  if (msbin_exp == 0xfe)
    return 1;
  msbin_exp += 2; /* actually, -127 + 128 + 1 */
  for (i = 0; i < 4; i++)
    msbin[i] = 0;
  msbin[3] = msbin_exp;
  msbin[2] |= sign;
  msbin[2] |= ieee[2] & 0x7f;
  msbin[1] = ieee[1];
  msbin[0] = ieee[0];
  *dest4 = (float)convertl((tLONG)*dest4);
  return 0;
}

int MsgArea::qwk_scan(Node *n, FILE *msgs_dat_fptr, FILE *pers_ndx_fptr, FILE *conf_ndx_fptr, int tot, int confno, unsigned int *last_msg_packed) {
  UMSGID lastread = n->get_user().user_get_lastread(file);
  char buffer[256];

  sq_msg_base_t *mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    return 0;
  }

  for (size_t msgno = 1; msgno <= mb->basehdr.num_msg; msgno++) {
    sq_msg_t *msg = SquishReadMsg(mb, msgno);

    if (msg == NULL) {
      continue;
    }

    if (msg->xmsg.attr & MSGPRIVATE && !is_to_me(n, msg)) {
      SquishFreeMsg(msg);
      continue;
    }

    if (msg->xmsg.umsgid <= lastread) {
      SquishFreeMsg(msg);
      continue;
    }

    std::string subject(msg->xmsg.subject);
    std::string sender(msg->xmsg.from);
    std::string recipient(msg->xmsg.to);
    int hour = (msg->xmsg.date_written.time >> 11) & 31;
    int minute = (msg->xmsg.date_written.time >> 5) & 63;

    int day = msg->xmsg.date_written.date & 31;
    int month = (msg->xmsg.date_written.date >> 5) & 15;
    int year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980;

    std::stringstream msgss;
    for (size_t i = 0; i < (size_t)msg->msg_len; i++) {
      if (msg->msg[i] == '\r') {
        if (i < (size_t)msg->msg_len - 2) {
          if (msg->msg[i + 1] == '\001') {
            i++;
            while (i < (size_t)msg->msg_len - 1 && msg->msg[i + 1] != '\r') {
              i++;
            }
            continue;
          }
        }
        if (i < (size_t)msg->msg_len - 10) {

          if (msg->msg[i + 1] == 'S' && msg->msg[i + 2] == 'E' && msg->msg[i + 3] == 'E' && msg->msg[i + 4] == 'N' && msg->msg[i + 5] == '-' &&
              msg->msg[i + 6] == 'B' && msg->msg[i + 7] == 'Y' && msg->msg[i + 8] == ':' && msg->msg[i + 9] == ' ') {
            i++;
            while (i < (size_t)msg->msg_len - 1 && msg->msg[i + 1] != '\r') {
              i++;
            }
            continue;
          }
        }
      }
      msgss << msg->msg[i];
    }
    unsigned int msgid = msg->xmsg.umsgid;
    SquishFreeMsg(msg);
    std::string msgbody(msgss.str());
    std::stringstream extra;
    struct QwkHeader q;

    uint32_t ndx = ftell(msgs_dat_fptr);
    float fndx;
    float mndx;
    uint8_t zero = 0;

    fndx = (float)ndx;
    ieee_to_msbin(&fndx, &mndx);

    q.Msgstat = ' ';
    snprintf(buffer, 7, "%d", msgid);

    memset(q.Msgnum, ' ', 7);
    memcpy(q.Msgnum, buffer, strlen(buffer));

    snprintf(buffer, sizeof buffer, "%02d-%02d-%02d", month, day, year - 2000);
    memcpy(q.Msgdate, buffer, 8);

    snprintf(buffer, sizeof buffer, "%02d:%02d", hour, minute);
    memcpy(q.Msgtime, buffer, 5);

    memset(q.Msgpass, ' ', 12);
    memset(q.Msgrply, ' ', 8);

    memset(q.MsgSubj, ' ', 25);

    memset(q.MsgTo, ' ', 25);

    std::stringstream mbody2;
    mbody2.str("");
    for (size_t i = 0; i < msgbody.length(); i++) {
      if (msgbody.at(i) != '\n') {
        mbody2 << msgbody.at(i);
      }
    }

    msgbody = mbody2.str();

    extra.str("");

    if (recipient.length() > 25) {
      extra << "To: " << recipient << "\r";
      memcpy(q.MsgTo, recipient.c_str(), 25);
    } else {
      memcpy(q.MsgTo, recipient.c_str(), recipient.length());
    }

    memset(q.MsgFrom, ' ', 25);
    if (sender.length() > 25) {
      extra << "From: " << sender << "\r";
      memcpy(q.MsgFrom, sender.c_str(), 25);
    } else {
      memcpy(q.MsgFrom, sender.c_str(), sender.length());
    }

    if (subject.length() > 25) {
      extra << "Subject: " << subject << "\r";
      memcpy(q.MsgSubj, subject.c_str(), 25);
    } else {
      memcpy(q.MsgSubj, subject.c_str(), subject.length());
    }

    if (extra.str().length() > 0) {
      extra << "\r" << msgbody;
      msgbody = extra.str();
    }

    size_t len = msgbody.length() / 128;

    if (len * 128 < msgbody.length()) {
      len++;
    }

    size_t lenbytes = len * 128;

    char *msgbuf = (char *)malloc(lenbytes);

    if (!lenbytes) {
      SquishCloseMsgBase(mb);
      return tot;
    }

    memset(msgbuf, ' ', lenbytes);

    for (size_t i = 0; i < msgbody.length(); i++) {
      if (msgbody.c_str()[i] == '\r') {
        msgbuf[i] = '\xe3';
      } else {
        msgbuf[i] = msgbody.c_str()[i];
      }
    }

    snprintf(buffer, 7, "%lu", len + 1);
    memset(q.Msgrecs, ' ', 6);
    memcpy(q.Msgrecs, buffer, strlen(buffer));

    q.Msglive = 0xE1;
    q.Msgarealo = qwk_base_no & 0xff;
    q.Msgareahi = (qwk_base_no >> 8) & 0xff;

    q.Msgoffhi = (ndx >> 8) & 0xff;
    q.Msgofflo = ndx & 0xff;

    q.Msgtagp = ' ';

    fwrite(&mndx, 4, 1, conf_ndx_fptr);
    fwrite(&zero, 1, 1, conf_ndx_fptr);

    if (strcasecmp(msg->xmsg.to, n->get_user().get_username().c_str()) != 0 &&
        strcasecmp(msg->xmsg.to, n->get_user().get_attribute("fullname", "UNKNOWN").c_str()) != 0) {
      fwrite(&mndx, 4, 1, pers_ndx_fptr);
      fwrite(&zero, 1, 1, pers_ndx_fptr);
    }

    fwrite(&q, sizeof(struct QwkHeader), 1, msgs_dat_fptr);
    fwrite(msgbuf, lenbytes, 1, msgs_dat_fptr);
    free(msgbuf);
    *last_msg_packed = msgid;
    tot++;
  }

  SquishCloseMsgBase(mb);

  return tot;
}

int MsgArea::bwave_scan(Node *n, int totmsgs, int areano, FILE *fti_file, FILE *mix_file, FILE *dat_file, int *last_ptr, int *last_read) {
  UMSGID lastread = n->get_user().user_get_lastread(file);
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int personal_msgs = 0;
  int area_msgs = 0;
  long mixptr;
  int tot_msgs = totmsgs;
  FTI_REC fti;
  MIX_REC mix;
  mixptr = ftell(fti_file);

  sq_msg_base_t *mb = SquishOpenMsgBase(file.c_str());
  if (!mb) {
    return 0;
  }

  for (size_t msgno = lastread + 1; msgno <= mb->basehdr.num_msg; msgno++) {
    sq_msg_t *msg = SquishReadMsg(mb, msgno);

    if (msg == NULL) {
      continue;
    }

    bool istome = is_to_me(n, msg);

    if (msg->xmsg.attr & MSGPRIVATE && !istome) {
      SquishFreeMsg(msg);
      continue;
    }

    if (msg->xmsg.umsgid <= lastread) {
      SquishFreeMsg(msg);
      continue;
    }

    if (istome) {
      personal_msgs++;
    }

    memset(&fti, 0, sizeof(FTI_REC));

    std::string subject(msg->xmsg.subject);
    std::string sender(msg->xmsg.from);
    std::string recipient(msg->xmsg.to);

    strncpy((char *)fti.from, sender.c_str(), sizeof(fti.from) - 1);
    strncpy((char *)fti.to, recipient.c_str(), sizeof(fti.to) - 1);
    strncpy((char *)fti.subject, subject.c_str(), sizeof(fti.subject) - 1);

    int hour = (msg->xmsg.date_written.time >> 11) & 31;
    int minute = (msg->xmsg.date_written.time >> 5) & 63;

    int day = msg->xmsg.date_written.date & 31;
    int month = (msg->xmsg.date_written.date >> 5) & 15;
    int year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980;

    snprintf((char *)fti.date, sizeof(fti.date), "%02d-%s-%04d %02d:%02d", day, months[month - 1], year, hour, minute);

    fti.msgnum = converts((tWORD)msg->xmsg.umsgid);
    fti.replyto = 0;
    fti.replyat = 0;

    fti.msgptr = convertl(*last_ptr);

    std::stringstream msgss;
    for (size_t i = 0; i < (size_t)msg->msg_len; i++) {
      if (msg->msg[i] == '\r') {
        if (i < (size_t)msg->msg_len - 2) {
          if (msg->msg[i + 1] == '\001') {
            i++;
            while (i < (size_t)msg->msg_len - 1 && msg->msg[i + 1] != '\r') {
              i++;
            }
            continue;
          }
        }
        if (i < (size_t)msg->msg_len - 10) {

          if (msg->msg[i + 1] == 'S' && msg->msg[i + 2] == 'E' && msg->msg[i + 3] == 'E' && msg->msg[i + 4] == 'N' && msg->msg[i + 5] == '-' &&
              msg->msg[i + 6] == 'B' && msg->msg[i + 7] == 'Y' && msg->msg[i + 8] == ':' && msg->msg[i + 9] == ' ') {
            i++;
            while (i < (size_t)msg->msg_len - 1 && msg->msg[i + 1] != '\r') {
              i++;
            }
            continue;
          }
        }
      }
      msgss << msg->msg[i];
    }

    fti.msglength = convertl(msgss.str().size());
    *last_ptr += msgss.str().size() + 1;

    if (msg->xmsg.attr & MSGLOCAL) {
      fti.flags |= FTI_MSGLOCAL;
    }

    fti.flags = converts(fti.flags);

    fti.orig_zone = converts(msg->xmsg.orig.zone);
    fti.orig_net = converts(msg->xmsg.orig.net);
    fti.orig_node = converts(msg->xmsg.orig.node);

    fwrite(" ", 1, 1, dat_file);
    fwrite(msgss.str().c_str(), 1, msgss.str().size(), dat_file);
    fwrite(&fti, sizeof(FTI_REC), 1, fti_file);

    area_msgs++;
    tot_msgs++;
    *last_read = msg->xmsg.umsgid;
    SquishFreeMsg(msg);
  }

  SquishCloseMsgBase(mb);

  memset(&mix, 0, sizeof(MIX_REC));
  snprintf((char *)mix.areanum, 6, "%d", areano);
  mix.totmsgs = converts(area_msgs);
  mix.numpers = converts(personal_msgs);
  mix.msghptr = convertl(mixptr);

  fwrite(&mix, sizeof(MIX_REC), 1, mix_file);

  return tot_msgs;
}

void MsgArea::download(Node *n, sq_msg_t *msg) {

  std::filesystem::path pth = std::filesystem::path("temp/" + std::to_string(n->getnodenum()) + "/message.txt");

  std::stringstream msgss;
  for (size_t i = 0; i < (size_t)msg->msg_len; i++) {
    if (msg->msg[i] == '\r') {
      if (i < (size_t)msg->msg_len - 2) {
        if (msg->msg[i + 1] == '\001') {
          i++;
          while (i < (size_t)msg->msg_len - 1 && msg->msg[i + 1] != '\r') {
            i++;
          }
          continue;
        }
      }

      if (i < (size_t)msg->msg_len - 10) {
        if (msg->msg[i + 1] == 'S' && msg->msg[i + 2] == 'E' && msg->msg[i + 3] == 'E' && msg->msg[i + 4] == 'N' && msg->msg[i + 5] == '-' &&
            msg->msg[i + 6] == 'B' && msg->msg[i + 7] == 'Y' && msg->msg[i + 8] == ':' && msg->msg[i + 9] == ' ') {
          i++;
          while (i < (size_t)msg->msg_len - 1 && msg->msg[i + 1] != '\r') {
            i++;
          }
          continue;
        }
      }
    }
    msgss << msg->msg[i];
  }

  std::ofstream of;
  of.open(pth, std::ofstream::out | std::ofstream::trunc);

  of << "To     : " << msg->xmsg.to << "\r\n";
  of << "From   : " << msg->xmsg.from << "\r\n";
  of << "Subject: " << msg->xmsg.subject << "\r\n";
  of << "-------------------------------------------------------------------------------"
     << "\r\n";

  for (size_t i = 0; i < msgss.str().size(); i++) {
    of << msgss.str().at(i);
    if (msgss.str().at(i) == '\r') {
      of << "\n";
    }
  }
  of << "-------------------------------------------------------------------------------"
     << "\r\n";

  of.close();

  if (!std::filesystem::exists(pth)) {
    return;
  }

  n->cls();

  Protocol *p = n->get_config()->select_protocol(n);

  if (p == nullptr) {
    return;
  }

  std::vector<std::filesystem::path> sendlist;
  sendlist.push_back(pth);

  p->download(n, n->get_socket(), &sendlist);

  return;
}