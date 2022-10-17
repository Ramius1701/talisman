#pragma once

constexpr auto VERSION_MAJOR = 0;
constexpr auto VERSION_MINOR = 45;
constexpr auto VERSION_STR = "dev";

constexpr unsigned char IAC = 255;
constexpr unsigned char IAC_WILL = 251;
constexpr unsigned char IAC_WONT = 252;
constexpr unsigned char IAC_DO = 253;
constexpr unsigned char IAC_DONT = 254;
constexpr unsigned char IAC_TRANSMIT_BINARY = 0;
constexpr unsigned char IAC_SUPPRESS_GO_AHEAD = 3;
constexpr unsigned char IAC_ECHO = 1;

constexpr unsigned char NAWS = 31;
constexpr unsigned char TERMINAL_TYPE = 24;

constexpr int MSGSEARCH_BODY = 0;
constexpr int MSGSEARCH_SUBJ = 1;
constexpr int MSGSEARCH_USER = 2;
