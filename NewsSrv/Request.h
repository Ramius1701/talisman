#pragma once

#include <string>
#include <vector>

class MsgGroup;
class Logger;

class Request {
public:
  Request();
  bool loadconfig();
  bool dorequest(int socket, std::string request);
private:
  std::string convert_utf8(std::string input);
  void doarticle(int group, int article, bool byid);
  void dohead(int group, int article, bool byid);
  void dolist(std::string arg);
  int socket;
  std::string msgid(int mid, std::string group);
  bool authenticated;
  std::string msgpath;
  std::string hostname;
  std::vector<MsgGroup *> groups;
  Logger *log;
  int selected_group;
  int selected_article;
};
