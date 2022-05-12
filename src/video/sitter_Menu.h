#ifndef SITTER_MENU_H
#define SITTER_MENU_H

class Game;
class Menu;

class MenuItem {
public:
  Game *game;
  char *label;
  int icontexid;
  int btnid;
  Menu *menu;
  
  // i will unload texid at deletion
  MenuItem(Game *game,const char *label=0,int icontexid=-1,int btnid=0);
  virtual ~MenuItem();
  
  void setLabel(const char *lbl);
  
  virtual bool acceptsUnicode() const { return false; }
  virtual void receiveUnicode(int ch) {}
  virtual void left() {}
  virtual void right() {}
};

class TextEntryMenuItem : public MenuItem {
  char **s; int c,a;
public:
  TextEntryMenuItem(Game *game,char **dst,int btnid=0);
  ~TextEntryMenuItem();
  bool acceptsUnicode() const { return true; }
  void receiveUnicode(int ch);
};

class ListMenuItem : public MenuItem {
  typedef struct {
    char *lbl; int icontexid;
  } ListEntry;
  ListEntry *entv; int entc,enta;
  char *pfx;
  int choice;
  int step;
public:
  ListMenuItem(Game *game,const char *pfx="",int btnid=-1);
  ~ListMenuItem();
  void addChoice(const char *lbl,int icontexid=-1);
  void choose(int index);
  int getChoice() const { return choice; }
  const char *getChoiceString() const { return entv[choice].lbl; }
  void updateLabel();
  void left();
  void right();
};

class IntegerMenuItem : public MenuItem {
  char *pfx;
  int step;
public:
  int val,lo,hi;
  IntegerMenuItem(Game *game,const char *lbl,int lo,int hi,int initial,int icontexid=-1,int btnid=0,int step=1);
  void left();
  void right();
  void updateLabel();
};

class Menu {
public:

  Game *game;
  int bgtexid;
  int x,y,w,h;
  int edgew,edgeh;
  int scrollbarw;
  int scrollbartexid;
  
  int itemfonttexid;
  uint32_t itemcolor; // normal item text color
  uint32_t itemdiscolor; // ...when no btnid
  uint32_t itemclickcolor; // ...when OK pressed, for current selection
  int itemchw,itemchh;
  int itemiconw,itemiconh,itemiconpad;
  int itemh; // itemh includes at least itemchh and at least (itemiconh+itemiconpad*2)
  int itemscroll; // pixels, not items
  int itemscrolllimit;
  
  char *title;
  int titlefonttexid;
  uint32_t titlecolor;
  int titlechw,titlechh;
  int titlepad; // extra blank space below title
  
  int selection; // index into items
  int maybeselect; // selection, when OK was pressed
  int indicatortexid;
  int indicatorw,indicatorh;
  int indicatortexidv[6]; // animation, temporary?
  int indicatortexidp;
  int indicatortexclock;
  
  MenuItem **itemv; int itemc,itema;
  
  Menu(Game *game,const char *title=0);
  ~Menu();
  
  /* I take ownership.
   */
  void addItem(MenuItem *item);
  void pack(bool reselect=true); // call when you're done adding items to finalise geometry, etc
  
  void update();
  
  void up_on();
  void up_off();
  void down_on();
  void down_off();
  void left_on();
  void left_off();
  void right_on();
  void right_off();
  void ok_on();
  void ok_off();
  void cancel_on();
  void cancel_off();
  void receiveUnicode(int ch);
  
  /* Try to put selection in the middle of the viewing area (by adjusting itemscroll).
   * Clamp itemscroll to 0..itemscrolllimit-1
   */
  void updateScroll();
  
};

#endif
