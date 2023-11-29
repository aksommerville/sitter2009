#include <malloc.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#ifdef SITTER_WII
  #include <gccore.h>
  #include <sdcard/wiisd_io.h>
  #include <fat.h>
#endif
#include "sitter_Error.h"
#include "sitter_File.h"
#include "sitter_Configuration.h"
#include "sitter_ResourceManager.h"
#include "sitter_VideoManager.h"
#include "sitter_InputManager.h"
#include "sitter_AudioManager.h"
#include "sitter_Timer.h"
#include "sitter_Menu.h"
#include "sitter_Sprite.h"
#include "sitter_PlayerSprite.h"
#include "sitter_Player.h"
#include "sitter_Grid.h"
#include "sitter_InputConfigurator.h"
#include "sitter_HighScores.h"
#include "sitter_HAKeyboard.h"
#include "sitter_Game.h"

#define PLRV_INCREMENT 4

/* This should be at least the players' walking speed (6).
 * Terminal velocity falling is 8, if this is a little slower it still works 
 * (actually, it looks kind of cool that way).
 */
#define CAMERA_SPEED_LIMIT 6

/* Wait a few frames when the game is over, to make sure it's really over and
 * to let the players catch their breath.
 */
#define VICTORY_DELAY_TIME 60

/* victory state */
#define SITTER_VICTORY_ASKAGAINLATER 0 // nothing to report, game in progress
#define SITTER_VICTORY_ALLDEAD       1 // everyone died
#define SITTER_VICTORY_ALLGOAL       2 // everyone wins (COOP)
#define SITTER_VICTORY_MORDER        3 // all on goal, failed due to murder
#define SITTER_VICTORY_HOLDBREATH    4 // i'm going to report something when the timer expires...
#define SITTER_VICTORY_PLRSDEAD      5 // some children survived, but the babysitters are all dead
#define SITTER_VICTORY_PLAYER        6 // (never returned) +plrid == player <plrid> wins
#define SITTER_VICTORY_PLAYER1       7 // player 1 wins (MOST or SURVIVAL)
#define SITTER_VICTORY_PLAYER2       8 // " 2
#define SITTER_VICTORY_PLAYER3       9 // " 3
#define SITTER_VICTORY_PLAYER4      10 // " 4
#define SITTER_VICTORY_DRAW         11 // draw, in competitive modes

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Game::Game(Configuration *cfg):cfg(cfg) {
  quit=false;
  plrv=NULL; plrc=plra=0;
  grid=NULL;
  editing_map=false;
  editor_set=NULL;
  editor_grid=NULL;
  editor_instr=NULL;
  grid_set=strdup("Cooperative");
  inputcfg=NULL;
  gameover=true;
  menu_fast=false;
  hisc=new HighScores(this);
  drawhighscores=false;
  drawradar=false;
  highscore_name=NULL;
  hakeyboard=NULL;
  lvlplayerc=0;
  
  hisc->readStoredScores();
  
  res=new ResourceManager(this);
  timer=new Timer();
  grp_vis=new SpriteGroup(this);
  grp_upd=new SpriteGroup(this);
  grp_all=new SpriteGroup(this);
  grp_solid=new SpriteGroup(this);
  grp_carry=new SpriteGroup(this);
  grp_quarry=new SpriteGroup(this);
  grp_crushable=new SpriteGroup(this);
  grp_slicable=new SpriteGroup(this);
  grp_alwaysempty=new SpriteGroup(this);
  
  int w,h,videoflags=0;
  const char *videodevice=cfg->getOption_str("video-device");
  if (cfg->getOption_bool("fullscreen")) {
    w=cfg->getOption_int("fswidth");
    h=cfg->getOption_int("fsheight");
    videoflags|=SITTER_VIDEO_FULLSCREEN;
    if (cfg->getOption_bool("fssmart")) videoflags|=SITTER_VIDEO_CHOOSEDIMS;
  } else {
    w=cfg->getOption_int("winwidth");
    h=cfg->getOption_int("winheight");
  }
  video=new VideoManager(this,w,h,videoflags,videodevice);
  video->spr_vis=grp_vis;
  
  input=new InputManager(this);
  input->reconfigure();
  if (cfg->getOption_bool("audio")) audio=new AudioManager(this);
  else audio=new DummyAudioManager(this);
  openMainMenu();
  
  clocktexid=video->loadTexture("andyfont.png");
  killtexid=video->loadTexture("skull.png");
  rabbit_mode=cfg->getOption_bool("rabbit_mode");
  hsfonttexid=video->loadTexture("andyfont.png");
  radartexid=video->loadTexture("radartiles.png",false,0);
}

Game::~Game() {
  // cfg owned by caller
  if (hakeyboard) { delete hakeyboard; hakeyboard=NULL; }
  hisc->writeScores();
  if (inputcfg) { delete inputcfg; inputcfg=NULL; }
  input->generaliseMappings();
  input->writeConfiguration();
  video->unloadTexture(clocktexid);
  video->unloadTexture(killtexid);
  video->unloadTexture(hsfonttexid);
  video->unloadTexture(radartexid);
  while (Menu *menu=video->popMenu()) delete menu;
  if (grid) { delete grid; grid=NULL; }
  delete grp_vis;
  delete grp_upd;
  delete grp_all;
  delete grp_solid;
  delete grp_carry;
  delete grp_quarry;
  delete grp_crushable;
  delete grp_slicable;
  delete grp_alwaysempty;
  delete audio; // delete audio before res -- res owns the sample caches; and after sprites (they might be looping something)
  for (int i=0;i<plrc;i++) delete plrv[i];
  if (plrv) free(plrv);
  delete input;
  delete video;
  delete timer;
  delete res;
  if (editor_set) free(editor_set);
  if (editor_grid) free(editor_grid);
  if (editor_instr) free(editor_instr);
  if (grid_set) free(grid_set);
  if (highscore_name) free(highscore_name);
}

/******************************************************************************
 * player list
 *****************************************************************************/
 
int Game::addPlayer(Player *plr) {
  int plrid=1;
  for (int i=0;i<plrc;i++) {
    if (plr==plrv[i]) return plr->plrid;
    if (plrv[i]->plrid==plrid) {
      plrid_again:
      plrid++;
      for (int j=0;j<i;j++) if (plrv[j]->plrid==plrid) goto plrid_again;
    }
  }
  plr->plrid=plrid;
  if (plrc>=plra) {
    plra+=PLRV_INCREMENT;
    if (!(plrv=(Player**)realloc(plrv,sizeof(void*)*plra))) sitter_throw(AllocationError,"");
  }
  plrv[plrc++]=plr;
  return plrid;
}

Player *Game::getPlayer(int plrid) const {
  for (int i=0;i<plrc;i++) if (plrv[i]->plrid==plrid) return plrv[i];
  return NULL;
}

void Game::removePlayer(int plrid) {
  for (int i=0;i<plrc;i++) if (plrv[i]->plrid==plrid) {
    delete plrv[i];
    plrc--;
    if (i<plrc) memmove(plrv+i,plrv+i+1,sizeof(void*)*(plrc-i));
    return;
  }
}

/******************************************************************************
 * begin game
 *****************************************************************************/
 
void Game::resetGame(const char *gridset,int index,int playerc) {
  audio->invalidateSprites();
  if (grid_set&&(grid_set!=gridset)) free(grid_set);
  if (!(grid_set=strdup(gridset))) sitter_throw(AllocationError,"");
  while (Menu *menu=video->popMenu()) delete menu;
  if (grid) delete grid;
  grid=NULL;
  editing_map=false;
  grp_all->killAllSprites();
  for (int i=playerc+1;i<=4;i++) removePlayer(i);
  drawhighscores=false;
  
  char gridname[12];
  again:
  if (index<0) grid_index=1;
  else grid_index=index;
  snprintf(gridname,12,"%d",grid_index);
  gridname[11]=0;
  try {
    grid=res->loadGrid(grid_set,gridname,playerc);
  } catch (DontUseThisGrid) {
    index++;
    goto again;
  } catch (FileNotFoundError) {
    //sitter_printError();
    openMainMenu();
    for (int i=0;i<plrc;i++) if (plrv[i]) plrv[i]->spr=NULL;
    return;
  }
  while (!getPlayer(playerc)) addPlayer(new Player(this));
  grid->makeSprites(playerc);
  lvlplayerc=playerc;
  if (grid->songname) audio->setSong(grid->songname);
  
  camerax=cameray=INT_MIN;
  murderc=0;
  murderlimit=0; // not even one little murder? c'mon, man...
  play_clock=0;
  gameover=false;
}

void Game::restartLevel() {
  resetGame(grid_set,grid_index,lvlplayerc);
}

void Game::nextLevel() {
  resetGame(grid_set,grid_index+1,lvlplayerc);
}

/******************************************************************************
 * event frame
 *****************************************************************************/
 
 
void Game::updateInput() {
  input->update();
  InputEvent evt;
  bool cancel_menu=false,open_menu=false; // don't do these in the loop, as one changes the behavior of the other
  while (input->popEvent(evt)) {
  
    if (inputcfg&&inputcfg->receiveEvent(evt)) continue;
    if (hakeyboard&&hakeyboard->checkEvent(evt)) continue;
  
    Player *plr=NULL;
    if (SITTER_BTN_ISPLR(evt.btnid)) {
      if (SITTER_BTN_ISPLR1(evt.btnid)) { if (!(plr=getPlayer(1))) ;}//continue; }
      else if (SITTER_BTN_ISPLR2(evt.btnid)) { if (!(plr=getPlayer(2))) ;}//continue; }
      else if (SITTER_BTN_ISPLR3(evt.btnid)) { if (!(plr=getPlayer(3))) ;}//continue; }
      else if (SITTER_BTN_ISPLR4(evt.btnid)) { if (!(plr=getPlayer(4))) ;}//continue; }
    }
    Menu *menu=video->getActiveMenu();
    
    switch (evt.type) {
      case SITTER_EVT_UNICODE: {
          if (menu) menu->receiveUnicode(evt.btnid);
          else if (editing_map) editorEvent(evt);
          else if (evt.btnid=='n') nextLevel(); // TEMP
          else if (evt.btnid=='t') runDebugTest(); // TEMP
        } break;
      case SITTER_EVT_BTNDOWN: switch (evt.btnid) {
          case SITTER_BTN_QUIT: quit=true; break;
          case SITTER_BTN_MN_UP: if (menu) menu->up_on(); break;
          case SITTER_BTN_MN_DOWN: if (menu) menu->down_on(); break;
          case SITTER_BTN_MN_LEFT: if (menu) menu->left_on(); break;
          case SITTER_BTN_MN_RIGHT: if (menu) menu->right_on(); break;
          case SITTER_BTN_MN_OK: if (menu) menu->ok_on(); break;
          case SITTER_BTN_MN_CANCEL: if (menu) cancel_menu=true; break;
          case SITTER_BTN_MENU: if (!menu) open_menu=true; break;
          case SITTER_BTN_BEGIN: if (menu) {
              int playerc=((ListMenuItem*)(menu->itemv[0]))->getChoice()+1;
              const char *gridset=((ListMenuItem*)(menu->itemv[1]))->getChoiceString();
              resetGame(gridset,1,playerc);
            } break;
          case SITTER_BTN_CREDITS: openCreditsMenu(); break;
          case SITTER_BTN_CONFIG: openConfigMenu(); break;
          case SITTER_BTN_RESTARTLVL: restartLevel(); break;
          case SITTER_BTN_NEXTLVL: nextLevel(); break;
          case SITTER_BTN_MAINMENU: while (Menu *m=video->popMenu()) delete m; openMainMenu(); break;
          case SITTER_BTN_EDITOR: openEditorMenu(); break;
          case SITTER_BTN_EDITORNEW: openQueryDialog("Map Name",&editor_grid,SITTER_BTN_EDITORNEW2); break;
          case SITTER_BTN_EDITORNEW2: editorNew(); break;
          case SITTER_BTN_OPENGRID: beginEditor(menu->itemv[menu->selection]->label); break;
          case SITTER_BTN_OPENGRIDSET: openEditorSetMenu(menu->itemv[menu->selection]->label); break;
          case SITTER_BTN_EDITORNEWSET: openQueryDialog("Map Set Name",&editor_set,SITTER_BTN_EDITORNEWSET2); break;
          case SITTER_BTN_EDITORNEWSET2: editorNewSet(); break;
          case SITTER_BTN_CFGINPUT: {
              if (inputcfg) delete inputcfg;
              inputcfg=new InputConfigurator(this);
            } break;
          case SITTER_BTN_CFGAUDIO: openAudioMenu(); break;
          case SITTER_BTN_CFGAUDIOOK: {
              if (menu) {
                bool musicon=((ListMenuItem*)(menu->itemv[0]))->getChoice();
                if (musicon) {
                  cfg->setOption_bool("music",true);
                  audio->setSong("mainmenu");
                } else {
                  audio->setSong(NULL);
                  cfg->setOption_bool("music",false);
                }
                cfg->setOption_bool("sound",((ListMenuItem*)(menu->itemv[1]))->getChoice());
                video->popMenu();
                delete menu;
              }
            } break;
          case SITTER_BTN_CFGVIDEO: openVideoMenu(); break;
          case SITTER_BTN_CFGVIDEOOK: closeVideoMenu(); break;
          case SITTER_BTN_MN_FAST: menu_fast=true; break;
          case SITTER_BTN_HSNAMERTN: receiveHighScoreName(); break;
          default: {
              if (editing_map) editorEvent(evt);
              if (!menu) { // when a menu is active, players get BTNUP but not BTNDOWN
                  if (SITTER_BTN_ISPLR_LEFT(evt.btnid)) { if (plr) plr->left_on(); }
                  else if (SITTER_BTN_ISPLR_RIGHT(evt.btnid)) { if (plr) plr->right_on(); }
                  else if (SITTER_BTN_ISPLR_JUMP(evt.btnid)) { if (plr) plr->jump_on(); }
                  else if (SITTER_BTN_ISPLR_DUCK(evt.btnid)) { if (plr) plr->duck_on(); }
                  else if (SITTER_BTN_ISPLR_PICKUP(evt.btnid)) { if (plr) plr->pickup_on(); }
                  else if (SITTER_BTN_ISPLR_TOSS(evt.btnid)) { if (plr) plr->toss_on(); }
                  else if (SITTER_BTN_ISPLR_ATN(evt.btnid)) { if (plr) plr->wantsattention=!plr->wantsattention; }
                  else if (SITTER_BTN_ISPLR_RADAR(evt.btnid)) { if (plr) drawradar=!drawradar; }
              }
            }
        } break;
      case SITTER_EVT_BTNUP: switch (evt.btnid) {
          case SITTER_BTN_MN_UP: if (menu) menu->up_off(); break;
          case SITTER_BTN_MN_DOWN: if (menu) menu->down_off(); break;
          case SITTER_BTN_MN_LEFT: if (menu) menu->left_off(); break;
          case SITTER_BTN_MN_RIGHT: if (menu) menu->right_off(); break;
          case SITTER_BTN_MN_OK: if (menu) menu->ok_off(); break;
          case SITTER_BTN_MN_CANCEL: if (menu) menu->cancel_off(); break;
          case SITTER_BTN_MENU: break;
          case SITTER_BTN_MN_FAST: menu_fast=false; break;
          default: {
              if (editing_map) editorEvent(evt);
              else {
                if (SITTER_BTN_ISPLR_LEFT(evt.btnid)) { if (plr) plr->left_off(); }
                else if (SITTER_BTN_ISPLR_RIGHT(evt.btnid)) { if (plr) plr->right_off(); }
                else if (SITTER_BTN_ISPLR_JUMP(evt.btnid)) { if (plr) plr->jump_off(); }
                else if (SITTER_BTN_ISPLR_DUCK(evt.btnid)) { if (plr) plr->duck_off(); }
                else if (SITTER_BTN_ISPLR_PICKUP(evt.btnid)) { if (plr) plr->pickup_off(); }
                else if (SITTER_BTN_ISPLR_TOSS(evt.btnid)) { if (plr) plr->toss_off(); }
              }
            }
        } break;
    }
    
  }
  if (open_menu) openPauseMenu();//openMainMenu();
  else if (cancel_menu) {
    if (Menu *menu=video->popMenu()) {
      delete menu;
      audio->playEffect("menucancel");
    }
    if (!video->getActiveMenu()) {
      if (!editing_map) {
        if (grid&&!gameover) audio->setSong(grid->songname);
        else if (grid) input->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MAINMENU,1));
        else input->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_QUIT,1));
      }
    }
  }
}

/******************************************************************************
 * main
 *****************************************************************************/
 
void Game::highScoreCheck(int plrid) {
  drawhighscores=true;
  hsresult=hisc->registerScore(grid_set,grid_index,lvlplayerc,play_clock,murderc);
  if (hsresult) {
    //if (editor_instr) { free(editor_instr); editor_instr=NULL; }
    char prompt[256];
    switch (plrid) {
      case 1: sprintf(prompt,"Player One gets #%d! Name:",hsresult); break;
      case 2: sprintf(prompt,"Player Two gets #%d! Name:",hsresult); break;
      case 3: sprintf(prompt,"Player Three gets #%d! Name:",hsresult); break;
      case 4: sprintf(prompt,"Player Four gets #%d! Name:",hsresult); break;
      default: sprintf(prompt,"New high score, #%d. Enter name:",hsresult);
    }
    openQueryDialog(prompt,&highscore_name,SITTER_BTN_HSNAMERTN,false);
    if (!hakeyboard) hakeyboard=new HAKeyboard(this);
  }
}

void Game::receiveHighScoreName() {
  hisc->setScoreName(grid_set,grid_index,lvlplayerc,hsresult,highscore_name);
  video->highscore_dirty=true;
  if (Menu *menu=video->popMenu()) delete menu;
  if (hakeyboard) { delete hakeyboard; hakeyboard=NULL; }
}
 
int Game::getVictoryState() {
  if (gameover) return SITTER_VICTORY_ASKAGAINLATER;
  /* examine sprites and players */
  if (!grp_quarry->sprc) {
    if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_ALLDEAD;
    return SITTER_VICTORY_HOLDBREATH;
  }
  bool alldead=true,allgoal=true,plrsdead=true;
  int goalc[4]={0,0,0,0}; // how many on each player's goal?
  for (int i=0;i<grp_quarry->sprc;i++) if (Sprite *spr=grp_quarry->sprv[i]) {
    if (spr->alive) alldead=false;
    if (spr->alive&&!spr->ongoal) allgoal=false;
    if (spr->alive&&spr->ongoal&&spr->goalid) { // doesn't count if goal's owner is not playing or is dead
      if (Player *plr=getPlayer(spr->goalid)) {
        if (plr->spr&&plr->spr->alive) goalc[spr->goalid-1]++;
        else allgoal=false;
      } else allgoal=false;
    }
  }
  int playeralivec=0;
  int playeraliveid=0; // meaningless if playeralivec!=1
  for (int i=0;i<plrc;i++) {
    if (!plrv[i]->spr) continue;
    if (plrv[i]->spr->alive) { plrsdead=false; playeralivec++; playeraliveid=plrv[i]->plrid; }
  }
  /* order is important: */
  if (alldead) {
    if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_ALLDEAD;
    return SITTER_VICTORY_HOLDBREATH;
  }
  if (plrsdead) {
    if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_PLRSDEAD;
    return SITTER_VICTORY_HOLDBREATH;
  }
  if (grid&&(grid->play_mode==SITTER_GRIDMODE_SURVIVAL)) { // last living player wins
    if (playeralivec==1) {
      if (++victorydelay>=VICTORY_DELAY_TIME) {
        return SITTER_VICTORY_PLAYER+playeraliveid;
      }
    }
  }
  if (allgoal) {
    if ((!grid&&murderc)||(grid&&(murderc>grid->murderlimit))) {
      if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_MORDER;
      return SITTER_VICTORY_HOLDBREATH;
    }
    if (grid&&(grid->play_mode==SITTER_GRIDMODE_MOST)) { // competitive...
      int bestc=-1,bestid=-1;
      bool tie=false;
      for (int i=0;i<4;i++) 
        if (goalc[i]>bestc) { tie=false; bestc=goalc[i]; bestid=i; }
        else if (goalc[i]==bestc) tie=true;
      if (bestid<0) { // malformed grid? all quarries on goal, but none on any player-goal. call it a draw.
        if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_DRAW;
        return SITTER_VICTORY_HOLDBREATH;
      } else if (tie) {
        if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_DRAW;
        return SITTER_VICTORY_HOLDBREATH;
      } else {
        if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_PLAYER1+bestid;
        return SITTER_VICTORY_HOLDBREATH;
      }
    } else { // cooperative...
      if (++victorydelay>=VICTORY_DELAY_TIME) return SITTER_VICTORY_ALLGOAL;
      return SITTER_VICTORY_HOLDBREATH;
    }
  }
  victorydelay=0;
  return SITTER_VICTORY_ASKAGAINLATER;
}
 
void Game::update() {
  if (!gameover) play_clock++;
  
  /* extra pre-frame hacks */
  for (int i=0;i<grp_solid->sprc;i++) {
    grp_solid->sprv[i]->belted=false;
  }
  if (hakeyboard) hakeyboard->update();
  
  if (video->getActiveMenu()) {
    for (int i=0;i<plrc;i++) plrv[i]->updateBackground();
    grp_upd->updateBackground();
  } else {
    grp_upd->update();
    for (int i=0;i<plrc;i++) {
      plrv[i]->update();
      if (rabbit_mode) plrv[i]->jump_on();
    }
    grp_vis->sort1();
  }
  if (grid) {
    /* center the camera on all living players */
    int centerx=0,centery=0,pc=0;
    for (int i=0;i<plrc;i++) {
      if (!plrv[i]->spr) continue;
      if (!plrv[i]->spr->alive) continue;
      if (!plrv[i]->wantsattention) continue;
      centerx+=plrv[i]->spr->r.centerx();
      centery+=plrv[i]->spr->r.centery();
      pc++;
    }
    if (pc) { // all players die? the camera stays put
      centerx/=pc; centery/=pc;
      /* compare to previous position, don't get a speeding ticket */
      int dx=centerx-camerax;
      int dy=centery-cameray;
      if (dx||dy) {
        if (dx<-CAMERA_SPEED_LIMIT) dx=-CAMERA_SPEED_LIMIT;
        else if (dx>CAMERA_SPEED_LIMIT) dx=CAMERA_SPEED_LIMIT;
        if (dy<-CAMERA_SPEED_LIMIT) dy=-CAMERA_SPEED_LIMIT;
        else if (dy>CAMERA_SPEED_LIMIT) dy=CAMERA_SPEED_LIMIT;
        if (camerax==INT_MIN) camerax=centerx; else camerax+=dx;
        if (cameray==INT_MIN) cameray=centery; else cameray+=dy;
        Rect camera(video->getBounds());
        camera.centerx(camerax);
        camera.centery(cameray);
        camera.trap(grid->getBounds());
        grid->scrollx=camera.x;
        grid->scrolly=camera.y;
        grp_vis->scrollx=grid->scrollx;
        grp_vis->scrolly=grid->scrolly;
      }
    }
  } else grp_vis->scrollx=grp_vis->scrolly=0;
  if (!video->getActiveMenu()) {
    int vs=getVictoryState();
    switch (vs) {
      case SITTER_VICTORY_ALLDEAD: gameover=true; drawhighscores=true; hsresult=0; openAllDeadMenu(); break;
      case SITTER_VICTORY_ALLGOAL: gameover=true; drawhighscores=true; openAllGoalMenu(); highScoreCheck(0); break;
      case SITTER_VICTORY_MORDER: gameover=true; drawhighscores=true; hsresult=0; openMurderMenu(); break;
      case SITTER_VICTORY_PLRSDEAD: gameover=true; drawhighscores=true; hsresult=0; openPlayersDeadMenu(); break;
      case SITTER_VICTORY_PLAYER1: gameover=true; drawhighscores=true; openCompetitiveWinMenu(1); highScoreCheck(1); break;
      case SITTER_VICTORY_PLAYER2: gameover=true; drawhighscores=true; openCompetitiveWinMenu(2); highScoreCheck(2); break;
      case SITTER_VICTORY_PLAYER3: gameover=true; drawhighscores=true; openCompetitiveWinMenu(3); highScoreCheck(3); break;
      case SITTER_VICTORY_PLAYER4: gameover=true; drawhighscores=true; openCompetitiveWinMenu(4); highScoreCheck(4); break;
      case SITTER_VICTORY_DRAW: gameover=true; drawhighscores=true; hsresult=0; openCompetitiveDrawMenu(); break;
    }
  }
}
 
void Game::mmain() {
  /* hack -- ignore any events from the first frame (joystick initialisation, etc) */
  { input->update();
    InputEvent evt; while (input->popEvent(evt)) ;
  }
  
  while (!quit) {
    updateInput();
    #ifdef SITTER_LINUX_DRM
      // DRM gives us reliable vsync delay. Don't bother with manual timing.
      // (also we're not doing the editor)
      audio->update();
      video->update();
      update();
    #else
      for (int i=timer->tick();i-->0;) {
        audio->update();
        video->update();
        if (editing_map) update_editor();
        else {
          update();
        }
      }
    #endif
    video->draw();
  }
}

/******************************************************************************
 * debuggery
 *****************************************************************************/
 
void Game::runDebugTest() {
  sitter_log("************************ begin debug test *****************************");
  
  sitter_log("no test. write one before you try to run it, brainiac.");
  
  _done_:
  sitter_log("************************  end debug test  *****************************");
}
