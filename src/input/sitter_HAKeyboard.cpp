#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_InputManager.h"
#include "sitter_VideoManager.h"
#include "sitter_AudioManager.h"
#include "sitter_HAKeyboard.h"

#define BGCOLOR_IDLE       0xc0c0c0ff
#define BGCOLOR_SELECT     0x4080ffff
#define FGCOLOR_IDLE       0x000000ff
#define FGCOLOR_SELECT     0x000000ff
#define LINECOLOR_IDLE     0x808080ff
#define LINECOLOR_SELECT   0x000000ff

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
HAKeyboard::HAKeyboard(Game *game):game(game) {
  keyv=NULL; keyc=keya=0;
  fonttexid=game->video->loadTexture("andyfont.png",false);
  shiftstate=SITTER_HAKEYBOARD_PLAIN;
  stickyshiftstate=-1;
  
  // TODO we should be smarter about positioning:
  Rect screenbounds=game->video->getBounds();
  bounds.w=(screenbounds.w*4)/5; // 4/5 of screen? arbitrary
  bounds.h=screenbounds.h/2; // half of screen? arbitrary
  bounds.centerx(screenbounds.x+(screenbounds.w>>1));
  bounds.bottom(screenbounds.y+screenbounds.h-50); // 50 from bottom, totally arbitrary
  
  setup_QWERTY();
  
  highlightCharacter(0x0d);
  game->input->never_remap_keys=true;
}

HAKeyboard::~HAKeyboard() {
  game->video->unloadTexture(fonttexid);
  if (keyv) free(keyv);
  game->input->never_remap_keys=false;
}

/******************************************************************************
 * events
 *****************************************************************************/
 
void HAKeyboard::update() {
}

bool HAKeyboard::checkEvent(InputEvent &evt) {
  switch (evt.type) {
    /*case SITTER_EVT_UNICODE: {
        highlightCharacter(evt.btnid);
        return false; // must not return true, passing these along is the whole point
      } break;*/
    case SITTER_EVT_BTNDOWN: switch (evt.btnid) {
        case SITTER_BTN_MN_UP: moveSelection(0,-1); return true;
        case SITTER_BTN_MN_DOWN: moveSelection(0,1); return true;
        case SITTER_BTN_MN_LEFT: moveSelection(-1,0); return true;
        case SITTER_BTN_MN_RIGHT: moveSelection(1,0); return true;
        case SITTER_BTN_MN_OK: acceptCharacter(); return true;
        case SITTER_BTN_MN_CANCEL: deleteCharacter(); return true;
      } break;
  }
  return false;
}

/******************************************************************************
 * internals
 *****************************************************************************/
 
void HAKeyboard::highlightCharacter(uint32_t ch) {
  selection=-1;
  for (int i=0;i<keyc;i++) {
    if ((keyv[i].val[shiftstate]==ch)&&(selection<0)) {
      keyv[i].fgcolor=FGCOLOR_SELECT;
      keyv[i].bgcolor=BGCOLOR_SELECT;
      keyv[i].linecolor=LINECOLOR_SELECT;
      selection=i;
    } else {
      keyv[i].fgcolor=FGCOLOR_IDLE;
      keyv[i].bgcolor=BGCOLOR_IDLE;
      keyv[i].linecolor=LINECOLOR_IDLE;
    }
  }
  if ((selection<0)&&(ch!=0x0d)) highlightCharacter(0x0d);
}

void HAKeyboard::moveSelection(int dx,int dy,bool squeak) {
  if ((selection<0)||(selection>=keyc)) return;
  if (dx&&dy) { moveSelection(dx,0,squeak); moveSelection(0,dy,false); return; }
  if (squeak) game->audio->playEffect("menuchange");
  int prev=selection;
       if (dx<0) selection=keyv[selection].next_left;
  else if (dx>0) selection=keyv[selection].next_right;
  else if (dy<0) selection=keyv[selection].next_up;
  else if (dy>0) selection=keyv[selection].next_down;
  if (selection<0) selection=prev;
  else {
    keyv[prev].fgcolor=FGCOLOR_IDLE;
    keyv[prev].bgcolor=BGCOLOR_IDLE;
    keyv[prev].linecolor=LINECOLOR_IDLE;
    keyv[selection].fgcolor=FGCOLOR_SELECT;
    keyv[selection].bgcolor=BGCOLOR_SELECT;
    keyv[selection].linecolor=LINECOLOR_SELECT;
  }
}

void HAKeyboard::acceptCharacter() {
  if ((selection<0)||(selection>=keyc)) return;
  if (keyv[selection].val[shiftstate]==0x0d) game->audio->playEffect("menuchoose");
  else if (keyv[selection].val[shiftstate]==0x08) game->audio->playEffect("menucancel");
  else game->audio->playEffect("menumove");
  switch (keyv[selection].val[shiftstate]) {
    case 0: return;
    case HAKEY_SHIFT: shiftstate=SITTER_HAKEYBOARD_SHIFT; stickyshiftstate=SITTER_HAKEYBOARD_PLAIN; return;
    case HAKEY_CAPS: shiftstate=SITTER_HAKEYBOARD_SHIFT; stickyshiftstate=-1; return;
    case HAKEY_SYMBOL: shiftstate=SITTER_HAKEYBOARD_SYMBOL; stickyshiftstate=-1; return;
    case HAKEY_PLAIN: shiftstate=SITTER_HAKEYBOARD_PLAIN; stickyshiftstate=-1; return;
    case HAKEY_PSHIFT: stickyshiftstate=shiftstate; shiftstate=SITTER_HAKEYBOARD_PLAIN; return;
    default: game->input->pushEvent(InputEvent(SITTER_EVT_UNICODE,keyv[selection].val[shiftstate],1));
  }
  if (stickyshiftstate>=0) {
    shiftstate=stickyshiftstate;
    stickyshiftstate=-1;
  }
}

void HAKeyboard::deleteCharacter() {
  game->audio->playEffect("menucancel");
  game->input->pushEvent(InputEvent(SITTER_EVT_UNICODE,0x08,1));
}

/******************************************************************************
 * setup
 *****************************************************************************/
 
void HAKeyboard::setup_QWERTY() {
  #define QWERTY_KEYC 42 // 26 letters, 10 digits, space, enter, backspace, shift, caps, symbol
  int bounds_w_tenth=bounds.w/10;
  int rowh=bounds.h/5;
  int row2shift=bounds_w_tenth/2;
  if (keya<QWERTY_KEYC) {
    keya=QWERTY_KEYC;
    if (keyv) free(keyv);
    if (!(keyv=(HAKey*)malloc(sizeof(HAKey)*keya))) sitter_throw(AllocationError,"");
  }
  keyc=QWERTY_KEYC;
  int keyi=0;
  /* digits, top row: 1..9,0 */
  const char *top_row_symbols="!@#$%^&*()"; // same as true QWERTY keyboard (US layout)
  for (int i=0;i<10;i++) {
    keyv[keyi].r.x=bounds.x+(i*bounds.w)/10;
    keyv[keyi].r.w=bounds_w_tenth;
    keyv[keyi].r.y=bounds.y;
    keyv[keyi].r.h=rowh;
    keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=((i==9)?0x30:(0x31+i));
    keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]; // must go to SYMBOL for others
    keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=top_row_symbols[i];
    keyv[keyi].next_left=(i?(keyi-1):(keyi+9));
    keyv[keyi].next_right=((i==9)?(keyi-9):(keyi+1));
    keyv[keyi].next_up=-1;
    keyv[keyi].next_down=keyi+10;
    keyi++;
  }
  /* letters, rows 1..3 */
  const char *letters="qwertyuiopasdfghjklzxcvbnm";
  const char *letter_symbols="\"'`+-/\\|[],.:;?<=>@_~{}   "; // arbitrary
  int rowi=1;
  for (int i=0;i<26;i++) {
    keyv[keyi].r.w=bounds_w_tenth;
    keyv[keyi].r.y=bounds.y+rowh*rowi;
    keyv[keyi].r.h=rowh;
    switch (rowi) {
      case 1: {
          keyv[keyi].r.x=bounds.x+(i*bounds.w)/10;
          keyv[keyi].next_up=i;
          keyv[keyi].next_down=((i==9)?(keyi+9):(keyi+10));
          keyv[keyi].next_left=(i?(keyi-1):(keyi+9));
          keyv[keyi].next_right=((i==9)?(keyi-9):(keyi+1));
          if (i==9) rowi++;
        } break;
      case 2: {
          keyv[keyi].r.x=bounds.x+row2shift+((i-10)*bounds.w)/10;
          keyv[keyi].next_up=keyi-10;
          keyv[keyi].next_down=((i>=16)?35:(keyi+9));
          keyv[keyi].next_left=((i==10)?(keyi+8):(keyi-1));
          keyv[keyi].next_right=((i==18)?(keyi-8):(keyi+1));
          if (i==18) rowi++;
        } break;
      case 3: {
          keyv[keyi].r.x=bounds.x+((i-18)*bounds.w)/10; // 18, not 19: we leave one empty space at row 3
          keyv[keyi].next_up=keyi-9;
          switch (i) {
            case 19: keyv[keyi].next_down=36; break;
            case 20: case 21: case 22: keyv[keyi].next_down=37; break;
            case 23: keyv[keyi].next_down=38; break;
            case 24: keyv[keyi].next_down=39; break;
            case 25: keyv[keyi].next_down=40; break;
          }
          keyv[keyi].next_left=((i==19)?35:(keyi-1));
          keyv[keyi].next_right=((i==25)?29:(keyi+1));
        } break;
    }
    keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=letters[i];
    keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=letters[i]-0x20;
    keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=letter_symbols[i];
    keyi++;
  }
  /* very special keys in row 4 */
  keyv[keyi].r.x=bounds.x; // backspace...
  keyv[keyi].r.y=bounds.y+rowh*4;
  keyv[keyi].r.w=bounds_w_tenth*2;
  keyv[keyi].r.h=rowh;
  keyv[keyi].next_up=29;
  keyv[keyi].next_left=41;
  keyv[keyi].next_right=37;
  keyv[keyi].next_down=-1;
  keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=0x08;
  keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=0x08;
  keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=0x08;
  keyi++;
  keyv[keyi].r.x=keyv[keyi-1].r.right(); // space...
  keyv[keyi].r.y=bounds.y+rowh*4;
  keyv[keyi].r.w=bounds_w_tenth*3;
  keyv[keyi].r.h=rowh;
  keyv[keyi].next_up=31;
  keyv[keyi].next_left=36;
  keyv[keyi].next_right=38;
  keyv[keyi].next_down=-1;
  keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=0x20;
  keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=0x20;
  keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=0x20;
  keyi++;
  keyv[keyi].r.x=keyv[keyi-1].r.right(); // shift...
  keyv[keyi].r.y=bounds.y+rowh*4;
  keyv[keyi].r.w=bounds_w_tenth;
  keyv[keyi].r.h=rowh;
  keyv[keyi].next_up=33;
  keyv[keyi].next_left=37;
  keyv[keyi].next_right=39;
  keyv[keyi].next_down=-1;
  keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=HAKEY_SHIFT;
  keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=HAKEY_PLAIN;
  keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=HAKEY_PSHIFT;
  keyi++;
  keyv[keyi].r.x=keyv[keyi-1].r.right(); // caps...
  keyv[keyi].r.y=bounds.y+rowh*4;
  keyv[keyi].r.w=bounds_w_tenth;
  keyv[keyi].r.h=rowh;
  keyv[keyi].next_up=34;
  keyv[keyi].next_left=38;
  keyv[keyi].next_right=40;
  keyv[keyi].next_down=-1;
  keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=HAKEY_CAPS;
  keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=HAKEY_PLAIN;
  keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=HAKEY_CAPS;
  keyi++;
  keyv[keyi].r.x=keyv[keyi-1].r.right(); // symbol...
  keyv[keyi].r.y=bounds.y+rowh*4;
  keyv[keyi].r.w=bounds_w_tenth;
  keyv[keyi].r.h=rowh;
  keyv[keyi].next_up=35;
  keyv[keyi].next_left=39;
  keyv[keyi].next_right=41;
  keyv[keyi].next_down=-1;
  keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=HAKEY_SYMBOL;
  keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=HAKEY_SYMBOL;
  keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=HAKEY_PLAIN;
  keyi++;
  keyv[keyi].r.x=keyv[keyi-1].r.right(); // ok...
  keyv[keyi].r.y=bounds.y+rowh*4;
  keyv[keyi].r.w=bounds_w_tenth*2;
  keyv[keyi].r.h=rowh;
  keyv[keyi].next_up=35;
  keyv[keyi].next_left=40;
  keyv[keyi].next_right=36;
  keyv[keyi].next_down=-1;
  keyv[keyi].val[SITTER_HAKEYBOARD_PLAIN]=0x0d;
  keyv[keyi].val[SITTER_HAKEYBOARD_SHIFT]=0x0d;
  keyv[keyi].val[SITTER_HAKEYBOARD_SYMBOL]=0x0d;
  keyi++;
  /* colors */
  for (int i=0;i<keyc;i++) {
    keyv[i].fgcolor=FGCOLOR_IDLE;
    keyv[i].bgcolor=BGCOLOR_IDLE;
    keyv[i].linecolor=LINECOLOR_IDLE;
  }
  #undef QWERTY_KEYC
}
