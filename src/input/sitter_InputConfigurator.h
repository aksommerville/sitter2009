#ifdef SITTER_WII
  #include "sitter_InputConfigurator_wii.h"
#else

#ifndef SITTER_INPUTCONFIGURATOR_H
#define SITTER_INPUTCONFIGURATOR_H

class Game;
class InputEvent;

class InputConfigurator {
public:

  typedef struct {
    int joyid,axisid;
  } ActiveAxisEntry;
  ActiveAxisEntry *activeaxisv; int activeaxisc;
  bool hasactiveaxis,oktoreadaxis;

  Game *game;
  int menudepth;
  int plrcfg; // which player we are setting up (simple mode)
  int plrcfg_btn;
  int addmap_devid,addmap_plrid,addmap_srcbtn;
  
  InputConfigurator(Game *game);
  ~InputConfigurator();
  
  /* If true, I processed the event and you should not look at it.
   */
  bool receiveEvent(InputEvent &evt);
  
  void beginPlayerSetup(int plrid);
  bool playerSetupEvent(InputEvent &evt);
  void playerSetupNext();
  void mapPlayerSetup(InputEvent &evt);
  
  void beginAdvancedSetup();
  void openAddMap_phase1();
  void openAddMap_phase2(int devid,int plrid); // devid==0 means keyboard, plrid==0 means menu
  void openAddMap_phase3();
  void openAddMap_phase4();
  
  void setAxis(int joyid,int axisid,int val);
  
};

#endif
#endif
