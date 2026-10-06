#pragma once
#include <string>
#include <vector>
#include <cctype>

// Classic values use ANSI order, indexed/RGB values have disjoint tags.
namespace AnsiColor {
constexpr int Indexed = 0x10000;
constexpr int RGB = 0x2000000;
constexpr int Default = 0x4000000;
inline bool parse(const std::string &s, int &color) {
  if (s.size() == 7 && s[0] == '#') {
    int value = 0;
    for (size_t i = 1; i < s.size(); ++i) {
      unsigned char c = static_cast<unsigned char>(s[i]);
      if (!std::isxdigit(c)) return false;
      value = value * 16 + (c <= '9' ? c - '0' : std::tolower(c) - 'a' + 10);
    }
    color = RGB + value;
    return true;
  }
  if (s.empty() || s.size() > 3) return false;
  int value = 0;
  for (unsigned char c : s) {
    if (!std::isdigit(c)) return false;
    value = value * 10 + c - '0';
  }
  if (value > 255) return false;
  color = Indexed + value;
  return true;
}
inline std::string parameter(int color, bool background) {
  const std::string prefix = background ? "48" : "38";
  if (color == Default) return background ? "49" : "39";
  if (color >= RGB && color < RGB + 0x1000000) {
    int value = color - RGB;
    return prefix + ";2;" + std::to_string((value >> 16) & 255) + ";" +
      std::to_string((value >> 8) & 255) + ";" + std::to_string(value & 255);
  }
  if (color >= Indexed && color < Indexed + 256)
    return prefix + ";5;" + std::to_string(color - Indexed);
  if (color < 0 || color > 15) color = background ? 0 : 7;
  return std::to_string((background ? (color < 8 ? 40 : 100) : (color < 8 ? 30 : 90)) + color % 8);
}
inline std::string sgr(int fg, int bg, bool bold) {
  return "\x1b[" + std::string(bold ? "1" : "22") + ";" + parameter(fg, false) + ";" + parameter(bg, true) + "m";
}
// i initially points to '['; on success it points to the CSI final byte.
// Bound both parameter count and numeric magnitude, including truncated input.
inline bool csi(const char *text, size_t len, size_t &i, std::vector<int> &params) {
  params.assign(1, 0);
  bool valid = true;
  for (++i; i < len; ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (c >= 0x40 && c <= 0x7e) return valid;
    if (c == ';') {
      if (params.size() == 64) valid = false;
      else params.push_back(0);
    } else if (c >= '0' && c <= '9') {
      if (params.back() > 100000) valid = false;
      else params.back() = params.back() * 10 + c - '0';
    } else valid = false;
  }
  return false;
}
inline void apply(const std::vector<int> &p, int &fg, int &bg, bool &bold) {
  for (size_t i = 0; i < p.size(); ++i) {
    int v = p[i];
    if (v == 38 || v == 48) {
      int color = -1;
      if (i + 1 == p.size()) break;
      int mode = p[++i];
      if (mode == 5) {
        if (i + 1 == p.size()) break;
        int index = p[++i];
        if (index >= 0 && index <= 255) color = Indexed + index;
      } else if (mode == 2) {
        if (i + 3 >= p.size()) break;
        int r = p[i+1], g = p[i+2], b = p[i+3];
        i += 3;
        if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
          color = RGB + (r << 16) + (g << 8) + b;
      } else break;
      if (color != -1) (v == 38 ? fg : bg) = color;
    } else if (v == 0) { fg = 7; bg = 0; bold = false; }
    else if (v == 1) bold = true;
    else if (v == 2 || v == 22) bold = false;
    else if (v >= 30 && v <= 37) fg = v - 30;
    else if (v >= 40 && v <= 47) bg = v - 40;
    else if (v >= 90 && v <= 97) fg = v - 90 + 8;
    else if (v >= 100 && v <= 107) bg = v - 100 + 8;
    else if (v == 39) fg = Default;
    else if (v == 49) bg = Default;
  }
}
}
