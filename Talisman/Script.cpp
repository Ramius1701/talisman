#include "../Common/Logger.h"
#include "../Common/Squish.h"
#include "CallLog.h"
#include "Config.h"
#include "Node.h"
#include "Script.h"
#include "User.h"
#include "AnsiEditor.h"
#include "Protocol.h"
#include "Rlogin.h"
#include "Telnet.h"
#include "MessageReader.h"
#include <cstring>
#include <sqlite3.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

extern "C" Node *lua_getNode(lua_State *L) {
  lua_pushstring(L, "bbs_node");
  lua_gettable(L, LUA_REGISTRYINDEX);
  return (Node *)lua_touserdata(L, -1);
}

extern "C" int lua_getBBSMsgDetail(lua_State *L) {
  const char *mbfile = lua_tostring(L, 1);
  uint32_t mid = (uint32_t)lua_tonumber(L, 2);
  const char *detail = lua_tostring(L, 3);
  Node *n = lua_getNode(L);

  sq_msg_base_t *mb;
  sq_msg_t *msg;

  mb = SquishOpenMsgBase(std::string(n->get_config()->msg_path() + "/" + mbfile).c_str());

  if (!mb) {
    lua_pushstring(L, "!ERROR");
    return 1;
  }
  msg = SquishReadMsg(mb, SquishUMSGID2Offset(mb, mid, 1));
  if (!msg) {
    lua_pushstring(L, "!ERROR");
    SquishCloseMsgBase(mb);
    return 1;
  }

  std::stringstream detail_ss;

  if (strcasecmp(detail, "ORIGINADDR") == 0) {
    if (msg->xmsg.orig.zone == 0 && msg->xmsg.orig.net == 0 && msg->xmsg.orig.node == 0 && msg->xmsg.orig.point == 0) {
      for (int i = 0; i < msg->ctrl_len - 10; i++) {
        if (strncmp(&msg->ctrl[i], "\x01QWKORIG: ", 10) == 0) {
          for (int j = i + 10; j < msg->ctrl_len && msg->ctrl[j] != '\x01'; j++) {
            detail_ss << msg->ctrl[j];
          }
          break;
        }
      }
    } else {
      detail_ss << msg->xmsg.orig.zone << ":" << msg->xmsg.orig.net << "/" << msg->xmsg.orig.node << "." << msg->xmsg.orig.point;
    }
  } else if (strcasecmp(detail, "LOCAL") == 0) {
    if (msg->xmsg.attr & MSGLOCAL) {
      detail_ss << "TRUE";
    } else {
      detail_ss << "FALSE";
    }
  } else {
    detail_ss << "!ERROR";
  }

  lua_pushstring(L, detail_ss.str().c_str());

  SquishFreeMsg(msg);
  SquishCloseMsgBase(mb);
  return 1;
}

extern "C" int lua_getTermType(lua_State *L) {
  Node *n = lua_getNode(L);
  lua_pushstring(L, n->get_term_type());

  return 1;
}

extern "C" int lua_GetSixelSupport(lua_State *L) {
  Node *n = lua_getNode(L);
  lua_pushboolean(L, n->sixel_support());

  return 1;
}

extern "C" int lua_getBBSMsg(lua_State *L) {
  const char *mbfile = lua_tostring(L, 1);
  uint32_t mid = (uint32_t)lua_tonumber(L, 2);
  Node *n = lua_getNode(L);

  sq_msg_base_t *mb;

  mb = SquishOpenMsgBase(std::string(n->get_config()->msg_path() + "/" + mbfile).c_str());

  if (!mb) {
    lua_pushnumber(L, 0);
    lua_pushstring(L, "Nobody");
    lua_pushstring(L, "Nobody");
    lua_pushstring(L, "No Message");
    lua_pushstring(L, "No Message");
    return 5;
  }

  if (mid < 1 || mid >= mb->basehdr.uid) {
    SquishCloseMsgBase(mb);
    lua_pushnumber(L, 0);
    lua_pushstring(L, "Nobody");
    lua_pushstring(L, "Nobody");
    lua_pushstring(L, "No Message");
    lua_pushstring(L, "No Message");
    return 5;
  }

  while (mid < mb->basehdr.uid) {
    sq_msg_t *msg;

    msg = SquishReadMsg(mb, SquishUMSGID2Offset(mb, mid, 1));
    if (!msg) {
      SquishCloseMsgBase(mb);
      lua_pushnumber(L, 0);
      lua_pushstring(L, "Nobody");
      lua_pushstring(L, "Nobody");
      lua_pushstring(L, "No Message");
      lua_pushstring(L, "No Message");
      return 5;
    }
    if (msg->xmsg.attr & MSGPRIVATE) {
      SquishFreeMsg(msg);
      mid++;
      continue;
    }

    char *msgc = (char *)malloc(msg->msg_len + 1);

    if (!msgc) {
      SquishFreeMsg(msg);
      SquishCloseMsgBase(mb);
      lua_pushnumber(L, 0);
      lua_pushstring(L, "Nobody");
      lua_pushstring(L, "Nobody");
      lua_pushstring(L, "No Message");
      lua_pushstring(L, "No Message");
      return 5;
    }

    memcpy(msgc, msg->msg, msg->msg_len);

    msgc[msg->msg_len] = '\0';

    lua_pushnumber(L, mid);
    lua_pushstring(L, msg->xmsg.to);
    lua_pushstring(L, msg->xmsg.from);
    lua_pushstring(L, msg->xmsg.subject);
    lua_pushstring(L, msgc);

    SquishFreeMsg(msg);
    SquishCloseMsgBase(mb);
    free(msgc);
    return 5;
  }

  SquishCloseMsgBase(mb);
  lua_pushnumber(L, 0);
  lua_pushstring(L, "Nobody");
  lua_pushstring(L, "Nobody");
  lua_pushstring(L, "No Message");
  lua_pushstring(L, "No Message");
  return 5;
}

extern "C" int lua_bbsPostMsg(lua_State *L) {
  const char *mbfile = lua_tostring(L, 1);
  const char *to = lua_tostring(L, 2);
  const char *from = lua_tostring(L, 3);
  const char *subj = lua_tostring(L, 4);
  const char *body = lua_tostring(L, 5);
  Node *n = lua_getNode(L);

  for (size_t msgconf = 0; msgconf < n->get_config()->msgconfs.size(); msgconf++) {
    for (size_t msgbase = 0; msgbase < n->get_config()->msgconfs.at(msgconf)->areas.size(); msgbase++) {
      if (n->get_config()->msgconfs.at(msgconf)->areas.at(msgbase)->get_file() == std::string(n->get_config()->msg_path() + "/" + mbfile) &&
          !n->get_config()->msgconfs.at(msgconf)->areas.at(msgbase)->is_netmail()) {
        std::stringstream ss;
        std::vector<std::string> msg;

        for (size_t i = 0; i < strlen(body); i++) {
          if (body[i] == '\n') {
            msg.push_back(ss.str());
            ss.str("");
          } else {
            ss << body[i];
          }
        }

        if (ss.str().size() > 0) {
          msg.push_back(ss.str());
        }

        n->get_config()->msgconfs.at(msgconf)->areas.at(msgbase)->save_message(std::string(to), std::string(from), std::string(subj), msg, "", -1);
      }
    }
  }
  return 0;
}

extern "C" int lua_BBSWrite(lua_State *L) {
  char *str = (char *)lua_tostring(L, -1);

  lua_getNode(L)->print_f("%s", str);
  return 0;
}

extern "C" int lua_BBSGetString(lua_State *L) {
  uint32_t length = (uint32_t)lua_tonumber(L, -1);

  std::string str = lua_getNode(L)->get_string(length, false, true);
  lua_pushstring(L, str.c_str());
  return 1;
}

extern "C" int lua_BBSGetMaskedString(lua_State *L) {
  uint32_t length = (uint32_t)lua_tonumber(L, -1);

  std::string str = lua_getNode(L)->get_string(length, true, true);
  lua_pushstring(L, str.c_str());
  return 1;
}

// Attempt to get a character from the user
//
// Optional Argument: a delay in milliseconds to delay upto (number/integer/whole number)
//
// Returns: nil | string (of what was recieved)
extern "C" int lua_BBSGetChar(lua_State *L) {
  int delay = (int)lua_tonumber(L, -1);
  if (delay <= 0) {
    delay = -1;
  }
  char c = lua_getNode(L)->getch(delay);
  if (c == -1) {
    lua_pushnil(L); // Returns nil when delay was reached
    return 1;
  }
  lua_pushlstring(L, &c, 1);
  return 1;
}

extern "C" int lua_BBSUsername(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_user().get_username().c_str());
  return 1;
}

extern "C" int lua_BBSNode(lua_State *L) {
  lua_pushnumber(L, lua_getNode(L)->getnodenum());
  return 1;
}

extern "C" int lua_BBSSysName(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->operating_system().c_str());
  return 1;
}

extern "C" int lua_BBSName(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_config()->sys_name().c_str());
  return 1;
}

extern "C" int lua_BBSSysopName(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_config()->op_name().c_str());
  return 1;
}

extern "C" int lua_BBSUserLocation(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_user().get_attribute("location", "Somewhere, The World").c_str());
  return 1;
}

extern "C" int lua_BBSClrScr(lua_State *L) {
  Node *n = lua_getNode(L);
  n->cls();
  return 0;
}

extern "C" int lua_BBSScriptDataPath(lua_State *L) {
  Node *n = lua_getNode(L);
  std::filesystem::path fspath(n->get_config()->script_path());
  fspath.append("data");

  if (!std::filesystem::exists(fspath)) {
    std::filesystem::create_directories(fspath);
  }

  lua_pushstring(L, fspath.string().c_str());

  return 1;
}

extern "C" int lua_BBSDisplayTextfile(lua_State *L) {
  const char *filename = lua_tostring(L, -1);

  Node *n = lua_getNode(L);
  n->send_gfile(filename);
  return 0;
}

extern "C" int lua_BBSDisplayTextfileP(lua_State *L) {
  const char *filename = lua_tostring(L, -1);

  Node *n = lua_getNode(L);
  n->send_gfile(filename, true);

  return 0;
}

extern "C" int lua_BBSTermWidth(lua_State *L) {
  lua_pushnumber(L, lua_getNode(L)->get_term_width());
  return 1;
}

extern "C" int lua_BBSTermHeight(lua_State *L) {
  lua_pushnumber(L, lua_getNode(L)->get_term_height());
  return 1;
}

extern "C" int lua_GetAttrib(lua_State *L) {
  const char *attrib = lua_tostring(L, 1);
  const char *def = lua_tostring(L, 2);
  Node *n = lua_getNode(L);

  lua_pushstring(L, n->get_user().get_attribute(std::string(attrib), std::string(def)).c_str());

  return 1;
}

extern "C" int lua_GetAttribByName(lua_State *L) {
  const char *name = lua_tostring(L, 1);
  const char *attrib = lua_tostring(L, 2);
  const char *def = lua_tostring(L, 3);
  Node *n = lua_getNode(L);

  lua_pushstring(L, User::get_attribute_s(n->get_config(), std::string(name), std::string(attrib), std::string(def)).c_str());

  return 1;
}

extern "C" int lua_SetAttrib(lua_State *L) {
  const char *attrib = lua_tostring(L, 1);
  const char *value = lua_tostring(L, 2);
  Node *n = lua_getNode(L);

  n->get_user().set_attribute(std::string(attrib), std::string(value));

  return 0;
}

extern "C" int lua_Pause(lua_State *L) {
  Node *n = lua_getNode(L);
  n->pause();

  return 0;
}

extern "C" int lua_getTotCalls(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::total_calls(n, std::string(username));
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotUploads(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_uploads(n, username);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotDownloads(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_downloads(n, username);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotMsgPosts(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_msgpost(n, username);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotDoorRuns(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_doors(n, username);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotBBSCalls(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::total_bbs_calls(n);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotBBSUploads(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_uploads(n);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotBBSDownloads(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_downloads(n);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotBBSMsgPosts(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_msgpost(n);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_getTotBBSDoorRuns(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_doors(n);
  lua_pushnumber(L, tot);

  return 1;
}

extern "C" int lua_ansiView(lua_State *L) {
  Node *n = lua_getNode(L);
  std::ifstream in;
  std::string file = std::string(lua_tostring(L, 1));
  bool is_ansi = false;
  std::stringstream ss;
  std::vector<struct line_t> lines;
  char c;
  in.open(file);
  if (in.is_open()) {
    while (in.get(c)) {
      if (c == 0x1a) {
        break;
      }
      ss << c;
    }
    in.close();
    std::vector<std::string> ansi = MsgArea::demangle_ansi(n, ss.str().c_str(), ss.str().size());

    for (size_t i = 0; i < ansi.size(); i++) {
      struct line_t l;
      l.line = ansi.at(i);
      l.type = 0;

      lines.push_back(l);
    }
    struct msg_reader_msg_t msg;

    msg.body = &lines;
    msg.to = "";
    msg.from = "";
    msg.subject = "";
    msg.date = "";
    msg.ansi = true;
    msg.msg_type = 4;
    msg.origaddr = NULL;
    msg.destaddr = NULL;
    msg.msg_no = 0;
    msg.msg_serial = 0;
    msg.showkluges = false;
    while (MessageReader::read_message(n, &msg, 0,
                                       DISABLE_DOWNLOAD | DISABLE_DELETE | DISABLE_HEADER | DISABLE_KLUDGE | DISABLE_NEXT | DISABLE_PREV | DISABLE_REPLY |
                                           DISABLE_SEARCH | DISABLE_UNREAD) == 4) {
      // nothing...
    }
  }

  return 0;
}

extern "C" int lua_getCallLogX(lua_State *L) {
  int x = lua_tointeger(L, 1);
  Node *n = lua_getNode(L);

  struct caller_t ct;

  if (CallLog::get_last_x(n, x, &ct)) {
    lua_pushnumber(L, ct.id);
    lua_pushnumber(L, ct.node);
    lua_pushstring(L, ct.username.c_str());
    lua_pushnumber(L, ct.timeon);
    lua_pushnumber(L, ct.timeoff);
    lua_pushnumber(L, ct.upload);
    lua_pushnumber(L, ct.download);
    lua_pushnumber(L, ct.msgpost);
    lua_pushnumber(L, ct.doors);

    return 9;
  } else {
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushstring(L, "No One");
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);
    lua_pushnumber(L, 0);

    return 9;
  }
}

extern "C" int lua_hasAnsi(lua_State *L) {
  Node *n = lua_getNode(L);
  bool hasAnsi = n->hasANSI;
  lua_pushboolean(L, hasAnsi);
  return 1;
}

extern "C" int lua_editAnsi(lua_State *L) {
  int width = lua_tonumber(L, 1);
  int height = lua_tonumber(L, 2);
  const char *filename = lua_tostring(L, 3);
  Node *n = lua_getNode(L);

  AnsiEditor a(n, width, height);
  a.edit(std::string(filename));

  return 0;
}

extern "C" int lua_download(lua_State *L) {
  const char *filename = lua_tostring(L, 1);
  Node *n = lua_getNode(L);

  if (!std::filesystem::exists(filename)) {
    return 0;
  }

  Protocol *p = n->get_config()->select_protocol(n);

  if (p == nullptr) {
    return 0;
  }

  std::vector<std::filesystem::path> sendlist;
  sendlist.push_back(std::filesystem::path(filename));

  p->download(n, n->get_socket(), &sendlist);

  return 0;
}

extern "C" int lua_upload(lua_State *L) {
  Node *n = lua_getNode(L);

  Protocol *p = n->get_config()->select_protocol(n);

  if (p == nullptr) {
    return 0;
  }

  std::filesystem::path updir(std::filesystem::absolute(n->get_config()->tmp_path()));
  updir.append(std::to_string(n->getnodenum()));
  updir.append("script_upload");

  if (std::filesystem::exists(updir)) {
    std::filesystem::remove_all(updir);
  }

  std::filesystem::create_directories(updir);

  p->upload(n, n->get_socket(), updir.u8string());

  std::vector<std::filesystem::path> uploadedfiles;
  for (auto &d : std::filesystem::directory_iterator(updir)) {
    uploadedfiles.push_back(d.path());
  }

  lua_newtable(L);

  for (size_t i = 0; i < uploadedfiles.size(); i++) {
    lua_pushinteger(L, i + 1);
    lua_pushstring(L, uploadedfiles.at(i).u8string().c_str());
    lua_settable(L, -3);
  }
  return 1;
}

extern "C" int lua_telnet_ip4(lua_State *L) {
  const char *host = lua_tostring(L, 1);
  int port = lua_tonumber(L, 2);
  Node *n = lua_getNode(L);

  bool result = Telnet::session(n, std::string(host), port, false);

  lua_pushboolean(L, result);

  return 1;
}

extern "C" int lua_telnet_ip6(lua_State *L) {
  const char *host = lua_tostring(L, 1);
  int port = lua_tonumber(L, 2);
  Node *n = lua_getNode(L);

  bool result = Telnet::session(n, std::string(host), port, true);

  lua_pushboolean(L, result);

  return 1;
}

extern "C" int lua_rlogin_ip4(lua_State *L) {
  const char *host = lua_tostring(L, 1);
  int port = lua_tonumber(L, 2);
  const char *luser = lua_tostring(L, 3);
  const char *ruser = lua_tostring(L, 4);
  const char *termtype = lua_tostring(L, 5);
  Node *n = lua_getNode(L);

  bool result = Rlogin::session(n, std::string(host), port, std::string(luser), std::string(ruser), std::string(termtype), false);

  lua_pushboolean(L, result);

  return 1;
}

extern "C" int lua_rlogin_ip6(lua_State *L) {
  const char *host = lua_tostring(L, 1);
  int port = lua_tonumber(L, 2);
  const char *luser = lua_tostring(L, 3);
  const char *ruser = lua_tostring(L, 4);
  const char *termtype = lua_tostring(L, 5);
  Node *n = lua_getNode(L);

  bool result = Rlogin::session(n, std::string(host), port, std::string(luser), std::string(ruser), std::string(termtype), true);

  lua_pushboolean(L, result);

  return 1;
}

extern "C" int lua_get_ipaddress(lua_State *L) {
  Node *n = lua_getNode(L);

  lua_pushstring(L, n->ipaddr.c_str());
  return 1;
}

extern "C" int lua_get_top(lua_State *L) {
  const char *attrib = lua_tostring(L, 1);
  int place = lua_tonumber(L, 2);

  Node *n = lua_getNode(L);
  std::string uname = "";

  uint64_t val = n->get_user().get_top(std::string(attrib), place, &uname);

  lua_pushstring(L, uname.c_str());
  lua_pushnumber(L, val);

  return 2;
}

extern "C" int lua_display_sixel(lua_State *L) {
  const char *sixel = lua_tostring(L, 1);
  Node *n = lua_getNode(L);

  if (n->sixel_support()) {
    n->send_raw(std::string(sixel));
  }

  return 0;
}

extern "C" int lua_set_timeleft(lua_State *L) {
  time_t tl = lua_tonumber(L, 1);
  Node *n = lua_getNode(L);
  n->get_user().set_attribute("time_left", std::to_string(tl));
  n->set_timeleft(tl * 60);
  return 0;
}

extern "C" int lua_get_timeleft(lua_State *L) {
  Node *n = lua_getNode(L);

  lua_pushnumber(L, n->get_timeleft() / 60);
  return 1;
}

extern "C" int lua_switch_font(lua_State *L) {
  int font = lua_tonumber(L, 1);
  int place = lua_tonumber(L, 2);
  Node *n = lua_getNode(L);

  n->switch_font(font, place);

  return 0;
}

void Script::init_state(Node *n, lua_State *l) {

  luaL_openlibs(l);

  lua_pushstring(l, "bbs_node");
  lua_pushlightuserdata(l, n);
  lua_settable(l, LUA_REGISTRYINDEX);

  lua_pushcfunction(l, lua_BBSWrite);
  lua_setglobal(l, "bbs_write_string");

  lua_pushcfunction(l, lua_BBSGetString);
  lua_setglobal(l, "bbs_read_string");

  lua_pushcfunction(l, lua_BBSGetMaskedString);
  lua_setglobal(l, "bbs_read_password");

  lua_pushcfunction(l, lua_BBSGetChar);
  lua_setglobal(l, "bbs_getchar");

  lua_pushcfunction(l, lua_BBSScriptDataPath);
  lua_setglobal(l, "bbs_get_data_path");

  lua_pushcfunction(l, lua_BBSDisplayTextfile);
  lua_setglobal(l, "bbs_display_gfile");

  lua_pushcfunction(l, lua_BBSDisplayTextfileP);
  lua_setglobal(l, "bbs_display_gfile_pause");

  lua_pushcfunction(l, lua_BBSUsername);
  lua_setglobal(l, "bbs_get_username");

  lua_pushcfunction(l, lua_BBSUserLocation);
  lua_setglobal(l, "bbs_get_user_location");

  lua_pushcfunction(l, lua_BBSSysName);
  lua_setglobal(l, "bbs_get_os");

  lua_pushcfunction(l, lua_BBSName);
  lua_setglobal(l, "bbs_get_bbs_name");

  lua_pushcfunction(l, lua_BBSSysopName);
  lua_setglobal(l, "bbs_get_sysop_name");

  lua_pushcfunction(l, lua_BBSClrScr);
  lua_setglobal(l, "bbs_clear_screen");

  lua_pushcfunction(l, lua_getBBSMsg);
  lua_setglobal(l, "bbs_get_message");

  lua_pushcfunction(l, lua_bbsPostMsg);
  lua_setglobal(l, "bbs_post_message");

  lua_pushcfunction(l, lua_BBSTermWidth);
  lua_setglobal(l, "bbs_get_term_width");

  lua_pushcfunction(l, lua_BBSTermHeight);
  lua_setglobal(l, "bbs_get_term_height");

  lua_pushcfunction(l, lua_Pause);
  lua_setglobal(l, "bbs_pause");

  lua_pushcfunction(l, lua_GetAttrib);
  lua_setglobal(l, "bbs_get_user_attribute");

  lua_pushcfunction(l, lua_GetAttribByName);
  lua_setglobal(l, "bbs_get_user_attribute_by_name");

  lua_pushcfunction(l, lua_SetAttrib);
  lua_setglobal(l, "bbs_set_user_attribute");

  lua_pushcfunction(l, lua_getCallLogX);
  lua_setglobal(l, "bbs_get_calllog_x");

  lua_pushcfunction(l, lua_getTotCalls);
  lua_setglobal(l, "bbs_user_get_total_calls");

  lua_pushcfunction(l, lua_getTotUploads);
  lua_setglobal(l, "bbs_user_get_total_uploads");

  lua_pushcfunction(l, lua_getTotDownloads);
  lua_setglobal(l, "bbs_user_get_total_downloads");

  lua_pushcfunction(l, lua_getTotMsgPosts);
  lua_setglobal(l, "bbs_user_get_total_msgposts");

  lua_pushcfunction(l, lua_getTotDoorRuns);
  lua_setglobal(l, "bbs_user_get_total_doorsrun");

  lua_pushcfunction(l, lua_getTotBBSCalls);
  lua_setglobal(l, "bbs_get_total_calls");

  lua_pushcfunction(l, lua_getTotBBSUploads);
  lua_setglobal(l, "bbs_get_total_uploads");

  lua_pushcfunction(l, lua_getTotBBSDownloads);
  lua_setglobal(l, "bbs_get_total_downloads");

  lua_pushcfunction(l, lua_getTotBBSMsgPosts);
  lua_setglobal(l, "bbs_get_total_msgposts");

  lua_pushcfunction(l, lua_getTotBBSDoorRuns);
  lua_setglobal(l, "bbs_get_total_doorsrun");

  lua_pushcfunction(l, lua_hasAnsi);
  lua_setglobal(l, "bbs_user_has_ansi");

  lua_pushcfunction(l, lua_GetSixelSupport);
  lua_setglobal(l, "bbs_user_has_sixel");

  lua_pushcfunction(l, lua_editAnsi);
  lua_setglobal(l, "bbs_edit_ansi");

  lua_pushcfunction(l, lua_upload);
  lua_setglobal(l, "bbs_upload");

  lua_pushcfunction(l, lua_download);
  lua_setglobal(l, "bbs_download");

  lua_pushcfunction(l, lua_rlogin_ip4);
  lua_setglobal(l, "bbs_rlogin_ip4");

  lua_pushcfunction(l, lua_rlogin_ip6);
  lua_setglobal(l, "bbs_rlogin_ip6");

  lua_pushcfunction(l, lua_telnet_ip4);
  lua_setglobal(l, "bbs_telnet_ip4");

  lua_pushcfunction(l, lua_telnet_ip6);
  lua_setglobal(l, "bbs_telnet_ip6");

  lua_pushcfunction(l, lua_BBSNode);
  lua_setglobal(l, "bbs_get_node");

  lua_pushcfunction(l, lua_getBBSMsgDetail);
  lua_setglobal(l, "bbs_get_message_detail");

  lua_pushcfunction(l, lua_get_top);
  lua_setglobal(l, "bbs_get_top_user");

  lua_pushcfunction(l, lua_get_ipaddress);
  lua_setglobal(l, "bbs_get_user_ip");

  lua_pushcfunction(l, lua_getTermType);
  lua_setglobal(l, "bbs_get_term_type");

  lua_pushcfunction(l, lua_display_sixel);
  lua_setglobal(l, "bbs_display_sixel");

  lua_pushcfunction(l, lua_switch_font);
  lua_setglobal(l, "bbs_switch_font");

  lua_pushcfunction(l, lua_set_timeleft);
  lua_setglobal(l, "bbs_set_time_left");

  lua_pushcfunction(l, lua_get_timeleft);
  lua_setglobal(l, "bbs_get_time_left");

  lua_pushcfunction(l, lua_ansiView);
  lua_setglobal(l, "bbs_ansi_view");
}

bool Script::msgheader(Node *n, std::string script, std::string file, unsigned int mid, std::string from, std::string to, std::string subject) {
  lua_State *l = luaL_newstate();

  init_state(n, l);
  luaL_loadfile(l, script.c_str());

  int ret = lua_pcall(l, 0, 1, 0);
  if (ret) {
    n->log->log(LOG_ERROR, "Error executing msgheader script. \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
    lua_close(l);
    return false;
  }

  lua_getglobal(l, "msgheader");
  lua_pushstring(l, file.c_str());
  lua_pushnumber(l, mid);
  lua_pushstring(l, from.c_str());
  lua_pushstring(l, to.c_str());
  lua_pushstring(l, subject.c_str());
  ret = lua_pcall(l, 5, 0, 0);
  if (ret) {
    n->log->log(LOG_ERROR, "Error executing msgheader script. \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
    lua_close(l);
    return false;
  }
  lua_close(l);
  return true;
}

bool Script::login(Node *n, std::string script, std::string *uname, std::string *password) {
  lua_State *l = luaL_newstate();

  init_state(n, l);
  luaL_loadfile(l, script.c_str());

  int ret = lua_pcall(l, 0, 1, 0);
  if (ret) {
    n->log->log(LOG_ERROR, "Error executing login script. \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
    lua_close(l);
    return false;
  }

  lua_getglobal(l, "login");
  ret = lua_pcall(l, 0, 2, 0);
  if (ret) {
    n->log->log(LOG_ERROR, "Error executing login script. \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
    lua_close(l);
    return false;
  }
  if (lua_tostring(l, -2) != NULL && lua_tostring(l, -1) != NULL) {
    *uname = std::string(lua_tostring(l, -2));
    *password = std::string(lua_tostring(l, -1));
    lua_close(l);
    return true;
  } else {
    lua_close(l);
    n->disconnected();
    return false;
  }
}

void Script::exec(Node *n, std::string script) {
  lua_State *l = luaL_newstate();

  init_state(n, l);

  int ret = luaL_dofile(l, script.c_str());
  if (ret != 0) {
    n->log->log(LOG_ERROR, "Error executing script \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
  }

  lua_close(l);
}

void Script::predoor(Node *n) {
  if (std::filesystem::exists(n->get_config()->script_path() + "/predoor.lua")) {
    exec(n, n->get_config()->script_path() + "/predoor.lua");
  }
}

bool Script::prelogin(Node *n, std::string script) {
  lua_State *l = luaL_newstate();

  init_state(n, l);
  luaL_loadfile(l, script.c_str());

  int ret = lua_pcall(l, 0, 1, 0);
  if (ret) {
    n->log->log(LOG_ERROR, "Error executing prelogin script. \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
    lua_close(l);
    n->disconnected();
    return false;
  }

  lua_getglobal(l, "prelogin");
  ret = lua_pcall(l, 0, 1, 0);
  if (ret) {
    n->log->log(LOG_ERROR, "Error executing prelogin script. \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
    lua_close(l);
    n->disconnected();
    return false;
  }
  if (lua_tonumber(l, -1) == 1) {
    lua_close(l);
    return true;
  } else {
    lua_close(l);
    n->disconnected();
    return false;
  }
}
