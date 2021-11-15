#ifdef _MSC_VER
#include <Windows.h>
#endif
#include "../Common/INIReader.h"
#include "../Common/Squish.h"
#include "../Common/toml.hpp"
#include "Archiver.h"
#include "Qwk.h"
#include "qwkie.h"
#include <curl/curl.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

static int safe_atoi(const char *str, int len) {
  int ret = 0;

  for (int i = 0; i < len; i++) {
    if (str[i] < '0' || str[i] > '9') {
      break;
    }
    ret = ret * 10 + (str[i] - '0');
  }
  return ret;
}

bool Qwkie::scan(int net) {
  std::cout << "Scanning network: " << networks.at(net).name << " ..." << std::endl;
  std::filesystem::path packpath(temppath + "/qwknet");
  struct QwkHeader qhdr;

  packpath.append(networks.at(net).qwkid + "_pack");

  if (std::filesystem::exists(packpath)) {
    std::filesystem::remove_all(packpath);
  }

  std::filesystem::create_directories(packpath);

  if (std::filesystem::exists(temppath + "/qwknet/" + networks.at(net).qwkid + ".REP")) {
    std::cerr << "Outbound REP packet exists... bailing." << std::endl;
    std::filesystem::remove_all(packpath);
    return false;
  }
  FILE *fptr = NULL;

  for (size_t a = 0; a < networks.at(net).areas.size(); a++) {
    sq_msg_base_t *mb = SquishOpenMsgBase(std::string(msgpath + "/" + networks.at(net).areas.at(a).msgarea).c_str());

    if (!mb) {
      continue;
    }

    for (size_t msgno = 1; msgno <= mb->basehdr.num_msg; msgno++) {
      sq_msg_t *msg = SquishReadMsg(mb, msgno);

      if (msg == NULL) {
        continue;
      }
      if (msg->xmsg.attr & MSGLOCAL && !(msg->xmsg.attr & MSGSENT)) {
        // pack message
        if (fptr == NULL) {
          fptr = fopen(std::string(packpath.u8string() + "/" + networks.at(net).qwkid + ".MSG").c_str(), "wb");
          if (!fptr) {
            SquishFreeMsg(msg);
            SquishCloseMsgBase(mb);
            return false;
          }
          memset((char *)&qhdr, ' ', 128);
          memcpy((char *)&qhdr, networks.at(net).qwkid.c_str(), networks.at(net).qwkid.size());
          fwrite(&qhdr, 128, 1, fptr);
        }

        std::string subject(msg->xmsg.subject);
        std::string sender(msg->xmsg.from);
        std::string recipient(msg->xmsg.to);
        int hour = (msg->xmsg.date_written.time >> 11) & 31;
        int minute = (msg->xmsg.date_written.time >> 5) & 63;

        int day = msg->xmsg.date_written.date & 31;
        int month = (msg->xmsg.date_written.date >> 5) & 15;
        int year = ((msg->xmsg.date_written.date >> 9) & 127) + 1980;

        char buffer[256];
        std::stringstream msgss;
        std::stringstream replyid;
        std::stringstream msgid;
        std::stringstream tzutc;

        for (int i = 0; i < msg->ctrl_len - 8; i++) {
          if (strncmp(&msg->ctrl[i], "\x01MSGID: ", 8) == 0) {
            for (int j = i + 8; j < msg->ctrl_len && msg->ctrl[j] != '\x01'; j++) {
              msgid << msg->ctrl[j];
            }
            break;
          }
        }
        for (int i = 0; i < msg->ctrl_len - 8; i++) {
          if (strncmp(&msg->ctrl[i], "\x01REPLY: ", 8) == 0) {
            for (int j = i + 8; j < msg->ctrl_len && msg->ctrl[j] != '\x01'; j++) {
              replyid << msg->ctrl[j];
            }
            break;
          }
        }
        for (int i = 0; i < msg->ctrl_len - 8; i++) {
          if (strncmp(&msg->ctrl[i], "\x01TZUTC: ", 8) == 0) {
            for (int j = i + 8; j < msg->ctrl_len && msg->ctrl[j] != '\x01'; j++) {
              tzutc << msg->ctrl[j];
            }
            break;
          }
        }

        for (size_t i = 0; i < (size_t)msg->msg_len; i++) {
          if (msg->msg[i] == '\r') {
            if (i < (size_t)msg->msg_len - 1) {
              if (msg->msg[i] == '\001') {
                i++;
                while (i < (size_t)msg->msg_len && msg->msg[i] != '\r') {
                  i++;
                }
                continue;
              }
            } else if (i < (size_t)msg->msg_len - 9) {
              if (msg->msg[i] == 'S' && msg->msg[i + 1] == 'E' && msg->msg[i + 2] == 'E' && msg->msg[i + 3] == 'N' && msg->msg[i + 4] == '-' &&
                  msg->msg[i + 5] == 'B' && msg->msg[i + 6] == 'Y' && msg->msg[i + 7] == ':' && msg->msg[i + 8] == ' ') {
                while (i < (size_t)msg->msg_len && msg->msg[i] != '\r') {
                  i++;
                }
                continue;
              }
            }
          }
          msgss << msg->msg[i];
        }
        std::string msgbody(msgss.str());
        std::stringstream extra;

        qhdr.Msgstat = ' ';

        memset(qhdr.Msgnum, ' ', 7);
        snprintf(buffer, sizeof buffer, "%d", networks.at(net).areas.at(a).qwkbaseno);
        memcpy(qhdr.Msgnum, buffer, strlen(buffer));

        snprintf(buffer, sizeof buffer, "%02d-%02d-%02d", month, day, year - 2000);
        memcpy(qhdr.Msgdate, buffer, 8);

        snprintf(buffer, sizeof buffer, "%02d:%02d", hour, minute);
        memcpy(qhdr.Msgtime, buffer, 5);

        memset(qhdr.Msgpass, ' ', 12);
        memset(qhdr.Msgrply, ' ', 8);

        memset(qhdr.MsgSubj, ' ', 25);

        memset(qhdr.MsgTo, ' ', 25);

        std::stringstream mbody2;
        mbody2.str("");
        for (size_t i = 0; i < msgbody.length(); i++) {
          if (msgbody.at(i) != '\n') {
            mbody2 << msgbody.at(i);
          }
        }

        mbody2 << " \r---\r \xfe Talisman \xfe " << networks.at(net).tagline << "\r";

        msgbody = mbody2.str();

        extra.str("");

        if (recipient.length() > 25) {
          extra << "To: " << recipient << "\r";
          memcpy(qhdr.MsgTo, recipient.c_str(), 25);
        } else {
          memcpy(qhdr.MsgTo, recipient.c_str(), recipient.length());
        }

        memset(qhdr.MsgFrom, ' ', 25);
        if (sender.length() > 25) {
          extra << "From: " << sender << "\r";
          memcpy(qhdr.MsgFrom, sender.c_str(), 25);
        } else {
          memcpy(qhdr.MsgFrom, sender.c_str(), sender.length());
        }

        if (subject.length() > 25) {
          extra << "Subject: " << subject << "\r";
          memcpy(qhdr.MsgSubj, subject.c_str(), 25);
        } else {
          memcpy(qhdr.MsgSubj, subject.c_str(), subject.length());
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
          SquishFreeMsg(msg);
          SquishCloseMsgBase(mb);
          return false;
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
        memset(qhdr.Msgrecs, ' ', 6);
        memcpy(qhdr.Msgrecs, buffer, strlen(buffer));

        qhdr.Msglive = 0xE1;
        qhdr.Msgarealo = networks.at(net).areas.at(a).qwkbaseno & 0xff;
        qhdr.Msgareahi = (networks.at(net).areas.at(a).qwkbaseno >> 8) & 0xff;

        uint32_t ndx = ftell(fptr);

        qhdr.Msgoffhi = (ndx >> 8) & 0xff;
        qhdr.Msgofflo = ndx & 0xff;

        qhdr.Msgtagp = ' ';


        uint32_t offset = ftell(fptr);
        FILE *hdrdat = fopen(std::string(packpath.u8string() + "/HEADERS.DAT").c_str(), "a");
        fprintf(hdrdat, "[%lx]\n", offset);
        fprintf(hdrdat, "To: %s\n", recipient.c_str());
        fprintf(hdrdat, "Sender: %s\n", sender.c_str());
        fprintf(hdrdat, "Subject: %s\n", subject.c_str());

        if (msgid.str().size() > 0) {
          fprintf(hdrdat, "Message-ID: %s\n", msgid.str().c_str());
        }
        if (replyid.str().size() > 0) {
          fprintf(hdrdat, "In-Reply-To: %s\n", replyid.str().c_str());
        }
        fprintf(hdrdat, "\n");

        fclose(hdrdat);

        fwrite(&qhdr, sizeof(struct QwkHeader), 1, fptr);
        fwrite(msgbuf, lenbytes, 1, fptr);

        free(msgbuf);

        // mark as sent
        msg->xmsg.attr |= MSGSENT;
        SquishLockMsgBase(mb);
        SquishUpdateHdr(mb, msg);
        SquishUnlockMsgBase(mb);
      }
      SquishFreeMsg(msg);
    }
    SquishCloseMsgBase(mb);
  }
  if (fptr != NULL) {
    // pack QWKID.MSG
    fclose(fptr);

    std::vector<std::string> filelist;

    filelist.push_back(std::string(packpath.u8string() + "/" + networks.at(net).qwkid + ".MSG"));
    filelist.push_back(std::string(packpath.u8string() + "/HEADERS.DAT"));

    for (size_t arc = 0; arc < archivers.size(); arc++) {
      if (strcasecmp(archivers.at(arc)->name.c_str(), networks.at(net).archiver.c_str()) == 0) {
        archivers.at(arc)->compress(temppath + "/qwknet/" + networks.at(net).qwkid + ".REP", filelist);
        std::filesystem::remove_all(packpath);
        break;
      }
    }
  } else {
    std::filesystem::remove_all(packpath);
  }
  return true;
}

bool Qwkie::scan(std::string network) {
  for (size_t net = 0; net < networks.size(); net++) {
    if (strcasecmp(networks.at(net).name.c_str(), network.c_str()) == 0) {
      std::cout << "Scanning " << network << "..." << std::endl;
      return scan(net);
    }
  }
  return false;
}

bool Qwkie::scanall() {
  std::cout << "Scanning all nets...." << std::endl;

  for (size_t net = 0; net < networks.size(); net++) {
    scan(net);
  }

  return true;
}

static size_t read_callback(char *ptr, size_t size, size_t nmemb, void *stream) {
  size_t retcode = fread(ptr, size, nmemb, (FILE *)stream);

  return retcode;
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *stream) {
  size_t retcode = fwrite(ptr, size, nmemb, (FILE *)stream);

  return retcode;
}

bool Qwkie::poll(int net) {
  CURL *curl;
  CURLcode res;

  if (std::filesystem::exists(temppath + "/qwknet/" + networks.at(net).qwkid + ".QWK")) {
    std::cerr << "QWK file for " << networks.at(net).qwkid << " exists, skipping fetch." << std::endl;
  } else {
    FILE *fptr = fopen(std::string(temppath + "/qwknet/" + networks.at(net).qwkid + ".QWK").c_str(), "wb");

    if (!fptr) {

      std::cerr << "Failed to open " << temppath << "/qwknet/" << networks.at(net).qwkid << ".QWK" << std::endl;
      return false;
    }

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    if (networks.at(net).port != 21) {
      curl_easy_setopt(curl, CURLOPT_URL,
                       std::string("ftp://" + networks.at(net).myqwkid + ":" + networks.at(net).password + "@" + networks.at(net).ftpserver + ":" +
                                   std::to_string(networks.at(net).port) + "/" + networks.at(net).qwkid + ".QWK")
                           .c_str());
    } else {
      curl_easy_setopt(curl, CURLOPT_URL,
                       std::string("ftp://" + networks.at(net).myqwkid + ":" + networks.at(net).password + "@" + networks.at(net).ftpserver + "/" +
                                   networks.at(net).qwkid + ".QWK")
                           .c_str());
    }
    // printf("%s\n", std::string("ftp://" + networks.at(net).myqwkid + ":" + networks.at(net).password + "@" + networks.at(net).ftpserver + "/" +
    // networks.at(net).qwkid + ".QWK").c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fptr);
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      fclose(fptr);
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
      std::filesystem::remove(temppath + "/qwknet/" + networks.at(net).qwkid + ".QWK");
    } else {
      fclose(fptr);
    }
    curl_easy_cleanup(curl);
  }

  if (!std::filesystem::exists(temppath + "/qwknet/" + networks.at(net).qwkid + ".REP")) {
    std::cerr << "REP file for " << networks.at(net).qwkid << " does not exist, skipping send." << std::endl;
  } else {
    struct stat s;

    if (stat(std::string(temppath + "/qwknet/" + networks.at(net).qwkid + ".REP").c_str(), &s) != 0) {
      std::cerr << "Failed to stat " << temppath << "/qwknet/" << networks.at(net).qwkid << ".REP" << std::endl;
      return false;
    }
    curl_off_t fsize = (curl_off_t)s.st_size;
    FILE *fptr = fopen(std::string(temppath + "/qwknet/" + networks.at(net).qwkid + ".REP").c_str(), "rb");

    if (!fptr) {

      std::cerr << "Failed to read " << temppath << "/qwknet/" << networks.at(net).qwkid << ".REP" << std::endl;
      return false;
    }

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    if (networks.at(net).port != 21) {
      curl_easy_setopt(curl, CURLOPT_URL,
                       std::string("ftp://" + networks.at(net).myqwkid + ":" + networks.at(net).password + "@" + networks.at(net).ftpserver + ":" +
                                   std::to_string(networks.at(net).port) + "/" + networks.at(net).qwkid + ".REP")
                           .c_str());
    } else {
      curl_easy_setopt(curl, CURLOPT_URL,
                       std::string("ftp://" + networks.at(net).myqwkid + ":" + networks.at(net).password + "@" + networks.at(net).ftpserver + "/" +
                                   networks.at(net).qwkid + ".REP")
                           .c_str());
    }
    // printf("%s\n", std::string("ftp://" + networks.at(net).myqwkid + ":" + networks.at(net).password + "@" + networks.at(net).ftpserver + "/" +
    // networks.at(net).qwkid + ".REP").c_str());
    curl_easy_setopt(curl, CURLOPT_READDATA, fptr);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)fsize);
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      fclose(fptr);
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    } else {
      fclose(fptr);
      std::filesystem::remove(temppath + "/qwknet/" + networks.at(net).qwkid + ".REP");
    }
    curl_easy_cleanup(curl);
  }
  return false;
}

bool Qwkie::poll(std::string network) {

  curl_global_init(CURL_GLOBAL_ALL);
  for (size_t net = 0; net < networks.size(); net++) {
    if (strcasecmp(networks.at(net).name.c_str(), network.c_str()) == 0) {
      std::cout << "Polling " << network << "..." << std::endl;
      return poll(net);
    }
  }
  curl_global_cleanup();
  return false;
}

bool Qwkie::pollall() {
  std::cout << "Polling all nets...." << std::endl;
  curl_global_init(CURL_GLOBAL_ALL);
  for (size_t net = 0; net < networks.size(); net++) {
    poll(net);
  }
  curl_global_cleanup();
  return true;
}

bool Qwkie::toss(std::string network) {
  for (size_t net = 0; net < networks.size(); net++) {
    if (strcasecmp(networks.at(net).name.c_str(), network.c_str()) == 0) {
      std::cout << "Tossing " << network << "...." << std::endl;
      return toss(net);
    }
  }
  return false;
}

bool Qwkie::tossall() {
  std::cout << "Tossing all nets...." << std::endl;
  for (size_t net = 0; net < networks.size(); net++) {
    toss(net);
  }
  return true;
}

bool Qwkie::toss(int net) {
  std::filesystem::path tmppath(temppath + "/qwknet");

  std::filesystem::path path(tmppath);

  path.append(networks.at(net).qwkid + ".qwk");

  std::cout << "Searching for " << path.u8string() << std::endl;

  if (!std::filesystem::exists(path)) {
    path = tmppath;
    path.append(networks.at(net).qwkid + ".QWK");
    if (!std::filesystem::exists(path)) {
      return false;
    }
  }

  // found qwk packet

  // unarchive packet
  std::filesystem::path extractpath(tmppath);
  extractpath.append(networks.at(net).qwkid + "_extract");

  if (std::filesystem::exists(extractpath)) {
    std::filesystem::remove_all(extractpath);
  }
  if (!std::filesystem::create_directories(extractpath)) {
    std::cerr << "Error creating temporary directory." << std::endl;
    return false;
  }

  bool unarced = false;

  for (size_t arc = 0; arc < archivers.size(); arc++) {
    FILE *fptr = fopen(path.u8string().c_str(), "rb");
    if (archivers.at(arc)->offset >= 0) {
      fseek(fptr, archivers.at(arc)->offset, SEEK_SET);
    } else {
      fseek(fptr, archivers.at(arc)->offset, SEEK_END);
    }
    uint8_t byte;
    bool match = true;
    for (int z = 0; z < archivers.at(arc)->bytelen; z++) {
      fread(&byte, 1, 1, fptr);
      if (byte != archivers.at(arc)->bytes[z]) {
        match = false;
        break;
      }
    }
    fclose(fptr);
    if (match == false)
      continue;
    archivers.at(arc)->extract(path.u8string(), extractpath.u8string());
    unarced = true;
    break;
  }
  if (unarced == false) {
    std::cerr << "Can't find archiver for packet." << std::endl;
    return false;
  }

  std::filesystem::remove(path);

  std::filesystem::path msgsdat(extractpath);
  msgsdat.append("MESSAGES.DAT");

  std::filesystem::path hdrdat(extractpath);
  hdrdat.append("HEADERS.DAT");
  INIReader inir(hdrdat.u8string());
  bool doheaders = true;
  if (inir.ParseError()) {
    doheaders = false;
  }

  FILE *fptr = fopen(msgsdat.u8string().c_str(), "rb");

  if (fptr != NULL) {
    // read message records.
    struct QwkHeader qwkrec;

    fread(&qwkrec, sizeof(struct QwkHeader), 1, fptr);
    while (!feof(fptr)) {
      uint32_t offset = ftell(fptr);
      
      if (fread(&qwkrec, sizeof(struct QwkHeader), 1, fptr) != 1) {
        break;
      }
      size_t msgrecs = safe_atoi((const char *)qwkrec.Msgrecs, 6);

      char *msgcontent = (char *)malloc(((msgrecs - 1) * 128) + 1);

      if (!msgcontent) {
        std::cerr << "Out of memory!" << std::endl;
        std::filesystem::remove_all(extractpath);
        return false;
      }

      memset(msgcontent, 0, ((msgrecs - 1) * 128) + 1);

      if (fread(msgcontent, sizeof(struct QwkHeader), msgrecs - 1, fptr) != msgrecs - 1) {
        free(msgcontent);
        std::cerr << "Short read on message." << std::endl;
        std::filesystem::remove_all(extractpath);
        return false;
      }
      for (size_t s = 0; s < strlen(msgcontent); s++) {
        if (msgcontent[s] == '\xe3') {
          msgcontent[s] = '\r';
        }
      }
      int msgbase = (qwkrec.Msgareahi << 8) | qwkrec.Msgarealo;

      bool found = false;

      size_t a;

      if ((char)qwkrec.Msgstat != 'V') {

        for (a = 0; a < networks.at(net).areas.size(); a++) {
          if (networks.at(net).areas.at(a).qwkbaseno == msgbase) {
            found = true;
            break;
          }
        }
        if (found == true) {
          std::string subject;
          std::string to;
          std::string from;

          std::stringstream ss;
          std::stringstream msgbody;

          msgbody.str("");

          bool gotkludge = false;
          size_t x = 0;
          std::vector<std::string> text;
          while (true) {
            ss.str("");
            for (; x < strlen(msgcontent); x++) {
              if (msgcontent[x] == '\r') {
                x++;
                break;
              }
              ss << msgcontent[x];
            }
            if (strcasecmp(ss.str().substr(0, 8).c_str(), "subject:") == 0) {
              gotkludge = true;
              size_t y;
              for (y = 8; y < ss.str().length(); y++) {
                if (ss.str().at(y) != ' ')
                  break;
              }
              subject = ss.str().substr(y);
            } else if (strcasecmp(ss.str().substr(0, 3).c_str(), "to:") == 0) {
              gotkludge = true;
              size_t y;
              for (y = 3; y < ss.str().length(); y++) {
                if (ss.str().at(y) != ' ')
                  break;
              }
              to = ss.str().substr(y);
            } else if (strcasecmp(ss.str().substr(0, 5).c_str(), "from:") == 0) {
              gotkludge = true;
              size_t y;
              for (y = 5; y < ss.str().length(); y++) {
                if (ss.str().at(y) != ' ')
                  break;
              }
              from = ss.str().substr(y);
            } else {
              if (gotkludge) {
                msgbody << &msgcontent[x];
              } else {
                msgbody << msgcontent;
              }
              break;
            }
          }

          free(msgcontent);

          ss.str("");
          for (size_t x = 0; x < msgbody.str().size(); x++) {
            if (msgbody.str().at(x) == '\r') {
              text.push_back(ss.str());
              ss.str("");
              continue;
            }
            ss << msgbody.str().at(x);
          }
          if (ss.str().size() > 0) {
            text.push_back(ss.str());
          }
          if (subject.length() == 0) {
            subject.append((const char *)qwkrec.MsgSubj, 25);
            for (int j = subject.length() - 1; j >= 0; j--) {
              if (subject.at(j) == ' ') {
                subject.pop_back();
              } else {
                break;
              }
            }
          }
          if (to.length() == 0) {
            to.append((const char *)qwkrec.MsgTo, 25);
            for (int j = to.length() - 1; j >= 0; j--) {
              if (to.at(j) == ' ') {
                to.pop_back();
              } else {
                break;
              }
            }
          }
          if (from.length() == 0) {
            from.append((const char *)qwkrec.MsgFrom, 25);
            for (int j = from.length() - 1; j >= 0; j--) {
              if (from.at(j) == ' ') {
                from.pop_back();
              } else {
                break;
              }
            }
          }
          struct tm thedate;
          memset(&thedate, 0, sizeof(struct tm));
          thedate.tm_mday = (qwkrec.Msgdate[3] - '0') * 10 + (qwkrec.Msgdate[4] - '0');
          thedate.tm_mon = ((qwkrec.Msgdate[0] - '0') * 10 + (qwkrec.Msgdate[1] - '0')) - 1;
          int year = (qwkrec.Msgdate[6] - '0') * 10 + (qwkrec.Msgdate[7] - '0');
          if (year < 80) {
            year += 100;
          }
          thedate.tm_year = year;
          thedate.tm_hour = (qwkrec.Msgtime[0] - '0') * 10 + (qwkrec.Msgtime[1] - '0');
          thedate.tm_min = (qwkrec.Msgtime[3] - '0') * 10 + (qwkrec.Msgtime[4] - '0');

          // post message

          sq_msg_t sqmsg;

          memset(&sqmsg, 0, sizeof(sq_msg_t));

          if (doheaders) {

            char obuf[67];

            snprintf(obuf, 67, "%x", offset);

            std::string msgid = inir.Get(obuf, "Message-ID", "");
            std::string replyid = inir.Get(obuf, "In-Reply-To", "");
            std::string qwkorig = inir.Get(obuf, "SenderNetAddr", "");
            int size = 0;

            if (msgid != "") {
              size = msgid.size() + 8;
            }
            if (replyid != "") {
              size += replyid.size() + 8;
            }
            if (qwkorig != "") {
              size += qwkorig.size() + 10;
            }

            sqmsg.ctrl_len = size;
            sqmsg.ctrl = (char *)malloc(size);
            char *ptr = sqmsg.ctrl;
            if (msgid != "") {
              memcpy(ptr, "\001MSGID: ", 8);
              ptr += 8;
              memcpy(ptr, msgid.c_str(), msgid.size());
              ptr += msgid.size();
            }
            if (replyid != "") {
              memcpy(ptr, "\001REPLY: ", 8);
              ptr += 8;
              memcpy(ptr, replyid.c_str(), replyid.size());
              ptr += replyid.size();
            }
            if (qwkorig != "") {
              memcpy(ptr, "\001QWKORIG: ", 10);
              ptr += 10;
              memcpy(ptr, qwkorig.c_str(), qwkorig.size());
              ptr += qwkorig.size();
            }
          }

          sqmsg.msg_len = msgbody.str().size();

          sqmsg.msg = (char *)malloc(sqmsg.msg_len);

          if (!sqmsg.msg) {
            std::cerr << "Out of memory!" << std::endl;
            return false;
          }
          memcpy(sqmsg.msg, msgbody.str().c_str(), msgbody.str().size());

          sqmsg.xmsg.date_written.date |= (((sq_word)thedate.tm_mday) & 31);
          sqmsg.xmsg.date_written.date |= (((sq_word)(thedate.tm_mon + 1)) & 15) << 5;
          sqmsg.xmsg.date_written.date |= (((sq_word)(thedate.tm_year - 80)) & 127) << 9;

          sqmsg.xmsg.date_written.time |= (((sq_word)thedate.tm_sec) & 31);
          sqmsg.xmsg.date_written.time |= (((sq_word)thedate.tm_min) & 63) << 5;
          sqmsg.xmsg.date_written.time |= (((sq_word)thedate.tm_hour) & 31) << 11;

          char datestr[22];
          strftime(datestr, 22, "%d %b %y  %H:%M:%S", &thedate);

          std::tm at;

          time_t now = time(NULL);
#ifdef _MSC_VER
          localtime_s(&at, &now);
#else
          localtime_r(&now, &at);
#endif
          sqmsg.xmsg.date_arrived.date |= (((sq_word)at.tm_mday) & 31);
          sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_mon + 1)) & 15) << 5;
          sqmsg.xmsg.date_arrived.date |= (((sq_word)(at.tm_year - 80)) & 127) << 9;

          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_sec) & 31);
          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_min) & 63) << 5;
          sqmsg.xmsg.date_arrived.time |= (((sq_word)at.tm_hour) & 31) << 11;

          strncpy(sqmsg.xmsg.subject, subject.c_str(), 71);
          strncpy(sqmsg.xmsg.from, from.c_str(), 35);
          strncpy(sqmsg.xmsg.to, to.c_str(), 35);

          strcpy(sqmsg.xmsg.__ftsc_date, datestr);

          sqmsg.xmsg.attr = MSGUID;
          sq_msg_base_t *mb = SquishOpenMsgBase(std::string(msgpath + "/" + networks.at(net).areas.at(a).msgarea).c_str());

          if (!mb) {
            std::cerr << "Unable to open message base: " << std::string(msgpath + "/" + networks.at(net).areas.at(a).msgarea) << std::endl;
          } else {
            SquishLockMsgBase(mb);
            SquishWriteMsg(mb, &sqmsg);
            SquishUnlockMsgBase(mb);
            SquishCloseMsgBase(mb);
          }

          free(sqmsg.msg);
        }
      }
    }

    fclose(fptr);
  }
  std::filesystem::remove_all(extractpath);

  return true;
}

bool Qwkie::loadConfig() {
  INIReader inir("talisman.ini");
  if (inir.ParseError()) {
    std::cerr << "Failed to parse talisman.ini" << std::endl;
    return false;
  }

  datapath = inir.Get("Paths", "Data Path", "data");
  temppath = inir.Get("Paths", "Temp Path", "temp");
  msgpath = inir.Get("Paths", "Message Path", "msgs");
  if (!load_archivers()) {
    std::cerr << "Failed to load archivers!" << std::endl;
    return false;
  }

  try {
    auto data = toml::parse_file(datapath + "/qwkie.toml");

    auto networkitems = data.get_as<toml::array>("network");
    if (networkitems != nullptr) {
      for (size_t i = 0; i < networkitems->size(); i++) {

        struct network_t newnet;

        auto itemtable = networkitems->get(i)->as_table();

        auto _name = itemtable->get("name");

        if (_name == nullptr) {
          newnet.name = "";
        } else {
          newnet.name = _name->as_string()->value_or("");
        }

        auto _qwkid = itemtable->get("hostqwkid");
        if (_qwkid == nullptr) {
          newnet.qwkid = "";
        } else {
          newnet.qwkid = _qwkid->as_string()->value_or("");
        }

        auto _myqwkid = itemtable->get("ftpuser");
        if (_myqwkid == nullptr) {
          newnet.myqwkid = "";
        } else {
          newnet.myqwkid = _myqwkid->as_string()->value_or("");
        }

        auto _ftphost = itemtable->get("ftphost");
        if (_ftphost == nullptr) {
          newnet.ftpserver = "";
        } else {
          newnet.ftpserver = _ftphost->as_string()->value_or("");
        }

        auto _ftpport = itemtable->get("ftpport");
        if (_ftpport == nullptr) {
          newnet.port = 21;
        } else {
          newnet.port = _ftpport->as_integer()->value_or(21);
        }

        auto _archiver = itemtable->get("archiver");
        if (_archiver == nullptr) {
          newnet.archiver = "";
        } else {
          newnet.archiver = _archiver->as_string()->value_or("");
        }

        auto _password = itemtable->get("password");
        if (_password == nullptr) {
          newnet.password = "";
        } else {
          newnet.password = _password->as_string()->value_or("");
        }

        auto _tagline = itemtable->get("tagline");
        if (_tagline == nullptr) {
          newnet.tagline = "";
        } else {
          newnet.tagline = _tagline->as_string()->value_or("");
        }

        if (newnet.name == "" || newnet.qwkid == "") {
          std::cerr << "Network name and Host QWK id required" << std::endl;
          continue;
        }
        networks.push_back(newnet);
      }
    }
    auto areaitems = data.get_as<toml::array>("area");
    if (areaitems != nullptr) {
      for (size_t i = 0; i < areaitems->size(); i++) {

        struct area_t newarea;
        std::string netname;
        auto itemtable = areaitems->get(i)->as_table();

        auto _netname = itemtable->get("network");
        if (_netname == nullptr) {
          netname = "";
        } else {
          netname = _netname->as_string()->value_or("");
        }

        auto _qwkbaseno = itemtable->get("basenumber");
        if (_qwkbaseno == nullptr) {
          newarea.qwkbaseno = -1;
        } else {
          newarea.qwkbaseno = _qwkbaseno->as_integer()->value_or(-1);
        }

        auto _msgbase = itemtable->get("msgbase");
        if (_msgbase == nullptr) {
          newarea.msgarea = "";
        } else {
          newarea.msgarea = _msgbase->as_string()->value_or("");
        }

        if (netname == "" || newarea.qwkbaseno == -1 || newarea.msgarea == "") {
          std::cerr << "Invalid area configuration" << std::endl;
          continue;
        }
        bool found = false;
        for (size_t j = 0; j < networks.size(); j++) {
          if (networks.at(j).name == netname) {
            found = true;
            networks.at(j).areas.push_back(newarea);
            break;
          }
        }

        if (!found) {
          std::cerr << "Area for unknown network" << std::endl;
        }
      }
    }

  } catch (toml::parse_error const&) {
    std::cerr << "Error parsing qwkie.toml" << std::endl;
    return false;
  }
  return true;
}

bool Qwkie::load_archivers() {
  try {
    auto data3 = toml::parse_file(datapath + "/archivers.toml");

    auto arcitems = data3.get_as<toml::array>("archiver");

    for (size_t i = 0; i < arcitems->size(); i++) {
      auto itemtable = arcitems->get(i)->as_table();
      std::string mysig;
      int myoffset;
      std::string myname;
      std::string myext;
      std::string myunarc;
      std::string myarc;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or("Invalid Name");
      } else {
        myname = "Unknown";
      }

      auto ext = itemtable->get("extension");
      if (ext != nullptr) {
        myext = ext->as_string()->value_or("");
      } else {
        myext = "";
      }

      auto unarc = itemtable->get("unarc");
      if (unarc != nullptr) {
        myunarc = unarc->as_string()->value_or("");
      } else {
        myunarc = "";
      }
      auto arc = itemtable->get("arc");
      if (arc != nullptr) {
        myarc = arc->as_string()->value_or("");
      } else {
        myarc = "";
      }

      auto sig = itemtable->get("signature");

      if (sig != nullptr) {
        mysig = sig->as_string()->value_or("");
      } else {
        mysig = "";
      }

      auto offset = itemtable->get("offset");

      if (offset != nullptr) {
        myoffset = offset->as_integer()->value_or(0);
      } else {
        myoffset = 0;
      }

      uint8_t *signature;

      if (mysig.size() % 2 != 0) {
        signature = NULL;
      } else {
        signature = (uint8_t *)malloc(mysig.size() / 2);

        if (!signature) {
          continue;
        }

        for (size_t i = 0; i < mysig.size() / 2; i++) {
          if (mysig.at(i * 2) >= '0' && mysig.at(i * 2) <= '9') {
            signature[i] = (mysig.at(i * 2) - '0') << 4;
          } else {
            signature[i] = (toupper(mysig.at(i * 2)) - 'A' + 10) << 4;
          }

          if (mysig.at(i * 2 + 1) >= '0' && mysig.at(i * 2 + 1) <= '9') {
            signature[i] = (signature[i] & 0xf0) | (mysig.at(i * 2 + 1) - '0');
          } else {
            signature[i] = (signature[i] & 0xf0) | (toupper(mysig.at(i * 2 + 1)) - 'A' + 10);
          }
        }
      }
      Archiver *a = new Archiver(myname, myext, myunarc, myarc, myoffset, signature, mysig.size() / 2);
      archivers.push_back(a);
    }
  }

  catch (toml::parse_error const&) {
    std::cerr << "Error parsing " << datapath << "/archivers.toml" << std::endl;
    return false;
  }
  return true;
}

Qwkie::~Qwkie() {
  for (size_t i = 0; i < archivers.size(); i++) {
    delete archivers.at(i);
  }
}
