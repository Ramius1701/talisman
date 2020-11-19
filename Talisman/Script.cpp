#include <sqlite3.h>
#include <sstream>
#include "Script.h"
#include "Node.h"
#include "Config.h"
#include "User.h"
#include "Logger.h"
#include "Squish.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

extern "C" Node *lua_getNode(lua_State *L) {
    lua_pushstring(L, "bbs_node");
    lua_gettable(L, LUA_REGISTRYINDEX);
    return (Node *)lua_touserdata(L, -1);	
}
/*
extern "C" int lua_getBBSFirstMsg(lua_State *L) {
	int mbid = lua_tonumber(L, -1);
	Node *n = lua_getNode(L);

	lua_pushnumber(L, MessageBase::getFirstMsg(n, n->user, mbid));
	return 1;
}

extern "C" int lua_getBBSNextMsg(lua_State *L) {
	int mbid = lua_tonumber(L, 1);
	int mid = lua_tonumber(L, 2);
	Node *n = lua_getNode(L);

	lua_pushnumber(L, MessageBase::getNextMsg(n, n->user, mbid, mid));
	return 1;	
}

extern "C" int lua_getBBSMsgHeader(lua_State *L) {
	int mid = lua_tonumber(L, -1);
	Node *n = lua_getNode(L);

	static const char *sql = "SELECT sender, recipient, subject FROM msgs WHERE id = ?";

	sqlite3 *db;
	sqlite3_stmt *stmt;

	if (!MessageBase::openDatabase(n->getConfig(), &db)) {
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "No Message");
		return 3;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "No Message");
		sqlite3_close(db);
		return 3;
	}
	sqlite3_bind_int(stmt, 1, mid);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		lua_pushstring(L, (const char *)sqlite3_column_text(stmt, 0));
		lua_pushstring(L, (const char *)sqlite3_column_text(stmt, 1));
		lua_pushstring(L, (const char *)sqlite3_column_text(stmt, 2));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return 3;
	}
	lua_pushstring(L, "Nobody");
	lua_pushstring(L, "Nobody");
	lua_pushstring(L, "No Message");
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return 3;
}

extern "C" int lua_getBBSMsgBody(lua_State *L) {
	int mid = lua_tonumber(L, -1);
	Node *n = lua_getNode(L);

	static const char *sql = "SELECT body FROM msgs WHERE id = ?";

	sqlite3 *db;
	sqlite3_stmt *stmt;

	if (!MessageBase::openDatabase(n->getConfig(), &db)) {
		lua_pushstring(L, "No Message");
		return 1;
	}

	if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
		lua_pushstring(L, "No Message");
		sqlite3_close(db);
		return 1;
	}
	sqlite3_bind_int(stmt, 1, mid);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		lua_pushstring(L, (const char *)sqlite3_column_text(stmt, 0));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return 1;
	}

	lua_pushstring(L, "No Message");
	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return 1;
}

extern "C" int lua_bbsPostNetmail(lua_State *L) {
	int mbid = lua_tonumber(L, 1);
	const char *to = lua_tostring(L, 2);
	const char *from = lua_tostring(L, 3);
	const char *subj = lua_tostring(L, 4);
	const char *body = lua_tostring(L, 5);
	const char *dest = lua_tostring(L, 6);

	Node *n = lua_getNode(L);
	time_t date = time(NULL);

	if (MessageBase::isNetmailBase(n->getConfig(), mbid)) {
		MessageBase::postMessage(n, n->user, mbid, std::string(to), std::string(from), std::string(subj), std::string(body), date, 0, dest);
	}
	return 0;
}
*/

extern "C" int lua_getBBSMsg(lua_State * L) {
	const char* mbfile = lua_tostring(L, 1);
	int mid = lua_tonumber(L, 2);
	Node* n = lua_getNode(L);

	sq_msg_base_t* mb;

	mb = SquishOpenMsgBase(mbfile);

	if (!mb) {
		lua_pushnumber(L, 0);
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "No Message");
		lua_pushstring(L, "No Message");
		return 5;
	}

	if (mid < 1 || mid > mb->basehdr.num_msg) {
		SquishCloseMsgBase(mb);
		lua_pushnumber(L, 0);
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "Nobody");
		lua_pushstring(L, "No Message");
		lua_pushstring(L, "No Message");
		return 5;
	}

	while (mid <= mb->basehdr.num_msg) {
		sq_msg_t* msg;

		msg = SquishReadMsg(mb, mid);
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
		
		char* msgc = (char*)malloc(msg->msg_len + 1);

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
	time_t date = time(NULL);

	for (size_t msgconf = 0; msgconf < n->get_config()->msgconfs.size(); msgconf++) {
		for (size_t msgbase = 0; msgbase < n->get_config()->msgconfs.at(msgconf).areas.size(); msgbase++) {
			if (n->get_config()->msgconfs.at(msgconf).areas.at(msgbase).get_file() == std::string(mbfile) && !n->get_config()->msgconfs.at(msgconf).areas.at(msgbase).is_netmail()) {
				std::stringstream ss;
				std::vector<std::string> msg;

				for (size_t i = 0; i < strlen(body); i++) {
					if (body[i] == '\n') {
						msg.push_back(ss.str());
						ss.str("");
					}
					else {
						ss << body[i];
					}
				}

				if (ss.str().size() > 0) {
					msg.push_back(ss.str());
				}

				n->get_config()->msgconfs.at(msgconf).areas.at(msgbase).save_message(std::string(to), std::string(from), std::string(subj), msg, "", -1);
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
	int length = lua_tonumber(L, -1);

	std::string str = lua_getNode(L)->get_string(length, false, true);
	lua_pushstring(L, str.c_str());
	return 1;
}

extern "C" int lua_BBSGetChar(lua_State *L) {
	char c = lua_getNode(L)->getch();
	lua_pushlstring(L, &c, 1);
	return 1;
}

extern "C" int lua_BBSUsername(lua_State *L) {
	lua_pushstring(L, lua_getNode(L)->get_user().get_username().c_str());
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

void Script::exec(Node *n, std::string script) {
	lua_State *l = luaL_newstate();
	luaL_openlibs(l);

	lua_pushstring(l, "bbs_node");
	lua_pushlightuserdata(l, n);
	lua_settable(l, LUA_REGISTRYINDEX);

	lua_pushcfunction(l, lua_BBSWrite);
	lua_setglobal(l, "bbs_write_string");

	lua_pushcfunction(l, lua_BBSGetString);
	lua_setglobal(l, "bbs_read_string");

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

	lua_pushcfunction(l, lua_BBSClrScr);
	lua_setglobal(l, "bbs_clear_screen");
	/*
	lua_pushcfunction(l, lua_getBBSNextMsg);
	lua_setglobal(l, "bbs_next_message_id");

	lua_pushcfunction(l, lua_getBBSMsgHeader);
	lua_setglobal(l, "bbs_get_message_header");
	*/
	lua_pushcfunction(l, lua_getBBSMsg);
	lua_setglobal(l, "bbs_get_message");
	
	lua_pushcfunction(l, lua_bbsPostMsg);
	lua_setglobal(l, "bbs_post_message");
	/*
	lua_pushcfunction(l, lua_bbsPostNetmail);
	lua_setglobal(l, "bbs_post_netmail");
	*/
	int ret = luaL_dofile(l, script.c_str());
	if(ret != 0){
		n->log->log(LOG_ERROR, "Error executing script \"%s\" -> %s", script.c_str(), lua_tostring(l, -1));
	}

	lua_close(l);
}
