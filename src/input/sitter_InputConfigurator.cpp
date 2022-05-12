#ifndef SITTER_WII

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_VideoManager.h"
#include "sitter_Menu.h"
#include "sitter_InputManager.h"
#include "sitter_InputConfigurator.h"

/* our button ids (Game will never see them) */
#define CFGBTN_PLAYER1      0x00130000
#define CFGBTN_PLAYER2      0x00130001
#define CFGBTN_PLAYER3      0x00130002
#define CFGBTN_PLAYER4      0x00130003
#define CFGBTN_ADVANCED     0x00130004
#define CFGBTN_ADDMAP       0x00130005
#define CFGBTN_ADDMAP2      0x00130006
#define CFGBTN_ADDMAP3      0x00130007
#define CFGBTN_ADDMAP4      0x00130008
#define CFGBTN_RMMAP        0x00131000 // + map id
#define CFGBTN_RMMAP_LIMIT  0x00132000

#define AXIS_LO 5000
#define AXIS_HI 30000
#define AXIS_THRESH 10000

static const struct { int btnid; const char *name; } player_button_names[]={
  {SITTER_BTN_PLR_LEFT    ,"left"},
  {SITTER_BTN_PLR_RIGHT   ,"right"},
  {SITTER_BTN_PLR_JUMP    ,"jump"},
  {SITTER_BTN_PLR_DUCK    ,"duck"},
  {SITTER_BTN_PLR_PICKUP  ,"pickup"},
  {SITTER_BTN_PLR_TOSS    ,"toss"},
  {SITTER_BTN_PLR_ATN     ,"focus"},
  {SITTER_BTN_PLR_RADAR   ,"radar"},
  {0,0},
};

static const struct { int btnid; const char *name; } menu_button_names[]={
  {SITTER_BTN_QUIT            ,"quit"},
  {SITTER_BTN_MN_UP           ,"up"},
  {SITTER_BTN_MN_DOWN         ,"down"},
  {SITTER_BTN_MN_LEFT         ,"left"},
  {SITTER_BTN_MN_RIGHT        ,"right"},
  {SITTER_BTN_MN_OK           ,"ok"},
  {SITTER_BTN_MN_CANCEL       ,"cancel"},
  {SITTER_BTN_MENU         ,"pause"},
  {SITTER_BTN_RESTARTLVL   ,"restart"},
  {SITTER_BTN_NEXTLVL      ,"next"},
  {0,0},
};

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
InputConfigurator::InputConfigurator(Game *game):game(game) {
  menudepth=1;
  if (Menu *menu=new Menu(game,"Input")) {
    menu->addItem(new MenuItem(game,"Set Player One",game->video->loadTexture("icon/ic_1up.png",true),CFGBTN_PLAYER1));
    menu->addItem(new MenuItem(game,"Set Player Two",game->video->loadTexture("icon/ic_2up.png",true),CFGBTN_PLAYER2));
    menu->addItem(new MenuItem(game,"Set Player Three",game->video->loadTexture("icon/ic_3up.png",true),CFGBTN_PLAYER3));
    menu->addItem(new MenuItem(game,"Set Player Four",game->video->loadTexture("icon/ic_4up.png",true),CFGBTN_PLAYER4));
    menu->addItem(new MenuItem(game,"Advanced",-1,CFGBTN_ADVANCED));
    menu->pack();
    game->video->pushMenu(menu);
  }
  plrcfg=0;
  activeaxisv=NULL;
  activeaxisc=0;
  hasactiveaxis=false;
}

InputConfigurator::~InputConfigurator() {
  if (game->inputcfg==this) game->inputcfg=NULL;
  while (menudepth-->0) if (Menu *menu=game->video->popMenu()) delete menu;
  if (activeaxisv) free(activeaxisv);
}

/******************************************************************************
 * axis memory
 *****************************************************************************/
 
void InputConfigurator::setAxis(int joyid,int axisid,int val) {
  if (((val<0)&&(val>-AXIS_LO))||((val>=0)&&(val<AXIS_LO))) { // remove from list
    hasactiveaxis=false;
    for (int i=0;i<activeaxisc;i++) {
      if ((activeaxisv[i].joyid==joyid)&&(activeaxisv[i].axisid==axisid)) activeaxisv[i].joyid=-1;
      else if (activeaxisv[i].joyid>=0) hasactiveaxis=true;
    }
  } else { // add to list
    int freeentry=-1;
    for (int i=0;i<activeaxisc;i++) {
      if (activeaxisv[i].joyid<0) { freeentry=i; }
      else if ((activeaxisv[i].joyid==joyid)&&(activeaxisv[i].axisid==axisid)) return;
    }
    if (freeentry<0) {
      int nactiveaxisc=activeaxisc+16;
      if (!(activeaxisv=(ActiveAxisEntry*)realloc(activeaxisv,sizeof(ActiveAxisEntry)*nactiveaxisc))) sitter_throw(AllocationError,"");
      memset(activeaxisv+activeaxisc,0xff,sizeof(ActiveAxisEntry)*16);
      freeentry=activeaxisc;
      activeaxisc=nactiveaxisc;
    }
    activeaxisv[freeentry].joyid=joyid;
    activeaxisv[freeentry].axisid=axisid;
    hasactiveaxis=true;
  }
}

/******************************************************************************
 * event dispatch
 *****************************************************************************/
 
bool InputConfigurator::receiveEvent(InputEvent &evt) {
  if (plrcfg) return playerSetupEvent(evt);
  if (evt.type==SITTER_EVT_BTNDOWN) switch (evt.btnid) {
    case CFGBTN_PLAYER1: beginPlayerSetup(1); return true;
    case CFGBTN_PLAYER2: beginPlayerSetup(2); return true;
    case CFGBTN_PLAYER3: beginPlayerSetup(3); return true;
    case CFGBTN_PLAYER4: beginPlayerSetup(4); return true;
    case CFGBTN_ADVANCED: beginAdvancedSetup(); return true;
    case CFGBTN_ADDMAP: {
        if (Menu *menu=game->video->popMenu()) { delete menu; menudepth--; }
        openAddMap_phase1(); 
      } return true;
    case CFGBTN_ADDMAP2: if (Menu *menu=game->video->getActiveMenu()) {
        int devid=((ListMenuItem*)(menu->itemv[0]))->getChoice();
        int plrid=((ListMenuItem*)(menu->itemv[1]))->getChoice();
        openAddMap_phase2(devid,plrid);
        return true;
      } else return true;
    case CFGBTN_ADDMAP3: if (Menu *menu=game->video->popMenu()) {
        if (addmap_devid) addmap_srcbtn=((ListMenuItem*)(menu->itemv[0]))->getChoice();
        else addmap_srcbtn=((IntegerMenuItem*)(menu->itemv[0]))->val;
        delete menu;
        openAddMap_phase3();
      } return true;
    case CFGBTN_ADDMAP4: openAddMap_phase4(); return true;
    case SITTER_BTN_MN_CANCEL: {
        if (Menu *menu=game->video->popMenu()) { delete menu; menudepth--; }
        if (--menudepth<=0) delete this;
      } return true;
    default: {
        if ((evt.btnid>=CFGBTN_RMMAP)&&(evt.btnid<CFGBTN_RMMAP_LIMIT)) {
          int mapid=evt.btnid-CFGBTN_RMMAP;
          if (mapid<game->input->mapc) {
            game->input->mapc--;
            if (mapid<game->input->mapc) memmove(game->input->mapv+mapid,game->input->mapv+mapid+1,sizeof(InputManager::Mapping)*(game->input->mapc-mapid));
            if (Menu *menu=game->video->popMenu()) { delete menu; menudepth--; }
          }
          return true;
        }
      }
  }
  return false;
}

/******************************************************************************
 * simple player setup
 *****************************************************************************/
 
void InputConfigurator::beginPlayerSetup(int plrid) {
  //sitter_log("beginPlayerSetup(%d)",plrid);
  plrcfg=plrid;
  plrcfg_btn=SITTER_BTN_PLR_LEFT;
  game->input->promiscuous=true;
  oktoreadaxis=false;
  
  char title[40];
  sprintf(title,"Player %d, press:",plrid);
  if (Menu *menu=new Menu(game,title)) {
    menu->itemfonttexid=game->video->loadTexture("andyfont.png",false);
    menu->itemdiscolor=0xff4080ff;
    menu->itemchw=32;
    menu->itemchh=48;
    menu->itemh=50;
    menu->addItem(new MenuItem(game,"Left"));
    menu->pack();
    game->video->pushMenu(menu);
    menudepth++;
  }
}

bool InputConfigurator::playerSetupEvent(InputEvent &evt) {
  if (evt.type==SITTER_EVT_SDL) { // raw input
    switch (evt.btnid&SITTER_BTN_SDLTYPEMASK) {
      case SITTER_BTN_SDLKEY: 
      case SITTER_BTN_SDLMBTN:
      case SITTER_BTN_SDLJBTN: mapPlayerSetup(evt); break;
      case SITTER_BTN_SDLJAXIS: {
          setAxis((evt.btnid&SITTER_BTN_SDLJOYMASK)>>SITTER_BTN_SDLJOYSHIFT,evt.btnid&SITTER_BTN_SDLINDMASK,evt.val);
          if (!oktoreadaxis) {
            if (!hasactiveaxis) oktoreadaxis=true;
            return true;
          } else if ((evt.val<-AXIS_HI)||(evt.val>AXIS_HI)) {
            mapPlayerSetup(evt);
          } else return true;
        } break;
      case SITTER_BTN_SDLJHAT: ; // TODO -- hats not acknowledged yet
      case SITTER_BTN_SDLJBALL: ; // TODO -- balls not acknowledged yet
    }
    playerSetupNext();
  
  } else if (evt.type==SITTER_EVT_BTNDOWN) switch (evt.btnid) { // other events, most we can simply absorb
    /*
    case SITTER_BTN_MN_CANCEL: {
        game->input->promiscuous=false;
        if (Menu *menu=game->video->popMenu()) delete menu;
        plrcfg=0;
        if (--menudepth<=0) delete this;
      } return true;
    */
    case SITTER_BTN_QUIT: return false;
  }
  return true;
}

void InputConfigurator::mapPlayerSetup(InputEvent &evt) {
  int btnid=plrcfg_btn|(plrcfg<<12);
  game->input->unmapButton(btnid);
  oktoreadaxis=false;
  #ifdef SITTER_WII
    //TODO
  #else
    SDL_Event sdlevt;
    switch (evt.btnid&SITTER_BTN_SDLTYPEMASK) {
      case SITTER_BTN_SDLJAXIS: {
          sdlevt.type=SDL_JOYAXISMOTION;
          sdlevt.jaxis.which=(evt.btnid&SITTER_BTN_SDLJOYMASK)>>SITTER_BTN_SDLJOYSHIFT;
          sdlevt.jaxis.axis=evt.btnid&SITTER_BTN_SDLINDMASK;
          if (evt.val<0) game->input->mapButtonToButton(&sdlevt,btnid,-AXIS_THRESH,true);
          else game->input->mapButtonToButton(&sdlevt,btnid,AXIS_THRESH);
        } return;
      case SITTER_BTN_SDLKEY: {
          sdlevt.type=SDL_KEYDOWN; 
          sdlevt.key.keysym.sym=(SDLKey)(evt.btnid&SITTER_BTN_SDLINDMASK);
        } break;
      case SITTER_BTN_SDLMBTN: {
          sdlevt.type=SDL_MOUSEBUTTONDOWN; 
          sdlevt.button.button=evt.btnid&SITTER_BTN_SDLINDMASK;
        } break;
      case SITTER_BTN_SDLJBTN: {
          sdlevt.type=SDL_JOYBUTTONDOWN; 
          sdlevt.jbutton.which=(evt.btnid&SITTER_BTN_SDLJOYMASK)>>SITTER_BTN_SDLJOYSHIFT;
          sdlevt.jbutton.button=evt.btnid&SITTER_BTN_SDLINDMASK;
        } break;
      default: sitter_log("event type not supported"); return; // TODO hat, ball...
    }
    game->input->mapButtonToButton(&sdlevt,btnid);
  #endif
}

void InputConfigurator::playerSetupNext() {
  oktoreadaxis=false;
  if (Menu *menu=game->video->getActiveMenu()) {
    plrcfg_btn++;
    switch (plrcfg_btn) {
      case SITTER_BTN_PLR_LEFT: menu->itemv[0]->setLabel("Left"); break; // actually set by beginPlayerSetup
      case SITTER_BTN_PLR_RIGHT: menu->itemv[0]->setLabel("Right"); break;
      case SITTER_BTN_PLR_JUMP: menu->itemv[0]->setLabel("Jump"); break;
      case SITTER_BTN_PLR_DUCK: menu->itemv[0]->setLabel("Duck"); break;
      case SITTER_BTN_PLR_PICKUP: menu->itemv[0]->setLabel("Pick Up"); break;
      case SITTER_BTN_PLR_TOSS: menu->itemv[0]->setLabel("Toss"); break;
      case SITTER_BTN_PLR_ATN: menu->itemv[0]->setLabel("(Un)Focus"); break;
      case SITTER_BTN_PLR_RADAR: menu->itemv[0]->setLabel("Radar"); break;
      default: {
        game->input->promiscuous=false;
        if (Menu *menu=game->video->popMenu()) delete menu;
        plrcfg=0;
        if (--menudepth<=0) delete this;
        return;
      }
    }
    menu->pack();
  } else { plrcfg=0; }
}

/******************************************************************************
 * advanced setup
 *****************************************************************************/
 
void InputConfigurator::beginAdvancedSetup() {
  Menu *menu=new Menu(game,"Input Mappings");
  menu->addItem(new MenuItem(game,"Add mapping...",-1,CFGBTN_ADDMAP));
  menu->addItem(new MenuItem(game,"Choose a mapping to delete it:"));
  #define MAP_NAME_LIMIT 1024
  char mapname[MAP_NAME_LIMIT];
  for (int i=0;i<game->input->mapc;i++) {
    switch (game->input->mapv[i].dsttype) {
      case SITTER_BTNMAP_REVBUTTON: {
          int dstbtnid=game->input->mapv[i].btn.btnid;
          snprintf(mapname,MAP_NAME_LIMIT,"%d: %s !=> %s",i,game->input->mappingSourceToString(i),game->input->btnidToString(dstbtnid));
        } break;
      case SITTER_BTNMAP_BUTTON: {
          int dstbtnid=game->input->mapv[i].btn.btnid;
          snprintf(mapname,MAP_NAME_LIMIT,"%d: %s => %s",i,game->input->mappingSourceToString(i),game->input->btnidToString(dstbtnid));
        } break;
      case SITTER_BTNMAP_AXIS: {
          int msglen=snprintf(mapname,MAP_NAME_LIMIT,"%d: %s => ",i,game->input->mappingSourceToString(i));
          msglen+=snprintf(mapname+msglen,MAP_NAME_LIMIT-msglen,"%s,",game->input->btnidToString(game->input->mapv[i].axis.btnid_lo));
          msglen+=snprintf(mapname+msglen,MAP_NAME_LIMIT-msglen,"%s,",game->input->btnidToString(game->input->mapv[i].axis.btnid_mid));
          msglen+=snprintf(mapname+msglen,MAP_NAME_LIMIT-msglen,"%s",game->input->btnidToString(game->input->mapv[i].axis.btnid_hi));
        } break;
      default: continue;
    }
    mapname[MAP_NAME_LIMIT-1]=0; // ...but seriously, 1024?
    MenuItem *item=new MenuItem(game,mapname,-1,CFGBTN_RMMAP+i);
    menu->addItem(item);
  }
  #undef MAP_NAME_LIMIT
  menu->pack();
  game->video->pushMenu(menu);
  menudepth++;
}

void InputConfigurator::openAddMap_phase1() {
  Menu *menu=new Menu(game,"Add Mapping");
  if (ListMenuItem *item=new ListMenuItem(game,"Source: ",CFGBTN_ADDMAP2)) {
    item->addChoice("keyboard",game->video->loadTexture("icon/ic_keyboard.png",true));
    char joynamebuf[256];
    for (int i=0;i<game->input->joyc;i++) {
      snprintf(joynamebuf,256,"(%d) %s",i,SDL_JoystickName(i));
      joynamebuf[255]=0;
      item->addChoice(joynamebuf,game->video->loadTexture("icon/ic_joystick.png",true));
    }
    item->choose(0);
    menu->addItem(item);
  }
  if (ListMenuItem *item=new ListMenuItem(game,"Output: ",CFGBTN_ADDMAP2)) {
    item->addChoice("Menu",game->video->loadTexture("icon/ic_mainmenu.png",true));
    item->addChoice("Player One",game->video->loadTexture("icon/ic_1up.png",true));
    item->addChoice("Player Two",game->video->loadTexture("icon/ic_2up.png",true));
    item->addChoice("Player Three",game->video->loadTexture("icon/ic_3up.png",true));
    item->addChoice("Player Four",game->video->loadTexture("icon/ic_4up.png",true));
    item->choose(0);
    menu->addItem(item);
  }
  menu->addItem(new MenuItem(game,"Next...",-1,CFGBTN_ADDMAP2));
  menu->pack();
  game->video->pushMenu(menu);
  menudepth++;
}

void InputConfigurator::openAddMap_phase2(int devid,int plrid) {
  addmap_devid=devid;
  addmap_plrid=plrid;
  Menu *menu=new Menu(game,"Add Mapping: Source");
  /* source */
  if (devid) {
    if ((devid<0)||(devid>game->input->joyc)||!game->input->joyv[devid-1]) {
      delete menu;
      sitter_log("InputConfigurator::openAddMap_phase2: joystick %d not found",devid-1);
      return;
    }
    if (ListMenuItem *item=new ListMenuItem(game,"Control: ",CFGBTN_ADDMAP3)) {
      int axisc=SDL_JoystickNumAxes(game->input->joyv[devid-1]);
      int btnc=SDL_JoystickNumButtons(game->input->joyv[devid-1]);
      char namebuf[32];
      for (int i=0;i<axisc;i++) {
        sprintf(namebuf,"Axis %d",i);
        item->addChoice(namebuf);
      }
      for (int i=0;i<btnc;i++) {
        sprintf(namebuf,"Button %d",i);
        item->addChoice(namebuf);
      }
      if (axisc||btnc) item->choose(0);
      menu->addItem(item);
    }
      
  } else {
    if (IntegerMenuItem *item=new IntegerMenuItem(game,"SDL keysym: ",1,SDLK_LAST,1,-1,CFGBTN_ADDMAP3)) {
      menu->addItem(item);
    }
  }
  menu->pack();
  game->video->pushMenu(menu);
  menudepth++;
}

void InputConfigurator::openAddMap_phase3() {
  bool isaxis=false;
  if (addmap_devid>0) {
    if ((addmap_devid<=game->input->joyc)&&game->input->joyv[addmap_devid-1]) {
      int axisc=SDL_JoystickNumAxes(game->input->joyv[addmap_devid-1]);
      isaxis=(addmap_srcbtn<axisc);
    }
  }
  Menu *menu=new Menu(game,"Add Mapping: Output");
  #define INSERT_BTNID_LIST { \
      if (addmap_plrid) { \
        item->addChoice("<none>"); \
        for (int i=0;player_button_names[i].btnid;i++) { \
          item->addChoice(player_button_names[i].name); \
        } \
      } else { \
        item->addChoice("<none>"); \
        for (int i=0;menu_button_names[i].btnid;i++) { \
          item->addChoice(menu_button_names[i].name); \
        } \
      } \
      item->choose(0); \
    }
  if (isaxis) {
    if (ListMenuItem *item=new ListMenuItem(game,"Low Button: ",CFGBTN_ADDMAP4)) {
      INSERT_BTNID_LIST
      menu->addItem(item);
    }
    if (ListMenuItem *item=new ListMenuItem(game,"Mid Button: ",CFGBTN_ADDMAP4)) {
      INSERT_BTNID_LIST
      menu->addItem(item);
    }
    if (ListMenuItem *item=new ListMenuItem(game,"High Button: ",CFGBTN_ADDMAP4)) {
      INSERT_BTNID_LIST
      menu->addItem(item);
    }
    if (IntegerMenuItem *item=new IntegerMenuItem(game,"Low Threshhold: ",-32768,32767,-10000,-1,CFGBTN_ADDMAP4,200)) {
      menu->addItem(item);
    }
    if (IntegerMenuItem *item=new IntegerMenuItem(game,"High Threshhold: ",-32768,32767,10000,-1,CFGBTN_ADDMAP4,200)) {
      menu->addItem(item);
    }
  } else {
    if (ListMenuItem *item=new ListMenuItem(game,"Button: ",CFGBTN_ADDMAP4)) {
      INSERT_BTNID_LIST
      menu->addItem(item);
    }
    if (ListMenuItem *item=new ListMenuItem(game,"Reverse: ",CFGBTN_ADDMAP4)) {
      item->addChoice("false");
      item->addChoice("true");
      item->choose(0);
      menu->addItem(item);
    }
  }
  #undef INSERT_BTNID_LIST
  menu->pack();
  game->video->pushMenu(menu);
  menudepth++;
}

void InputConfigurator::openAddMap_phase4() {
  if (Menu *menu=game->video->popMenu()) { 
    bool isaxis=false;
    if (addmap_devid>0) {
      if ((addmap_devid<=game->input->joyc)&&game->input->joyv[addmap_devid-1]) {
        int axisc=SDL_JoystickNumAxes(game->input->joyv[addmap_devid-1]);
        if (!(isaxis=(addmap_srcbtn<axisc))) addmap_srcbtn-=axisc;
      }
    }
    if (isaxis) {
      int lowindex=((ListMenuItem*)(menu->itemv[0]))->getChoice();
      int midindex=((ListMenuItem*)(menu->itemv[1]))->getChoice();
      int hiindex=((ListMenuItem*)(menu->itemv[2]))->getChoice();
      int lothresh=((IntegerMenuItem*)(menu->itemv[3]))->val;
      int hithresh=((IntegerMenuItem*)(menu->itemv[4]))->val;
      int lowid,midid,highid;
      if (addmap_plrid) {
        lowid=lowindex?(player_button_names[lowindex].btnid|(addmap_plrid<<12)):0;
        midid=midindex?(player_button_names[midindex].btnid|(addmap_plrid<<12)):0;
        highid=hiindex?(player_button_names[hiindex].btnid|(addmap_plrid<<12)):0;
      } else {
        lowid=lowindex?menu_button_names[lowindex].btnid:0;
        midid=midindex?menu_button_names[midindex].btnid:0;
        highid=hiindex?menu_button_names[hiindex].btnid:0;
      }
      SDL_Event sdlevt;
      sdlevt.type=SDL_JOYAXISMOTION;
      sdlevt.jaxis.which=addmap_devid-1;
      sdlevt.jaxis.axis=addmap_srcbtn;
      game->input->mapAxisToButtons(&sdlevt,lothresh,hithresh,lowid,midid,highid);
    } else {
      int btnindex=((ListMenuItem*)(menu->itemv[0]))->getChoice();
      if (btnindex) {
        int btnid=0;
        if (addmap_plrid) btnid=(player_button_names[btnindex].btnid|(addmap_plrid<<12));
        else btnid=menu_button_names[btnindex].btnid;
        bool reverse=((ListMenuItem*)(menu->itemv[1]))->getChoice();
        SDL_Event sdlevt;
        if (addmap_devid) {
          sdlevt.type=SDL_JOYBUTTONDOWN;
          sdlevt.jbutton.which=addmap_devid-1;
          sdlevt.jbutton.button=addmap_srcbtn;
        } else {
          sdlevt.type=SDL_KEYDOWN;
          sdlevt.key.keysym.sym=(SDLKey)addmap_srcbtn;
        }
        game->input->mapButtonToButton(&sdlevt,btnid,1,reverse);
      }
    }
    delete menu;
    menudepth--;
  }
}

#endif
