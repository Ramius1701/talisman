#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "Node.h"
#include "AnsiEditor.h"
#include "MsgArea.h"
#include "AnsiColor.h"
struct AnsiEditorTestAccess {
 static bool load(AnsiEditor &e, const std::string &f) { return e.load(f); }
 static bool save(AnsiEditor &e, const std::string &f) { return e.save(f); }
 static character **screen(AnsiEditor &e) { return e.screen; }
 static void release(AnsiEditor &e) { for(int y=0;y<e.height;++y) free(e.screen[y]); free(e.screen); }
};
static int checks = 0;
void check(bool ok, const char *name) { ++checks; if (!ok) throw std::runtime_error(name); }
std::string join(const std::vector<std::string> &v) { std::string s; for (auto &x:v) s += x; return s; }
int main() {
 try {
  using namespace AnsiColor;
  int fg=7,bg=0,color=0; bool bold=false;
  check(parse("255",color) && color==Indexed+255,"palette literal");
  check(parse("#01aBfF",color) && color==RGB+0x01abff,"RGB literal");
  for (auto s:{"256","-1","","#xyzxyz","#12345","12a"}) check(!parse(s,color),"invalid literal");
  apply({38,2,1,0,22,48,5,196},fg,bg,bold);
  check(fg==RGB+0x010016 && bg==Indexed+196 && !bold,"RGB channels consumed atomically");
  check(sgr(fg,bg,bold)=="\x1b[22;38;2;1;0;22;48;5;196m","serialize extended colors");
  apply({1,91,104,22},fg,bg,bold); check(fg==9 && bg==12 && !bold,"bright colors and intensity reset");
  apply({39,49},fg,bg,bold); check(fg==Default && bg==Default,"default colors");
  apply({0},fg,bg,bold); check(fg==7 && bg==0 && !bold,"classic reset");
  apply({38,2,255,1},fg,bg,bold); check(fg==7 && !bold,"incomplete RGB not treated as attributes");
  std::vector<int> params; size_t i=1;
  std::string seq="\x1b[38;2;1;2;3;48;2;4;5;6m";
  check(csi(seq.data(),seq.size(),i,params) && params.size()==10,"combined ten parameter RGB");
  i=1; check(!csi("\x1b[38;2",6,i,params),"truncated CSI");
  std::string many="\x1b["; for(int k=0;k<80;++k) many+="1;"; many+="m"; i=1;
  check(!csi(many.data(),many.size(),i,params),"bounded CSI parameters");
  Node node(1,0,false); node.set_term_width(80); node.set_term_height(25);
  const std::string art="\x1b[38;2;1;2;3;48;2;4;5;6mA\x1b[38;5;196;48;5;21mB\x1b[91;104mC\x1b[39;49mD";
  auto rendered=join(MsgArea::demangle_ansi(&node,art.data(),art.size()));
  check(rendered.find("38;2;1;2;3;48;2;4;5;6mA")!=std::string::npos,"message RGB and final row");
  check(rendered.find("38;5;196;48;5;21mB")!=std::string::npos,"message indexed");
  check(rendered.find("91;104mC")!=std::string::npos,"message bright background");
  check(rendered.find("39;49mD")!=std::string::npos,"message defaults");
  std::string control="\001MSGID: fixture";
  check(join(MsgArea::demangle_ansi(&node,control.data(),control.size())).front()=='\001',"message control line stays identifiable");
  std::string spaces="\x1b[48;5;21m   ";
  check(join(MsgArea::demangle_ansi(&node,spaces.data(),spaces.size())).find("48;5;21m   ")!=std::string::npos,"colored trailing spaces");
  for (auto malformed:{"\x1b","\x1b[","\x1b[38;2","\x1b[H","\x1b[2J"})
    MsgArea::demangle_ansi(&node,malformed,strlen(malformed));
  {std::ofstream f("input.ans",std::ios::binary); f<<art;}
  AnsiEditor editor(&node,80,25);
  check(AnsiEditorTestAccess::load(editor, "input.ans"),"editor load");
  check(AnsiEditorTestAccess::screen(editor)[0][0].fg_colour==RGB+0x010203 && AnsiEditorTestAccess::screen(editor)[0][0].bg_colour==RGB+0x040506,"editor RGB state");
  check(AnsiEditorTestAccess::screen(editor)[0][1].fg_colour==Indexed+196 && AnsiEditorTestAccess::screen(editor)[0][1].bg_colour==Indexed+21,"editor palette state");
  check(AnsiEditorTestAccess::save(editor, "output.ans"),"editor save");
  std::ifstream in("output.ans",std::ios::binary); std::string saved((std::istreambuf_iterator<char>(in)),{});
  check(saved.find("38;2;1;2;3;48;2;4;5;6mA")!=std::string::npos,"editor RGB roundtrip");
  check(saved.find("38;5;196;48;5;21mB")!=std::string::npos,"editor indexed roundtrip");
  AnsiEditorTestAccess::release(editor);
  check(!AnsiEditorTestAccess::load(editor, "missing.ans"),"missing artwork file");
  std::cout<<checks<<" color regression checks passed\n";
  return 0;
 } catch(const std::exception &e) { std::cerr<<e.what()<<"\n"; return 1; }
}
