#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_AudioManager.h"
#include "sitter_VideoManager.h"
#include "sitter_InputManager.h"
#include "sitter_Menu.h"

#define ITEMV_INCREMENT 8
#define FRAME_DELAY 4

/******************************************************************************
 * MenuItem
 *****************************************************************************/
 
MenuItem::MenuItem(Game *game,const char *label,int icontexid,int btnid) {
  this->game=game;
  this->btnid=btnid;
  if (label) {
    if (!(this->label=strdup(label))) sitter_throw(AllocationError,"");
  } else this->label=NULL;
  this->icontexid=icontexid;
  menu=NULL;
}

MenuItem::~MenuItem() {
  if (label) free(label);
  if (icontexid>=0) game->video->unloadTexture(icontexid);
}

void MenuItem::setLabel(const char *lbl) {
  if (this->label) free(label);
  if (!lbl) { this->label=NULL; return; }
  if (!(this->label=strdup(lbl))) sitter_throw(AllocationError,"");
}

/******************************************************************************
 * text-entry item
 *****************************************************************************/
 
TextEntryMenuItem::TextEntryMenuItem(Game *game,char **dst,int btnid):MenuItem(game,NULL,-1,btnid) {
  s=dst;
  if (*s) {
    c=0; while ((*s)[c]) c++;
    a=c;
  } else {
    c=0;
    a=255;
    if (!(*s=(char*)malloc(a+1))) sitter_throw(AllocationError,"");
    (*s)[c]=0;
  }
  label=*s;
}

TextEntryMenuItem::~TextEntryMenuItem() {
  label=NULL;
}

void TextEntryMenuItem::receiveUnicode(int ch) {
  if (ch==0x08) { // backspace
    if (!c) return;
    (*s)[--c]=0;
    return;
  }
  if ((ch<0x20)||(ch>=0x7f)) return; // only accept printable ascii
  if (c>=a) {
    a+=256;
    if (!(*s=(char*)realloc(*s,a+1))) sitter_throw(AllocationError,"");
    label=*s;
  }
  (*s)[c++]=ch;
  (*s)[c]=0;
}

/******************************************************************************
 * list item
 *****************************************************************************/
 
ListMenuItem::ListMenuItem(Game *game,const char *pfx,int btnid):MenuItem(game,pfx,-1,btnid) {
  entv=NULL; entc=enta=0;
  if (!(this->pfx=strdup(pfx))) sitter_throw(AllocationError,"");
  choice=-1;
}

ListMenuItem::~ListMenuItem() {
  for (int i=0;i<entc;i++) {
    free(entv[i].lbl);
    if (entv[i].icontexid>=0) game->video->unloadTexture(entv[i].icontexid);
  }
  if (entv) free(entv);
  free(pfx);
  icontexid=-1;
}

void ListMenuItem::addChoice(const char *lbl,int iconid) {
  if (entc>=enta) {
    enta+=8;
    if (!(entv=(ListEntry*)realloc(entv,sizeof(ListEntry)*enta))) sitter_throw(AllocationError,"");
  }
  if (!(entv[entc].lbl=strdup(lbl))) sitter_throw(AllocationError,"");
  entv[entc].icontexid=iconid;
  entc++;
}

void ListMenuItem::choose(int choice) {
  if (choice<entc) this->choice=choice;
  updateLabel();
}

void ListMenuItem::updateLabel() {
  if (label) free(label);
  int pfxlen=0; while (pfx[pfxlen]) pfxlen++;
  int sfxlen=0;
  if (choice>=0) {
    while (entv[choice].lbl[sfxlen]) sfxlen++;
    icontexid=entv[choice].icontexid;
  }
  if (!(label=(char*)malloc(pfxlen+sfxlen+1))) sitter_throw(AllocationError,"");
  if (pfxlen) memcpy(label,pfx,pfxlen);
  if (sfxlen) memcpy(label+pfxlen,entv[choice].lbl,sfxlen);
  label[pfxlen+sfxlen]=0;
  if (menu) menu->pack(false);
}

void ListMenuItem::left() {
  if (!entc) return;
  game->audio->playEffect("menuchange");
  if (--choice<0) choice=entc-1;
  updateLabel();
}

void ListMenuItem::right() {
  if (!entc) return;
  game->audio->playEffect("menuchange");
  if (++choice>=entc) choice=0;
  updateLabel();
}

/******************************************************************************
 * integer menu item
 *****************************************************************************/
 
IntegerMenuItem::IntegerMenuItem(Game *game,const char *lbl,int lo,int hi,int initial,int icontexid,int btnid,int step)
:MenuItem(game,NULL,icontexid,btnid) {
  val=initial;
  this->lo=lo;
  this->hi=hi;
  if (step<1) step=1;
  this->step=step;
  if (!(pfx=strdup(lbl))) sitter_throw(AllocationError,"");
  updateLabel();
}

void IntegerMenuItem::left() {
  int sstep=step*(game->menu_fast?10:1);
  if ((val-=sstep)<lo) val=lo;
  game->audio->playEffect("menuchange");
  updateLabel();
}

void IntegerMenuItem::right() {
  int sstep=step*(game->menu_fast?10:1);
  if ((val+=sstep)>hi) val=hi;
  game->audio->playEffect("menuchange");
  updateLabel();
}

void IntegerMenuItem::updateLabel() {
  if (label) free(label);
  int pfxlen=0; while (pfx[pfxlen]) pfxlen++;
  if (!(label=(char*)malloc(pfxlen+2+11+1))) sitter_throw(AllocationError,""); // 2=": " 11=INT_MIN 1=term
  sprintf(label,"%s: %d",pfx,val);
}

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Menu::Menu(Game *game,const char *title):game(game) {
  itemv=NULL; itemc=itema=0;
  if (title) {
    if (!(this->title=strdup(title))) sitter_throw(AllocationError,"");
  } else this->title=NULL;
  bgtexid=game->video->loadTexture("menuborder.png");
  itemfonttexid=game->video->loadTexture("erinfont.png");
  itemcolor=0x000000ff;
  itemdiscolor=0x40404080;
  itemclickcolor=0xaaffaaff;
  itemchw=15;
  itemchh=20;
  itemiconw=itemiconh=16;
  itemiconpad=4;
  itemh=28;
  itemscroll=itemscrolllimit=0;
  titlefonttexid=game->video->loadTexture("erinfont.png");
  titlecolor=0x000040ff;
  titlechw=24;
  titlechh=32;
  titlepad=8;
  edgew=edgeh=16;
  scrollbarw=16;
  scrollbartexid=game->video->loadTexture("scrollbar.png",true,0);
  
  selection=0;
  maybeselect=-1;
  indicatorw=32;
  indicatorh=16;
  indicatortexidv[0]=game->video->loadTexture("indicator-0.png");
  indicatortexidv[1]=game->video->loadTexture("indicator-1.png");
  indicatortexidv[2]=game->video->loadTexture("indicator-2.png");
  indicatortexidv[3]=game->video->loadTexture("indicator-3.png");
  indicatortexidv[4]=game->video->loadTexture("indicator-2.png");
  indicatortexidv[5]=game->video->loadTexture("indicator-1.png");
  indicatortexidp=0;
  indicatortexclock=FRAME_DELAY;
  indicatortexid=indicatortexidv[indicatortexidp];
  
  w=300; h=300; // temporary guess, just in case our caller forgets to pack()
  x=(game->video->getScreenWidth()>>1)-(w>>1);
  y=(game->video->getScreenHeight()>>1)-(h>>1);
}

Menu::~Menu() {
  for (int i=0;i<6;i++) game->video->unloadTexture(indicatortexidv[i]);
  for (int i=0;i<itemc;i++) delete itemv[i];
  if (itemv) free(itemv);
  game->video->unloadTexture(bgtexid);
  game->video->unloadTexture(itemfonttexid);
  game->video->unloadTexture(titlefonttexid);
  game->video->unloadTexture(scrollbartexid);
  if (title) free(title);
}

/******************************************************************************
 * item list
 *****************************************************************************/
 
void Menu::addItem(MenuItem *item) {
  if (itemc>=itema) {
    itema+=ITEMV_INCREMENT;
    if (!(itemv=(MenuItem**)realloc(itemv,sizeof(void*)*itema))) sitter_throw(AllocationError,"");
  }
  itemv[itemc++]=item;
  itemscrolllimit+=itemh;
  item->menu=this;
}

void Menu::pack(bool reselect) {
  int minitemw=scrollbarw+50; // 50=arbitrary, could be zero
  int hlimit=(game->video->getScreenHeight()*4)/5; // 80% of screen (arbitrary)
  int wlimit=(game->video->getScreenWidth()*4)/5; // 80% of screen (arbitrary)
  w=(edgew<<1)+indicatorw+itemiconw+(itemiconpad<<1);
  h=(edgeh<<1)+titlechh+titlepad;
  int titlew=0;
  if (title) while (title[titlew]) titlew++;
  titlew*=titlechw;
  int longitemw=0;
  if (reselect) selection=-1;
  for (int i=0;i<itemc;i++) {
    int iw=0; if (itemv[i]->label) while (itemv[i]->label[iw]) iw++;
    iw*=itemchw;
    if (iw>longitemw) longitemw=iw;
    h+=itemh;
    if (itemv[i]->btnid&&(selection<0)) selection=i;
  }
  longitemw+=scrollbarw;
  if (selection<0) { w-=indicatorw; indicatorw=0; }
  if (longitemw<minitemw) longitemw=minitemw;
  if (longitemw>titlew) w+=longitemw; else w+=titlew;
  if (w>wlimit) w=wlimit;
  if (h>hlimit) h=hlimit;
  x=(game->video->getScreenWidth()>>1)-(w>>1);
  if (game->drawhighscores) y=10;
  else y=(game->video->getScreenHeight()>>1)-(h>>1);
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
void Menu::update() {
  if (--indicatortexclock<0) {
    indicatortexclock=FRAME_DELAY;
    if (++indicatortexidp>=6) indicatortexidp=0;
    indicatortexid=indicatortexidv[indicatortexidp];
  }
}

/******************************************************************************
 * event receipt
 *****************************************************************************/
  
void Menu::up_on() {
  if (selection<0) return;
  game->audio->playEffect("menumove");
  maybeselect=-1;
  int panic=selection;
  do {
    if (--selection<0) selection=itemc-1;
    if (selection==panic) return; // nothing selectable
  } while (!itemv[selection]->btnid);
  updateScroll();
}

void Menu::up_off() {
}

void Menu::down_on() {
  if (selection<0) return;
  game->audio->playEffect("menumove");
  maybeselect=-1;
  int panic=selection;
  do {
    if (++selection>=itemc) selection=0;
    if (selection==panic) return;
  } while (!itemv[selection]->btnid);
  updateScroll();
}

void Menu::down_off() {
}

void Menu::left_on() {
  if ((selection>=0)&&(selection<itemc)) itemv[selection]->left();
}

void Menu::left_off() {
}

void Menu::right_on() {
  if ((selection>=0)&&(selection<itemc)) itemv[selection]->right();
}

void Menu::right_off() {
}

void Menu::ok_on() {
  if ((selection>=0)&&(selection<itemc)) {
    if (itemv[selection]->acceptsUnicode()) return; // must either move to another item or hit RETURN
    if (itemv[selection]->btnid) {
      maybeselect=selection;
    }
  }
}

void Menu::ok_off() {
  if (selection<0) return;
  if (maybeselect==selection) {
    game->input->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,itemv[selection]->btnid));
    game->audio->playEffect("menuchoose");
  }
  maybeselect=-1;
}

void Menu::cancel_on() {
}

void Menu::cancel_off() {
}

void Menu::receiveUnicode(int ch) {
  if (selection<0) return;
  if (!itemv[selection]->acceptsUnicode()) return;
  if (ch==0x0d) game->input->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,itemv[selection]->btnid));
  else itemv[selection]->receiveUnicode(ch);
}

/******************************************************************************
 * scrolling
 *****************************************************************************/
 
void Menu::updateScroll() {
  int zh=h-(edgeh<<1)-titlechh-titlepad; // height of item zone
  int selctr=selection*itemh+(itemh>>1); // vertical center of selected zone
  itemscroll=selctr-(zh>>1);
  if (itemscroll+zh>=itemscrolllimit) itemscroll=itemscrolllimit-zh; // clamp height (which may be LESS than zh)
  if (itemscroll<0) itemscroll=0; // and, seriously, clamp to zero
}
