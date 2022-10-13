#pragma once

#include <string>

class MsgGroup {
public:
  MsgGroup(std::string newsgrp, std::string msgbase, std::string description, bool readonly) {
    this->newsgrp = newsgrp;
    this->msgbase = msgbase;
    this->readonly = readonly;
    this->desc = description;
  }

  std::string newsgrp;
  std::string msgbase;
  std::string desc;
  bool readonly;
};
