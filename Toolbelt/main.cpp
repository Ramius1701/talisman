#include "../Common/INIReader.h"
#include "../Common/Squish.h"
#include "Files.h"
#include "Nodelist.h"
#include "User.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _MSC_VER
#include <Windows.h>
#define strcasecmp _stricmp
#endif

bool save_message(std::string datapath, std::string file, std::string to, std::string from, std::string subject, std::string text, std::string orig_addr,
                  time_t date) {
  sq_msg_base_t *mb;

  char charsbuffer[] = "\001CHRS: CP437 2\r";
  char tzutcbuffer[256];
  char msgidbuffer[256];
  FILE *fptr;
  uint32_t msgid;
  time_t thetime;

  int at;
  struct tm lt;
  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int ret;
  std::stringstream ss;

  if (date == 0) {
    thetime = time(NULL);
  } else {
    thetime = date;
  }

  sq_msg_t newmsg;

  memset(&newmsg, 0, sizeof(newmsg));

  if (orig_addr.find(":") != std::string::npos) {
#ifdef _MSC_VER
    TIME_ZONE_INFORMATION tz;
    GetTimeZoneInformation(&tz);
    int bias = tz.Bias;
    if (bias > 0) {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: -%02d%02d\r", abs(bias / 60), abs(bias % 60));
    } else {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: %02d%02d\r", abs(bias / 60), abs(bias % 60));
    }
#else
    time_t gmt, rawtime = time(NULL);
    struct tm *ptm;

    struct tm gbuf;
    ptm = gmtime_r(&rawtime, &gbuf);
    // Request that mktime() looksup dst in timezone database
    ptm->tm_isdst = -1;
    gmt = mktime(ptm);

    int bias = (int)difftime(rawtime, gmt);
    bias /= 60;
    if (bias < 0) {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: -%02d%02d\r", abs(bias / 60), abs(bias % 60));
    } else {
      snprintf(tzutcbuffer, sizeof tzutcbuffer, "\x01TZUTC: %02d%02d\r", abs(bias / 60), abs(bias % 60));
    }
#endif

    memset(msgidbuffer, 0, 256);
    if (orig_addr != "") {
      fptr = fopen(std::string(datapath + "/msgserial.dat").c_str(), "rb");

      if (!fptr) {
        msgid = (uint32_t)thetime;
      } else {
        fread(&msgid, sizeof(uint32_t), 1, fptr);
        fclose(fptr);

        if (thetime > msgid) {
          msgid = (uint32_t)thetime;
        } else {
          msgid++;
        }
      }

      fptr = fopen(std::string(datapath + "/msgserial.dat").c_str(), "wb");
      if (fptr) {
        fwrite(&msgid, sizeof(uint32_t), 1, fptr);
        fclose(fptr);
      }
      snprintf(msgidbuffer, sizeof msgidbuffer, "\x01MSGID: %s %08X\r", orig_addr.c_str(), msgid);
    }

    // are we a netmail
    newmsg.ctrl_len = strlen(msgidbuffer) + strlen(tzutcbuffer) + strlen(charsbuffer);

    if (orig_addr != "") {
      NETADDR *orig = parse_fido_addr(orig_addr.c_str());
      if (orig != NULL) {
        newmsg.xmsg.orig.zone = orig->zone;
        newmsg.xmsg.orig.net = orig->net;
        newmsg.xmsg.orig.node = orig->node;
        newmsg.xmsg.orig.point = orig->point;
        free(orig);
      } else {
        newmsg.xmsg.orig.zone = 0;
        newmsg.xmsg.orig.net = 0;
        newmsg.xmsg.orig.node = 0;
        newmsg.xmsg.orig.point = 0;
      }
    } else {
      newmsg.xmsg.orig.zone = 0;
      newmsg.xmsg.orig.net = 0;
      newmsg.xmsg.orig.node = 0;
      newmsg.xmsg.orig.point = 0;
    }

    newmsg.ctrl = (char *)malloc(newmsg.ctrl_len);
    if (!newmsg.ctrl) {
      return false;
    }
    at = 0;
    memcpy(newmsg.ctrl, tzutcbuffer, strlen(tzutcbuffer));
    at += strlen(tzutcbuffer);
    memcpy(&newmsg.ctrl[at], charsbuffer, strlen(charsbuffer));
    at += strlen(charsbuffer);
    if (orig_addr != "") {
      memcpy(&newmsg.ctrl[at], msgidbuffer, strlen(msgidbuffer));
      // at += strlen(msgidbuffer);
    }
  } else if (orig_addr != "") {
    newmsg.ctrl = NULL;
    newmsg.ctrl_len = 0;

    newmsg.xmsg.orig.zone = 20000;
    newmsg.xmsg.orig.net = 20000;
    newmsg.xmsg.orig.node = stoi(orig_addr);
    newmsg.xmsg.orig.point = 0;
  } else {
    newmsg.ctrl = NULL;
    newmsg.ctrl_len = 0;

    newmsg.xmsg.orig.zone = 0;
    newmsg.xmsg.orig.net = 0;
    newmsg.xmsg.orig.node = 0;
    newmsg.xmsg.orig.point = 0;
  }

  newmsg.msg_len = text.size();
  newmsg.msg = (char *)malloc(text.size());
  if (!newmsg.msg) {
    free(newmsg.ctrl);
    return false;
  }
  memcpy(newmsg.msg, text.c_str(), text.size());

  strncpy(newmsg.xmsg.subject, subject.c_str(), 71);
  strncpy(newmsg.xmsg.from, from.c_str(), 35);
  strncpy(newmsg.xmsg.to, to.c_str(), 35);

  newmsg.xmsg.replyto = 0;
  newmsg.xmsg.attr = MSGLOCAL | MSGUID;

#if _MSC_VER
  localtime_s(&lt, &thetime);
#else
  localtime_r(&thetime, &lt);
#endif
  newmsg.xmsg.date_written.date |= (((sq_word)lt.tm_mday) & 31);
  newmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_mon + 1)) & 15) << 5;
  newmsg.xmsg.date_written.date |= (((sq_word)(lt.tm_year - 80)) & 127) << 9;

  newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_sec) & 31);
  newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_min) & 63) << 5;
  newmsg.xmsg.date_written.time |= (((sq_word)lt.tm_hour) & 31) << 11;

  newmsg.xmsg.date_arrived.date = newmsg.xmsg.date_written.date;
  newmsg.xmsg.date_arrived.time = newmsg.xmsg.date_written.time;

  snprintf(newmsg.xmsg.__ftsc_date, 20, "%02d %s %02d  %02d:%02d:%02d", lt.tm_mday, months[lt.tm_mon], lt.tm_year - 100, lt.tm_hour, lt.tm_min, lt.tm_sec);

  mb = SquishOpenMsgBase(file.c_str());

  if (!mb) {
    free(newmsg.msg);
    free(newmsg.ctrl);
    return false;
  }

  SquishLockMsgBase(mb);
  ret = SquishWriteMsg(mb, &newmsg);
  SquishUnlockMsgBase(mb);
  SquishCloseMsgBase(mb);

  free(newmsg.msg);
  free(newmsg.ctrl);

  return (ret == 1);
}

std::string lower(std::string in) {
  std::stringstream ss;
  for (size_t c : in) {
    ss << (char)(tolower(c));
  }
  return ss.str();
}

static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

int main(int argc, char **argv) {
  INIReader inir("talisman.ini");

  if (inir.ParseError() != 0) {
    std::cerr << "Unable to parse talisman.ini!" << std::endl;
    return -1;
  }

  if (argc > 1) {
    if (strcasecmp(argv[1], "password") == 0) {
      if (argc == 4) {
        std::string user(argv[2]);
        std::string newpass(argv[3]);

        if (User::update_password(inir.Get("paths", "data path", "data"), user, newpass)) {
          std::cout << "Successfully updated password for " << user << "." << std::endl;
        } else {
          std::cerr << "Failed to update password for " << user << "." << std::endl;
        }
        return 0;
      }
    } else if (strcasecmp(argv[1], "seclevel") == 0) {
      if (argc == 4) {
        std::string user(argv[2]);
        int seclevel;
        try {
          seclevel = stoi(std::string(argv[3]));
        } catch (std::invalid_argument const &) {
          std::cerr << "Invalid argument for sec level." << std::endl;
          return -1;
        } catch (std::out_of_range const &) {
          std::cerr << "Out of range for sec level." << std::endl;
          return -1;
        }
        if (!User::set_attribute(inir.Get("paths", "data path", "data"), user, "seclevel", std::to_string(seclevel))) {
          std::cout << "Failed.";
        } else {
          std::cout << "Done." << std::endl;
        }
        return 0;
      }
    } else if (strcasecmp(argv[1], "uploadindex") == 0) {
      std::string uploaded_by = "Unknown";
      if (argc == 6) {
        uploaded_by = std::string(argv[5]);
      }

      if (argc >= 5) {
        std::ifstream infile;
        std::filesystem::path folder(argv[3]);
        std::string database = std::string(argv[4]);
        std::vector<std::string> descr;

        infile.open(argv[2]);
        std::string line;
        std::string file = "";
        Files files;

        while (std::getline(infile, line)) {

          if (line[0] == ' ' || line[0] == '\t') {
            // description extended from last filename
            std::string d = line;
            trim(d);
            descr.push_back(d);
          } else {
            if (file != "") {
              // add file
              std::filesystem::path fpath = folder;
              fpath.append(file);
              if (std::filesystem::exists(fpath)) {
                files.insert_file(database, std::filesystem::absolute(fpath).u8string(), descr, uploaded_by);
                std::cout << "Added: " << file << std::endl;
              } else {
                std::cout << "Skipped: " << file << " (Missing) " << std::endl;
              }
            }
            file = line.substr(0, line.find(' '));
            std::string d = line.substr(line.find(' '));
            trim(d);
            descr.clear();
            descr.push_back(d);
          }
        }
        if (file != "") {
          // add file
          std::filesystem::path fpath = folder;
          fpath.append(file);
          if (std::filesystem::exists(fpath)) {
            files.insert_file(database, std::filesystem::absolute(fpath).u8string(), descr, uploaded_by);
            std::cout << "Added: " << file << std::endl;
          } else {
            std::cout << "Skipped: " << file << " (Missing) " << std::endl;
          }
        }
        infile.close();
      }
    } else if (strcasecmp(argv[1], "uploadbulk") == 0) {
      std::string uploaded_by = "Unknown";
      if (argc == 5) {
        uploaded_by = std::string(argv[4]);
      }
      if (argc >= 4) {
        std::filesystem::path folder(argv[2]);
        std::string database = std::string(argv[3]);
        Files files;
        files.load_archivers(inir.Get("paths", "data path", "data"));

        for (auto &d : std::filesystem::directory_iterator(folder)) {
          if (d.path().filename().u8string() != "files.bbs") {
            if (!files.add_file(inir.Get("paths", "temp path", "data"), database, std::filesystem::absolute(d.path()).u8string(), uploaded_by)) {
              std::cout << "Failed to add: " << std::filesystem::absolute(d.path()).u8string() << std::endl;
            } else {
              std::cout << "Successfully added: " << std::filesystem::absolute(d.path()).u8string() << std::endl;
            }
          } else {
            std::cout << "Skipping files.bbs" << std::endl;
          }
        }
      }
    } else if (strcasecmp(argv[1], "filetrim") == 0) {
      if (argc == 3) {
        std::string database = std::string(argv[2]);
        Files files;

        int trimmed = files.trim(database);

        std::cout << "Trimmed " << std::to_string(trimmed) << " missing files from database." << std::endl;
      }
    } else if (strcasecmp(argv[1], "nodelistp") == 0) {
      if (argc == 5) {
        std::string nodelist = std::string(argv[3]);
        std::string database = std::string(argv[4]);
        std::string domain = std::string(argv[2]);
        if (!Nodelist::parse(domain, nodelist, database)) {
          std::cout << "Failure" << std::endl;
        } else {
          std::cout << "Done" << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "allfiles") == 0) {
      if (argc == 4) {
        int seclevel;
        try {
          seclevel = stoi(std::string(argv[2]));
        } catch (std::invalid_argument const &) {
          std::cerr << "Invalid argument for sec level." << std::endl;
          return -1;
        } catch (std::out_of_range const &) {
          std::cerr << "Out of range for sec level." << std::endl;
          return -1;
        }

        Files files;

        int tot = files.all_files(inir.Get("paths", "data path", "data"), seclevel, 0, std::string(argv[3]));

        std::cout << "All files list " << argv[3] << " created listing " << tot << " total files." << std::endl;
      }
    } else if (strcasecmp(argv[1], "movefile") == 0) {
      if (argc == 6) {
        std::string srcfile(argv[2]);
        std::string destfile(argv[3]);
        std::string srcdb(argv[4]);
        std::string destdb(argv[5]);

        Files files;

        if (!files.move_file(inir.Get("paths", "data path", "data"), srcfile, destfile, srcdb, destdb)) {
          std::cout << "Failed to move file!" << std::endl;
        } else {
          std::cout << "Successfully moved file!" << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "newfiles") == 0) {
      if (argc == 5) {
        int seclevel;
        try {
          seclevel = stoi(std::string(argv[2]));
        } catch (std::invalid_argument const &) {
          std::cerr << "Invalid argument for sec level." << std::endl;
          return -1;
        } catch (std::out_of_range const &) {
          std::cerr << "Out of range for sec level." << std::endl;
          return -1;
        }

        if (strlen(argv[3]) != 10 || argv[3][4] != '.' || argv[3][7] != '.') {
          std::cerr << "Invalid date!" << std::endl;
          return -1;
        }

        Files files;

        struct tm dtm;

        memset(&dtm, 0, sizeof(struct tm));

        dtm.tm_year = ((argv[3][0] - '0') * 1000 + (argv[3][1] - '0') * 100 + (argv[3][2] - '0') * 10 + (argv[3][3] - '0') - 1900);
        dtm.tm_mon = (argv[3][5] - '0') * 10 + (argv[3][6] - '0') - 1;
        dtm.tm_mday = (argv[3][8] - '0') * 10 + (argv[3][9] - '0');
        dtm.tm_hour = 0;
        dtm.tm_min = 0;
        dtm.tm_sec = 0;
        dtm.tm_isdst = -1;
        time_t then = mktime(&dtm);

        int tot = files.all_files(inir.Get("paths", "data path", "data"), seclevel, then, std::string(argv[4]));

        std::cout << "New files list " << argv[4] << " created listing " << tot << " total files." << std::endl;
      }
    } else if (strcasecmp(argv[1], "clearlrall") == 0) {
      if (!User::clear_lastread(inir.Get("paths", "data path", "data"), "", "")) {
        std::cout << "Error clearing last read." << std::endl;
      } else {
        std::cout << "Success clearing last read." << std::endl;
      }
    } else if (strcasecmp(argv[1], "clearlruser") == 0) {
      if (argc == 3) {
        if (!User::clear_lastread(inir.Get("paths", "data path", "data"), "", std::string(argv[2]))) {
          std::cout << "Error clearing last read." << std::endl;
        } else {
          std::cout << "Success clearing last read." << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "clearlrbase") == 0) {
      if (argc == 3) {
        if (!User::clear_lastread(inir.Get("paths", "data path", "data"), std::string(argv[2]), "")) {
          std::cout << "Error clearing last read." << std::endl;
        } else {
          std::cout << "Success clearing last read." << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "clearlrbaseuser") == 0) {
      if (argc == 4) {
        if (!User::clear_lastread(inir.Get("paths", "data path", "data"), std::string(argv[2]), std::string(argv[3]))) {
          std::cout << "Error clearing last read." << std::endl;
        } else {
          std::cout << "Success clearing last read." << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "deleteuser") == 0) {
      if (argc == 3) {
        if (!User::delete_user(inir.Get("paths", "data path", "data"), std::string(argv[2]))) {
          std::cout << "Failed" << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "convertmsgna") == 0) {
      if (argc >= 9) {
        int read_sl = strtol(argv[4], NULL, 10);
        int post_sl = strtol(argv[5], NULL, 10);
        std::string src = std::string(argv[3]);
        std::string myaka = std::string(argv[6]);
        std::string uplink = std::string(argv[7]);
        int qwk_id = strtol(argv[8], NULL, 10);
        std::string prefix = "";
        if (argc == 10) {
          prefix = std::string(argv[9]) + " ";
        }

        std::ifstream infile(src);
        if (infile.is_open()) {
          std::string line;
          while (std::getline(infile, line)) {
            std::string tag = line.substr(0, line.find(" "));
            std::string desc = line.substr(line.find(" "));
            trim(desc);

            if (std::string(argv[2]) == "-m") {
              std::cout << "[[messagearea]]" << std::endl;
              std::cout << "name = \"" << prefix << desc << "\"" << std::endl;
              std::cout << "file = \"" << lower(tag) << "\"" << std::endl;
              std::cout << "read_sec_level = " << read_sl << std::endl;
              std::cout << "write_sec_level = " << post_sl << std::endl;
              std::cout << "aka = \"" << myaka << "\"" << std::endl;
              std::cout << "qwk_base_no = " << qwk_id << std::endl;
              qwk_id++;
              std::cout << std::endl;
            } else {
              std::cout << "[[area]]" << std::endl;
              std::cout << "aka = \"" << myaka << "\"" << std::endl;
              std::cout << "file = \"" << lower(tag) << "\"" << std::endl;
              std::cout << "links = \"" << uplink << "\"" << std::endl;
              std::cout << "tag = \"" << tag << "\"" << std::endl;
              std::cout << std::endl;
            }
          }
          infile.close();
        }
      }
    } else if (strcasecmp(argv[1], "convertfilena") == 0) {
      if (argc >= 11) {
        if (strcasecmp(argv[10], "true") != 0 && strcasecmp(argv[10], "false")) {
          std::cerr << "create argument must be true or false!" << std::endl;
          return -1;
        }
        int up_sl = strtol(argv[4], NULL, 10);
        int dl_sl = strtol(argv[5], NULL, 10);
        int vis_sl = strtol(argv[6], NULL, 10);
        std::string src = std::string(argv[3]);
        std::string myaka = std::string(argv[7]);
        std::string uplink = std::string(argv[8]);
        std::string root = std::string(argv[9]);
        std::string prefix = "";

        bool create = (strcasecmp(argv[10], "true") == 0);

        if (argc == 12) {
          prefix = std::string(argv[11]) + " ";
        }

        std::ifstream infile(src);
        if (infile.is_open()) {
          std::string line;
          while (std::getline(infile, line)) {
            std::stringstream ss(line);
            std::string fragment;
            std::vector<std::string> frags;
            while (std::getline(ss, fragment, ' ')) {
              frags.push_back(fragment);
            }
            if (frags.size() > 0 && frags.at(0) == "Area") {
              std::string tag = frags.at(1);
              std::string desc = line.substr(28);
              trim(desc);

              if (create) {
                std::filesystem::path filepath(root + "/" + lower(tag));
                if (!std::filesystem::exists(filepath)) {
                  std::filesystem::create_directories(filepath);
                }
              }

              if (std::string(argv[2]) == "-f") {
                std::cout << "[[filearea]]" << std::endl;
                std::cout << "name = \"" << prefix << desc << "\"" << std::endl;
                std::cout << "database = \"fb_" << lower(tag) << "\"" << std::endl;
                std::cout << "file_path = \"" << root << "/" << lower(tag) << "\"" << std::endl;
                std::cout << "upload_sec_level = " << up_sl << std::endl;
                std::cout << "download_sec_level = " << dl_sl << std::endl;
                std::cout << "visible_sec_level = " << vis_sl << std::endl;
                std::cout << std::endl;
              } else {
                std::cout << "[[filearea]]" << std::endl;
                std::cout << "aka = \"" << myaka << "\"" << std::endl;
                std::cout << "database = \"fb_" << lower(tag) << "\"" << std::endl;
                std::cout << "directory = \"" << root << "/" << lower(tag) << "\"" << std::endl;
                std::cout << "links = \"" << uplink << "\"" << std::endl;
                std::cout << "tag = \"" << tag << "\"" << std::endl;
                std::cout << std::endl;
              }
            }
          }
          infile.close();
        }
      }
    } else if (strcasecmp(argv[1], "adpost") == 0) {
      if (argc > 2) {
        INIReader mailinir(argv[2]);
        if (mailinir.ParseError() != 0) {
          std::cerr << "Unable to parse " << argv[2] << "!" << std::endl;
          return -1;
        }
        std::string msgbase = mailinir.Get("main", "Message Base", "");
        std::string oaddr = mailinir.Get("main", "Origin Address", "");
        std::string tagline = mailinir.Get("main", "Tagline", "A Talisman BBS");
        std::string to = mailinir.Get("main", "To", "All");
        std::string from = mailinir.Get("main", "From", "Toolbelt");
        std::string subject = mailinir.Get("main", "Subject", "");
        std::string content = mailinir.Get("main", "Message File", "");

        if (content == "" || subject == "" || msgbase == "") {
          return -1;
        }

        std::ifstream infile(content);

        std::string line;
        std::stringstream msgcontent;
        while (std::getline(infile, line)) {
          if (line.find("\r") != std::string::npos) {
            msgcontent << line;
          } else {
            msgcontent << line << "\r";
          }
        }

        if (oaddr != "") {
          msgcontent << "\r---\r * Origin: " << tagline << " (" << oaddr << ")"
                     << "\r";
        }

        if (!save_message(inir.Get("paths", "data path", "data"), inir.Get("paths", "message path", "msgs") + "/" + msgbase, to, from, subject,
                          msgcontent.str(), oaddr, time(NULL))) {
          std::cerr << "Message failed to save!" << std::endl;
        } else {
          std::cerr << "Message saved!" << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "pack") == 0) {
      if (argc > 2) {
        int ret = SquishPackMsgBase(std::string(inir.Get("paths", "message path", "msgs") + "/" + argv[2]).c_str());
        if (ret != -1) {
          std::cout << "Packed " << ret << " messages into " << argv[2] << "." << std::endl;
        } else {
          std::cout << "An error occured, msg base not packed!" << std::endl;
        }
      }
    } else if (strcasecmp(argv[1], "prune") == 0) {
      if (argc > 3) {
        sq_msg_base_t *mb;

        int toleave = strtol(argv[3], NULL, 10);

        mb = SquishOpenMsgBase(std::string(inir.Get("paths", "message path", "msgs") + "/" + argv[2]).c_str());
        if (mb) {
          SquishLockMsgBase(mb);
          SquishPruneMsgBase(mb, toleave);
          SquishUnlockMsgBase(mb);
          SquishCloseMsgBase(mb);
        }
      }
    }
  } else {
    std::cerr << "Usage: " << argv[0] << " command [args]" << std::endl;
    std::cerr << "   COMMAND password         ARGS username newpassword" << std::endl;
    std::cerr << "   COMMAND seclevel         ARGS username newlevel" << std::endl;
    std::cerr << "   COMMAND uploadindex      ARGS indexfile folder database [uploadedby]" << std::endl;
    std::cerr << "   COMMAND uploadbulk       ARGS folder database [uploadedby]" << std::endl;
    std::cerr << "   COMMAND filetrim         ARGS database" << std::endl;
    std::cerr << "   COMMAND movefile         ARGS srcfilename destdir srcdatabase destdatabase" << std::endl;
    std::cerr << "   COMMAND allfiles         ARGS sec_level outfile" << std::endl;
    std::cerr << "   COMMAND newfiles         ARGS sec_level yyyy.mm.dd outfile" << std::endl;
    std::cerr << "   COMMAND nodelistp        ARGS domain nodelist database" << std::endl;
    std::cerr << "   COMMAND clearlrall       ARGS (NONE)" << std::endl;
    std::cerr << "   COMMAND clearlruser      ARGS username" << std::endl;
    std::cerr << "   COMMAND clearlrbase      ARGS msgbasefile" << std::endl;
    std::cerr << "   COMMAND clearlrbaseuser  ARGS msgbasefile username" << std::endl;
    std::cerr << "   COMMAND deleteuser       ARGS username" << std::endl;
    std::cerr << "   COMMAND convertmsgna     ARGS [-m|-p] srcfilename read_sl post_sl myaka uplink qwkid_start [prefix]" << std::endl;
    std::cerr << "   COMMAND convertfilena    ARGS [-f|-p] srcfilename up_sl dl_sl vis_sl myaka uplink root create [prefix]" << std::endl;
    std::cerr << "   COMMAND adpost           ARGS message.ini" << std::endl;
    std::cerr << "   COMMAND pack             ARGS msgbasefile" << std::endl;
    std::cerr << "   COMMAND prune            ARGS msgbasefile numbertoleave" << std::endl;
  }
}
