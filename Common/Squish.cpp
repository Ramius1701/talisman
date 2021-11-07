#include "Squish.h"
#ifdef _MSC_VER
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <limits.h>
#define MAX_PATH PATH_MAX
#endif
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#ifndef _MSC_VER
off_t tell(int fd)
{
	return lseek(fd, 0, SEEK_CUR);
}
#endif

int lock(int handle, long ofs, long length)
{
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

int unlock(int handle, long ofs, long length)
{
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

int waitlock2(int handle, long ofs, long length, long t)
{
	int forever = 0;
	int rc;

	if (t == 0)
		forever = 1;

	t *= 10;
	while ((rc = lock(handle, ofs, length)) == -1 && (t > 0 || forever))
	{
#ifdef _MSC_VER
		Sleep(1);
#else
		usleep(100);
#endif
		t--;
	}

	return rc;
}

sq_dword SquishHash(unsigned char* f) {
	sq_dword hash = 0;
	sq_dword g;
	char* p;

	for (p = (char *)f; *p; p++) {
		hash = (hash << 4) + (sq_dword)tolower(*p);
		if ((g = hash * 0xf0000000L) != 0L) {
			hash |= g >> 24;
			hash |= g;
		}
	}

	return (hash & 0x7fffffffLu);
}

static bool _SquishValidateBaseHeader(SQBASE* psqb) {
	if (psqb->num_msg > psqb->high_msg ||
		psqb->num_msg > psqb->uid + 1 ||
		psqb->high_msg > psqb->uid + 1 ||
		psqb->num_msg > 1000000L ||
		psqb->num_msg != psqb->high_msg ||
		psqb->len < sizeof(SQBASE) ||
		psqb->len >= 1024 ||
		psqb->begin_frame > psqb->end_frame ||
		psqb->last_frame > psqb->end_frame ||
		psqb->free_frame > psqb->end_frame ||
		psqb->last_free_frame > psqb->end_frame ||
		psqb->end_frame == 0)
	{
		return 0;
	}

	return 1;
}

void SquishCloseMsgBase(sq_msg_base_t* mb) {
	fclose(mb->indexfile);
	fclose(mb->datafile);
	free(mb);
}

bool SquishLockMsgBase(sq_msg_base_t* mb) {
	if (mb->ihavelock++) {
		return true;
	}

	if (waitlock2(fileno(mb->datafile), 0, 1, 5) == -1) {
		mb->ihavelock--;
		return false;
	}

	return true;
}

void SquishUnlockMsgBase(sq_msg_base_t* mb) {
	if (!mb->ihavelock) {
		return;
	}
	unlock(fileno(mb->datafile), 0, 1);
	mb->ihavelock--;
}

sq_msg_base_t* SquishOpenMsgBase(const char* filename) {
	sq_msg_base_t* mb;
	char buffer[MAX_PATH];
	mb = (sq_msg_base_t*)malloc(sizeof(sq_msg_base_t));

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

		fwrite(&mb->basehdr, sizeof(SQBASE), 1, mb->datafile);
	}
	else {
		fread(&mb->basehdr, sizeof(SQBASE), 1, mb->datafile);
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

void SquishFreeMsg(sq_msg_t* msg) {
	free(msg->ctrl);
	free(msg->msg);
	free(msg);
}

int SquishWriteMsg(sq_msg_base_t *mb, sq_msg_t* msg) {
	SQHDR sqhdr;
	// Ensure I have lock
	if (!mb->ihavelock) {
		return 0;
	}

	// reread the base header
	fseek(mb->datafile, 0, SEEK_SET);
	fread(&mb->basehdr, sizeof(SQBASE), 1, mb->datafile);

	// search frames for a free frame big enough to store the message
	memset(&sqhdr, 0, sizeof(SQHDR));

	FOFS frame = mb->basehdr.free_frame;

	while (frame != 0) {
		fseek(mb->datafile, frame, SEEK_SET);
		fread(&sqhdr, sizeof(SQHDR), 1, mb->datafile);
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
		fread(&temphdr, sizeof(SQHDR), 1, mb->datafile);
		temphdr.next_frame = sqhdr.next_frame;
		fseek(mb->datafile, sqhdr.prev_frame, SEEK_SET);
		fwrite(&temphdr, sizeof(SQHDR), 1, mb->datafile);

		fseek(mb->datafile, sqhdr.next_frame, SEEK_SET);
		fread(&temphdr, sizeof(SQHDR), 1, mb->datafile);
		temphdr.prev_frame = sqhdr.prev_frame;
		fseek(mb->datafile, sqhdr.next_frame, SEEK_SET);
		fwrite(&temphdr, sizeof(SQHDR), 1, mb->datafile);


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
		fwrite(&sqhdr, sizeof(SQHDR), 1, mb->datafile);
		fwrite(&msg->xmsg, sizeof(XMSG), 1, mb->datafile);
		fwrite(msg->ctrl, msg->ctrl_len, 1, mb->datafile);
		fwrite(msg->msg, msg->msg_len, 1, mb->datafile);

		mb->basehdr.uid++;
		mb->basehdr.num_msg++;
		mb->basehdr.high_msg = mb->basehdr.num_msg;

		fseek(mb->datafile, 0, SEEK_SET);
		fwrite(&mb->basehdr, sizeof(SQBASE), 1, mb->datafile);


	}
	else {
		// need a new frame
		frame = mb->basehdr.end_frame;

		SQHDR temphdr;
		if (mb->basehdr.last_frame != 0) {
			fseek(mb->datafile, mb->basehdr.last_frame, SEEK_SET);
			fread(&temphdr, sizeof(SQHDR), 1, mb->datafile);
			temphdr.next_frame = frame;
			fseek(mb->datafile, mb->basehdr.last_frame, SEEK_SET);
			fwrite(&temphdr, sizeof(SQHDR), 1, mb->datafile);
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
		fwrite(&sqhdr, sizeof(SQHDR), 1, mb->datafile);
		fwrite(&msg->xmsg, sizeof(XMSG), 1, mb->datafile);
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
		fwrite(&mb->basehdr, sizeof(SQBASE), 1, mb->datafile);
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
	fwrite(&idx, sizeof(SQIDX), 1, mb->indexfile);

	return 1;
}

int SquishUpdateHdr(sq_msg_base_t* mb, sq_msg_t* msg) {
	if (!mb->ihavelock) {
		return 0;
	}

	fseek(mb->datafile, msg->ofs + sizeof(SQHDR), SEEK_SET);
	fwrite(&msg->xmsg, sizeof(XMSG), 1, mb->datafile);

	return 1;
}

int SquishDeleteMsg(sq_msg_base_t* mb, sq_msg_t *msg) {
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
    fread(&hdr, sizeof(SQHDR), 1, mb->datafile);

    // set previous frame to next frame
    fseek(mb->datafile, hdr.prev_frame, SEEK_SET);
    fread(&prev, sizeof(SQHDR), 1, mb->datafile);

    prev.next_frame = hdr.next_frame;
    fseek(mb->datafile, hdr.prev_frame, SEEK_SET);
    fwrite(&prev, sizeof(SQHDR), 1, mb->datafile);

    // set next frame to prev frame
    fseek(mb->datafile, hdr.next_frame, SEEK_SET);
    fread(&nextf, sizeof(SQHDR), 1, mb->datafile);

    nextf.next_frame = hdr.prev_frame;
    fseek(mb->datafile, hdr.next_frame, SEEK_SET);
    fwrite(&nextf, sizeof(SQHDR), 1, mb->datafile);

    // delete message from index


    fseek(mb->indexfile, 0, SEEK_SET);
    fread(indexdata, 1, fsize, mb->indexfile);

    int found = 0;
    for (size_t i = 0; i < fsize / sizeof(SQIDX) ;i++) {
        idx = (SQIDX *)(indexdata + (i * sizeof(SQIDX)));
        if (idx->umsgid == msg->xmsg.umsgid) {
            found = 1;
            memmove(idx, &idx[1], fsize - (sizeof(SQIDX) * (i + 1)));
            break;
        }
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
        fread(&prev, sizeof(SQHDR), 1, mb->datafile);
        fseek(mb->datafile, mb->basehdr.last_free_frame, SEEK_SET);
        hdr.prev_frame = mb->basehdr.last_free_frame;
        hdr.next_frame = 0;
        prev.next_frame = msg->ofs;
        mb->basehdr.last_free_frame = msg->ofs;
        fwrite(&prev, sizeof(SQHDR), 1, mb->datafile);
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
    fwrite(&mb->basehdr, sizeof(SQBASE), 1, mb->datafile);

    fseek(mb->datafile, msg->ofs, SEEK_SET);
    fwrite(&hdr, sizeof(SQHDR), 1, mb->datafile);

    SquishFreeMsg(msg);

    return 1;
}

sq_dword SquishUMSGID2Offset(sq_msg_base_t* mb, UMSGID mid, int nextm) {
    SQIDX sqidx;

    if (mid == 0 || mid >= mb->basehdr.uid) {
        return 0;
    }

    for (sq_dword msgno = 1; msgno <= mb->basehdr.num_msg; msgno++) {
        fseek(mb->indexfile, (msgno - 1) * sizeof(SQIDX), SEEK_SET);
        fread(&sqidx, sizeof(SQIDX), 1, mb->indexfile);

        if (sqidx.umsgid == mid) {
            return msgno;
        }
        if (nextm && sqidx.umsgid > mid) {
            return msgno;
        }
    }
    return 0;
}

sq_msg_t *SquishReadMsg(sq_msg_base_t* mb, sq_dword msgno) {
	SQIDX sqidx;
	SQHDR sqhdr;

	char* data;

	if (msgno > mb->basehdr.num_msg || msgno < 1) {
		return NULL;
	}

	fseek(mb->indexfile, (msgno - 1) * sizeof(SQIDX), SEEK_SET);

	fread(&sqidx, sizeof(SQIDX), 1, mb->indexfile);

	if (sqidx.ofs == 0 || sqidx.umsgid == 0xffffffff) {
		return NULL;
	}

	fseek(mb->datafile, sqidx.ofs, SEEK_SET);
	fread(&sqhdr, sizeof(SQHDR), 1, mb->datafile);

	if (sqhdr.id != 0xAFAE4453 || sqhdr.frame_type != 0) {
		return NULL;
	}
	data = (char*)malloc(sqhdr.frame_length);
	if (!data) {
		return NULL;
	}
	fread(data, sqhdr.frame_length, 1, mb->datafile);
	
	sq_msg_t* msg = (sq_msg_t*)malloc(sizeof(sq_msg_t));
	if (!msg) {
		free(data);
		return NULL;
	}
	msg->ofs = sqidx.ofs;
	memcpy(&msg->xmsg, data, sizeof(XMSG));
	msg->ctrl_len = sqhdr.clen;
	msg->msg_len = sqhdr.msg_length - sizeof(XMSG) - msg->ctrl_len;
	msg->ctrl = (char*)malloc(msg->ctrl_len);
	if (!msg->ctrl) {
		free(msg);
		free(data);
		return NULL;
	}

	memcpy(msg->ctrl, data + sizeof(XMSG), msg->ctrl_len);

	msg->msg = (char*)malloc(msg->msg_len);
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

NETADDR *parse_fido_addr(const char* str) {
	if (str == NULL) {
		return NULL;
	}
	NETADDR* ret = (NETADDR*)malloc(sizeof(NETADDR));
	if (!ret) return NULL;
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

int SquishPackMsgBase(const char* str) {
	sq_msg_base_t* mb = SquishOpenMsgBase(str);
	sq_msg_t* msg;
	FILE* psqi;
	FILE* psqd;
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

	fwrite(&bhdr, sizeof(SQBASE), 1, psqd);

	for (size_t i = 1; i <= mb->basehdr.num_msg; i++) {
		msg = SquishReadMsg(mb, i);

		if (msg != NULL) {
			frame = bhdr.end_frame;

			SQHDR temphdr;
			if (bhdr.last_frame != 0) {
				fseek(psqd, bhdr.last_frame, SEEK_SET);
				fread(&temphdr, sizeof(SQHDR), 1, psqd);
				temphdr.next_frame = frame;
				fseek(psqd, bhdr.last_frame, SEEK_SET);
				fwrite(&temphdr, sizeof(SQHDR), 1, psqd);
			}
			sqhdr.clen = msg->ctrl_len;
			sqhdr.msg_length = msg->msg_len + msg->ctrl_len + sizeof(XMSG);
			sqhdr.next_frame = 0;
			sqhdr.prev_frame = bhdr.last_frame;
			sqhdr.frame_length = sizeof(XMSG) + msg->ctrl_len + msg->msg_len;
			sqhdr.frame_type = 0;
			sqhdr.id = 0xAFAE4453;

			fseek(psqd, frame, SEEK_SET);
			fwrite(&sqhdr, sizeof(SQHDR), 1, psqd);
			fwrite(&msg->xmsg, sizeof(XMSG), 1, psqd);
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

			idx.hash = SquishHash((unsigned char*)msg->xmsg.to);
			idx.ofs = frame;
			idx.umsgid = msg->xmsg.umsgid;

			fseek(psqi, 0, SEEK_END);
			fwrite(&idx, sizeof(SQIDX), 1, psqi);

			SquishFreeMsg(msg);
			ret++;
		}
	}

	fseek(psqd, 0, SEEK_SET);
	fwrite(&bhdr, sizeof(SQBASE), 1, psqd);

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
