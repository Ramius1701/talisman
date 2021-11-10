#include "AnsiEditor.h"
#include "Node.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <cstring>

static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

AnsiEditor::AnsiEditor(Node *n, int width, int height) {
  this->width = width;
  this->height = height;
  this->n = n;
  loc_x = 0;
  loc_y = 0;
  cur_fg = 7;
  cur_bg = 0;
  off_x = 0;
  off_y = 0;
  cur_pallet = -1;
  cur_bold = false;
  save_warning = false;
}

void AnsiEditor::draw_pallet() {
  n->print_f_nc("\x1b[%d;1H", n->get_term_height());

  if (cur_pallet == -1) {
    n->print_f_nc("\x1b[%d;3%d;4%dmAlpha / Numeric Pallet\x1b[K", (cur_bold ? 1 : 0), cur_fg, cur_bg);
  } else {
    // to do fill in pallet
    n->print_f_nc("\x1b[%d;3%d;4%dm", (cur_bold ? 1 : 0), cur_fg, cur_bg);
    for (size_t i = 0; i < pallets.at(cur_pallet).size(); i++) {
      n->print_f_nc("%d:%c ", i, pallets.at(cur_pallet).at(i));
    }
    n->print_f_nc("\x1b[K");
  }
  n->print_f_nc("\x1b[%d;%dH%s Ctrl-Z For Options\x1b[K", n->get_term_height(), n->get_term_width() - 29, n->get_config()->get_prompt_colour());
}

void AnsiEditor::refresh_screen() {
  for (int y = off_y; y < (int)n->get_term_height() + off_y - 1; y++) {
    for (int x = off_x; x < (int)n->get_term_width() + off_x; x++) {
      if (y < height && x < width) {
        n->print_f_nc("\x1b[%d;%dH\x1b[%d;3%d;4%dm%c", y - off_y + 1, x - off_x + 1, (screen[y][x].bold ? 1 : 0), screen[y][x].fg_colour,
                      screen[y][x].bg_colour, screen[y][x].c);
      } else {
        n->print_f_nc("\x1b[%d;%dH\x1b[1;30;40m\xb1", y + 1 - off_y, x + 1 - off_x);
      }
    }
  }

  n->print_f_nc("\x1b[%d;%dH", loc_x, loc_y);
}

bool AnsiEditor::load(std::string filename) {
  FILE *fptr = fopen(filename.c_str(), "rb");

  fseek(fptr, 0, SEEK_END);

  size_t len = ftell(fptr);

  fseek(fptr, 0, SEEK_SET);

  char *contents = (char *)malloc(len);
  if (!contents) {
    fclose(fptr);
    return false;
  }

  fread(contents, 1, len, fptr);

  fclose(fptr);

  char * sauce = strrchr(contents, 0x1a);

  if (sauce != NULL) {
      size_t new_len = sauce - contents;
      char *tmp = (char *)realloc(contents, new_len);
      if (!tmp) {
          free(contents);
          return false;
      }
      contents = tmp;
      len = new_len;
  }

  int line_at = 0;
  int lines = 0;
  int col_at = 0;
  int param_count;
  int params[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  int save_col = 0;
  int save_row = 0;
  bool bold = false;
  int fg_colour = 7;
  int bg_colour = 0;

  for (size_t i = 0; i < len; i++) {
    if (contents[i] == '\r' || (i >= 1 && contents[i] == '\n' && contents[i - 1] != '\r')) {
      line_at++;
      if (line_at > lines) {
        lines = line_at;
      }
      col_at = 0;
    } else if (contents[i] == '\x1b') {
      i++;
      if (contents[i] != '[') {
        i--;
        continue;
      } else {
        param_count = 0;
        while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", contents[i]) == NULL) {
          if (contents[i] == ';') {
            param_count++;
          } else if (contents[i] >= '0' && contents[i] <= '9') {
            if (param_count == 0) {
              param_count = 1;
              for (int j = 0; j < 9; j++) {
                params[j] = 0;
              }
            }
            params[param_count - 1] = params[param_count - 1] * 10 + (contents[i] - '0');
          }
          i++;
        }
        switch (contents[i]) {
        case 'A':
          if (param_count > 0) {
            line_at -= params[0];
          } else {
            line_at--;
          }
          if (line_at < 0)
            line_at = 0;
          break;
        case 'B':
          if (param_count > 0) {
            line_at += params[0];
          } else {
            line_at++;
          }
          if (line_at > lines) {
            lines = line_at;
          }
          break;
        case 'C':
          if (param_count > 0) {
            col_at += params[0];
          } else {
            col_at++;
          }
          if (col_at >= width) {
            col_at = width - 1;
          }
          break;
        case 'D':
          if (param_count > 0) {
            col_at -= params[0];
          } else {
            col_at--;
          }
          if (col_at < 0)
            col_at = 0;
          break;
        case 'H':
        case 'f':
          if (param_count > 1) {
            params[0]--;
            params[1]--;
          }
          line_at = params[0];
          col_at = params[1];

          if (line_at > lines) {
            lines = line_at;
          }
          if (col_at >= width) {
            col_at = width - 1;
          }
          if (line_at < 0)
            line_at = 0;
          if (col_at < 0)
            col_at = 0;
          break;
        case 'u':
          col_at = save_col;
          line_at = save_row;
          break;
        case 's':
          save_col = col_at;
          save_row = line_at;
          break;
        case 'm':
          break;
        default:

          break;
        }
      }
    } else if (contents[i] != '\n') {
      col_at++;

      if (col_at == width && i + 1 < len) {
        col_at = 0;
        line_at++;
        if (line_at > lines) {
          lines = line_at;
        }
      }
    }
  }

  screen = (struct character **)malloc(sizeof(struct character *) * (lines + 1));

  if (!screen) {
    free(contents);
    return false;
  }

  for (int i = 0; i <= lines; i++) {
    screen[i] = (struct character *)malloc(sizeof(struct character) * width);
    if (!screen[i]) {
      for (int j = i - 1; j >= 0; j--) {
        free(screen[j]);
      }
      free(screen);
      free(contents);
      return false;
    }
    for (int x = 0; x < width; x++) {
      screen[i][x].c = ' ';
      screen[i][x].fg_colour = 7;
      screen[i][x].bg_colour = 0;
      screen[i][x].bold = false;
    }
  }
  line_at = 0;
  col_at = 0;
  save_row = 0;
  save_col = 0;
  for (size_t i = 0; i < len; i++) {
    if (contents[i] == '\r' || (i >= 1 && contents[i] == '\n' && contents[i - 1] != '\r')) {
      line_at++;
      col_at = 0;
    } else if (contents[i] == '\x1b') {
      i++;
      if (contents[i] != '[') {
        i--;
        continue;
      } else {
        param_count = 0;
        while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", contents[i]) == NULL) {
          if (contents[i] == ';') {
            param_count++;
          } else if (contents[i] >= '0' && contents[i] <= '9') {
            if (param_count == 0) {
              param_count = 1;
              for (int j = 0; j < 9; j++) {
                params[j] = 0;
              }
            }
            params[param_count - 1] = params[param_count - 1] * 10 + (contents[i] - '0');
          }
          i++;
        }
        switch (contents[i]) {
        case 'A':
          if (param_count > 0) {
            line_at -= params[0];
          } else {
            line_at--;
          }
          if (line_at < 0)
            line_at = 0;
          break;
        case 'B':
          if (param_count > 0) {
            line_at += params[0];
          } else {
            line_at++;
          }
          break;
        case 'C':
          if (param_count > 0) {
            col_at += params[0];
          } else {
            col_at++;
          }
          if (col_at >= width) {
            col_at = width - 1;
          }
          break;
        case 'D':
          if (param_count > 0) {
            col_at -= params[0];
          } else {
            col_at--;
          }
          if (col_at < 0)
            col_at = 0;
          break;
        case 'H':
        case 'f':
          if (param_count > 1) {
            params[0]--;
            params[1]--;
          }
          line_at = params[0];
          col_at = params[1];
          if (line_at < 0)
            line_at = 0;
          if (col_at < 0)
            col_at = 0;
          if (col_at >= width)
            col_at = width - 1;
          break;
        case 'm':
          for (int z = 0; z < param_count; z++) {
            if (params[z] == 0) {
              bold = false;
              fg_colour = 7;
              bg_colour = 0;
            } else if (params[z] == 1) {
              bold = true;
            } else if (params[z] == 2) {
              bold = false;
            }

            else if (params[z] >= 30 && params[z] <= 37) {
              fg_colour = params[z] - 30;
            } else if (params[z] >= 40 && params[z] <= 47) {
              bg_colour = params[z] - 40;
            }
          }
          break;
        case 'u':
          col_at = save_col;
          line_at = save_row;
          break;
        case 's':
          save_col = col_at;
          save_row = line_at;
          break;
        }
      }
    } else if (contents[i] != '\n') {
      screen[line_at][col_at].c = contents[i];
      screen[line_at][col_at].bold = bold;
      screen[line_at][col_at].fg_colour = fg_colour;
      screen[line_at][col_at].bg_colour = bg_colour;
      col_at++;
      if (col_at == width && i + 1 < len) {
        line_at++;
        col_at = 0;
      }
    }
  }

  height = lines + 1;
  free(contents);
  return true;
}

bool AnsiEditor::save(std::string filename) {
  FILE *fptr;

  int fg = 7;
  int bg = 0;
  bool bold = false;

  std::vector<std::string> lines;

  for (int y = 0; y < height; y++) {
    std::stringstream ss;
    for (int x = 0; x < width; x++) {
      if (screen[y][x].fg_colour != fg || screen[y][x].bg_colour != bg || screen[y][x].bold != bold) {
        ss << "\x1b[" << (screen[y][x].bold ? 1 : 0) << ";3" << screen[y][x].fg_colour << ";4" << screen[y][x].bg_colour << "m";
        fg = screen[y][x].fg_colour;
        bg = screen[y][x].bg_colour;
        bold = screen[y][x].bold;
      }
      ss << screen[y][x].c;
    }
    lines.push_back(ss.str());
  }

  fptr = fopen(filename.c_str(), "wb");
  if (!fptr)
    return false;

  for (size_t l = 0; l < lines.size(); l++) {
    if (width == 80) {
      fprintf(fptr, "%s", lines.at(l).c_str());
    } else {
      fprintf(fptr, "%s\r\n", lines.at(l).c_str());
    }
  }

  fclose(fptr);

  return true;
}

void AnsiEditor::edit(std::string filename) {

  if (!n->hasANSI) {
    n->print_f_nc("Sorry, you need an ANSI terminal to edit ANSI\r\n");
    n->pause();
    return;
  }

  std::vector<int> temp(10);

  temp[0] = 0xda;
  temp[1] = 0xbf;
  temp[2] = 0xc0;
  temp[3] = 0xd9;
  temp[4] = 0xc4;
  temp[5] = 0xb3;
  temp[6] = 0xc3;
  temp[7] = 0xb4;
  temp[8] = 0xc1;
  temp[9] = 0xc2;

  pallets.push_back(temp);

  temp[0] = 0xc9;
  temp[1] = 0xbb;
  temp[2] = 0xc8;
  temp[3] = 0xbc;
  temp[4] = 0xcd;
  temp[5] = 0xba;
  temp[6] = 0xcc;
  temp[7] = 0xb9;
  temp[8] = 0xca;
  temp[9] = 0xcb;

  pallets.push_back(temp);

  temp[0] = 0xd5;
  temp[1] = 0xb8;
  temp[2] = 0xd4;
  temp[3] = 0xbe;
  temp[4] = 0xcd;
  temp[5] = 0xb3;
  temp[6] = 0xc6;
  temp[7] = 0xb5;
  temp[8] = 0xcf;
  temp[9] = 0xd1;

  pallets.push_back(temp);

  temp[0] = 0xd6;
  temp[1] = 0xb7;
  temp[2] = 0xd3;
  temp[3] = 0xbd;
  temp[4] = 0xc4;
  temp[5] = 0xba;
  temp[6] = 0xc7;
  temp[7] = 0xb6;
  temp[8] = 0xd0;
  temp[9] = 0xd2;

  pallets.push_back(temp);

  temp[0] = 0xc5;
  temp[1] = 0xce;
  temp[2] = 0xd8;
  temp[3] = 0xd7;
  temp[4] = 0xe8;
  temp[5] = 0xe8;
  temp[6] = 0x9b;
  temp[7] = 0x9c;
  temp[8] = 0x99;
  temp[9] = 0xef;

  pallets.push_back(temp);

  temp[0] = 0xb0;
  temp[1] = 0xb1;
  temp[2] = 0xb2;
  temp[3] = 0xdb;
  temp[4] = 0xdf;
  temp[5] = 0xdc;
  temp[6] = 0xdd;
  temp[7] = 0xde;
  temp[8] = 0xfe;
  temp[9] = 0xfa;

  pallets.push_back(temp);

  temp[0] = 0x01;
  temp[1] = 0x02;
  temp[2] = 0x03;
  temp[3] = 0x04;
  temp[4] = 0x05;
  temp[5] = 0x06;
  temp[6] = 0xf0;
  temp[7] = 0x0e;
  temp[8] = 0x0f;
  temp[9] = 0x20;

  pallets.push_back(temp);

  temp[0] = 0x18;
  temp[1] = 0x19;
  temp[2] = 0x1e;
  temp[3] = 0x1f;
  temp[4] = 0x10;
  temp[5] = 0x11;
  temp[6] = 0x12;
  temp[7] = 0x1d;
  temp[8] = 0x14;
  temp[9] = 0x15;

  pallets.push_back(temp);

  temp[0] = 0xae;
  temp[1] = 0xaf;
  temp[2] = 0xf2;
  temp[3] = 0xf3;
  temp[4] = 0xa9;
  temp[5] = 0xaa;
  temp[6] = 0xfd;
  temp[7] = 0xf6;
  temp[8] = 0xab;
  temp[9] = 0xac;

  pallets.push_back(temp);

  temp[0] = 0xe3;
  temp[1] = 0xf1;
  temp[2] = 0xf4;
  temp[3] = 0xf5;
  temp[4] = 0xea;
  temp[5] = 0x9d;
  temp[6] = 0xe4;
  temp[7] = 0xf8;
  temp[8] = 0xfb;
  temp[9] = 0xfc;

  pallets.push_back(temp);

  temp[0] = 0xe0;
  temp[1] = 0xe1;
  temp[2] = 0xe2;
  temp[3] = 0xe5;
  temp[4] = 0xe6;
  temp[5] = 0xe7;
  temp[6] = 0xeb;
  temp[7] = 0xec;
  temp[8] = 0xed;
  temp[9] = 0xee;

  pallets.push_back(temp);

  temp[0] = 0x80;
  temp[1] = 0x87;
  temp[2] = 0xa5;
  temp[3] = 0xa4;
  temp[4] = 0x98;
  temp[5] = 0x9f;
  temp[6] = 0xf7;
  temp[7] = 0xf9;
  temp[8] = 0xad;
  temp[9] = 0xa8;

  pallets.push_back(temp);

  temp[0] = 0x83;
  temp[1] = 0x84;
  temp[2] = 0x85;
  temp[3] = 0xa0;
  temp[4] = 0xa6;
  temp[5] = 0x86;
  temp[6] = 0x8e;
  temp[7] = 0x8f;
  temp[8] = 0x91;
  temp[9] = 0x92;

  pallets.push_back(temp);

  temp[0] = 0x88;
  temp[1] = 0x89;
  temp[2] = 0x8a;
  temp[3] = 0x82;
  temp[4] = 0x90;
  temp[5] = 0x8c;
  temp[6] = 0x8b;
  temp[7] = 0x8d;
  temp[8] = 0xa1;
  temp[9] = 0x9e;

  pallets.push_back(temp);
  temp[0] = 0x93;
  temp[1] = 0x94;
  temp[2] = 0x95;
  temp[3] = 0xa2;
  temp[4] = 0xa7;
  temp[5] = 0x96;
  temp[6] = 0x81;
  temp[7] = 0x97;
  temp[8] = 0xa3;
  temp[9] = 0x9a;

  pallets.push_back(temp);

  temp[0] = 0x01;
  temp[1] = 0x02;
  temp[2] = 0x03;
  temp[3] = 0x04;
  temp[4] = 0x05;
  temp[5] = 0x06;
  temp[6] = 0xf0;
  temp[7] = 0x0e;
  temp[8] = 0x0f;
  temp[9] = 0x20;

  pallets.push_back(temp);

  if (std::filesystem::exists(filename)) {
    // TODO: load file
    if (!load(filename)) {
      return;
    }
  } else {
    // allocate screen
    screen = (struct character **)malloc(sizeof(struct character *) * height);
    if (!screen) {
      return;
    }
    for (int y = 0; y < height; y++) {
      screen[y] = (struct character *)malloc(sizeof(struct character) * width);
      if (!screen[y]) {
        for (int y2 = 0; y2 < y; y2++) {
          free(screen[y]);
        }
        free(screen);
        return;
      }
      for (int x = 0; x < width; x++) {
        screen[y][x].bg_colour = 0;
        screen[y][x].fg_colour = 7;
        screen[y][x].c = ' ';
        screen[y][x].bold = false;
      }
    }
  }
  refresh_screen();
  draw_pallet();
  n->print_f_nc("\x1b[%d;%dH", loc_y + 1, loc_x + 1);

  while (true) {
    char ch = n->getch();
    bool should_redraw = false;
    if (ch == '\x1b') {
      ch = n->getch();
      if (ch == '[') {
        ch = n->getch();
        switch (ch) {
        case 'A': // UP
          loc_y--;
          if (loc_y < 0) {
            loc_y++;
            off_y--;
            if (off_y < 0) {
              off_y++;
            } else {
              should_redraw = true;
            }
          }
          break;
        case 'B': // Down
          loc_y++;
          if (height <= (int)n->get_term_height() - 1) {
            if (loc_y >= height) {
              loc_y--;
            }
          } else if (loc_y >= (int)n->get_term_height() - 1) {
            loc_y--;
            off_y++;
            if (off_y + loc_y >= height) {
              off_y--;
            } else {
              should_redraw = true;
            }
          }
          break;
        case 'C': // right
          loc_x++;
          if (width <= (int)n->get_term_width()) {
            if (loc_x >= width) {
              loc_x--;
            }
          } else if (loc_x >= (int)n->get_term_width()) {
            loc_x--;
            off_x++;
            if (off_x + loc_x >= width) {
              off_x--;
            } else {
              should_redraw = true;
            }
          }
          break;
        case 'D': // left
          loc_x--;
          if (loc_x < 0) {
            loc_x++;
            off_x--;
            if (off_x < 0) {
              off_x++;
            } else {
              should_redraw = true;
            }
          }
          break;
        }
        if (should_redraw) {
          refresh_screen();
          draw_pallet();
        }
        n->print_f_nc("\x1b[%d;%dH%s(%3d,%3d)\x1b[%d;3%d;4%dm", n->get_term_height(), n->get_term_width() - 9, n->get_config()->get_prompt_colour(),
                      off_y + loc_y + 1, off_x + loc_x + 1, (cur_bold ? 1 : 0), cur_fg, cur_bg);

        n->print_f_nc("\x1b[%d;%dH", loc_y + 1, loc_x + 1);
      }
    } else {
      if (ch == 'z' - 'a' + 1) {
        // display tool screen
        n->print_f_nc("\x1b[%d;3H\x1b[0;30;47m+--------[MENU]---------+", n->get_term_height() / 2 - 4);
        n->print_f_nc("\x1b[%d;3H|                       |", (n->get_term_height() / 2 - 4) + 1);
        n->print_f_nc("\x1b[%d;3H| (P) Select Pallet     |", (n->get_term_height() / 2 - 4) + 2);
        n->print_f_nc("\x1b[%d;3H| (F) Select ForeGround |", (n->get_term_height() / 2 - 4) + 3);
        n->print_f_nc("\x1b[%d;3H| (B) Select Background |", (n->get_term_height() / 2 - 4) + 4);
        n->print_f_nc("\x1b[%d;3H| (S) Save Artwork      |", (n->get_term_height() / 2 - 4) + 5);
        n->print_f_nc("\x1b[%d;3H| (R) Return to Editor  |", (n->get_term_height() / 2 - 4) + 6);
        n->print_f_nc("\x1b[%d;3H| (Q) Quit Editor       |", (n->get_term_height() / 2 - 4) + 7);
        n->print_f_nc("\x1b[%d;3H+-----------------------+\x1b[0m", (n->get_term_height() / 2 - 4) + 8);

        bool done = false;

        while (!done) {

          ch = tolower(n->getch());
          switch (ch) {
          case 'p': {
            const char *choices = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            n->print_f_nc("\x1b[%d;28H\x1b[0;30;47m+----[SELECT Pallet]-----+", 1);
            n->print_f_nc("\x1b[%d;28H\x1b[0;30;47m| (0) Alpha / Numeric    |", 2);
            for (size_t i = 0; i < pallets.size(); i++) {
              n->print_f_nc("\x1b[%d;28H\x1b[0;30;47m| (%c) ", 3 + i, choices[i]);
              for (size_t j = 0; j < pallets.at(i).size(); j++) {
                n->print_f_nc("%c", pallets.at(i).at(j));
              }
              n->print_f_nc("         |");
            }
            n->print_f_nc("\x1b[%d;28H\x1b[0;30;47m+------------------------+", pallets.size() + 2);

            ch = toupper(n->getch());
            int old_pallet = cur_pallet;
            if (ch >= 'A' && ch <= 'Z') {
              cur_pallet = ch - 'A' + 10;
            } else if (ch >= '1' && ch <= '9') {
              cur_pallet = ch - '1';
            } else if (ch == '0') {
              cur_pallet = -1;
            }

            if (cur_pallet > (int)pallets.size() || cur_pallet < -1) {
              cur_pallet = old_pallet;
            }
          }
            done = true;
            break;
          case 'f': {
            n->print_f_nc("\x1b[%d;28H\x1b[0;30;47m+----[SELECT FG COLOUR]-----+", n->get_term_height() / 2 - 4);
            n->print_f_nc("\x1b[%d;28H|                           |", (n->get_term_height() / 2 - 4) + 1);
            n->print_f_nc("\x1b[%d;28H| (1) Black   (A) D Grey    |", (n->get_term_height() / 2 - 4) + 2);
            n->print_f_nc("\x1b[%d;28H| (2) Red     (B) B Red     |", (n->get_term_height() / 2 - 4) + 3);
            n->print_f_nc("\x1b[%d;28H| (3) Green   (C) B Green   |", (n->get_term_height() / 2 - 4) + 4);
            n->print_f_nc("\x1b[%d;28H| (4) Brown   (D) Yellow    |", (n->get_term_height() / 2 - 4) + 5);
            n->print_f_nc("\x1b[%d;28H| (5) Blue    (E) B Blue    |", (n->get_term_height() / 2 - 4) + 6);
            n->print_f_nc("\x1b[%d;28H| (6) Magenta (F) B Magenta |", (n->get_term_height() / 2 - 4) + 7);
            n->print_f_nc("\x1b[%d;28H| (7) Cyan    (G) B Cyan    |", (n->get_term_height() / 2 - 4) + 8);
            n->print_f_nc("\x1b[%d;28H| (8) Grey    (H) White     |", (n->get_term_height() / 2 - 4) + 9);
            n->print_f_nc("\x1b[%d;28H+---------------------------+\x1b[0m", (n->get_term_height() / 2 - 4) + 10);
            ch = tolower(n->getch());
            switch (ch) {
            case '1':
              cur_fg = 0;
              cur_bold = 0;
              break;
            case '2':
              cur_fg = 1;
              cur_bold = 0;
              break;
            case '3':
              cur_fg = 2;
              cur_bold = 0;
              break;
            case '4':
              cur_fg = 3;
              cur_bold = 0;
              break;
            case '5':
              cur_fg = 4;
              cur_bold = 0;
              break;
            case '6':
              cur_fg = 5;
              cur_bold = 0;
              break;
            case '7':
              cur_fg = 6;
              cur_bold = 0;
              break;
            case '8':
              cur_fg = 7;
              cur_bold = 0;
              break;
            case 'a':
              cur_fg = 0;
              cur_bold = 1;
              break;
            case 'b':
              cur_fg = 1;
              cur_bold = 1;
              break;
            case 'c':
              cur_fg = 2;
              cur_bold = 1;
              break;
            case 'd':
              cur_fg = 3;
              cur_bold = 1;
              break;
            case 'e':
              cur_fg = 4;
              cur_bold = 1;
              break;
            case 'f':
              cur_fg = 5;
              cur_bold = 1;
              break;
            case 'g':
              cur_fg = 6;
              cur_bold = 1;
              break;
            case 'h':
              cur_fg = 7;
              cur_bold = 1;
              break;
            }
          }
            done = true;
            break;
          case 'b': {
            n->print_f_nc("\x1b[%d;28H\x1b[0;30;47m+----[SELECT BG COLOUR]-----+", n->get_term_height() / 2 - 4);
            n->print_f_nc("\x1b[%d;28H|                           |", (n->get_term_height() / 2 - 4) + 1);
            n->print_f_nc("\x1b[%d;28H| (1) Black                 |", (n->get_term_height() / 2 - 4) + 2);
            n->print_f_nc("\x1b[%d;28H| (2) Red                   |", (n->get_term_height() / 2 - 4) + 3);
            n->print_f_nc("\x1b[%d;28H| (3) Green                 |", (n->get_term_height() / 2 - 4) + 4);
            n->print_f_nc("\x1b[%d;28H| (4) Brown                 |", (n->get_term_height() / 2 - 4) + 5);
            n->print_f_nc("\x1b[%d;28H| (5) Blue                  |", (n->get_term_height() / 2 - 4) + 6);
            n->print_f_nc("\x1b[%d;28H| (6) Magenta               |", (n->get_term_height() / 2 - 4) + 7);
            n->print_f_nc("\x1b[%d;28H| (7) Cyan                  |", (n->get_term_height() / 2 - 4) + 8);
            n->print_f_nc("\x1b[%d;28H| (8) Grey                  |", (n->get_term_height() / 2 - 4) + 9);
            n->print_f_nc("\x1b[%d;28H+---------------------------+\x1b[0m", (n->get_term_height() / 2 - 4) + 10);
            ch = n->getch();
            switch (ch) {
            case '1':
              cur_bg = 0;
              break;
            case '2':
              cur_bg = 1;
              break;
            case '3':
              cur_bg = 2;
              break;
            case '4':
              cur_bg = 3;
              break;
            case '5':
              cur_bg = 4;
              break;
            case '6':
              cur_bg = 5;
              break;
            case '7':
              cur_bg = 6;
              break;
            case '8':
              cur_bg = 7;
              break;
            }
          }
            done = true;
            break;
          case 's':
            save_warning = false;
            // TODO: save file
            save(filename);
            done = true;
            break;
          case '\x1b':
          case 'r':
            done = true;
            break;
          case 'q':
            if (save_warning) {
              while (true) {
                n->cls();
                n->print_f_nc("Save changes? (Y/N)");
                ch = tolower(n->getch());
                if (ch == 'y') {
                  // TODO: save file
                  save(filename);
                  return;
                } else if (ch == 'n') {
                  n->cls();
                  return;
                }
              }
            } else {
              n->cls();
              return;
            }
            break;
          }
        }
        refresh_screen();
        draw_pallet();
        n->print_f_nc("\x1b[%d;%dH", loc_y + 1, loc_x + 1);
      } else if (ch != '\r' && ch != '\b' && ch != 127) {
        save_warning = true;
        // place character
        if (cur_pallet == -1) {
          // place alpha/numeric character
          screen[loc_y + off_y][loc_x + off_x].c = ch;
          screen[loc_y + off_y][loc_x + off_x].bold = cur_bold;
          screen[loc_y + off_y][loc_x + off_x].fg_colour = cur_fg;
          screen[loc_y + off_y][loc_x + off_x].bg_colour = cur_bg;

          n->print_f_nc("\x1b[%d;3%d;4%dm%c", (cur_bold ? 1 : 0), cur_fg, cur_bg, ch);

          loc_x++;
          if (loc_x + off_x >= width) {
            loc_x--;
          }

          if (loc_x >= (int)n->get_term_width()) {
            loc_x--;
            off_x++;
            if (off_x + loc_x >= width) {
              off_x--;
            } else {
              refresh_screen();
              draw_pallet();
            }
          }
          n->print_f_nc("\x1b[%d;%dH%s(%3d,%3d)\x1b[%d;3%d;4%dm", n->get_term_height(), n->get_term_width() - 9, n->get_config()->get_prompt_colour(),
                        off_y + loc_y + 1, off_x + loc_x + 1, (cur_bold ? 1 : 0), cur_fg, cur_bg);
          n->print_f_nc("\x1b[%d;%dH", loc_y + 1, loc_x + 1);
        } else {
          // place pallet character
          if (ch >= '0' && ch <= '9') {
            ch = pallets.at(cur_pallet).at(ch - '0');
          }
          screen[loc_y + off_y][loc_x + off_x].c = ch;
          screen[loc_y + off_y][loc_x + off_x].bold = cur_bold;
          screen[loc_y + off_y][loc_x + off_x].fg_colour = cur_fg;
          screen[loc_y + off_y][loc_x + off_x].bg_colour = cur_bg;

          n->print_f_nc("\x1b[%d;3%d;4%dm%c", (cur_bold ? 1 : 0), cur_fg, cur_bg, ch);
          loc_x++;
          if (loc_x + off_x >= width) {
            loc_x--;
          }

          if (loc_x >= (int)n->get_term_width()) {
            loc_x--;
            off_x++;

            if (off_x + loc_x >= width) {
              off_x--;
            } else {
              refresh_screen();
              draw_pallet();
            }
          }
          n->print_f_nc("\x1b[%d;%dH%s(%3d,%3d)\x1b[%d;3%d;4%dm", n->get_term_height(), n->get_term_width() - 9, n->get_config()->get_prompt_colour(),
                        off_y + loc_y + 1, off_x + loc_x + 1, (cur_bold ? 1 : 0), cur_fg, cur_bg);

          n->print_f_nc("\x1b[%d;%dH", loc_y + 1, loc_x + 1);
        }
      }
    }
  }
}
