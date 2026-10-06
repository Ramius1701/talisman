# Lua API evidence

Generated from the checkout by `generate_reference.py`. All matched registered C functions are indexed with exact wrapper implementations below. Stack indices are raw C API positions (negative indices count from the top); return counts describe branches, not semantic types. Use the wrappers to establish argument order, return values, error sentinels and side effects before extending scripts. Wrapper excerpts retain the project source license.

| Lua global | C wrapper | Read stack indices | Return counts | Source |
| --- | --- | --- | --- | --- |
| bbs_write_string | lua_BBSWrite | -1 | 0 | [Talisman/Script.cpp:216](../../../Talisman/Script.cpp#L216) |
| bbs_read_string | lua_BBSGetString | -1 | 1 | [Talisman/Script.cpp:223](../../../Talisman/Script.cpp#L223) |
| bbs_read_password | lua_BBSGetMaskedString | -1 | 1 | [Talisman/Script.cpp:231](../../../Talisman/Script.cpp#L231) |
| bbs_getchar | lua_BBSGetChar | -1 | 1 | [Talisman/Script.cpp:244](../../../Talisman/Script.cpp#L244) |
| bbs_get_data_path | lua_BBSScriptDataPath | none | 1 | [Talisman/Script.cpp:294](../../../Talisman/Script.cpp#L294) |
| bbs_display_gfile | lua_BBSDisplayTextfile | -1 | 0 | [Talisman/Script.cpp:308](../../../Talisman/Script.cpp#L308) |
| bbs_display_gfile_pause | lua_BBSDisplayTextfileP | -1 | 0 | [Talisman/Script.cpp:316](../../../Talisman/Script.cpp#L316) |
| bbs_get_username | lua_BBSUsername | none | 1 | [Talisman/Script.cpp:258](../../../Talisman/Script.cpp#L258) |
| bbs_get_user_location | lua_BBSUserLocation | none | 1 | [Talisman/Script.cpp:283](../../../Talisman/Script.cpp#L283) |
| bbs_get_os | lua_BBSSysName | none | 1 | [Talisman/Script.cpp:268](../../../Talisman/Script.cpp#L268) |
| bbs_get_bbs_name | lua_BBSName | none | 1 | [Talisman/Script.cpp:273](../../../Talisman/Script.cpp#L273) |
| bbs_get_sysop_name | lua_BBSSysopName | none | 1 | [Talisman/Script.cpp:278](../../../Talisman/Script.cpp#L278) |
| bbs_clear_screen | lua_BBSClrScr | none | 0 | [Talisman/Script.cpp:288](../../../Talisman/Script.cpp#L288) |
| bbs_get_message | lua_getBBSMsg | 1, 2 | 5 | [Talisman/Script.cpp:96](../../../Talisman/Script.cpp#L96) |
| bbs_post_message | lua_bbsPostMsg | 1, 2, 3, 4, 5 | 0 | [Talisman/Script.cpp:181](../../../Talisman/Script.cpp#L181) |
| bbs_get_term_width | lua_BBSTermWidth | none | 1 | [Talisman/Script.cpp:325](../../../Talisman/Script.cpp#L325) |
| bbs_get_term_height | lua_BBSTermHeight | none | 1 | [Talisman/Script.cpp:330](../../../Talisman/Script.cpp#L330) |
| bbs_pause | lua_Pause | none | 0 | [Talisman/Script.cpp:366](../../../Talisman/Script.cpp#L366) |
| bbs_get_user_attribute | lua_GetAttrib | 1, 2 | 1 | [Talisman/Script.cpp:335](../../../Talisman/Script.cpp#L335) |
| bbs_get_user_attribute_by_name | lua_GetAttribByName | 1, 2, 3 | 1 | [Talisman/Script.cpp:345](../../../Talisman/Script.cpp#L345) |
| bbs_set_user_attribute | lua_SetAttrib | 1, 2 | 0 | [Talisman/Script.cpp:356](../../../Talisman/Script.cpp#L356) |
| bbs_get_calllog_x | lua_getCallLogX | 1 | 9 | [Talisman/Script.cpp:508](../../../Talisman/Script.cpp#L508) |
| bbs_user_get_total_calls | lua_getTotCalls | 1 | 1 | [Talisman/Script.cpp:373](../../../Talisman/Script.cpp#L373) |
| bbs_user_get_total_uploads | lua_getTotUploads | 1 | 1 | [Talisman/Script.cpp:382](../../../Talisman/Script.cpp#L382) |
| bbs_user_get_total_downloads | lua_getTotDownloads | 1 | 1 | [Talisman/Script.cpp:391](../../../Talisman/Script.cpp#L391) |
| bbs_user_get_total_msgposts | lua_getTotMsgPosts | 1 | 1 | [Talisman/Script.cpp:400](../../../Talisman/Script.cpp#L400) |
| bbs_user_get_total_doorsrun | lua_getTotDoorRuns | 1 | 1 | [Talisman/Script.cpp:409](../../../Talisman/Script.cpp#L409) |
| bbs_get_total_calls | lua_getTotBBSCalls | none | 1 | [Talisman/Script.cpp:418](../../../Talisman/Script.cpp#L418) |
| bbs_get_total_uploads | lua_getTotBBSUploads | none | 1 | [Talisman/Script.cpp:426](../../../Talisman/Script.cpp#L426) |
| bbs_get_total_downloads | lua_getTotBBSDownloads | none | 1 | [Talisman/Script.cpp:434](../../../Talisman/Script.cpp#L434) |
| bbs_get_total_msgposts | lua_getTotBBSMsgPosts | none | 1 | [Talisman/Script.cpp:442](../../../Talisman/Script.cpp#L442) |
| bbs_get_total_doorsrun | lua_getTotBBSDoorRuns | none | 1 | [Talisman/Script.cpp:450](../../../Talisman/Script.cpp#L450) |
| bbs_user_has_ansi | lua_hasAnsi | none | 1 | [Talisman/Script.cpp:541](../../../Talisman/Script.cpp#L541) |
| bbs_user_has_sixel | lua_GetSixelSupport | none | 1 | [Talisman/Script.cpp:89](../../../Talisman/Script.cpp#L89) |
| bbs_edit_ansi | lua_editAnsi | 1, 2, 3 | 0 | [Talisman/Script.cpp:548](../../../Talisman/Script.cpp#L548) |
| bbs_upload | lua_upload | none | 0, 1 | [Talisman/Script.cpp:582](../../../Talisman/Script.cpp#L582) |
| bbs_download | lua_download | 1 | 0 | [Talisman/Script.cpp:560](../../../Talisman/Script.cpp#L560) |
| bbs_rlogin_ip4 | lua_rlogin_ip4 | 1, 2, 3, 4, 5 | 1 | [Talisman/Script.cpp:642](../../../Talisman/Script.cpp#L642) |
| bbs_rlogin_ip6 | lua_rlogin_ip6 | 1, 2, 3, 4, 5 | 1 | [Talisman/Script.cpp:657](../../../Talisman/Script.cpp#L657) |
| bbs_telnet_ip4 | lua_telnet_ip4 | 1, 2 | 1 | [Talisman/Script.cpp:618](../../../Talisman/Script.cpp#L618) |
| bbs_telnet_ip6 | lua_telnet_ip6 | 1, 2 | 1 | [Talisman/Script.cpp:630](../../../Talisman/Script.cpp#L630) |
| bbs_get_node | lua_BBSNode | none | 1 | [Talisman/Script.cpp:263](../../../Talisman/Script.cpp#L263) |
| bbs_get_message_detail | lua_getBBSMsgDetail | 1, 2, 3 | 1 | [Talisman/Script.cpp:28](../../../Talisman/Script.cpp#L28) |
| bbs_get_top_user | lua_get_top | 1, 2 | 2 | [Talisman/Script.cpp:679](../../../Talisman/Script.cpp#L679) |
| bbs_get_user_ip | lua_get_ipaddress | none | 1 | [Talisman/Script.cpp:672](../../../Talisman/Script.cpp#L672) |
| bbs_get_term_type | lua_getTermType | none | 1 | [Talisman/Script.cpp:82](../../../Talisman/Script.cpp#L82) |
| bbs_display_sixel | lua_display_sixel | 1 | 0 | [Talisman/Script.cpp:694](../../../Talisman/Script.cpp#L694) |
| bbs_switch_font | lua_switch_font | 1, 2 | 0 | [Talisman/Script.cpp:720](../../../Talisman/Script.cpp#L720) |
| bbs_set_time_left | lua_set_timeleft | 1 | 0 | [Talisman/Script.cpp:705](../../../Talisman/Script.cpp#L705) |
| bbs_get_time_left | lua_get_timeleft | none | 1 | [Talisman/Script.cpp:713](../../../Talisman/Script.cpp#L713) |
| bbs_ansi_view | lua_ansiView | 1 | 0 | [Talisman/Script.cpp:458](../../../Talisman/Script.cpp#L458) |

## bbs_write_string

Source: [Talisman/Script.cpp:216](../../../Talisman/Script.cpp#L216).

```cpp
extern "C" int lua_BBSWrite(lua_State *L) {
  char *str = (char *)lua_tostring(L, -1);

  lua_getNode(L)->print_f("%s", str);
  return 0;
}
```

## bbs_read_string

Source: [Talisman/Script.cpp:223](../../../Talisman/Script.cpp#L223).

```cpp
extern "C" int lua_BBSGetString(lua_State *L) {
  uint32_t length = (uint32_t)lua_tonumber(L, -1);

  std::string str = lua_getNode(L)->get_string(length, false, true);
  lua_pushstring(L, str.c_str());
  return 1;
}
```

## bbs_read_password

Source: [Talisman/Script.cpp:231](../../../Talisman/Script.cpp#L231).

```cpp
extern "C" int lua_BBSGetMaskedString(lua_State *L) {
  uint32_t length = (uint32_t)lua_tonumber(L, -1);

  std::string str = lua_getNode(L)->get_string(length, true, true);
  lua_pushstring(L, str.c_str());
  return 1;
}
```

## bbs_getchar

Source: [Talisman/Script.cpp:244](../../../Talisman/Script.cpp#L244).

```cpp
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
```

## bbs_get_data_path

Source: [Talisman/Script.cpp:294](../../../Talisman/Script.cpp#L294).

```cpp
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
```

## bbs_display_gfile

Source: [Talisman/Script.cpp:308](../../../Talisman/Script.cpp#L308).

```cpp
extern "C" int lua_BBSDisplayTextfile(lua_State *L) {
  const char *filename = lua_tostring(L, -1);

  Node *n = lua_getNode(L);
  n->send_gfile(filename);
  return 0;
}
```

## bbs_display_gfile_pause

Source: [Talisman/Script.cpp:316](../../../Talisman/Script.cpp#L316).

```cpp
extern "C" int lua_BBSDisplayTextfileP(lua_State *L) {
  const char *filename = lua_tostring(L, -1);

  Node *n = lua_getNode(L);
  n->send_gfile(filename, true);

  return 0;
}
```

## bbs_get_username

Source: [Talisman/Script.cpp:258](../../../Talisman/Script.cpp#L258).

```cpp
extern "C" int lua_BBSUsername(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_user().get_username().c_str());
  return 1;
}
```

## bbs_get_user_location

Source: [Talisman/Script.cpp:283](../../../Talisman/Script.cpp#L283).

```cpp
extern "C" int lua_BBSUserLocation(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_user().get_attribute("location", "Somewhere, The World").c_str());
  return 1;
}
```

## bbs_get_os

Source: [Talisman/Script.cpp:268](../../../Talisman/Script.cpp#L268).

```cpp
extern "C" int lua_BBSSysName(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->operating_system().c_str());
  return 1;
}
```

## bbs_get_bbs_name

Source: [Talisman/Script.cpp:273](../../../Talisman/Script.cpp#L273).

```cpp
extern "C" int lua_BBSName(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_config()->sys_name().c_str());
  return 1;
}
```

## bbs_get_sysop_name

Source: [Talisman/Script.cpp:278](../../../Talisman/Script.cpp#L278).

```cpp
extern "C" int lua_BBSSysopName(lua_State *L) {
  lua_pushstring(L, lua_getNode(L)->get_config()->op_name().c_str());
  return 1;
}
```

## bbs_clear_screen

Source: [Talisman/Script.cpp:288](../../../Talisman/Script.cpp#L288).

```cpp
extern "C" int lua_BBSClrScr(lua_State *L) {
  Node *n = lua_getNode(L);
  n->cls();
  return 0;
}
```

## bbs_get_message

Source: [Talisman/Script.cpp:96](../../../Talisman/Script.cpp#L96).

```cpp
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
```

## bbs_post_message

Source: [Talisman/Script.cpp:181](../../../Talisman/Script.cpp#L181).

```cpp
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
```

## bbs_get_term_width

Source: [Talisman/Script.cpp:325](../../../Talisman/Script.cpp#L325).

```cpp
extern "C" int lua_BBSTermWidth(lua_State *L) {
  lua_pushnumber(L, lua_getNode(L)->get_term_width());
  return 1;
}
```

## bbs_get_term_height

Source: [Talisman/Script.cpp:330](../../../Talisman/Script.cpp#L330).

```cpp
extern "C" int lua_BBSTermHeight(lua_State *L) {
  lua_pushnumber(L, lua_getNode(L)->get_term_height());
  return 1;
}
```

## bbs_pause

Source: [Talisman/Script.cpp:366](../../../Talisman/Script.cpp#L366).

```cpp
extern "C" int lua_Pause(lua_State *L) {
  Node *n = lua_getNode(L);
  n->pause();

  return 0;
}
```

## bbs_get_user_attribute

Source: [Talisman/Script.cpp:335](../../../Talisman/Script.cpp#L335).

```cpp
extern "C" int lua_GetAttrib(lua_State *L) {
  const char *attrib = lua_tostring(L, 1);
  const char *def = lua_tostring(L, 2);
  Node *n = lua_getNode(L);

  lua_pushstring(L, n->get_user().get_attribute(std::string(attrib), std::string(def)).c_str());

  return 1;
}
```

## bbs_get_user_attribute_by_name

Source: [Talisman/Script.cpp:345](../../../Talisman/Script.cpp#L345).

```cpp
extern "C" int lua_GetAttribByName(lua_State *L) {
  const char *name = lua_tostring(L, 1);
  const char *attrib = lua_tostring(L, 2);
  const char *def = lua_tostring(L, 3);
  Node *n = lua_getNode(L);

  lua_pushstring(L, User::get_attribute_s(n->get_config(), std::string(name), std::string(attrib), std::string(def)).c_str());

  return 1;
}
```

## bbs_set_user_attribute

Source: [Talisman/Script.cpp:356](../../../Talisman/Script.cpp#L356).

```cpp
extern "C" int lua_SetAttrib(lua_State *L) {
  const char *attrib = lua_tostring(L, 1);
  const char *value = lua_tostring(L, 2);
  Node *n = lua_getNode(L);

  n->get_user().set_attribute(std::string(attrib), std::string(value));

  return 0;
}
```

## bbs_get_calllog_x

Source: [Talisman/Script.cpp:508](../../../Talisman/Script.cpp#L508).

```cpp
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
```

## bbs_user_get_total_calls

Source: [Talisman/Script.cpp:373](../../../Talisman/Script.cpp#L373).

```cpp
extern "C" int lua_getTotCalls(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::total_calls(n, std::string(username));
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_user_get_total_uploads

Source: [Talisman/Script.cpp:382](../../../Talisman/Script.cpp#L382).

```cpp
extern "C" int lua_getTotUploads(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_uploads(n, username);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_user_get_total_downloads

Source: [Talisman/Script.cpp:391](../../../Talisman/Script.cpp#L391).

```cpp
extern "C" int lua_getTotDownloads(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_downloads(n, username);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_user_get_total_msgposts

Source: [Talisman/Script.cpp:400](../../../Talisman/Script.cpp#L400).

```cpp
extern "C" int lua_getTotMsgPosts(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_msgpost(n, username);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_user_get_total_doorsrun

Source: [Talisman/Script.cpp:409](../../../Talisman/Script.cpp#L409).

```cpp
extern "C" int lua_getTotDoorRuns(lua_State *L) {
  const char *username = lua_tostring(L, 1);
  Node *n = lua_getNode(L);
  int tot = CallLog::get_tot_doors(n, username);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_get_total_calls

Source: [Talisman/Script.cpp:418](../../../Talisman/Script.cpp#L418).

```cpp
extern "C" int lua_getTotBBSCalls(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::total_bbs_calls(n);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_get_total_uploads

Source: [Talisman/Script.cpp:426](../../../Talisman/Script.cpp#L426).

```cpp
extern "C" int lua_getTotBBSUploads(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_uploads(n);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_get_total_downloads

Source: [Talisman/Script.cpp:434](../../../Talisman/Script.cpp#L434).

```cpp
extern "C" int lua_getTotBBSDownloads(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_downloads(n);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_get_total_msgposts

Source: [Talisman/Script.cpp:442](../../../Talisman/Script.cpp#L442).

```cpp
extern "C" int lua_getTotBBSMsgPosts(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_msgpost(n);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_get_total_doorsrun

Source: [Talisman/Script.cpp:450](../../../Talisman/Script.cpp#L450).

```cpp
extern "C" int lua_getTotBBSDoorRuns(lua_State *L) {
  Node *n = lua_getNode(L);
  int tot = CallLog::get_bbs_tot_doors(n);
  lua_pushnumber(L, tot);

  return 1;
}
```

## bbs_user_has_ansi

Source: [Talisman/Script.cpp:541](../../../Talisman/Script.cpp#L541).

```cpp
extern "C" int lua_hasAnsi(lua_State *L) {
  Node *n = lua_getNode(L);
  bool hasAnsi = n->hasANSI;
  lua_pushboolean(L, hasAnsi);
  return 1;
}
```

## bbs_user_has_sixel

Source: [Talisman/Script.cpp:89](../../../Talisman/Script.cpp#L89).

```cpp
extern "C" int lua_GetSixelSupport(lua_State *L) {
  Node *n = lua_getNode(L);
  lua_pushboolean(L, n->sixel_support());

  return 1;
}
```

## bbs_edit_ansi

Source: [Talisman/Script.cpp:548](../../../Talisman/Script.cpp#L548).

```cpp
extern "C" int lua_editAnsi(lua_State *L) {
  int width = lua_tonumber(L, 1);
  int height = lua_tonumber(L, 2);
  const char *filename = lua_tostring(L, 3);
  Node *n = lua_getNode(L);

  AnsiEditor a(n, width, height);
  a.edit(std::string(filename));

  return 0;
}
```

## bbs_upload

Source: [Talisman/Script.cpp:582](../../../Talisman/Script.cpp#L582).

```cpp
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
```

## bbs_download

Source: [Talisman/Script.cpp:560](../../../Talisman/Script.cpp#L560).

```cpp
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
```

## bbs_rlogin_ip4

Source: [Talisman/Script.cpp:642](../../../Talisman/Script.cpp#L642).

```cpp
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
```

## bbs_rlogin_ip6

Source: [Talisman/Script.cpp:657](../../../Talisman/Script.cpp#L657).

```cpp
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
```

## bbs_telnet_ip4

Source: [Talisman/Script.cpp:618](../../../Talisman/Script.cpp#L618).

```cpp
extern "C" int lua_telnet_ip4(lua_State *L) {
  const char *host = lua_tostring(L, 1);
  int port = lua_tonumber(L, 2);
  Node *n = lua_getNode(L);

  bool result = Telnet::session(n, std::string(host), port, false);

  lua_pushboolean(L, result);

  return 1;
}
```

## bbs_telnet_ip6

Source: [Talisman/Script.cpp:630](../../../Talisman/Script.cpp#L630).

```cpp
extern "C" int lua_telnet_ip6(lua_State *L) {
  const char *host = lua_tostring(L, 1);
  int port = lua_tonumber(L, 2);
  Node *n = lua_getNode(L);

  bool result = Telnet::session(n, std::string(host), port, true);

  lua_pushboolean(L, result);

  return 1;
}
```

## bbs_get_node

Source: [Talisman/Script.cpp:263](../../../Talisman/Script.cpp#L263).

```cpp
extern "C" int lua_BBSNode(lua_State *L) {
  lua_pushnumber(L, lua_getNode(L)->getnodenum());
  return 1;
}
```

## bbs_get_message_detail

Source: [Talisman/Script.cpp:28](../../../Talisman/Script.cpp#L28).

```cpp
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
```

## bbs_get_top_user

Source: [Talisman/Script.cpp:679](../../../Talisman/Script.cpp#L679).

```cpp
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
```

## bbs_get_user_ip

Source: [Talisman/Script.cpp:672](../../../Talisman/Script.cpp#L672).

```cpp
extern "C" int lua_get_ipaddress(lua_State *L) {
  Node *n = lua_getNode(L);

  lua_pushstring(L, n->ipaddr.c_str());
  return 1;
}
```

## bbs_get_term_type

Source: [Talisman/Script.cpp:82](../../../Talisman/Script.cpp#L82).

```cpp
extern "C" int lua_getTermType(lua_State *L) {
  Node *n = lua_getNode(L);
  lua_pushstring(L, n->get_term_type());

  return 1;
}
```

## bbs_display_sixel

Source: [Talisman/Script.cpp:694](../../../Talisman/Script.cpp#L694).

```cpp
extern "C" int lua_display_sixel(lua_State *L) {
  const char *sixel = lua_tostring(L, 1);
  Node *n = lua_getNode(L);

  if (n->sixel_support()) {
    n->send_raw(std::string(sixel));
  }

  return 0;
}
```

## bbs_switch_font

Source: [Talisman/Script.cpp:720](../../../Talisman/Script.cpp#L720).

```cpp
extern "C" int lua_switch_font(lua_State *L) {
  int font = lua_tonumber(L, 1);
  int place = lua_tonumber(L, 2);
  Node *n = lua_getNode(L);

  n->switch_font(font, place);

  return 0;
}
```

## bbs_set_time_left

Source: [Talisman/Script.cpp:705](../../../Talisman/Script.cpp#L705).

```cpp
extern "C" int lua_set_timeleft(lua_State *L) {
  time_t tl = lua_tonumber(L, 1);
  Node *n = lua_getNode(L);
  n->get_user().set_attribute("time_left", std::to_string(tl));
  n->set_timeleft(tl * 60);
  return 0;
}
```

## bbs_get_time_left

Source: [Talisman/Script.cpp:713](../../../Talisman/Script.cpp#L713).

```cpp
extern "C" int lua_get_timeleft(lua_State *L) {
  Node *n = lua_getNode(L);

  lua_pushnumber(L, n->get_timeleft() / 60);
  return 1;
}
```

## bbs_ansi_view

Source: [Talisman/Script.cpp:458](../../../Talisman/Script.cpp#L458).

```cpp
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
```
