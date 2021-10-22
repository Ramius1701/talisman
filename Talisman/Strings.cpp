#include <sstream>
#include <fstream>
#include <iostream>
#include "Strings.h"

Strings::Strings() {
    loaded = false;
}

bool Strings::validate(std::string msgid, std::string msgstr) {
    // count '%' signs
    int psigns1 = 0;
    int psigns2 = 0;

    for (size_t a = 0; a < msgid.size(); a++) {
        if (msgid[a] == '%') {
            if (a < msgid.size() - 1 && msgid[a + 1] == '%') {
                a++;
                continue;
            }
            psigns1++;
        }
    }

    for (size_t a = 0; a < msgstr.size(); a++) {
        if (msgstr[a] == '%') {
            if (a < msgstr.size() - 1 && msgstr[a + 1] == '%') {
                a++;
                continue;
            }
            psigns2++;
        }
    }

    return (psigns1 == psigns2);
}

std::string Strings::escapechars(std::string in) {
    bool escape = false;
    bool counting = false;
    bool hex = false;
    int counted = 0;
    uint8_t num = 0;
    std::stringstream out;
    for (char c : in) {
        if (c == '\\' && !escape) {
            escape = true;
        }
        else {
            if (counting == true) {
                counted++;
                if (hex) {
                    if (c >= '0' && c <= '9' && counted < 3) { 
                        num = num * 16 + (c - '0');
                    }
                    else if (c >= 'a' && c <= 'f' && counted < 3) {
                        num = num * 16 + (c - 'a' + 10);
                    }
                    else if (c >= 'A' && c <= 'F' && counted < 3) {
                        num = num * 16 + (c - 'A' + 10);
                    }
                    else {
                        counting = false;
                        escape = false;
                        out << num;
                    }
                }
                else {
                    if (c >= '0' && c <= '8' && counted < 3) {
                        num = num * 8 + (c - '0');
                    }
                    else {
                        counting = false;
                        escape = false;
                        out << num;
                    }
                }
            } else if (escape == true) {
                switch (c) {
                case 'r':
                    out << '\r';
                    break;
                case 'n':
                    out << '\n';
                    break;
                case 'b':
                    out << '\b';
                    break;
                case 'e':
                    out << '\033';
                    break;
                case 'f':
                    out << '\f';
                    break;
                case 't':
                    out << '\t';
                    break;
                case 'v':
                    out << '\v';
                    break;
                case '\'':
                    out << '\'';
                    break;
                case '\"':
                    out << '\"';
                    break;
                case '?':
                    out << '\?';
                    break;
                case 'x':
                    counting = true;
                    hex = true;
                    num = 0;
                    counted = 0;
                    break;
                case 'u':
                case 'U':
                    break;
                default:
                    if (c >= '0' || c <= '8') {
                        counting = true;
                        hex = false;
                        num = c - '0';
                        counted = 0;
                    }
                    break;

                }
                escape = false;
            }
            else {
                out << c;
            }
        }
    }

    return out.str();
}

void Strings::load(std::string stringfile) {
    std::ifstream infile(stringfile);
    std::string line;
    std::string msgid;
    std::string msgstr;

    bool lastid = false;

    if (infile.is_open()) {

        while (std::getline(infile, line))
        {
            if (line.size() >= 1 && line[0] == '#') continue;
            if (line.size() > 6 && line.substr(0, 7) == "msgid \"") {
                msgid = escapechars(line.substr(7, line.size() - 8));
                lastid = true;
            }
            else {
                if (lastid == false) {
                    continue;
                }
                else {
                    if (line.size() > 7 && line.substr(0, 8) == "msgstr \"") {
                        msgstr = escapechars(line.substr(8, line.size() - 9));
                        lastid = false;
                        if (validate(msgid, msgstr)) {
                            string_map[msgid] = msgstr;
                        }
                    }
                }
            }
        }

        infile.close();
        loaded = true;
    }
}

const char* Strings::fetch(const char* str) {
    if (!loaded) return str;
    if (string_map.find(std::string(str)) != string_map.end()) {
        return string_map[std::string(str)].c_str();
    }
    return str;
}
