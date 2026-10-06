#pragma once

#include <string>
#include <vector>

class Node;

struct character {
  unsigned char c;
  int fg_colour;
  int bg_colour;
  bool bold;
};

struct ansi_line {
  std::string line;
  bool trimmed;
};

class AnsiEditor {
  friend struct AnsiEditorTestAccess;
public:
  AnsiEditor(Node *n, int width, int height);
  void edit(std::string filename);

private:
  void draw_pallet();
  void refresh_screen();
  bool save(std::string filename);
  bool load(std::string filename);
  int width;
  int height;
  int loc_x;
  int loc_y;
  int cur_fg;
  int cur_bg;
  bool cur_bold;
  int off_x;
  int off_y;
  int cur_pallet;
  struct character **screen;
  bool save_warning;
  Node *n;
  std::vector<std::vector<int>> pallets;
};
