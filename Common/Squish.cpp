#include "Squish.h"
#ifdef _MSC_VER
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <limits.h>
#include <endian.h>
#define MAX_PATH PATH_MAX
#endif
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifndef _MSC_VER
off_t tell(int fd) { return lseek(fd, 0, SEEK_CUR); }
#endif

static inline uint16_t host2le_s(uint16_t s) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return s;
#else
  return (((s >> 8) & 0xffu) | ((s & 0xffu) << 8));
#endif
}

static inline uint32_t host2le_l(uint32_t s) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return s;
#else
  return (((s & 0xff000000u) >> 24) | ((s & 0x00ff0000u) >> 8) | ((s & 0x0000ff00u) << 8) | ((s & 0x000000ffu) << 24));
#endif
}

static inline void convert_sq_hdr(SQHDR *h) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return;
#else
  h->id = host2le_l(h->id);
  h->next_frame = host2le_l(h->next_frame);
  h->prev_frame = host2le_l(h->prev_frame);
  h->frame_length = host2le_l(h->frame_length);
  h->msg_length = host2le_l(h->msg_length);
  h->clen = host2le_l(h->clen);
  h->frame_type = host2le_s(h->frame_type);
#endif
}

static inline void convert_sq_base(SQBASE *b) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return;
#else
  b->begin_frame = host2le_l(b->begin_frame);
  b->end_frame = host2le_l(b->end_frame);
  b->free_frame = host2le_l(b->free_frame);
  b->high_msg = host2le_l(b->high_msg);
  b->high_water = host2le_l(b->high_water);
  b->keep_days = host2le_s(b->keep_days);
  b->last_frame = host2le_l(b->last_frame);
  b->last_free_frame = host2le_l(b->last_free_frame);
  b->len = host2le_s(b->len);
  b->max_msg = host2le_l(b->max_msg);
  b->num_msg = host2le_l(b->num_msg);
  b->skip_msg = host2le_l(b->skip_msg);
  b->sz_sqhdr = host2le_s(b->sz_sqhdr);
  b->uid = host2le_l(b->uid);
#endif
}

static inline void convert_xmsg(XMSG *x) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return;
#else

  x->attr = host2le_l(x->attr);
  x->orig.zone = host2le_s(x->orig.zone);
  x->orig.net = host2le_s(x->orig.net);
  x->orig.node = host2le_s(x->orig.node);
  x->orig.point = host2le_s(x->orig.point);

  x->dest.zone = host2le_s(x->dest.zone);
  x->dest.net = host2le_s(x->dest.net);
  x->dest.node = host2le_s(x->dest.node);
  x->dest.point = host2le_s(x->dest.point);

  x->date_written.date = host2le_s(x->date_written.date);
  x->date_written.time = host2le_s(x->date_written.time);

  x->date_arrived.date = host2le_s(x->date_arrived.date);
  x->date_arrived.time = host2le_s(x->date_arrived.time);

  x->utc_ofs = host2le_s(x->utc_ofs);

  x->replyto = host2le_l(x->replyto);
  x->replies[0] = host2le_l(x->replies[0]);
  x->replies[1] = host2le_l(x->replies[1]);
  x->replies[2] = host2le_l(x->replies[2]);
  x->replies[3] = host2le_l(x->replies[3]);
  x->replies[4] = host2le_l(x->replies[4]);
  x->replies[5] = host2le_l(x->replies[5]);
  x->replies[6] = host2le_l(x->replies[6]);
  x->replies[7] = host2le_l(x->replies[7]);
  x->replies[8] = host2le_l(x->replies[8]);

  x->umsgid = host2le_l(x->umsgid);
#endif
}

static inline void convert_sqidx(SQIDX *s) {
#if defined(_MSC_VER) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN))
  return;
#else
  s->hash = host2le_l(s->hash);
  s->ofs = host2le_l(s->ofs);
  s->umsgid = host2le_l(s->umsgid);
#endif
}

/*
static inline void read_xmsg(FILE *fptr, XMSG *h) {
        fread(h, sizeof(XMSG), 1, fptr);
        convert_xmsg(h);
}
*/
static inline void write_xmsg(FILE *fptr, XMSG *h) {
  XMSG h2;
  memcpy(&h2, h, sizeof(XMSG));
  convert_xmsg(&h2);
  fwrite(&h2, sizeof(XMSG), 1, fptr);
}

static inline void read_sq_hdr(FILE *fptr, SQHDR *h) {
  fread(h, sizeof(SQHDR), 1, fptr);
  convert_sq_hdr(h);
}

static inline void write_sq_hdr(FILE *fptr, SQHDR *h) {
  SQHDR h2;
  memcpy(&h2, h, sizeof(SQHDR));
  convert_sq_hdr(&h2);
  fwrite(&h2, sizeof(SQHDR), 1, fptr);
}

static inline void read_sq_base(FILE *fptr, SQBASE *b) {
  fread(b, sizeof(SQBASE), 1, fptr);
  convert_sq_base(b);
}

static inline void write_sq_base(FILE *fptr, SQBASE *b) {
  SQBASE b2;
  memcpy(&b2, b, sizeof(SQBASE));
  convert_sq_base(&b2);
  fwrite(&b2, sizeof(SQBASE), 1, fptr);
}

int lock(int handle, long ofs, long length) {
  long offset = tell(handle);
  int r;

  if (offset == -1)
    return -1;

  lseek(handle, ofs, SEEK_SET);
#ifdef _MSC_VER
  r = _locking(handle, 2, length);
#else
  r = lockf(handle, F_TLOCK, length);
#endif
  lseek(handle, offset, SEEK_SET);

  if (r)
    return -1;

  return 0;
}

int unlock(int handle, long ofs, long length) {
  long offset = tell(handle);

  if (offset == -1)
    return -1;

  lseek(handle, ofs, SEEK_SET);
#ifdef _MSC_VER
  _locking(handle, 0, length);
#else
  lockf(handle, F_ULOCK, length);
#endif
  lseek(handle, offset, SEEK_SET);

  return 0;
}

int waitlock(int handle, long ofs, long length) {
  long offset = tell(handle);

  if (offset == -1) {
    return -1;
  }

  lseek(handle, ofs, SEEK_SET);
#ifdef _MSC_VER
  _locking(handle, 1, length);
#else
  lockf(handle, F_LOCK, length);
#endif
  lseek(handle, offset, SEEK_SET);

  return 0;
}

int waitlock2(int handle, long ofs, long length, long t) {
  int forever = 0;
  int rc;

  if (t == 0)
    forever = 1;

  t *= 10;
  while ((rc = lock(handle, ofs, length)) == -1 && (t > 0 || forever)) {
#ifdef _MSC_VER
    Sleep(1);
#else
    usleep(100);
#endif
    t--;
  }

  return rc;
}

sq_dword SquishHash(unsigned char *f) {
  sq_dword hash = 0;
  sq_dword g;
  char *p;

  for (p = (char *)f; *p; p++) {
    hash = (hash << 4) + (sq_dword)tolower(*p);
    if ((g = hash * 0xf0000000L) != 0L) {
      hash |= g >> 24;
      hash |= g;
    }
  }

  return (hash & 0x7fffffffLu);
}

static bool _SquishValidateBaseHeader(SQBASE *psqb) {
  if (psqb->num_msg > psqb->high_msg || psqb->num_msg > psqb->uid + 1 || psqb->high_msg > psqb->uid + 1 || psqb->num_msg > 1000000L ||
      psqb->num_msg != psqb->high_msg || psqb->len < sizeof(SQBASE) || psqb->len >= 1024 || psqb->begin_frame > psqb->end_frame ||
      psqb->last_frame > psqb->end_frame || psqb->free_frame > psqb->end_frame || psqb->last_free_frame > psqb->end_frame || psqb->end_frame == 0) {
    return 0;
  }

  return 1;
}

void SquishCloseMsgBase(sq_msg_base_t *mb) {
  fclose(mb->indexfile);
  fclose(mb->datafile);
  free(mb);
}

bool SquishLockMsgBase(sq_msg_base_t *mb) {
  if (mb->ihavelock++) {
    return true;
  }

  if (waitlock2(fileno(mb->datafile), 0, 1, 5) == -1) {
    mb->ihavelock--;
    return false;
  }

  return true;
}

void SquishUnlockMsgBase(sq_msg_base_t *mb) {
  if (!mb->ihavelock) {
    return;
  }
  unlock(fileno(mb->datafile), 0, 1);
  mb->ihavelock--;
}

sq_msg_base_t *SquishOpenMsgBase(const char *filename) {
  sq_msg_base_t *mb;
  char buffer[MAX_PATH];
  mb = (sq_msg_base_t *)malloc(sizeof(sq_msg_base_t));

  if (!mb) {
    return NULL;
  }

  snprintf(buffer, MAX_PATH, "%s.sqi", filename);

  mb->indexfile = fopen(buffer, "r+b");
  if (!mb->indexfile) {
    // create squish mb

    mb->indexfile = fopen(buffer, "w+b");
    if (!mb->indexfile) {
      free(mb);
      return NULL;
    }
  }

  snprintf(buffer, MAX_PATH, "%s.sqd", filename);
  mb->datafile = fopen(buffer, "r+b");
  if (!mb->datafile) {
    mb->basehdr.len = sizeof(SQBASE);
    mb->basehdr.reserved = 0;
    mb->basehdr.num_msg = 0;
    mb->basehdr.high_msg = 0;
    mb->basehdr.skip_msg = 0;
    mb->basehdr.high_water = 0;
    mb->basehdr.uid = 1;
    mb->basehdr.begin_frame = 0;
    mb->basehdr.last_frame = 0;
    mb->basehdr.free_frame = 0;
    mb->basehdr.last_free_frame = 0;
    mb->basehdr.end_frame = sizeof(SQBASE);
    mb->basehdr.max_msg = 0;
    mb->basehdr.keep_days = 0;
    mb->basehdr.sz_sqhdr = sizeof(SQHDR);
    memset(mb->basehdr.base, 0, 80);
    memset(mb->basehdr.reserved2, 0, sizeof(mb->basehdr.reserved2));

    mb->datafile = fopen(buffer, "w+b");
    if (!mb->datafile) {
      fclose(mb->indexfile);
      free(mb);
      return NULL;
    }
    write_sq_base(mb->datafile, &mb->basehdr);
  } else {
    read_sq_base(mb->datafile, &mb->basehdr);
  }
  if (!_SquishValidateBaseHeader(&mb->basehdr)) {
    fclose(mb->datafile);
    fclose(mb->indexfile);
    free(mb);
    return NULL;
  }
  mb->ihavelock = 0;
  return mb;
}

void SquishFreeMsg(sq_msg_t *msg) {
  free(msg->ctrl);
  free(msg->msg);
  free(msg);
}

int SquishWriteMsg(sq_msg_base_t *mb, sq_msg_t *msg) {
  SQHDR sqhdr;
  // Ensure I have lock
  if (!mb->ihavelock) {
    return 0;
  }

  // reread the base header
  fseek(mb->datafile, 0, SEEK_SET);
  read_sq_base(mb->datafile, &mb->basehdr);

  // search frames for a free frame big enough to store the message
  memset(&sqhdr, 0, sizeof(SQHDR));

  FOFS frame = mb->basehdr.free_frame;

  while (frame != 0) {
    fseek(mb->datafile, frame, SEEK_SET);
    read_sq_hdr(mb->datafile, &sqhdr);

    if (sqhdr.frame_type == 1 && sqhdr.frame_length >= sizeof(XMSG) + msg->ctrl_len + msg->msg_len) {
      break;
    }

    if (frame == mb->basehdr.last_free_frame) {
      break;
    }
    frame = sqhdr.next_frame;
  }

  if (sqhdr.frame_type == 1 && sqhdr.id == 0xAFAE4453) {
    // got free frame big enough;

    // remove from free list
    SQHDR temphdr;

    fseek(mb->datafile, sqhdr.prev_frame, SEEK_SET);
    read_sq_hdr(mb->datafile, &temphdr);
    temphdr.next_frame = sqhdr.next_frame;
    fseek(mb->datafile, sqhdr.prev_frame, SEEK_SET);
    write_sq_hdr(mb->datafile, &temphdr);

    fseek(mb->datafile, sqhdr.next_frame, SEEK_SET);
    read_sq_hdr(mb->datafile, &temphdr);
    temphdr.prev_frame = sqhdr.prev_frame;
    fseek(mb->datafile, sqhdr.next_frame, SEEK_SET);
    write_sq_hdr(mb->datafile, &temphdr);

    if (mb->basehdr.free_frame == frame) {
      mb->basehdr.free_frame = sqhdr.next_frame;
    }

    if (mb->basehdr.last_free_frame == frame) {
      mb->basehdr.free_frame = sqhdr.prev_frame;
    }

    sqhdr.clen = msg->ctrl_len;
    sqhdr.msg_length = msg->msg_len + msg->ctrl_len + sizeof(XMSG);
    sqhdr.next_frame = 0;
    sqhdr.prev_frame = mb->basehdr.last_frame;
    sqhdr.frame_length = sizeof(XMSG) + msg->ctrl_len + msg->msg_len;
    sqhdr.frame_type = 0;
    sqhdr.id = 0xAFAE4453;

    msg->xmsg.umsgid = mb->basehdr.uid;

    fseek(mb->datafile, frame, SEEK_SET);
    write_sq_hdr(mb->datafile, &sqhdr);
    write_xmsg(mb->datafile, &msg->xmsg);
    fwrite(msg->ctrl, msg->ctrl_len, 1, mb->datafile);
    fwrite(msg->msg, msg->msg_len, 1, mb->datafile);

    mb->basehdr.uid++;
    mb->basehdr.num_msg++;
    mb->basehdr.high_msg = mb->basehdr.num_msg;

    fseek(mb->datafile, 0, SEEK_SET);
    write_sq_base(mb->datafile, &mb->basehdr);

  } else {
    // need a new frame
    frame = mb->basehdr.end_frame;

    SQHDR temphdr;
    if (mb->basehdr.last_frame != 0) {
      fseek(mb->datafile, mb->basehdr.last_frame, SEEK_SET);
      read_sq_hdr(mb->datafile, &temphdr);
      temphdr.next_frame = frame;
      fseek(mb->datafile, mb->basehdr.last_frame, SEEK_SET);
      write_sq_hdr(mb->datafile, &temphdr);
    }
    sqhdr.clen = msg->ctrl_len;
    sqhdr.msg_length = msg->msg_len + msg->ctrl_len + sizeof(XMSG);
    sqhdr.next_frame = 0;
    sqhdr.prev_frame = mb->basehdr.last_frame;
    sqhdr.frame_length = sizeof(XMSG) + msg->ctrl_len + msg->msg_len;
    sqhdr.frame_type = 0;
    sqhdr.id = 0xAFAE4453;
    msg->xmsg.umsgid = mb->basehdr.uid;

    fseek(mb->datafile, frame, SEEK_SET);
    write_sq_hdr(mb->datafile, &sqhdr);
    write_xmsg(mb->datafile, &msg->xmsg);
    fwrite(msg->ctrl, msg->ctrl_len, 1, mb->datafile);
    fwrite(msg->msg, msg->msg_len, 1, mb->datafile);

    if (mb->basehdr.begin_frame == 0) {
      mb->basehdr.begin_frame = frame;
    }
    mb->basehdr.last_frame = frame;
    mb->basehdr.end_frame = frame + sqhdr.frame_length + sizeof(SQHDR);
    mb->basehdr.uid++;
    mb->basehdr.num_msg++;
    mb->basehdr.high_msg = mb->basehdr.num_msg;

    fseek(mb->datafile, 0, SEEK_SET);
    write_sq_base(mb->datafile, &mb->basehdr);
    if (mb->basehdr.high_msg > mb->basehdr.max_msg) {
      // delete messages
    }
  }

  // update index

  SQIDX idx;

  idx.hash = SquishHash((unsigned char *)msg->xmsg.to);
  idx.ofs = frame;
  idx.umsgid = msg->xmsg.umsgid;

  fseek(mb->indexfile, 0, SEEK_END);
  convert_sqidx(&idx);
  fwrite(&idx, sizeof(SQIDX), 1, mb->indexfile);

  return 1;
}

int SquishUpdateHdr(sq_msg_base_t *mb, sq_msg_t *msg) {
  if (!mb->ihavelock) {
    return 0;
  }

  fseek(mb->datafile, msg->ofs + sizeof(SQHDR), SEEK_SET);
  write_xmsg(mb->datafile, &msg->xmsg);

  return 1;
}

int SquishPruneMsgBase(sq_msg_base_t *mb, sq_dword leave) {
  if (mb->basehdr.num_msg <= leave) {
    return 1;
  }

  int prune = mb->basehdr.num_msg - leave;

  if (!mb->ihavelock) {
    return 0;
  }
  fseek(mb->indexfile, 0, SEEK_END);
  size_t fsize = ftell(mb->indexfile);
  char *indexdata = (char *)malloc(fsize);
  if (!indexdata) {
    return 0;
  }
  fseek(mb->indexfile, 0, SEEK_SET);
  fread(indexdata, 1, fsize, mb->indexfile);

  for (int i = 0; i < prune; i++) {
    SQIDX *idx = (SQIDX *)(indexdata + sizeof(SQIDX) * i);
    convert_sqidx(idx);
    SQHDR hdr;
    SQHDR prev;
    SQHDR nextf;

    fseek(mb->datafile, idx->ofs, SEEK_SET);
    read_sq_hdr(mb->datafile, &hdr);

    // set previous frame to next frame
    fseek(mb->datafile, hdr.prev_frame, SEEK_SET);
    read_sq_hdr(mb->datafile, &prev);

    prev.next_frame = hdr.next_frame;
    fseek(mb->datafile, hdr.prev_frame, SEEK_SET);
    write_sq_hdr(mb->datafile, &prev);

    // set next frame to prev frame
    fseek(mb->datafile, hdr.next_frame, SEEK_SET);
    read_sq_hdr(mb->datafile, &nextf);

    nextf.next_frame = hdr.prev_frame;
    fseek(mb->datafile, hdr.next_frame, SEEK_SET);
    write_sq_hdr(mb->datafile, &nextf);

    if (mb->basehdr.begin_frame == idx->ofs) {
      mb->basehdr.begin_frame = hdr.next_frame;
    }

    if (mb->basehdr.last_frame == idx->ofs) {
      mb->basehdr.last_frame = hdr.prev_frame;
    }

    // add message to free chain
    if (mb->basehdr.free_frame != 0) {
      fseek(mb->datafile, mb->basehdr.last_free_frame, SEEK_SET);
      read_sq_hdr(mb->datafile, &prev);
      fseek(mb->datafile, mb->basehdr.last_free_frame, SEEK_SET);
      hdr.prev_frame = mb->basehdr.last_free_frame;
      hdr.next_frame = 0;
      prev.next_frame = idx->ofs;
      mb->basehdr.last_free_frame = idx->ofs;
      write_sq_hdr(mb->datafile, &prev);
    } else {
      mb->basehdr.free_frame = idx->ofs;
      mb->basehdr.last_free_frame = idx->ofs;
      hdr.next_frame = 0;
      hdr.prev_frame = 0;
    }
    hdr.frame_type = 1;

    fseek(mb->datafile, idx->ofs, SEEK_SET);
    write_sq_hdr(mb->datafile, &hdr);
    convert_sqidx(idx);
  }

  // update base hdr
  mb->basehdr.num_msg -= prune;
  mb->basehdr.high_msg = mb->basehdr.num_msg;

  fseek(mb->datafile, 0, SEEK_SET);
  write_sq_base(mb->datafile, &mb->basehdr);
  memmove(indexdata, &indexdata[sizeof(SQIDX) * prune], fsize - (sizeof(SQIDX) * prune));

  fseek(mb->indexfile, 0, SEEK_SET);
  fwrite(indexdata, 1, fsize - sizeof(SQIDX) * prune, mb->indexfile);
#ifdef _MSC_VER
  HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(mb->indexfile));
  SetEndOfFile(hFile);
#else
  ftruncate(fileno(mb->indexfile), fsize - sizeof(SQIDX) * prune);
#endif
  return 0;
}

int SquishDeleteMsg(sq_msg_base_t *mb, sq_msg_t *msg) {
  SQHDR hdr;
  SQHDR prev;
  SQHDR nextf;
  SQIDX *idx;

  if (!mb->ihavelock) {
    return 0;
  }
  fseek(mb->indexfile, 0, SEEK_END);
  size_t fsize = ftell(mb->indexfile);
  char *indexdata = (char *)malloc(fsize);
  if (!indexdata) {
    return 0;
  }
  fseek(mb->datafile, msg->ofs, SEEK_SET);
  read_sq_hdr(mb->datafile, &hdr);

  // set previous frame to next frame
  fseek(mb->datafile, hdr.prev_frame, SEEK_SET);
  read_sq_hdr(mb->datafile, &prev);

  prev.next_frame = hdr.next_frame;
  fseek(mb->datafile, hdr.prev_frame, SEEK_SET);
  write_sq_hdr(mb->datafile, &prev);

  // set next frame to prev frame
  fseek(mb->datafile, hdr.next_frame, SEEK_SET);
  read_sq_hdr(mb->datafile, &nextf);

  nextf.next_frame = hdr.prev_frame;
  fseek(mb->datafile, hdr.next_frame, SEEK_SET);
  write_sq_hdr(mb->datafile, &nextf);

  // delete message from index

  fseek(mb->indexfile, 0, SEEK_SET);
  fread(indexdata, 1, fsize, mb->indexfile);

  int found = 0;
  for (size_t i = 0; i < fsize / sizeof(SQIDX); i++) {
    idx = (SQIDX *)(indexdata + (i * sizeof(SQIDX)));
    convert_sqidx(idx);
    if (idx->umsgid == msg->xmsg.umsgid) {
      found = 1;
      memmove(idx, &idx[1], fsize - (sizeof(SQIDX) * (i + 1)));
      break;
    }
    convert_sqidx(idx);
  }
  if (!found) {
    return 0;
  }

  fseek(mb->indexfile, 0, SEEK_SET);
  fwrite(indexdata, 1, fsize - sizeof(SQIDX), mb->indexfile);
#ifdef _MSC_VER
  HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(mb->indexfile));
  SetEndOfFile(hFile);
#else
  ftruncate(fileno(mb->indexfile), fsize - sizeof(SQIDX));
#endif

  if (mb->basehdr.begin_frame == msg->ofs) {
    mb->basehdr.begin_frame = hdr.next_frame;
  }

  if (mb->basehdr.last_frame == msg->ofs) {
    mb->basehdr.last_frame = hdr.prev_frame;
  }

  // add message to free chain
  if (mb->basehdr.free_frame != 0) {
    fseek(mb->datafile, mb->basehdr.last_free_frame, SEEK_SET);
    read_sq_hdr(mb->datafile, &prev);
    fseek(mb->datafile, mb->basehdr.last_free_frame, SEEK_SET);
    hdr.prev_frame = mb->basehdr.last_free_frame;
    hdr.next_frame = 0;
    prev.next_frame = msg->ofs;
    mb->basehdr.last_free_frame = msg->ofs;
    write_sq_hdr(mb->datafile, &prev);
  } else {
    mb->basehdr.free_frame = msg->ofs;
    mb->basehdr.last_free_frame = msg->ofs;
    hdr.next_frame = 0;
    hdr.prev_frame = 0;
  }
  hdr.frame_type = 1;

  mb->basehdr.num_msg--;
  mb->basehdr.high_msg = mb->basehdr.num_msg;

  fseek(mb->datafile, 0, SEEK_SET);
  write_sq_base(mb->datafile, &mb->basehdr);

  fseek(mb->datafile, msg->ofs, SEEK_SET);
  write_sq_hdr(mb->datafile, &hdr);

  SquishFreeMsg(msg);

  return 1;
}

sq_dword SquishUMSGID2Offset(sq_msg_base_t *mb, UMSGID mid, int nextm) {
  SQIDX sqidx;

  if (mid == 0 || mid >= mb->basehdr.uid) {
    return 0;
  }

  for (sq_dword msgno = 1; msgno <= mb->basehdr.num_msg; msgno++) {
    fseek(mb->indexfile, (msgno - 1) * sizeof(SQIDX), SEEK_SET);
    fread(&sqidx, sizeof(SQIDX), 1, mb->indexfile);
    convert_sqidx(&sqidx);
    if (sqidx.umsgid == mid) {
      return msgno;
    }
    if (nextm && sqidx.umsgid > mid) {
      return msgno;
    }
  }
  return 0;
}

sq_msg_t *SquishReadMsg(sq_msg_base_t *mb, sq_dword msgno) {
  SQIDX sqidx;
  SQHDR sqhdr;

  char *data;

  if (msgno > mb->basehdr.num_msg || msgno < 1) {
    return NULL;
  }

  fseek(mb->indexfile, (msgno - 1) * sizeof(SQIDX), SEEK_SET);

  fread(&sqidx, sizeof(SQIDX), 1, mb->indexfile);
  convert_sqidx(&sqidx);
  if (sqidx.ofs == 0 || sqidx.umsgid == 0xffffffff) {
    return NULL;
  }

  fseek(mb->datafile, sqidx.ofs, SEEK_SET);
  read_sq_hdr(mb->datafile, &sqhdr);

  if (sqhdr.id != 0xAFAE4453 || sqhdr.frame_type != 0) {
    return NULL;
  }
  data = (char *)malloc(sqhdr.frame_length);
  if (!data) {
    return NULL;
  }
  fread(data, sqhdr.frame_length, 1, mb->datafile);

  sq_msg_t *msg = (sq_msg_t *)malloc(sizeof(sq_msg_t));
  if (!msg) {
    free(data);
    return NULL;
  }
  msg->ofs = sqidx.ofs;

  memcpy(&msg->xmsg, data, sizeof(XMSG));
  convert_xmsg(&msg->xmsg);

  msg->ctrl_len = sqhdr.clen;
  msg->msg_len = sqhdr.msg_length - sizeof(XMSG) - msg->ctrl_len;
  msg->ctrl = (char *)malloc(msg->ctrl_len);
  if (!msg->ctrl) {
    free(msg);
    free(data);
    return NULL;
  }

  memcpy(msg->ctrl, data + sizeof(XMSG), msg->ctrl_len);

  msg->msg = (char *)malloc(msg->msg_len);
  if (!msg->msg) {
    free(msg->ctrl);
    free(msg);
    free(data);
    return NULL;
  }
  memcpy(msg->msg, data + sizeof(XMSG) + msg->ctrl_len, msg->msg_len);

  free(data);

  return msg;
}

NETADDR *parse_fido_addr(const char *str) {
  if (str == NULL) {
    return NULL;
  }
  NETADDR *ret = (NETADDR *)malloc(sizeof(NETADDR));
  if (!ret)
    return NULL;
  size_t c;
  int state = 0;

  ret->zone = 0;
  ret->net = 0;
  ret->node = 0;
  ret->point = 0;

  for (c = 0; c < strlen(str); c++) {
    switch (str[c]) {
    case ':':
      state = 1;
      break;
    case '/':
      state = 2;
      break;
    case '.':
      state = 3;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      switch (state) {
      case 0:
        ret->zone = ret->zone * 10 + (str[c] - '0');
        break;
      case 1:
        ret->net = ret->net * 10 + (str[c] - '0');
        break;
      case 2:
        ret->node = ret->node * 10 + (str[c] - '0');
        break;
      case 3:
        ret->point = ret->point * 10 + (str[c] - '0');
        break;
      }
    } break;
    default:
      free(ret);
      return NULL;
    }
  }
  return ret;
}

int SquishPackMsgBase(const char *str) {
  sq_msg_base_t *mb = SquishOpenMsgBase(str);
  sq_msg_t *msg;
  FILE *psqi;
  FILE *psqd;
  SQBASE bhdr;
  SQHDR sqhdr;
  FOFS frame;
  int ret = 0;

  char psqi_str[MAX_PATH];
  char psqd_str[MAX_PATH];

  char sqi_str[MAX_PATH];
  char sqd_str[MAX_PATH];

  if (!mb) {
    return -1;
  }

  snprintf(sqi_str, MAX_PATH, "%s.sqi", str);
  snprintf(sqd_str, MAX_PATH, "%s.sqd", str);
  snprintf(psqi_str, MAX_PATH, "%s.sqi.pack", str);
  psqi = fopen(psqi_str, "w+b");
  if (psqi == NULL) {
    SquishCloseMsgBase(mb);
    return -1;
  }

  snprintf(psqd_str, MAX_PATH, "%s.sqd.pack", str);
  psqd = fopen(psqd_str, "w+b");
  if (psqd == NULL) {
    fclose(psqi);
    unlink(psqi_str);
    SquishCloseMsgBase(mb);
    return -1;
  }

  SquishLockMsgBase(mb);

  memcpy(&bhdr, &mb->basehdr, sizeof(SQBASE));
  bhdr.begin_frame = 0;
  bhdr.last_frame = 0;
  bhdr.free_frame = 0;
  bhdr.last_free_frame = 0;
  bhdr.end_frame = sizeof(SQBASE);
  bhdr.sz_sqhdr = sizeof(SQHDR);

  write_sq_base(psqd, &bhdr);

  for (size_t i = 1; i <= mb->basehdr.num_msg; i++) {
    msg = SquishReadMsg(mb, i);

    if (msg != NULL) {
      frame = bhdr.end_frame;

      SQHDR temphdr;
      if (bhdr.last_frame != 0) {
        fseek(psqd, bhdr.last_frame, SEEK_SET);
        read_sq_hdr(psqd, &temphdr);
        temphdr.next_frame = frame;
        fseek(psqd, bhdr.last_frame, SEEK_SET);
        write_sq_hdr(psqd, &temphdr);
      }
      sqhdr.clen = msg->ctrl_len;
      sqhdr.msg_length = msg->msg_len + msg->ctrl_len + sizeof(XMSG);
      sqhdr.next_frame = 0;
      sqhdr.prev_frame = bhdr.last_frame;
      sqhdr.frame_length = sizeof(XMSG) + msg->ctrl_len + msg->msg_len;
      sqhdr.frame_type = 0;
      sqhdr.id = 0xAFAE4453;

      fseek(psqd, frame, SEEK_SET);
      write_sq_hdr(psqd, &sqhdr);
      write_xmsg(psqd, &msg->xmsg);
      fwrite(msg->ctrl, msg->ctrl_len, 1, psqd);
      fwrite(msg->msg, msg->msg_len, 1, psqd);

      if (bhdr.begin_frame == 0) {
        bhdr.begin_frame = frame;
      }
      bhdr.last_frame = frame;
      bhdr.end_frame = frame + sqhdr.frame_length + sizeof(SQHDR);

      if (bhdr.uid <= msg->xmsg.umsgid) {
        bhdr.uid = msg->xmsg.umsgid + 1;
      }

      SQIDX idx;

      idx.hash = SquishHash((unsigned char *)msg->xmsg.to);
      idx.ofs = frame;
      idx.umsgid = msg->xmsg.umsgid;

      fseek(psqi, 0, SEEK_END);

      convert_sqidx(&idx);

      fwrite(&idx, sizeof(SQIDX), 1, psqi);

      SquishFreeMsg(msg);
      ret++;
    }
  }

  fseek(psqd, 0, SEEK_SET);
  fwrite(&bhdr, sizeof(SQBASE), 1, psqd);
  write_sq_base(psqd, &bhdr);

  SquishUnlockMsgBase(mb);
  SquishCloseMsgBase(mb);
  fclose(psqd);
  fclose(psqi);

  unlink(sqi_str);
  unlink(sqd_str);

  rename(psqi_str, sqi_str);
  rename(psqd_str, sqd_str);

  return ret;
}
