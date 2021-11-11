#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#define PATH_MAX MAX_PATH
#else
#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define _w_inet_ntop inet_ntop
#define _w_inet_pton inet_pton
#endif

#include <cstring>

#include "GenDefs.h"
#include "Rlogin.h"
#include "Node.h"

#if defined(_MSC_VER) || defined(WIN32)
int _w_inet_pton(int af, const char *src, void *dst) {
  struct sockaddr_storage ss;
  int size = sizeof(ss);
  char src_copy[INET6_ADDRSTRLEN + 1];

  ZeroMemory(&ss, sizeof(ss));
  /* stupid non-const API */
  strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
  src_copy[INET6_ADDRSTRLEN] = 0;

  if (WSAStringToAddressA(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
    switch (af) {
    case AF_INET:
      *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
      return 1;
    case AF_INET6:
      *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
      return 1;
    }
  }
  return 0;
}

const char *_w_inet_ntop(int af, const void *src, char *dst, socklen_t size) {
  struct sockaddr_storage ss;
  unsigned long s = size;

  ZeroMemory(&ss, sizeof(ss));
  ss.ss_family = af;

  switch (af) {
  case AF_INET:
    ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
    break;
  case AF_INET6:
    ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
    break;
  default:
    return NULL;
  }
  /* cannot direclty use &size because of strict aliasing rules */
  return (WSAAddressToStringA((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
}

#endif

int hostname_to_ip(const char *hostname, char *ip, bool v4) {
  struct addrinfo hints, *res, *p;
  int status;
  struct sockaddr_in *ipv4;
  struct sockaddr_in6 *ipv6;

  memset(&hints, 0, sizeof(hints));

  if (v4) {
    hints.ai_family = AF_INET;
  } else {
    hints.ai_family = AF_INET6;
  }
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
    return 1;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET && v4) {
      ipv4 = (struct sockaddr_in *)p->ai_addr;
      _w_inet_ntop(p->ai_family, &(ipv4->sin_addr), ip, INET_ADDRSTRLEN);
      freeaddrinfo(res);
      return 0;
    } else if (p->ai_family == AF_INET6 && !v4) {
      ipv6 = (struct sockaddr_in6 *)p->ai_addr;
      _w_inet_ntop(p->ai_family, &(ipv6->sin6_addr), ip, INET6_ADDRSTRLEN);
      freeaddrinfo(res);
      return 0;
    }
  }
  freeaddrinfo(res);
  return 1;
}

int rlogin_connect_ipv4(const char *server, uint16_t port, int *socketp) {
  struct sockaddr_in servaddr;
  int rlogin_socket;
  char buffer[513];
  memset(&servaddr, 0, sizeof(struct sockaddr_in));
  if (_w_inet_pton(AF_INET, server, &servaddr.sin_addr) != 1) {
    if (hostname_to_ip(server, buffer, true)) {
      return 0;
    }
    if (!_w_inet_pton(AF_INET, buffer, &servaddr.sin_addr)) {
      return 0;
    }
  }
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  if ((rlogin_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return 0;
  }

  if (connect(rlogin_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    return 0;
  }
  *socketp = rlogin_socket;
  return 1;
}

int rlogin_connect_ipv6(const char *server, uint16_t port, int *socketp) {
  struct sockaddr_in6 servaddr;
  int rlogin_socket;
  char buffer[513];
  memset(&servaddr, 0, sizeof(struct sockaddr_in));
  if (_w_inet_pton(AF_INET6, server, &servaddr.sin6_addr) != 1) {
    if (hostname_to_ip(server, buffer, false)) {
      return 0;
    }
    if (!_w_inet_pton(AF_INET6, buffer, &servaddr.sin6_addr)) {
      return 0;
    }
  }
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_port = htons(port);
  if ((rlogin_socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
    return 0;
  }

  if (connect(rlogin_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    return 0;
  }
  *socketp = rlogin_socket;
  return 1;
}

bool Rlogin::session(Node *n, std::string host, int port, std::string luser, std::string ruser, std::string termtype, bool ipv6) {
  int rlogin_socket;
  int ret;
  unsigned char buffer[512];
  int len;
  int stage = 0;
  if (ipv6) {
    ret = rlogin_connect_ipv6(host.c_str(), (uint16_t)port, &rlogin_socket);
  } else {
    ret = rlogin_connect_ipv4(host.c_str(), (uint16_t)port, &rlogin_socket);
  }

  if (ret == 0) {
    n->print_f("\r\n|12Failed to connect!\r\n");
    n->pause();
    return false;
  }

  len = luser.size() + ruser.size() + termtype.size() + 4;
  buffer[0] = '\0';
  memcpy(&buffer[1], luser.c_str(), luser.size());
  buffer[1 + luser.size()] = '\0';
  memcpy(&buffer[2 + luser.size()], ruser.c_str(), ruser.size());
  buffer[2 + luser.size() + ruser.size()] = '\0';
  memcpy(&buffer[3 + luser.size() + ruser.size()], termtype.c_str(), termtype.size());
  buffer[3 + luser.size() + ruser.size() + termtype.size()] = '\0';

  send(rlogin_socket, buffer, len, 0);

  struct timeval tv;

  int timeout = 0;

  while (true) {
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(rlogin_socket, &rfd);
    FD_SET(n->get_socket(), &rfd);
    tv.tv_sec = 60;
    tv.tv_usec = 0;

    int rs = select((rlogin_socket > n->get_socket() ? rlogin_socket : n->get_socket()) + 1, &rfd, NULL, NULL, &tv);

    if (rs == -1 && errno != EINTR) {
      n->print_f("\r\n|12An Error Occured, Disconnected!\r\n");
      n->pause();
#ifdef _MSC_VER
      closesocket(rlogin_socket);
#else
      close(rlogin_socket);
#endif
      return false;
    } else if (FD_ISSET(rlogin_socket, &rfd)) {
      len = recv(rlogin_socket, buffer, 512, 0);
      if (len < 0) {
        n->print_f("\r\n|12An Error Occured, Disconnected!\r\n");
        n->pause();
#ifdef _MSC_VER
        closesocket(rlogin_socket);
#else
        close(rlogin_socket);
#endif
        return false;
      } else if (len == 0) {
        n->print_f("\r\n|12Remote Closed Connection.\r\n");
        n->pause();
#ifdef _MSC_VER
        closesocket(rlogin_socket);
#else
        close(rlogin_socket);
#endif
        return true;
      } else {
        send(n->get_socket(), buffer, len, 0);
      }
    } else if (FD_ISSET(n->get_socket(), &rfd)) {
      len = recv(n->get_socket(), buffer, 512, 0);
      if (len < 0) {
#ifdef _MSC_VER
        closesocket(rlogin_socket);
#else
        close(rlogin_socket);
#endif
        n->disconnected();
      } else if (len == 0) {
#ifdef _MSC_VER
        closesocket(rlogin_socket);
#else
        close(rlogin_socket);
#endif
        n->disconnected();
      } else {
        timeout = 0;
        for (int i = 0; i < len; i++) {
          if (stage == 0) {
            if (buffer[i] == IAC && n->is_telnet()) {
              stage = 1;
            } else {
              send(rlogin_socket, &buffer[i], 1, 0);
            }
          } else if (stage == 1) {
            if (buffer[i] == IAC) {
              send(rlogin_socket, &buffer[i], 1, 0);
              stage = 0;
            } else if (buffer[i] == 250) {
              stage = 3;
            } else {
              stage = 2;
            }
          } else if (stage == 2) {
            stage = 0;
          } else if (stage == 3) {
            if (buffer[i] == 240) {
              stage = 0;
            }
          }
        }
      }
    } else {
      // timeout check
      if (!n->stop_timeout) {
        timeout++;
        if (timeout == n->timeoutmax - 1) {
          n->print_f("|14You are about to time out!\r\n");
        } else if (timeout == n->timeoutmax) {
          n->print_f("|12You have timed out, call back when you're there!\r\n");
#ifdef _MSC_VER
          closesocket(rlogin_socket);
#else
          close(rlogin_socket);
#endif
          n->disconnected();
        }
      }
      if (!n->time_check()) {
        n->print_f("|14You are out of time for today!\r\n");
#ifdef _MSC_VER
        closesocket(rlogin_socket);
#else
        close(rlogin_socket);
#endif
        n->disconnected();
      }
    }
  }
}
