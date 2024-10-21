#pragma once

#include <vector>
#include <string>
#include "../Common/Squish.h"

class MsgArea;
class Node;

static const uint32_t DISABLE_REPLY = 0x00000001;
static const uint32_t DISABLE_DELETE = 0x00000002;
static const uint32_t DISABLE_DOWNLOAD = 0x00000004;
static const uint32_t DISABLE_HEADER = 0x00000008;
static const uint32_t DISABLE_SEARCH = 0x00000010;
static const uint32_t DISABLE_UNREAD = 0x00000020;
static const uint32_t DISABLE_NEXT = 0x00000040;
static const uint32_t DISABLE_PREV = 0x00000080;
static const uint32_t DISABLE_KLUDGE = 0x00000100;

struct line_t {
  std::string line;
  int type;
};

struct msg_reader_msg_t {
  bool ansi;
  bool showkluges;
  int msg_type;

  int msg_no;
  int msg_serial;

  MsgArea *area;
  std::string subject;
  std::string to;
  std::string from;
  std::string date;
  NETADDR *origaddr;
  NETADDR *destaddr;

  std::vector<struct line_t> *body;
};

class MessageReader {
public:
  static int read_message(Node *n, struct msg_reader_msg_t *msg, int tot_msgs, uint32_t flags);

private:
  static bool print_header(Node *n, struct msg_reader_msg_t *msg, int tot_msgs);
  static int read_message_fsr(Node *n, struct msg_reader_msg_t *msg, int tot_msgs, uint32_t flags);
  static int read_message_classic(Node *n, struct msg_reader_msg_t *msg, int tot_msgs, uint32_t flags);
};