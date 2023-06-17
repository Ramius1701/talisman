#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define strcasecmp _stricmp
#else
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <filesystem>
#include <optional>
#include <thread>
#include <fstream>
#include "mime.h"

#define PROGRAM_VERSION "0.1"

#define HTTP_VERSION_1_0 1
#define HTTP_VERSION_1_1 2

#define HTTP_TYPE_GET  1
#define HTTP_TYPE_POST 2
#define HTTP_TYPE_HEAD 3


std::string ts() {
  time_t now = time(NULL);
  struct tm now_tm;
  char buffer[22];

#ifdef _MSC_VER
  localtime_s(&now_tm, &now);
#else
  localtime_r(&now, &now_tm);
#endif
  snprintf(buffer, sizeof buffer, "%04d/%02d/%02d %02d:%02d:%02d: ", now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday, now_tm.tm_hour, now_tm.tm_min,
           now_tm.tm_sec);

  return std::string(buffer);
}

std::string err() {
#ifdef _MSC_VER
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, 12);
  return "";
#else
  return "\x1b[1;31m";
#endif
}

std::string norm() {
#ifdef _MSC_VER
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, 15);
  return "";
#else
  return "\x1b[1;37m";
#endif
}

std::string rst() {
#ifdef _MSC_VER
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, 7);
  return "";
#else
  return "\x1b[0m";
#endif
}


bool read_line(int csock, char **str) { 
  std::stringstream ss;
  char c;

  while(true) {
    if (recv(csock, &c, 1, 0) <= 0) {
      return false;
    }
    if (c == '\r')
      continue;
    if (c == '\n') {

      *str = strdup(ss.str().c_str());
      return true;
    }
    ss << c;
  } 
}

std::optional<std::filesystem::path> MakeAbsolute(const std::filesystem::path &root, const std::filesystem::path &userPath) {
  auto finalPath = (root / userPath).lexically_normal();

  auto [rootEnd, nothing] = std::mismatch(root.begin(), root.end(), finalPath.begin());

  if (rootEnd != root.end())
    return std::nullopt;

  return finalPath;
}

#if 0
std::string MimeTypeFromString(const std::string &str) {
  LPWSTR pwzMimeOut = NULL;
  int wchars_num = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  wchar_t *wstr = new wchar_t[wchars_num];
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wstr, wchars_num);
  // do whatever with wstr

  HRESULT hr = FindMimeFromData(NULL, wstr, NULL, 0, NULL, FMFD_URLASFILENAME, &pwzMimeOut, 0x0);
  if (SUCCEEDED(hr)) {
    std::wstring strResult(pwzMimeOut);
    // Despite the documentation stating to call operator delete, the
    // returned string must be cleaned up using CoTaskMemFree
    CoTaskMemFree(pwzMimeOut);
    
    
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, wchars_num, NULL, 0, NULL, NULL);
    char *nstr = new char[size_needed];
    WideCharToMultiByte(CP_UTF8, 0, wstr, wchars_num, nstr, size_needed, NULL, NULL);

    std::string ret = std::string(nstr);
    delete[] nstr;
    delete[] wstr;
    return ret;
  }

  return "";
}
#else
std::string MimeTypeFromString(const std::string &str) { return mime::lookup(str); }
#endif

void service(int csock, std::string root) {

  

  std::vector<std::string> args;

  char *line;
  char *a;
  int req_type;
  int req_vers;

  if (read_line(csock, &line)) {
    a = strtok(line, " \t");
    if (a == NULL) {
      goto end;
    }
    args.push_back(std::string(a));

    a = strtok(NULL, " \t");
    if (a == NULL) {
      goto end;
    }
    args.push_back(std::string(a));

    a = strtok(NULL, " \t");
    if (a == NULL) {
      goto end;
    }
    args.push_back(std::string(a));

    free(line);

    if (args[2] == "HTTP/1.0") {
      req_vers = HTTP_VERSION_1_0;
    } else if (args[2] == "HTTP/1.1") {
      req_vers = HTTP_VERSION_1_1;
    } else {
      goto end;
    }
    if (args[0] == "GET") {
      req_type = HTTP_TYPE_GET;
    } else if (args[0] == "POST") {
      req_type = HTTP_TYPE_POST;
    } else if (args[0] == "HEAD") {
      req_type = HTTP_TYPE_HEAD;
    } else {
      goto end;
    }


    if (req_vers == HTTP_VERSION_1_1) {
      while (true) {
        if (!read_line(csock, &line))
          goto end;
        if (strlen(line) == 0) {
          free(line);
          break;
        }
        // TODO: Parse Headers...
        free(line);
      }
    }
    std::filesystem::path rootp(root);
    std::filesystem::path reqp(args[1] == "/" ? "/" : args[1].substr(1));
    std::filesystem::path resource = rootp / MakeAbsolute(rootp, reqp).value_or("");

    if (!resource.has_filename()) {
      resource /= "index.html";
    }
    std::stringstream ss;



    if (std::filesystem::exists(resource)) {
      size_t clen = std::filesystem::file_size(resource);

      std::cout << norm() << ts() << "200 " << resource << rst() << std::endl;

      ss << (req_vers == HTTP_VERSION_1_0 ? "HTTP/1.0" : "HTTP/1.1") << " 200 OK\r\n";
      ss << "Server: TalismanHTTPd/" << PROGRAM_VERSION << "\r\n";
      if (req_vers == HTTP_VERSION_1_1) {
        ss << "Connection: close\r\n";
      }
      ss << "Content-Type: " << MimeTypeFromString(resource.extension().string()) << "\r\n";
      ss << "Content-Length: " << clen << "\r\n";
      ss << "\r\n";

      send(csock, ss.str().c_str(), ss.str().size(), 0);

      if (req_type == HTTP_TYPE_GET || req_type == HTTP_TYPE_POST) {

        char *data = (char *)malloc(clen);
        if (!data) {
          goto end;
        }
        FILE *fptr = fopen(resource.string().c_str(), "rb");
        if (!fptr) {
          free(data);

          std::cout << err() << ts() << "Unable to open resource" << rst() << std::endl;

          goto end;
        }
        if (fread(data, 1, clen, fptr) != clen) {
          std::cout << err() << ts() << "Unable to read resource" << rst() << std::endl;

          free(data);
          fclose(fptr);
          goto end;
        }
        send(csock, data, clen, 0);
        free(data);
        fclose(fptr);
      }
    } else {
      std::cout << err() << ts() << "404 " << resource << rst() << std::endl;
      std::string err404 = "<html><head>\r\n"
      "<title>404 Not Found</title>\r\n"
      "</head><body>\r\n"
      "<h1>Not Found</h1>\r\n"
      "The requested resource was not found.\r\n"
      "</body></html>\r\n";


      ss << (req_vers == HTTP_VERSION_1_0 ? "HTTP/1.0" : "HTTP/1.1") << " 404 Not Found\r\n";
      ss << "Server: TalismanHTTPd/" << PROGRAM_VERSION << "\r\n";
      if (req_vers == HTTP_VERSION_1_1) {
        ss << "Connection: close\r\n";
      }
      ss << "Content-Type: text/html\r\n";
      ss << "Content-Length: " << err404.size() << "\r\n";
      ss << "\r\n";

      send(csock, ss.str().c_str(), ss.str().size(), 0);

      if (req_type == HTTP_TYPE_GET || req_type == HTTP_TYPE_POST) {
        send(csock, err404.c_str(), err404.size(), 0);
      }
    }
  }

end:
#ifdef _MSC_VER
  closesocket(csock);
#else
  close(csock);
#endif
}


int main(int argc, char **argv)
{
  struct sockaddr_in serv_addr, client_addr;
  struct sockaddr_in6 serv_addr6, client_addr6;
  if (argc < 3) {
    std::cerr << "Usage: httpsrv [port] [root] -6" << std::endl;
    return -1;
  }

  int port = std::stoi(argv[1]);
  int on = 1;

#ifdef _MSC_VER
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    return -1;
  }

#endif
  int httpdfd = socket(AF_INET, SOCK_STREAM, 0);
  int httpd6fd = -1;
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);
  if (setsockopt(httpdfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
    return -1;
  }
  if (bind(httpdfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) < 0) {
    return -1;
  }

  if (argc == 4 && strcmp(argv[3], "-6") == 0) {
    httpd6fd = socket(AF_INET6, SOCK_STREAM, 0);
    memset(&serv_addr6, 0, sizeof(struct sockaddr_in6));

    serv_addr6.sin6_family = AF_INET6;
    serv_addr6.sin6_addr = in6addr_any;
    serv_addr6.sin6_port = htons(port);
    if (setsockopt(httpd6fd, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0) {
      return -1;
    }
    if (setsockopt(httpd6fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      return -1;
    }  
    if (bind(httpd6fd, (struct sockaddr *)&serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
      return -1;
    }
    listen(httpd6fd, 5);
  }
  listen(httpdfd, 5);

  int nfds;
  int maxfd = httpdfd;
  fd_set server_fds;
  FD_ZERO(&server_fds);
  FD_SET(httpdfd, &server_fds);

  if (httpd6fd != -1) {
    if (httpd6fd > maxfd) {
      maxfd = httpd6fd;
    }
    FD_SET(httpd6fd, &server_fds);
  }

  nfds = maxfd + 1;

  int csockfd;

  while (1) {
    csockfd = -1;
    bool ipv6con = false;
    fd_set copy_fds = server_fds;

    int clen = sizeof(struct sockaddr_in);
    int clen6 = sizeof(struct sockaddr_in6);

    memset(&client_addr, 0, clen);

    if (select(nfds, &copy_fds, NULL, NULL, NULL) < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        exit(-1);
      }
    }
    if (FD_ISSET(httpdfd, &copy_fds)) {
      csockfd = accept(httpdfd, (struct sockaddr *)&client_addr, (socklen_t *)&clen);
      service(csockfd, std::string(argv[2]));
    }
    if (httpd6fd != -1) {
      if (FD_ISSET(httpd6fd, &copy_fds)) {
        csockfd = accept(httpd6fd, (struct sockaddr *)&client_addr6, (socklen_t *)&clen6);
        ipv6con = true;
        std::thread th(service, csockfd, argv[2]);
        th.detach();
      }
    }
  }
}


