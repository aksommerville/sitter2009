#if !defined(SITTER_WII)

#include <cstring>
#include <cstdlib>
#include <malloc.h>
#include "sitter_Error.h"
#include "sitter_string.h"
#include "sitter_Game.h"
#include "sitter_Configuration.h"
#include "sitter_InputManager.h"

#define MAPV_INCREMENT 32

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
InputManager::InputManager(Game *game,int evtqlen):game(game) {
  if (evtqlen<1) sitter_throw(ArgumentError,"InputManager, evtqlen=%d",evtqlen);
  promiscuous=false;
  evta=evtqlen;
  evtc=evtp=0;
  if (!(evtq=(InputEvent*)malloc(sizeof(InputEvent)*evta))) sitter_throw(AllocationError,"");
  mapv=NULL; mapc=mapa=0;
  mousex=mousey=-1;
  SDL_EnableUNICODE(1);
  never_remap_keys=false;
  
  #if SITTER_LINUX_DRM
    sitter_evdev_set_InputManager(this);
  #endif
  
  /* joysticks */
  if (SDL_Init(SDL_INIT_JOYSTICK)) {
    sitter_log("SDL_Init(SDL_INIT_JOYSTICK) failed (%s)",SDL_GetError());
    joyv=NULL; joyc=0;
  } else {
    joyc=SDL_NumJoysticks();
    if (joyc) { if (!(joyv=(SDL_Joystick**)malloc(sizeof(void*)*joyc))) sitter_throw(AllocationError,"");
    } else joyv=NULL;
    for (int i=0;i<joyc;i++) {
      joyv[i]=SDL_JoystickOpen(i);
    }
  }
}

InputManager::~InputManager() {
  free(evtq);
  for (int i=0;i<joyc;i++) if (joyv[i]) SDL_JoystickClose(joyv[i]);
  if (joyv) free(joyv);
  if (mapv) free(mapv);
}

/******************************************************************************
 * read strings
 *****************************************************************************/
 
int InputManager::stringToKeysym(const char *s) {
  if (!s||!s[0]) return 0;
  if (!s[1]) { // if it's one character, take it as a literal ASCII value (SDLK==ASCII, mostly). force letters to lowercase
    int ks=s[0];
    if ((ks>=0x41)&&(ks<=0x5a)) ks+=0x20;
    return ks;
  }
  char *tail;
  int ks=strtol(s,&tail,0);
  if (tail&&!tail[0]) return ks;
  if ((s[0]=='f')||(s[0]=='F')) { // F-keys
    ks=strtol(s+1,&tail,10);
    if (tail&&!tail[0]) { // ok, it's "F[0-9]*"
      if ((ks>=1)&&(ks<=15)) return SDLK_F1+(ks-1);
    }
  }
  if ((s[0]=='k')||(s[0]=='K')||(s[0]=='[')) { // "KP?[0-9]" == numeric keypad; SDL call it eg "[0]"
    const char *kps=s+1;
    if ((kps[0]=='p')||(kps[0]=='P')) kps++;
    if (kps[0]&&(!kps[1]||((kps[1]==']')&&!kps[2]))) {
      if ((kps[0]>=0x30)&&(kps[0]<=0x39)) return SDLK_KP0+(kps[0]-0x30);
      switch (kps[0]) { // allow some others, eg "KP." "K/"
        case '.': return SDLK_KP_PERIOD;
        case '/': return SDLK_KP_DIVIDE;
        case '*': return SDLK_KP_MULTIPLY;
        case '-': return SDLK_KP_MINUS;
        case '+': return SDLK_KP_PLUS;
        case '=': return SDLK_KP_EQUALS;
      }
    }
  }
  // It's not empty, a single character, F-key, or keypad. try string compares.
  // I'm trying to cover most ASCII names and SDLK_ names, and some common aliases (eg "dot"=='.').
  // Start with the four most likely:
  if (!strcasecmp(s,"up")) return SDLK_UP;
  if (!strcasecmp(s,"down")) return SDLK_DOWN;
  if (!strcasecmp(s,"left")) return SDLK_LEFT;
  if (!strcasecmp(s,"right")) return SDLK_RIGHT;
  // everything else is unprioritised:
  if (!strcasecmp(s,"backspace")||!strcasecmp(s,"bs")) return SDLK_BACKSPACE;
  if (!strcasecmp(s,"tab")||!strcasecmp(s,"ht")) return SDLK_TAB;
  if (!strcasecmp(s,"clear")||!strcasecmp(s,"ff")) return SDLK_CLEAR; // clear==form-feed? don't ask me...
  if (!strcasecmp(s,"return")||!strcasecmp(s,"cr")) return SDLK_RETURN;
  if (!strcasecmp(s,"pause")||!strcasecmp(s,"dc3")) return SDLK_PAUSE;
  if (!strcasecmp(s,"escape")||!strcasecmp(s,"esc")) return SDLK_ESCAPE;
  if (!strcasecmp(s,"space")||!strcasecmp(s,"sp")) return SDLK_SPACE;
  if (!strcasecmp(s,"quote")||!strcasecmp(s,"tick")||!strcasecmp(s,"apostrophe")) return SDLK_QUOTE;
  if (!strcasecmp(s,"minus")||!strcasecmp(s,"dash")) return SDLK_MINUS;
  if (!strcasecmp(s,"period")||!strcasecmp(s,"dot")) return SDLK_PERIOD;
  if (!strcasecmp(s,"slash")) return SDLK_SLASH;
  if (!strcasecmp(s,"semicolon")) return SDLK_SEMICOLON;
  if (!strcasecmp(s,"equals")) return SDLK_EQUALS;
  if (!strcasecmp(s,"leftbracket")) return SDLK_LEFTBRACKET;
  if (!strcasecmp(s,"rightbracket")) return SDLK_RIGHTBRACKET;
  if (!strcasecmp(s,"backslash")) return SDLK_BACKSLASH;
  if (!strcasecmp(s,"backquote")||!strcasecmp(s,"grave")) return SDLK_BACKQUOTE; // "backquote"? i never quite got that...
  if (!strcasecmp(s,"delete")||!strcasecmp(s,"del")) return SDLK_DELETE;
  if (!strcasecmp(s,"enter")) return SDLK_KP_ENTER;
  if (!strcasecmp(s,"home")) return SDLK_HOME;
  if (!strcasecmp(s,"insert")) return SDLK_INSERT;
  if (!strcasecmp(s,"pageup")) return SDLK_PAGEUP;
  if (!strcasecmp(s,"pagedown")) return SDLK_PAGEDOWN;
  if (!strcasecmp(s,"end")) return SDLK_END;
  if (!strcasecmp(s,"numlock")) return SDLK_NUMLOCK;
  if (!strcasecmp(s,"capslock")) return SDLK_CAPSLOCK;
  if (!strcasecmp(s,"scrolllock")) return SDLK_SCROLLOCK; // note that our "scroll lock" has 3 Ls
  if (!strcasecmp(s,"rshift")) return SDLK_RSHIFT;
  if (!strcasecmp(s,"lshift")) return SDLK_LSHIFT;
  if (!strcasecmp(s,"rctrl")) return SDLK_RCTRL;
  if (!strcasecmp(s,"lctrl")) return SDLK_LCTRL;
  if (!strcasecmp(s,"ralt")) return SDLK_RALT;
  if (!strcasecmp(s,"lalt")) return SDLK_LALT;
  if (!strcasecmp(s,"rmeta")) return SDLK_RMETA;
  if (!strcasecmp(s,"lmeta")) return SDLK_LMETA;
  if (!strcasecmp(s,"lsuper")) return SDLK_LSUPER;
  if (!strcasecmp(s,"rsuper")) return SDLK_RSUPER;
  if (!strcasecmp(s,"mode")||!strcasecmp(s,"altgr")) return SDLK_MODE;
  if (!strcasecmp(s,"compose")) return SDLK_COMPOSE;
  if (!strcasecmp(s,"help")) return SDLK_HELP;
  if (!strcasecmp(s,"print")) return SDLK_PRINT;
  if (!strcasecmp(s,"sysreq")) return SDLK_SYSREQ;
  if (!strcasecmp(s,"break")) return SDLK_BREAK;
  if (!strcasecmp(s,"menu")) return SDLK_MENU;
  if (!strcasecmp(s,"power")) return SDLK_POWER;
  if (!strcasecmp(s,"euro")) return SDLK_EURO;
  if (!strcasecmp(s,"undo")) return SDLK_UNDO;
  return 0;
}
  
bool InputManager::stringToJoyctl(const char *s,int *type,int *index) {
  if (!s||!s[0]) return false;
  char *tail=0;
  *index=strtol(s,&tail,0); // no prefix == button
  if (tail&&!tail[0]) { *type=SDL_JOYBUTTONDOWN; return true; }
  switch (s[0]) {
    case 'A': // 'a' == axis
    case 'a': { *index=strtol(s+1,&tail,0); if (tail&&!tail[0]) { *type=SDL_JOYAXISMOTION; return true; } } break;
    case 'H': // 'h' == hat
    case 'h': { *index=strtol(s+1,&tail,0); if (tail&&!tail[0]) { *type=SDL_JOYHATMOTION; return true; } } break;
    case 'B': // 'b' == ball
    case 'b': switch (s[1]) {
        case 'X':
        case 'x': { *index=strtol(s+2,&tail,0); if (tail&&!tail[0]) { *type=SDL_JOYBALLMOTION; (*index)<<=1; return true; } } break;
        case 'Y':
        case 'y': { *index=strtol(s+2,&tail,0); if (tail&&!tail[0]) { *type=SDL_JOYBALLMOTION; (*index)<<=1; (*index)|=1; return true; } } break;
      }
  }
  return false;
}

int InputManager::stringToBtnid(const char *s,int nextplr) {
  char *tail=0;
  int btnid=strtol(s,&tail,0);
  if (tail&&!tail[0]) return btnid;
  if ((s[0]=='p')||(s[0]=='P')) { // player control?
    int plrid=-1;
    if ((s[1]=='x')||(s[1]=='X')) plrid=nextplr;
    else if ((s[1]>='0')&&(s[1]<='9')) plrid=s[1]-'0';
    if (plrid>=0) {
      plrid<<=12;
      if (!strcasecmp(s+2,"left")) return plrid|SITTER_BTN_PLR_LEFT;
      if (!strcasecmp(s+2,"right")) return plrid|SITTER_BTN_PLR_RIGHT;
      if (!strcasecmp(s+2,"jump")) return plrid|SITTER_BTN_PLR_JUMP;
      if (!strcasecmp(s+2,"duck")) return plrid|SITTER_BTN_PLR_DUCK;
      if (!strcasecmp(s+2,"pickup")) return plrid|SITTER_BTN_PLR_PICKUP;
      if (!strcasecmp(s+2,"toss")) return plrid|SITTER_BTN_PLR_TOSS;
      if (!strcasecmp(s+2,"focus")) return plrid|SITTER_BTN_PLR_ATN;
      if (!strcasecmp(s+2,"radar")) return plrid|SITTER_BTN_PLR_RADAR;
      sitter_throw(ArgumentError,"invalid button name '%s'",s);
    }
  }
  if (!strcasecmp(s,"quit")) return SITTER_BTN_QUIT;
  if (!strcasecmp(s,"menu")) return SITTER_BTN_MENU;
  if (!strcasecmp(s,"restart")) return SITTER_BTN_RESTARTLVL;
  if (!strcasecmp(s,"next")) return SITTER_BTN_NEXTLVL;
  if (((s[0]=='m')||(s[0]=='M'))&&((s[1]=='n')||(s[1]=='N'))) { // "mn.*" == menu
    if (!strcasecmp(s+2,"up")) return SITTER_BTN_MN_UP;
    if (!strcasecmp(s+2,"down")) return SITTER_BTN_MN_DOWN;
    if (!strcasecmp(s+2,"left")) return SITTER_BTN_MN_LEFT;
    if (!strcasecmp(s+2,"right")) return SITTER_BTN_MN_RIGHT;
    if (!strcasecmp(s+2,"ok")) return SITTER_BTN_MN_OK;
    if (!strcasecmp(s+2,"cancel")) return SITTER_BTN_MN_CANCEL;
    if (!strcasecmp(s+2,"fast")) return SITTER_BTN_MN_FAST;
  }
  sitter_throw(ArgumentError,"invalid button name '%s'",s);
}

/******************************************************************************
 * id to string
 *****************************************************************************/
 
static char btnidToString_buffer[32];
static char mappingSourceToString_buffer[32];
  
const char *InputManager::btnidToString(int btnid) const {
  switch (btnid) {
    case SITTER_BTN_QUIT: return "quit";
    case SITTER_BTN_MN_UP: return "mnup";
    case SITTER_BTN_MN_DOWN: return "mndown";
    case SITTER_BTN_MN_LEFT: return "mnleft";
    case SITTER_BTN_MN_RIGHT: return "mnright";
    case SITTER_BTN_MN_OK: return "mnok";
    case SITTER_BTN_MN_CANCEL: return "mncancel";
    case SITTER_BTN_MENU: return "menu";
    case SITTER_BTN_RESTARTLVL: return "restart";
    case SITTER_BTN_NEXTLVL: return "next";
    case SITTER_BTN_MN_FAST: return "mnfast";
    default: {
        if ((btnid&0xffff0000)==0x00120000) { // player...
          btnidToString_buffer[0]='p';
          if (!(btnid&0x0000f000)) { // generic
            btnidToString_buffer[1]='x';
          } else if ((btnid&0x0000f000)<0x00005000) { // numbered player
            btnidToString_buffer[1]=((btnid>>12)&0xf)+0x30;
          } else break;
          switch (btnid&0xffff0fff) {
            case SITTER_BTN_PLR_LEFT: memcpy(btnidToString_buffer+2,"left",5); return btnidToString_buffer;
            case SITTER_BTN_PLR_RIGHT: memcpy(btnidToString_buffer+2,"right",6); return btnidToString_buffer;
            case SITTER_BTN_PLR_JUMP: memcpy(btnidToString_buffer+2,"jump",5); return btnidToString_buffer;
            case SITTER_BTN_PLR_DUCK: memcpy(btnidToString_buffer+2,"duck",5); return btnidToString_buffer;
            case SITTER_BTN_PLR_PICKUP: memcpy(btnidToString_buffer+2,"pickup",7); return btnidToString_buffer;
            case SITTER_BTN_PLR_TOSS: memcpy(btnidToString_buffer+2,"toss",5); return btnidToString_buffer;
            case SITTER_BTN_PLR_ATN: memcpy(btnidToString_buffer+2,"focus",6); return btnidToString_buffer;
            case SITTER_BTN_PLR_RADAR: memcpy(btnidToString_buffer+2,"radar",6); return btnidToString_buffer;
          }
        }
      }
  }
  sprintf(btnidToString_buffer,"0x%x",btnid);
  return btnidToString_buffer;
}

const char *InputManager::mappingSourceToString(int mapid) const {
  if ((mapid<0)||(mapid>=mapc)) {
    wheres_the_beef:
    mappingSourceToString_buffer[0]='0';
    mappingSourceToString_buffer[1]=0;
    return mappingSourceToString_buffer;
  }
  switch (mapv[mapid].src.type) {
    case SDL_KEYDOWN: {
        switch (mapv[mapid].src.key.keysym.sym) { // some keys, we don't let SDL describe
          case SDLK_LSHIFT: return "lshift";
          case SDLK_RSHIFT: return "rshift";
          case SDLK_LALT: return "lalt";
          case SDLK_RALT: return "ralt";
          case SDLK_LCTRL: return "lctrl";
          case SDLK_RCTRL: return "rctrl";
          case SDLK_LSUPER: return "lsuper";
          case SDLK_RSUPER: return "rsuper";
          case SDLK_LMETA: return "lmeta";
          case SDLK_RMETA: return "rmeta";
        }
        const char *kname=SDL_GetKeyName(mapv[mapid].src.key.keysym.sym);
        if (!kname) goto wheres_the_beef;
        return kname;
      } break;
    case SDL_JOYBUTTONDOWN: {
        snprintf(mappingSourceToString_buffer,32,"joy %d, btn %d",mapv[mapid].src.jbutton.which,mapv[mapid].src.jbutton.button);
        mappingSourceToString_buffer[31]=0;
        return mappingSourceToString_buffer;
      }
    case SDL_JOYAXISMOTION: {
        snprintf(mappingSourceToString_buffer,32,"joy %d, axis %d",mapv[mapid].src.jaxis.which,mapv[mapid].src.jaxis.axis);
        mappingSourceToString_buffer[31]=0;
        return mappingSourceToString_buffer;
      }
    // TODO mouse,hat,ball. like we're ever going to use them...
  }
  goto wheres_the_beef;
}

/******************************************************************************
 * read configuration
 *****************************************************************************/
 
void InputManager::reconfigure() {
  mapc=0;
  if (!game||!game->cfg) return;
  
  /* keyboard mapping */
  if (const char *keymap=game->cfg->getOption_str("keymap")) {
    char **mapdescv=sitter_strsplit_selfDescribing(keymap);
    for (char **mapdesci=mapdescv;*mapdesci;mapdesci++) {
      char **descelemv=sitter_strsplit_white(*mapdesci);
      int descelemc=0; while (descelemv[descelemc]) descelemc++;
      if ((descelemc<2)||((descelemc>4)&&(descelemc!=6)))
        sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
      int src_keysym=stringToKeysym(descelemv[0]);
      if (!src_keysym) sitter_throw(ArgumentError,"invalid key name '%s'",descelemv[0]);
      SDL_Event srcevt;
      srcevt.type=SDL_KEYDOWN;
      srcevt.key.keysym.sym=(SDLKey)src_keysym;
      switch (descelemc) {
        case 2: { // 2 elements, direct mapping
            int btnid=stringToBtnid(descelemv[1]);
            if (((btnid&0xffff0000)==0x00120000)&&!(btnid&0x0000f000)) btnid|=0x00001000;
            mapButtonToButton(&srcevt,btnid);
          } break;
        case 3: { // 3 elements, reverse mapping. elem[1]=="!"
            if (!strcmp(descelemv[1],"!")) sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            int btnid=stringToBtnid(descelemv[2]);
            if (((btnid&0xffff0000)==0x00120000)&&!(btnid&0x0000f000)) btnid|=0x00001000;
            mapButtonToButton(&srcevt,btnid,1,true);
          } break;
        // the 4 and 6 possibilities don't really make sense for a keyboard, but... ok:
        case 4: { // 4 elements, button map with threshhold, elem[1]==("<",">")
            if (!descelemv[1][0]||descelemv[1][1]||((descelemv[1][0]!='<')&&(descelemv[1][0]!='>')))
              sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            char *tail=0; int thresh=strtol(descelemv[2],&tail,0);
            if (!tail||tail[0]) sitter_throw(ArgumentError,"invalid threshhold '%s'",descelemv[2]);
            int btnid=stringToBtnid(descelemv[3]);
            if (((btnid&0xffff0000)==0x00120000)&&!(btnid&0x0000f000)) btnid|=0x00001000;
            if (descelemv[1][0]=='>') mapButtonToButton(&srcevt,btnid,thresh);
            else mapButtonToButton(&srcevt,btnid,thresh,true);
          } break;
        case 6: { // 6 elements, src threshlo threshhi btnlo btnmid btnhi
            char *tail=0; 
            int threshlo=strtol(descelemv[1],&tail,0);
            if (!tail||tail[0]) sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            int threshhi=strtol(descelemv[2],&tail,0);
            if (!tail||tail[0]) sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            int btnlo=stringToBtnid(descelemv[3]);
            int btnmid=stringToBtnid(descelemv[4]);
            int btnhi=stringToBtnid(descelemv[5]);
            if (((btnlo&0xffff0000)==0x00120000)&&!(btnlo&0x0000f000)) btnlo|=0x00001000;
            if (((btnmid&0xffff0000)==0x00120000)&&!(btnmid&0x0000f000)) btnlo|=0x00001000;
            if (((btnhi&0xffff0000)==0x00120000)&&!(btnhi&0x0000f000)) btnlo|=0x00001000;
            mapAxisToButtons(&srcevt,threshlo,threshhi,btnlo,btnmid,btnhi);
          } break;
      }
      for (int i=0;i<descelemc;i++) free(descelemv[i]);
      free(descelemv);
      free(*mapdesci);
    }
    free(mapdescv);
  }
  
  /* joystick mapping */
  int nextplr=1;
  for (int joyi=0;joyi<joyc;joyi++) {
    if (joyi>=10) break; // could easily support more, change limit in mapname below. but who has 11 joysticks?
    if (!joyv[joyi]) continue;
    #define MAPNAMELIMIT 256
    char mapname[MAPNAMELIMIT];
    const char *joymap=0;
    /* device-name map */
    if (snprintf(mapname,MAPNAMELIMIT,"joymap-%s",SDL_JoystickName(joyi))<MAPNAMELIMIT) {
      joymap=game->cfg->getOption_str(mapname);
    }
    /* device-number map */
    if (!joymap||!joymap[0]) {
      memcpy(mapname,"joymap",6);
      mapname[6]=joyi+0x30;
      mapname[7]=0;
      joymap=game->cfg->getOption_str(mapname);
    }
    /* generic joystick map */
    if (!joymap||!joymap[0]) {
      joymap=game->cfg->getOption_str("joymap");
    }
    if (!joymap||!joymap[0]) continue;
    bool usedgeneric=false;
    
    char **mapdescv=sitter_strsplit_selfDescribing(joymap);
    for (char **mapdesci=mapdescv;*mapdesci;mapdesci++) {
      char **descelemv=sitter_strsplit_white(*mapdesci);
      int descelemc=0; while (descelemv[descelemc]) descelemc++;
      if ((descelemc<2)||((descelemc>4)&&(descelemc!=6)))
        sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
      int ctltype,ctlindex;
      if (!stringToJoyctl(descelemv[0],&ctltype,&ctlindex))
        sitter_throw(ArgumentError,"invalid joystick button name '%s'",descelemv[0]);
      SDL_Event srcevt;
      char axis='x';
      switch (srcevt.type=ctltype) {
        case SDL_JOYBUTTONDOWN: srcevt.jbutton.which=joyi; srcevt.jbutton.button=ctlindex; break;
        case SDL_JOYAXISMOTION: srcevt.jaxis.which=joyi; srcevt.jaxis.axis=ctlindex; break;
        case SDL_JOYHATMOTION: srcevt.jhat.which=joyi; srcevt.jhat.hat=ctlindex; break;
        case SDL_JOYBALLMOTION: srcevt.jball.which=joyi; srcevt.jball.ball=ctlindex>>1; if (ctlindex&1) axis='y'; break;
        default: sitter_throw(IdiotProgrammerError,"stringToJoyctl returned %d in ctltype, expected SDL event",ctltype);
      }
      switch (descelemc) {
        case 2: { // 2 elements, direct mapping
            int btnid=stringToBtnid(descelemv[1],nextplr);
            if (((btnid&0xffff0000)==0x00120000)&&!(btnid&0x0000f000)) { usedgeneric=true; btnid|=(nextplr<<12); }
            if ((descelemv[1][0]=='p')||(descelemv[1][0]=='P')) {
              if ((descelemv[1][1]=='x')||(descelemv[1][1]=='X')) usedgeneric=true;
              else while (descelemv[1][1]-0x30>nextplr) { nextplr++; usedgeneric=true; }
            }
            mapButtonToButton(&srcevt,btnid);
          } break;
        case 3: { // 3 elements, reverse mapping. elem[1]=="!"
            if (!strcmp(descelemv[1],"!")) sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            int btnid=stringToBtnid(descelemv[2],nextplr);
            if (((btnid&0xffff0000)==0x00120000)&&!(btnid&0x0000f000)) { usedgeneric=true; btnid|=(nextplr<<12); }
            if ((descelemv[2][0]=='p')||(descelemv[2][0]=='P')) {
              if ((descelemv[2][1]=='x')||(descelemv[2][1]=='X')) usedgeneric=true;
              else while (descelemv[2][1]-0x30>nextplr) { nextplr++; usedgeneric=true; }
            }
            mapButtonToButton(&srcevt,btnid,1,true);
          } break;
        case 4: { // 4 elements, button map with threshhold, elem[1]==("<",">")
            if (!descelemv[1][0]||descelemv[1][1]||((descelemv[1][0]!='<')&&(descelemv[1][0]!='>')))
              sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            char *tail=0; int thresh=strtol(descelemv[2],&tail,0);
            if (!tail||tail[0]) sitter_throw(ArgumentError,"invalid threshhold '%s'",descelemv[2]);
            int btnid=stringToBtnid(descelemv[3],nextplr);
            if (((btnid&0xffff0000)==0x00120000)&&!(btnid&0x0000f000)) { usedgeneric=true; btnid|=(nextplr<<12); }
            if ((descelemv[3][0]=='p')||(descelemv[3][0]=='P')) {
              if ((descelemv[3][1]=='x')||(descelemv[3][1]=='X')) usedgeneric=true;
              else while (descelemv[3][1]-0x30>nextplr) { nextplr++; usedgeneric=true; }
            }
            if (descelemv[1][0]=='>') mapButtonToButton(&srcevt,btnid,thresh);
            else mapButtonToButton(&srcevt,btnid,thresh,true);
          } break;
        case 6: { // 6 elements, src threshlo threshhi btnlo btnmid btnhi
            char *tail=0; 
            int threshlo=strtol(descelemv[1],&tail,0);
            if (!tail||tail[0]) sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            int threshhi=strtol(descelemv[2],&tail,0);
            if (!tail||tail[0]) sitter_throw(ArgumentError,"invalid input mapping '%s'",*mapdesci);
            int btnlo=stringToBtnid(descelemv[3],nextplr);
            int btnmid=stringToBtnid(descelemv[4],nextplr);
            int btnhi=stringToBtnid(descelemv[5],nextplr);
            if (((btnlo&0xffff0000)==0x00120000)&&!(btnlo&0x0000f000)) { usedgeneric=true; btnlo|=(nextplr<<12); }
            if (((btnmid&0xffff0000)==0x00120000)&&!(btnmid&0x0000f000)) { usedgeneric=true; btnmid|=(nextplr<<12); }
            if (((btnhi&0xffff0000)==0x00120000)&&!(btnhi&0x0000f000)) { usedgeneric=true; btnhi|=(nextplr<<12); }
            if ((descelemv[3][0]=='p')||(descelemv[3][0]=='P')) {
              if ((descelemv[3][1]=='x')||(descelemv[3][1]=='X')) usedgeneric=true;
              else while (descelemv[3][1]-0x30>nextplr) { nextplr++; usedgeneric=true; }
            }
            if ((descelemv[4][0]=='p')||(descelemv[4][0]=='P')) {
              if ((descelemv[4][1]=='x')||(descelemv[4][1]=='X')) usedgeneric=true;
              else while (descelemv[4][1]-0x30>nextplr) { nextplr++; usedgeneric=true; }
            }
            if ((descelemv[5][0]=='p')||(descelemv[5][0]=='P')) {
              if ((descelemv[5][1]=='x')||(descelemv[5][1]=='X')) usedgeneric=true;
              else while (descelemv[5][1]-0x30>nextplr) { nextplr++; usedgeneric=true; }
            }
            mapAxisToButtons(&srcevt,threshlo,threshhi,btnlo,btnmid,btnhi);
          } break;
      }
      for (int i=0;i<descelemc;i++) free(descelemv[i]);
      free(descelemv);
      free(*mapdesci);
    }
    free(mapdescv);
    if (usedgeneric) nextplr++;
  }
  
}

/******************************************************************************
 * write config
 *****************************************************************************/
 
class TrivialDictionary {
public:
  typedef struct {
    char *k,*v; int vc,va;
  } Entry;
  Entry *entv; int entc,enta;

  TrivialDictionary() {
    entv=NULL; entc=enta=0;
  }
  
  ~TrivialDictionary() {
    for (int i=0;i<entc;i++) {
      if (entv[i].k) free(entv[i].k);
      if (entv[i].v) free(entv[i].v);
    }
    if (entv) free(entv);
  }
  
  const char *lookup(const char *k) {
    for (int i=0;i<entc;i++) if (entv[i].k&&!strcmp(k,entv[i].k)) {
      if (entv[i].v) return entv[i].v;
      return "";
    }
    return "";
  }
  
  void append(const char *k,const char *v) {
    int ei=-1;
    for (int i=0;i<entc;i++) if (entv[i].k&&!strcmp(k,entv[i].k)) { ei=i; break; }
    if (ei<0) {
      if (entc>=enta) {
        enta+=16;
        if (!(entv=(Entry*)realloc(entv,sizeof(Entry)*enta))) sitter_throw(AllocationError,"");
      }
      ei=entc++;
      if (!(entv[ei].k=strdup(k))) sitter_throw(AllocationError,"");
      if (!(entv[ei].v=(char*)malloc(256))) sitter_throw(AllocationError,"");
      entv[ei].vc=0;
      entv[ei].va=255;
      entv[ei].v[0]=0;
    }
    int nvlen=0; while (v[nvlen]) nvlen++;
    if (entv[ei].vc+nvlen>entv[ei].va) {
      entv[ei].va=(entv[ei].vc+nvlen+256);
      if (!(entv[ei].v=(char*)realloc(entv[ei].v,entv[ei].va+1))) sitter_throw(AllocationError,"");
    }
    memcpy(entv[ei].v+entv[ei].vc,v,nvlen);
    entv[ei].vc+=nvlen;
    entv[ei].v[entv[ei].vc]=0;
  }
  
};
 
void InputManager::writeConfiguration() {
  TrivialDictionary d;
  char **joyname=(char**)malloc(sizeof(void*)*joyc);
  if (!joyname) sitter_throw(AllocationError,"");
  for (int i=0;i<joyc;i++) {
    const char *name=SDL_JoystickName(i);
    if (!name) name="";
    if (!(joyname[i]=strdup(name))) sitter_throw(AllocationError,"");
  }
  
  #define KBUF_LEN 1024
  #define VBUF_LEN 1024
  char kbuf[KBUF_LEN];
  char vbuf[VBUF_LEN];
  for (int i=0;i<mapc;i++) {
    int vc;
    /* determine config key */
    switch (mapv[i].src.type) {
      case SDL_KEYDOWN: memcpy(kbuf,"keymap",7); vc=sprintf(vbuf,":%s ",mappingSourceToString(i)); break; //vc=sprintf(vbuf,":%d ",mapv[i].src.key.keysym.sym); break;
      case SDL_MOUSEBUTTONDOWN: continue; // TODO
      case SDL_JOYBUTTONDOWN: {
          if (!joyname[mapv[i].src.jbutton.which][0]) sprintf(kbuf,"joymap%d",mapv[i].src.jbutton.which);
          else if (snprintf(kbuf,KBUF_LEN,"joymap-%s",joyname[mapv[i].src.jbutton.which])>=KBUF_LEN) 
            sitter_throw(SDLError,"choked on ridiculously long joystick name");
          vc=sprintf(vbuf,":%d ",mapv[i].src.jbutton.button);
        } break;
      case SDL_JOYAXISMOTION: {
          if (!joyname[mapv[i].src.jaxis.which][0]) sprintf(kbuf,"joymap%d",mapv[i].src.jaxis.which);
          else if (snprintf(kbuf,KBUF_LEN,"joymap-%s",joyname[mapv[i].src.jaxis.which])>=KBUF_LEN) 
            sitter_throw(SDLError,"choked on ridiculously long joystick name");
          vc=sprintf(vbuf,":a%d ",mapv[i].src.jaxis.axis);
        } break;
      case SDL_JOYHATMOTION: {
          if (!joyname[mapv[i].src.jhat.which][0]) sprintf(kbuf,"joymap%d",mapv[i].src.jhat.which);
          else if (snprintf(kbuf,KBUF_LEN,"joymap-%s",joyname[mapv[i].src.jhat.which])>=KBUF_LEN) 
            sitter_throw(SDLError,"choked on ridiculously long joystick name");
          vc=sprintf(vbuf,":h%d ",mapv[i].src.jhat.hat);
        } break;
      case SDL_JOYBALLMOTION: {
          if (!joyname[mapv[i].src.jball.which][0]) sprintf(kbuf,"joymap%d",mapv[i].src.jball.which);
          else if (snprintf(kbuf,KBUF_LEN,"joymap-%s",joyname[mapv[i].src.jball.which])>=KBUF_LEN) 
            sitter_throw(SDLError,"choked on ridiculously long joystick name");
          vc=sprintf(vbuf,":b%c%d ",mapv[i].srcaxis,mapv[i].src.jball.ball);
        } break;
      default: sitter_log("not saving input mapping, SDL event type = %d",mapv[i].src.type); continue;
    }
    switch (mapv[i].dsttype) {
      case SITTER_BTNMAP_BUTTON: {
          if (mapv[i].btn.thresh==1) { sprintf(vbuf+vc,"%s",btnidToString(mapv[i].btn.btnid)); }
          else sprintf(vbuf+vc,"> %d %s",mapv[i].btn.thresh,btnidToString(mapv[i].btn.btnid));
        } break;
      case SITTER_BTNMAP_REVBUTTON: {
          sprintf(vbuf+vc,"< %d %s",mapv[i].btn.thresh,btnidToString(mapv[i].btn.btnid));
        } break;
      case SITTER_BTNMAP_AXIS: {
          vc+=sprintf(vbuf+vc,"%d %d %s ",mapv[i].axis.thresh_lo,mapv[i].axis.thresh_hi,btnidToString(mapv[i].axis.btnid_lo));
          vc+=sprintf(vbuf+vc,"%s ",btnidToString(mapv[i].axis.btnid_mid));
          vc+=sprintf(vbuf+vc,"%s",btnidToString(mapv[i].axis.btnid_hi));
        } break;
    }
    d.append(kbuf,vbuf);
  }
  game->cfg->removeInputOptions();
  for (int i=0;i<d.entc;i++) game->cfg->addOption(d.entv[i].k,"",SITTER_CFGTYPE_STR,d.entv[i].v);
  for (int i=0;i<joyc;i++) free(joyname[i]);
  free(joyname);
  #undef KBUF_LEN
  #undef VBUF_LEN
}

void InputManager::generaliseMappings() {
  uint32_t samejoysticktype[32]={0};
  for (int a=1;a<joyc;a++) {
    if (a>=32) break;
    const char *aname=SDL_JoystickName(a);
    if (!aname||!aname[0]) continue;
    for (int b=0;b<a;b++) {
      const char *bname=SDL_JoystickName(b);
      if (!bname||!bname[0]) continue;
      if (!strcmp(aname,bname)) { samejoysticktype[a]|=(1<<b); samejoysticktype[b]|=(1<<a); }
    }
  }
  #define SAMEJOYSTICKTYPE(a,b) (((a>=32)||(b>=32))?0:(samejoysticktype[a]&(1<<b)))
  
  for (int i=0;i<mapc;) {
    switch (mapv[i].src.type) {
      case SDL_JOYBUTTONDOWN: {
          if (mapv[i].dsttype==SITTER_BTNMAP_AXIS) {
            mapv[i].axis.btnid_lo&=0xffff0fff;
            mapv[i].axis.btnid_mid&=0xffff0fff;
            mapv[i].axis.btnid_hi&=0xffff0fff;
          } else mapv[i].btn.btnid&=0xffff0fff;
        } break;
      case SDL_JOYAXISMOTION: {
          if (mapv[i].dsttype==SITTER_BTNMAP_AXIS) {
            mapv[i].axis.btnid_lo&=0xffff0fff;
            mapv[i].axis.btnid_mid&=0xffff0fff;
            mapv[i].axis.btnid_hi&=0xffff0fff;
          } else mapv[i].btn.btnid&=0xffff0fff;
        } break;
      case SDL_JOYHATMOTION: {
          if (mapv[i].dsttype==SITTER_BTNMAP_AXIS) {
            mapv[i].axis.btnid_lo&=0xffff0fff;
            mapv[i].axis.btnid_mid&=0xffff0fff;
            mapv[i].axis.btnid_hi&=0xffff0fff;
          } else mapv[i].btn.btnid&=0xffff0fff;
        } break;
      case SDL_JOYBALLMOTION: {
          if (mapv[i].dsttype==SITTER_BTNMAP_AXIS) {
            mapv[i].axis.btnid_lo&=0xffff0fff;
            mapv[i].axis.btnid_mid&=0xffff0fff;
            mapv[i].axis.btnid_hi&=0xffff0fff;
          } else mapv[i].btn.btnid&=0xffff0fff;
        } break;
      default: i++; continue;
    }
    bool collide=false;
    for (int j=0;j<i;j++) {
      if (mapv[i].src.type!=mapv[j].src.type) continue;
      if (mapv[i].dsttype!=mapv[j].dsttype) continue;
      if (mapv[i].srcaxis!=mapv[j].srcaxis) continue;
      switch (mapv[i].src.type) {
        case SDL_JOYBUTTONDOWN: {
            if (!SAMEJOYSTICKTYPE(mapv[i].src.jbutton.which,mapv[j].src.jbutton.which)) continue;
            if (mapv[i].src.jbutton.button!=mapv[j].src.jbutton.button) continue;
          } break;
        case SDL_JOYAXISMOTION: {
            if (!SAMEJOYSTICKTYPE(mapv[i].src.jaxis.which,mapv[j].src.jaxis.which)) continue;
            if (mapv[i].src.jaxis.axis!=mapv[j].src.jaxis.axis) continue;
          } break;
        case SDL_JOYHATMOTION: {
            if (!SAMEJOYSTICKTYPE(mapv[i].src.jhat.which,mapv[j].src.jhat.which)) continue;
            if (mapv[i].src.jhat.hat!=mapv[j].src.jhat.hat) continue;
          } break;
        case SDL_JOYBALLMOTION: {
            if (!SAMEJOYSTICKTYPE(mapv[i].src.jball.which,mapv[j].src.jball.which)) continue;
            if (mapv[i].src.jball.ball!=mapv[j].src.jball.ball) continue;
          } break;
      }
      switch (mapv[i].dsttype) {
        case SITTER_BTNMAP_BUTTON:
        case SITTER_BTNMAP_REVBUTTON: {
            if (mapv[i].btn.btnid!=mapv[j].btn.btnid) continue;
            if (mapv[i].btn.thresh!=mapv[j].btn.thresh) continue;
          } break;
        case SITTER_BTNMAP_AXIS: {
            if (mapv[i].axis.thresh_lo!=mapv[j].axis.thresh_lo) continue;
            if (mapv[i].axis.thresh_hi!=mapv[j].axis.thresh_hi) continue;
            if (mapv[i].axis.btnid_lo!=mapv[j].axis.btnid_lo) continue;
            if (mapv[i].axis.btnid_mid!=mapv[j].axis.btnid_mid) continue;
            if (mapv[i].axis.btnid_hi!=mapv[j].axis.btnid_hi) continue;
          } break;
      }
      collide=true;
      break;
    }
    if (!collide) { i++; continue; }
    mapc--;
    if (i<mapc) memmove(mapv+i,mapv+i+1,sizeof(Mapping)*(mapc-i));
  }
  #undef SAMEJOYSTICKTYPE
}

/******************************************************************************
 * button mapping
 *****************************************************************************/
 
void InputManager::mapButtonToButton(const SDL_Event *src,int btnid,int thresh,bool rev,char srcaxis) {
  if (mapc>=mapa) {
    mapa+=MAPV_INCREMENT;
    if (!(mapv=(Mapping*)realloc(mapv,sizeof(Mapping)*mapa))) sitter_throw(AllocationError,"");
  }
  memcpy(&(mapv[mapc].src),src,sizeof(SDL_Event));
  mapv[mapc].srcaxis=srcaxis;
  mapv[mapc].dsttype=(rev?SITTER_BTNMAP_REVBUTTON:SITTER_BTNMAP_BUTTON);
  mapv[mapc].state=0;
  mapv[mapc].btn.btnid=btnid;
  mapv[mapc].btn.thresh=thresh;
  mapc++;
}

void InputManager::mapAxisToButtons(const SDL_Event *src,int thresh_lo,int thresh_hi,int btnid_lo,int btnid_mid,int btnid_hi,char srcaxis) {
  if (thresh_hi<thresh_lo) sitter_throw(ArgumentError,"InputManager::mapAxisToButtons thresh:%d..%d",thresh_lo,thresh_hi);
  if (mapc>=mapa) {
    mapa+=MAPV_INCREMENT;
    if (!(mapv=(Mapping*)realloc(mapv,sizeof(Mapping)*mapa))) sitter_throw(AllocationError,"");
  }
  memcpy(&(mapv[mapc].src),src,sizeof(SDL_Event));
  mapv[mapc].srcaxis=srcaxis;
  mapv[mapc].dsttype=SITTER_BTNMAP_AXIS;
  mapv[mapc].state=0;
  mapv[mapc].axis.thresh_lo=thresh_lo;
  mapv[mapc].axis.thresh_hi=thresh_hi;
  mapv[mapc].axis.btnid_lo=btnid_lo;
  mapv[mapc].axis.btnid_mid=btnid_mid;
  mapv[mapc].axis.btnid_hi=btnid_hi;
  mapc++;
}

void InputManager::unmapButton(int btnid) {
  for (int i=0;i<mapc;) {
    switch (mapv[i].dsttype) {
      case SITTER_BTNMAP_BUTTON:
      case SITTER_BTNMAP_REVBUTTON: {
          if (btnid==mapv[i].btn.btnid) {
            mapc--;
            if (i<mapc) memmove(mapv+i,mapv+i+1,sizeof(Mapping)*(mapc-i));
          } else i++;
        } break;
      case SITTER_BTNMAP_AXIS: {
          if (mapv[i].axis.btnid_lo==btnid) mapv[i].axis.btnid_lo=0;
          if (mapv[i].axis.btnid_mid==btnid) mapv[i].axis.btnid_mid=0;
          if (mapv[i].axis.btnid_hi==btnid) mapv[i].axis.btnid_hi=0;
          if (!mapv[i].axis.btnid_lo&&!mapv[i].axis.btnid_mid&&!mapv[i].axis.btnid_hi) {
            mapc--;
            if (i<mapc) memmove(mapv+i,mapv+i+1,sizeof(Mapping)*(mapc-i));
          } else i++;
        } break;
      default: i++;
    }
  }
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
bool InputManager::eventsMatch(const SDL_Event *a,const SDL_Event *b) {
  if (a->type!=b->type) return false;
  switch (a->type) {
    case SDL_KEYDOWN: return (a->key.keysym.sym==b->key.keysym.sym);
    case SDL_MOUSEBUTTONDOWN: return (a->button.button==b->button.button);
    case SDL_MOUSEMOTION: return true;
    case SDL_JOYBUTTONDOWN: return ((a->jbutton.which==b->jbutton.which)&&(a->jbutton.button==b->jbutton.button));
    case SDL_JOYAXISMOTION: return ((a->jaxis.which==b->jaxis.which)&&(a->jaxis.axis==b->jaxis.axis));
    case SDL_JOYHATMOTION: return ((a->jhat.which==b->jhat.which)&&(a->jhat.hat==b->jhat.hat));
    case SDL_JOYBALLMOTION: return ((a->jball.which==b->jball.which)&&(a->jball.ball==b->jball.ball));
  }
  return false;
}
 
void InputManager::receiveSDLEvent(const SDL_Event *evt) {
  for (int i=0;i<mapc;i++) if (eventsMatch(evt,&(mapv[i].src))) {
    int val;
    switch (evt->type) {
      case SDL_KEYDOWN: val=evt->key.state; break;
      case SDL_MOUSEBUTTONDOWN: val=evt->button.state; break;
      case SDL_MOUSEMOTION: if (mapv[i].srcaxis=='x') val=evt->motion.xrel; else val=evt->motion.yrel; break;
      case SDL_JOYBUTTONDOWN: val=evt->jbutton.state; break;
      case SDL_JOYAXISMOTION: val=evt->jaxis.value; break;
      case SDL_JOYHATMOTION: val=evt->jhat.value; break;
      case SDL_JOYBALLMOTION: if (mapv[i].srcaxis=='y') val=evt->jball.xrel; else val=evt->jball.yrel; break;
      default: continue;
    }
    switch (mapv[i].dsttype) {
      case SITTER_BTNMAP_BUTTON: {
          if ((val<mapv[i].btn.thresh)==(mapv[i].state<mapv[i].btn.thresh)) continue;
          mapv[i].state=val;
          if (val>=mapv[i].btn.thresh) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,mapv[i].btn.btnid,val));
          else pushEvent(InputEvent(SITTER_EVT_BTNUP,mapv[i].btn.btnid,val));
        } break;
      case SITTER_BTNMAP_REVBUTTON: {
          if ((val<mapv[i].btn.thresh)==(mapv[i].state<mapv[i].btn.thresh)) continue;
          mapv[i].state=val;
          if (val>=mapv[i].btn.thresh) pushEvent(InputEvent(SITTER_EVT_BTNUP,mapv[i].btn.btnid,val));
          else pushEvent(InputEvent(SITTER_EVT_BTNDOWN,mapv[i].btn.btnid,val));
        } break;
      case SITTER_BTNMAP_AXIS: {
          int prev=((mapv[i].state<=mapv[i].axis.thresh_lo)?-1:(mapv[i].state>=mapv[i].axis.thresh_hi)?1:0);
          int curr=((val<=mapv[i].axis.thresh_lo)?-1:(val>=mapv[i].axis.thresh_hi)?1:0);
          if (prev==curr) continue;
          mapv[i].state=val;
          switch (prev) {
            case -1: if (mapv[i].axis.btnid_lo)  pushEvent(InputEvent(SITTER_EVT_BTNUP,mapv[i].axis.btnid_lo,0)); break;
            case  0: if (mapv[i].axis.btnid_mid) pushEvent(InputEvent(SITTER_EVT_BTNUP,mapv[i].axis.btnid_mid,0)); break;
            case  1: if (mapv[i].axis.btnid_hi)  pushEvent(InputEvent(SITTER_EVT_BTNUP,mapv[i].axis.btnid_hi,0)); break;
          }
          switch (curr) {
            case -1: if (mapv[i].axis.btnid_lo)  pushEvent(InputEvent(SITTER_EVT_BTNDOWN,mapv[i].axis.btnid_lo,1)); break;
            case  0: if (mapv[i].axis.btnid_mid) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,mapv[i].axis.btnid_mid,1)); break;
            case  1: if (mapv[i].axis.btnid_hi)  pushEvent(InputEvent(SITTER_EVT_BTNDOWN,mapv[i].axis.btnid_hi,1)); break;
          }
        } break;
    }
  }
}
 
void InputManager::update() {

  //NOTE: When evdev in play, it always returns zero from SDL_PollEvent, and populates our queue on its own.

  /* SDL event loop */
  SDL_Event evt; while (SDL_PollEvent(&evt)) switch (evt.type) {
    case SDL_QUIT: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_QUIT)); break;
    case SDL_KEYDOWN: {
        if (!never_remap_keys||!evt.key.keysym.unicode) receiveSDLEvent(&evt); 
        if (evt.key.keysym.unicode) pushEvent(InputEvent(SITTER_EVT_UNICODE,evt.key.keysym.unicode,1));
        if (promiscuous) pushEvent(InputEvent(SITTER_EVT_SDL,SITTER_BTN_SDLKEY|evt.key.keysym.sym,1));
      } break;
    case SDL_KEYUP: evt.type=SDL_KEYDOWN; receiveSDLEvent(&evt); break;
    case SDL_MOUSEBUTTONDOWN: {
        receiveSDLEvent(&evt); 
        switch (evt.button.button) {
          case SDL_BUTTON_LEFT: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MSLEFT,1)); break;
          case SDL_BUTTON_MIDDLE: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MSMIDDLE,1)); break;
          case SDL_BUTTON_RIGHT: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MSRIGHT,1)); break;
          case SDL_BUTTON_WHEELUP: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MSWUP,1)); break;
          case SDL_BUTTON_WHEELDOWN: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MSWDOWN,1)); break;
        }
        if (promiscuous) pushEvent(InputEvent(SITTER_EVT_SDL,SITTER_BTN_SDLMBTN|evt.button.button,1));
      } break;
    case SDL_MOUSEBUTTONUP: {
        evt.type=SDL_MOUSEBUTTONDOWN; 
        receiveSDLEvent(&evt); 
        switch (evt.button.button) {
          case SDL_BUTTON_LEFT: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MSLEFT,0)); break;
          case SDL_BUTTON_MIDDLE: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MSMIDDLE,0)); break;
          case SDL_BUTTON_RIGHT: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MSRIGHT,0)); break;
          case SDL_BUTTON_WHEELUP: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MSWUP,0)); break;
          case SDL_BUTTON_WHEELDOWN: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MSWDOWN,0)); break;
        }
      } break;
    case SDL_MOUSEMOTION: mousex=evt.motion.x; mousey=evt.motion.y; receiveSDLEvent(&evt); break;
    case SDL_JOYBUTTONDOWN: {
        receiveSDLEvent(&evt); 
        if (promiscuous) pushEvent(InputEvent(SITTER_EVT_SDL,SITTER_BTN_SDLJBTN|(evt.jbutton.which<<SITTER_BTN_SDLJOYSHIFT)|evt.jbutton.button,1));
      } break;
    case SDL_JOYBUTTONUP: evt.type=SDL_JOYBUTTONDOWN; receiveSDLEvent(&evt); break;
    case SDL_JOYAXISMOTION: {
        receiveSDLEvent(&evt); 
        if (promiscuous) pushEvent(InputEvent(SITTER_EVT_SDL,SITTER_BTN_SDLJAXIS|(evt.jaxis.which<<SITTER_BTN_SDLJOYSHIFT)|evt.jaxis.axis,evt.jaxis.value));
      } break;
    case SDL_JOYHATMOTION: {
        receiveSDLEvent(&evt); 
        if (promiscuous) pushEvent(InputEvent(SITTER_EVT_SDL,SITTER_BTN_SDLJHAT|(evt.jhat.which<<SITTER_BTN_SDLJOYSHIFT)|evt.jhat.hat,evt.jhat.value));
      } break;
    case SDL_JOYBALLMOTION: {
        receiveSDLEvent(&evt); 
        if (promiscuous) pushEvent(InputEvent(SITTER_EVT_SDL,SITTER_BTN_SDLJBALL|(evt.jball.which<<SITTER_BTN_SDLJOYSHIFT)|evt.jball.ball,(evt.jball.xrel<<16)|(evt.jball.yrel&0xffff)));
      } break;
  }
  
}

#endif
