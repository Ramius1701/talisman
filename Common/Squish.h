#pragma once

#include <cstdint>
#include <cstdio>

typedef uint8_t sq_char;
typedef uint16_t sq_word;
typedef int16_t sq_sword;
typedef uint32_t sq_dword;
typedef uint32_t FOFS;
typedef uint32_t UMSGID;

#define MSGPRIVATE 0x00000001
#define MSGCRASH 0x00000002
#define MSGREAD 0x00000004
#define MSGSENT 0x00000008
#define MSGFILE 0x00000010
#define MSGFWD 0x00000020
#define MSGORPHAN 0x00000040
#define MSGKILL 0x00000080
#define MSGLOCAL 0x00000100
#define MSGHOLD 0x00000200
#define MSGXX2 0x00000400
#define MSGFRQ 0x00000800
#define MSGRRQ 0x00001000
#define MSGCPT 0x00002000
#define MSGARQ 0x00004000
#define MSGURQ 0x00008000
#define MSGSCANNED 0x00010000
#define MSGUID 0x00020000

#pragma pack(push, 1)

typedef struct _SCOMBO {
  sq_word date;
  sq_word time;
} SCOMBO;

typedef struct _NETADDR {
  sq_word zone;
  sq_word net;
  sq_word node;
  sq_word point;
} NETADDR;

typedef struct _sqbase {
  sq_word len;
  sq_word reserved;
  sq_dword num_msg;
  sq_dword high_msg;
  sq_dword skip_msg;
  sq_dword high_water;
  sq_dword uid;
  char base[80];
  FOFS begin_frame;
  FOFS last_frame;
  FOFS free_frame;
  FOFS last_free_frame;
  FOFS end_frame;
  sq_dword max_msg;
  sq_word keep_days;
  sq_word sz_sqhdr;
  char reserved2[124];
} SQBASE;

typedef struct _sqhdr {
  sq_dword id;
  FOFS next_frame;
  FOFS prev_frame;
  sq_dword frame_length;
  sq_dword msg_length;
  sq_dword clen;
  sq_word frame_type;
  sq_word reserved;
} SQHDR;

typedef struct _xmsg {
  sq_dword attr;
  char from[36];
  char to[36];
  char subject[72];
  NETADDR orig;
  NETADDR dest;
  SCOMBO date_written;
  SCOMBO date_arrived;
  sq_sword utc_ofs;
  UMSGID replyto;
  UMSGID replies[9];
  UMSGID umsgid;
  char __ftsc_date[20];
} XMSG;

typedef struct _sqidx {
  FOFS ofs;
  UMSGID umsgid;
  sq_dword hash;
} SQIDX;

typedef struct sq_msg_base {
  FILE *indexfile;
  FILE *datafile;
  SQBASE basehdr;
  int ihavelock;
} sq_msg_base_t;

typedef struct sq_msg {
  FOFS ofs;
  XMSG xmsg;
  char *ctrl;
  int ctrl_len;
  char *msg;
  int msg_len;
} sq_msg_t;

#pragma pack(pop)

extern sq_msg_base_t *SquishOpenMsgBase(const char *filename);
extern sq_msg_t *SquishReadMsg(sq_msg_base_t *mb, sq_dword msgno);
extern void SquishFreeMsg(sq_msg_t *msg);
extern void SquishCloseMsgBase(sq_msg_base_t *mb);
extern bool SquishLockMsgBase(sq_msg_base_t *mb);
extern void SquishUnlockMsgBase(sq_msg_base_t *mb);
extern int SquishWriteMsg(sq_msg_base_t *mb, sq_msg_t *msg);
extern NETADDR *parse_fido_addr(const char *str);
extern int SquishUpdateHdr(sq_msg_base_t *mb, sq_msg_t *msg);
extern int SquishDeleteMsg(sq_msg_base_t *mb, sq_msg_t *msg);
extern sq_dword SquishUMSGID2Offset(sq_msg_base_t *mb, UMSGID mid, int next_m_if_not_found);
extern int SquishPackMsgBase(const char *str);
extern int SquishPruneMsgBase(sq_msg_base_t *mb, sq_dword leave);
