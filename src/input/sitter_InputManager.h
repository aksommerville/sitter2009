#ifdef SITTER_WII
  #include "sitter_InputManager_wii.h"
#else

#ifndef SITTER_INPUTMANAGER_H
#define SITTER_INPUTMANAGER_H

#ifdef SITTER_WIN32
  typedef int iconv_t;
#endif
#ifdef SITTER_LINUX_DRM
  #include "drm_alsa_evdev/sitter_evdev.h"
#else
  #include <SDL/SDL.h>
#endif

class Game;

/* event type */
#define SITTER_EVT_NONE       0 // placeholder
#define SITTER_EVT_SIGNAL     1 // stateless notification !!! not using this anymore, use BTNDOWN instead
#define SITTER_EVT_BTNDOWN    2 // activate 2-state button
#define SITTER_EVT_BTNUP      3 // deactivate 2-state button
#define SITTER_EVT_UNICODE    4 // stateless keyboard input
#define SITTER_EVT_SDL        5 // direct event from SDL, should only be used by InputConfigurator
// Sources with more than 2 states are handled internally by InputManager, and never seen by the rest of the program.

/* button ID */
#define SITTER_BTN_UNICODE     0x00000000 // button ids below 0x110000 are unicode code points
// 0x00110000..0x0011ffff are system-wide controls (ie, not associated with 1 player)
#define SITTER_BTN_QUIT        0x00110000 // stateless, end program as soon as possible
#define SITTER_BTN_MN_UP       0x00110001 // 2-state, move selection up in menu
#define SITTER_BTN_MN_DOWN     0x00110002 // " down
#define SITTER_BTN_MN_LEFT     0x00110003 // " left
#define SITTER_BTN_MN_RIGHT    0x00110004 // " right
#define SITTER_BTN_MN_OK       0x00110005 // stateless, choose something in menu
#define SITTER_BTN_MN_CANCEL   0x00110006 // stateless, exit menu (to previous menu, maybe?)
#define SITTER_BTN_MENU        0x00110007 // stateless, bring up menu, when playing
#define SITTER_BTN_BEGIN1      0x00110008 // stateless, begin 1-player game REMOVING, replaced by SITTER_BTN_BEGIN
#define SITTER_BTN_BEGIN2      0x00110009 // stateless, begin 2-player game "
#define SITTER_BTN_BEGIN3      0x0011000a // stateless, begin 3-player game "
#define SITTER_BTN_BEGIN4      0x0011000b // stateless, begin 4-player game "
#define SITTER_BTN_CREDITS     0x0011000c // stateless, show credits
#define SITTER_BTN_CONFIG      0x0011000d // stateless, enter configuration editor
#define SITTER_BTN_RESTARTLVL  0x0011000e // stateless, restart this level
#define SITTER_BTN_NEXTLVL     0x0011000f // stateless, begin next level
#define SITTER_BTN_MAINMENU    0x00110010 // stateless, open main menu (like, from a Game Over menu)
#define SITTER_BTN_EDITOR      0x00110011 // stateless, enter map editor
#define SITTER_BTN_EDITORNEW   0x00110012 // stateless, map editor :: new map
#define SITTER_BTN_OPENGRID    0x00110013 // stateless, map editor :: open (get name from menu)
#define SITTER_BTN_OPENGRIDSET 0x00110014 // stateless, map editor :: open set
#define SITTER_BTN_EDITORNEWSET 0x00110015 // stateless, map editor :: new set
#define SITTER_BTN_MSLEFT      0x00110016 // mouse, left
#define SITTER_BTN_MSMIDDLE    0x00110017 // mouse, middle
#define SITTER_BTN_MSRIGHT     0x00110018 // mouse, right
#define SITTER_BTN_MSWUP       0x00110019 // mouse, wheel up
#define SITTER_BTN_MSWDOWN     0x0011001a // mouse, wheel down
#define SITTER_BTN_EDITORNEWSET2 0x0011001b
#define SITTER_BTN_EDITORNEW2  0x0011001c
#define SITTER_BTN_EDITORPENT  0x0011001d
#define SITTER_BTN_EDITRMSPR   0x0011001e
#define SITTER_BTN_EDITADDSPR  0x0011001f
#define SITTER_BTN_EDITHDR     0x00110020
#define SITTER_BTN_BEGIN       0x00110021 // start game
#define SITTER_BTN_CFGINPUT    0x00110022 // open input configuration dialogue
#define SITTER_BTN_CFGAUDIO    0x00110023 // open audio configuration
#define SITTER_BTN_CFGAUDIOOK  0x00110024 // accept new audio configuration
#define SITTER_BTN_CFGVIDEO    0x00110025 // open video configuration
#define SITTER_BTN_CFGVIDEOOK  0x00110026 // accept new video configuration
#define SITTER_BTN_MN_FAST     0x00110027 // 2-state, make integer menu items move faster
#define SITTER_BTN_HSNAMERTN   0x00110028 // accept high-score name
// 0x00120000..0x0012ffff are player controls, nybble 0000f000 is player ID 1..4
#define SITTER_BTN_PLR_LEFT    0x00120000 // player 0 or 'x' is not a player; these are for snarkification purposes
#define SITTER_BTN_PLR_RIGHT   0x00120001
#define SITTER_BTN_PLR_JUMP    0x00120002
#define SITTER_BTN_PLR_DUCK    0x00120003
#define SITTER_BTN_PLR_PICKUP  0x00120004
#define SITTER_BTN_PLR_TOSS    0x00120005
#define SITTER_BTN_PLR_ATN     0x00120006 // "attention" -- toggle whether camera focuses on this player
#define SITTER_BTN_PLR_RADAR   0x00120007 // each player can toggle the radar
#define SITTER_BTN_PLR1_LEFT   0x00121000 // same thing with '1' in bits 12..15 = player 1
#define SITTER_BTN_PLR1_RIGHT  0x00121001
#define SITTER_BTN_PLR1_JUMP   0x00121002
#define SITTER_BTN_PLR1_DUCK   0x00121003
#define SITTER_BTN_PLR1_PICKUP 0x00121004
#define SITTER_BTN_PLR1_TOSS   0x00121005
#define SITTER_BTN_PLR1_ATN    0x00121006
#define SITTER_BTN_PLR1_RADAR  0x00121007
#define SITTER_BTN_PLR2_LEFT   0x00122000 // same thing with '2' in bits 12..15 = player 2
#define SITTER_BTN_PLR2_RIGHT  0x00122001
#define SITTER_BTN_PLR2_JUMP   0x00122002
#define SITTER_BTN_PLR2_DUCK   0x00122003
#define SITTER_BTN_PLR2_PICKUP 0x00122004
#define SITTER_BTN_PLR2_TOSS   0x00122005
#define SITTER_BTN_PLR2_ATN    0x00122006
#define SITTER_BTN_PLR2_RADAR  0x00122007
#define SITTER_BTN_PLR3_LEFT   0x00123000 // same thing with '3' in bits 12..15 = player 3
#define SITTER_BTN_PLR3_RIGHT  0x00123001
#define SITTER_BTN_PLR3_JUMP   0x00123002
#define SITTER_BTN_PLR3_DUCK   0x00123003
#define SITTER_BTN_PLR3_PICKUP 0x00123004
#define SITTER_BTN_PLR3_TOSS   0x00123005
#define SITTER_BTN_PLR3_ATN    0x00123006
#define SITTER_BTN_PLR3_RADAR  0x00123007
#define SITTER_BTN_PLR4_LEFT   0x00124000 // same thing with '4' in bits 12..15 = player 4
#define SITTER_BTN_PLR4_RIGHT  0x00124001
#define SITTER_BTN_PLR4_JUMP   0x00124002
#define SITTER_BTN_PLR4_DUCK   0x00124003
#define SITTER_BTN_PLR4_PICKUP 0x00124004
#define SITTER_BTN_PLR4_TOSS   0x00124005
#define SITTER_BTN_PLR4_ATN    0x00124006
#define SITTER_BTN_PLR4_RADAR  0x00124007
// some macros for testing button id:
#define SITTER_BTN_ISUNICODE(btnid) ((btnid)<0x00110000)
#define SITTER_BTN_ISSYSTEM(btnid) (((btnid)>=0x00110000)&&((btnid)<0x00120000))
#define SITTER_BTN_ISPLR(btnid)    (((btnid)>=0x00121000)&&((btnid)<0x00125000))
#define SITTER_BTN_ISPLR1(btnid)         (((btnid)&0xfffff000)==0x00121000)
#define SITTER_BTN_ISPLR2(btnid)         (((btnid)&0xfffff000)==0x00122000)
#define SITTER_BTN_ISPLR3(btnid)         (((btnid)&0xfffff000)==0x00123000)
#define SITTER_BTN_ISPLR4(btnid)         (((btnid)&0xfffff000)==0x00124000)
#define SITTER_BTN_ISPLR_LEFT(btnid)     (((btnid)&0xffff0fff)==0x00120000)
#define SITTER_BTN_ISPLR_RIGHT(btnid)    (((btnid)&0xffff0fff)==0x00120001)
#define SITTER_BTN_ISPLR_JUMP(btnid)     (((btnid)&0xffff0fff)==0x00120002)
#define SITTER_BTN_ISPLR_DUCK(btnid)     (((btnid)&0xffff0fff)==0x00120003)
#define SITTER_BTN_ISPLR_PICKUP(btnid)   (((btnid)&0xffff0fff)==0x00120004)
#define SITTER_BTN_ISPLR_TOSS(btnid)     (((btnid)&0xffff0fff)==0x00120005)
#define SITTER_BTN_ISPLR_ATN(btnid)      (((btnid)&0xffff0fff)==0x00120006)
#define SITTER_BTN_ISPLR_RADAR(btnid)    (((btnid)&0xffff0fff)==0x00120007)

/* button id for SDL events */
#define SITTER_BTN_SDLTYPEMASK 0xff000000 // what kind of event
#define SITTER_BTN_SDLKEY      0x01000000
#define SITTER_BTN_SDLMBTN     0x02000000
#define SITTER_BTN_SDLJBTN     0x03000000
#define SITTER_BTN_SDLJAXIS    0x04000000
#define SITTER_BTN_SDLJHAT     0x05000000
#define SITTER_BTN_SDLJBALL    0x06000000
#define SITTER_BTN_SDLJOYMASK  0x00ff0000 // which joystick
#define SITTER_BTN_SDLJOYSHIFT 16
#define SITTER_BTN_SDLINDMASK  0x0000ffff // which button,axis,hat,etc?
#define SITTER_BTN_SDLINDSHIFT 0

class InputEvent {
public:

  int type;
  int btnid;
  int val;
  
  InputEvent() {}
  InputEvent(int type,int btnid=-1,int val=0):type(type),btnid(btnid),val(val) {}
  InputEvent(const InputEvent &evt):type(evt.type),btnid(evt.btnid),val(evt.val) {}
  void operator=(const InputEvent &evt) { type=evt.type; btnid=evt.btnid; val=evt.val; }
  
};

/* button mapping destination type */
#define SITTER_BTNMAP_BUTTON     1 // map zero/nonzero source to zero/nonzero dest
#define SITTER_BTNMAP_REVBUTTON  2 // BUTTON, sense reversed
#define SITTER_BTNMAP_AXIS       3 // treat three ranges of an analogue control as button presses (you don't have to use all 3)

class InputManager {
friend class InputConfigurator;

  typedef struct {
    SDL_Event src;
    char srcaxis; // 'x','y'
    int state;
    int dsttype;
    union {
      struct { int btnid; int thresh; } btn; // BUTTON,REVBUTTON
      struct { int thresh_lo,thresh_hi; int btnid_lo,btnid_mid,btnid_hi; } axis; // AXIS
    };
  } Mapping;

  Game *game;
  InputEvent *evtq;
  int evtp,evtc,evta;
  Mapping *mapv; int mapc,mapa;
  
  SDL_Joystick **joyv; int joyc;
  
  int mousex,mousey;
  
public:

  bool promiscuous;
  
  /* When true, keystrokes with a Unicode equivalent will only be sent as Unicode.
   * HAKeyboard uses this.
   */
  bool never_remap_keys;

  InputManager(Game *game,int evtqlen=64);
  ~InputManager();
  
  /* Flush button-mapping list. Reread mappings from game->cfg.
   * Button states are forgotten, so try not to do this very often. Ideally, just once at init.
   * (writeConfiguration does the reverse -- updates game->cfg)
   * Call generaliseMappings before writing configuration to turn joystick maps into "PX" instead of "P1","P2",
   * and to remove redundant mappings. This is destructive -- you can't use your joysticks after calling it.
   */
  void reconfigure();
  void writeConfiguration();
  void generaliseMappings();
  
  void update();
  
  int countEvents() const { return evtc; }
  bool peekEvent(InputEvent &evt,int index=0) const { 
    if ((index<0)||(index>=evtc)) return false;
    if ((index+=evtp)>=evta) index-=evta;
    evt=evtq[index];
    return true;
  }
  void removeEvents(int ct) { if (ct>evtc) ct=evtc; if (ct<1) return; if ((evtp+=ct)>=evta) evtp-=evta; evtc-=ct; }
  void flushEvents() { evtc=0; }
  bool popEvent(InputEvent &evt) { if (!peekEvent(evt)) return false; removeEvents(1); return true; }
  bool pushEvent(const InputEvent &evt) {
    if (evtc>=evta) return false;
    int index=evtp+evtc; if (index>=evta) index-=evta;
    evtq[index]=evt;
    evtc++;
    return true;
  }
  
  bool getPointer(int *x,int *y) const {
    if ((mousex<0)||(mousey<0)) { *x=*y=0; return false; }
    *x=mousex; *y=mousey; return true;
  }
  
  /* Call this with any event that might be mapped to something.
   * Filter out the window manager stuff.
   * evt->type should be a DOWN, never an UP.
   */
  void receiveSDLEvent(const SDL_Event *evt);
  static bool eventsMatch(const SDL_Event *a,const SDL_Event *b);
  
  static int stringToKeysym(const char *s);
  static bool stringToJoyctl(const char *s,int *type,int *index);
  static int stringToBtnid(const char *s,int nextplr=1);
  
  /* Either of these may use a static buffer (but they won't use the same one).
   */
  const char *btnidToString(int btnid) const;
  const char *mappingSourceToString(int mapid) const;
  
  /* <srcaxis> is 'x' or 'y', only relevant for mice and trackballs.
   * btnid zero is ignored in axis mappings.
   */
  void mapButtonToButton(const SDL_Event *src,int btnid,int thresh=1,bool rev=false,char srcaxis='x');
  void mapAxisToButtons(const SDL_Event *src,int thresh_lo,int thresh_hi,int btnid_lo,int btnid_mid,int btnid_hi,char srcaxis='x');
  void unmapButton(int btnid);
  
};

#endif
#endif
