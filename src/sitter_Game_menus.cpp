#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_Grid.h"
#include "sitter_Menu.h"
#include "sitter_VideoManager.h"
#include "sitter_AudioManager.h"
#include "sitter_InputManager.h"
#include "sitter_Configuration.h"
#include "sitter_Sprite.h"
#include "sitter_Player.h"
#include "sitter_ResourceManager.h"
#include "sitter_Game.h"

/******************************************************************************
 * menus
 *****************************************************************************/
 
void Game::openMainMenu() {
  if (grid) { delete grid; grid=NULL; }
  grp_all->killAllSprites();
  for (int i=0;i<plrc;i++) plrv[i]->spr=NULL;
  editing_map=false;
  drawhighscores=false;
  
  Menu *menu=new Menu(this,"Sitter");
  if (ListMenuItem *item=new ListMenuItem(this,"",SITTER_BTN_BEGIN)) {
    item->addChoice("One Player",video->loadTexture("icon/ic_1up.png",true));
    item->addChoice("Two Players",video->loadTexture("icon/ic_2up.png",true));
    item->addChoice("Three Players",video->loadTexture("icon/ic_3up.png",true));
    item->addChoice("Four Players",video->loadTexture("icon/ic_4up.png",true));
    item->choose((lvlplayerc>0)?(lvlplayerc-1):0);
    menu->addItem(item);
  }
  if (ListMenuItem *item=new ListMenuItem(this,"Mode: ",SITTER_BTN_BEGIN)) {
    int choice=0;
    char **setv=res->listGridSets();
    if (!setv[0]) sitter_throw(IdiotProgrammerError,"no grid sets");
    int i=0;
    for (char **seti=setv;*seti;seti++) {
      int texid=-1;
      if (!strcmp(*seti,"Cooperative")) texid=video->loadTexture("icon/ic_coop.png",true);
      if (!strcmp(*seti,"Competitive")) texid=video->loadTexture("icon/ic_compet.png",true);
      if (!strcmp(*seti,"Tutorial")) texid=video->loadTexture("icon/ic_tutorial.png",true);
      if (grid_set&&!strcmp(*seti,grid_set)) choice=i;
      item->addChoice(*seti,texid);
      free(*seti);
      i++;
    }
    free(setv);
    item->choose(choice);
    menu->addItem(item);
  }
  menu->addItem(new MenuItem(this,"Settings",video->loadTexture("icon/ic_settings.png",true),SITTER_BTN_CONFIG));
  #ifndef SITTER_WII
    menu->addItem(new MenuItem(this,"Map Editor",video->loadTexture("icon/ic_edit.png",true),SITTER_BTN_EDITOR));
  #endif
  menu->addItem(new MenuItem(this,"Credits",video->loadTexture("icon/ic_credits.png",true),SITTER_BTN_CREDITS));
  menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("mainmenu");
}

void Game::openPauseMenu() {
  Menu *menu=new Menu(this,"Paused");
  if (editing_map) {
    menu->addItem(new MenuItem(this,"Resume",video->loadTexture("icon/ic_resume.png",true),SITTER_BTN_MN_CANCEL));
    menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
    menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  } else {
    menu->addItem(new MenuItem(this,"Resume",video->loadTexture("icon/ic_resume.png",true),SITTER_BTN_MN_CANCEL));
    menu->addItem(new MenuItem(this,"Restart Level",video->loadTexture("icon/ic_restart.png",true),SITTER_BTN_RESTARTLVL));
    menu->addItem(new MenuItem(this,"Next Level",video->loadTexture("icon/ic_next.png",true),SITTER_BTN_NEXTLVL));
    menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
    menu->addItem(new MenuItem(this,"Settings",video->loadTexture("icon/ic_settings.png",true),SITTER_BTN_CONFIG));
    menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  }
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("mainmenu");
}

void Game::openCreditsMenu() {
  Menu *menu=new Menu(this,"Credits");
  menu->itemdiscolor=0x404040ff;
  menu->addItem(new MenuItem(this,"Sitter 1.0, 21 December 2009."));
  menu->addItem(new MenuItem(this,"Licensed under GNU GPL, see enclosed files."));
  menu->addItem(new MenuItem(this,"Code, graphics, music: Andy Sommerville."));
  menu->addItem(new MenuItem(this,"Handwriting: Erin Rose Sommerville."));
  menu->addItem(new MenuItem(this,"Sound Effects: www.a1freesoundeffects.com"));
  menu->addItem(new MenuItem(this,"           and www.freesound.org"));
  menu->pack();
  video->pushMenu(menu);
}

void Game::openConfigMenu() {
  Menu *menu=new Menu(this,"Settings");
  menu->addItem(new MenuItem(this,"Input",video->loadTexture("icon/ic_input.png",true),SITTER_BTN_CFGINPUT));
  menu->addItem(new MenuItem(this,"Audio",video->loadTexture("icon/ic_audio.png",true),SITTER_BTN_CFGAUDIO));
  menu->addItem(new MenuItem(this,"Video",video->loadTexture("icon/ic_video.png",true),SITTER_BTN_CFGVIDEO));
  menu->pack();
  video->pushMenu(menu);
}

void Game::openAllDeadMenu() {
  audio->playEffect("loselevel");
  Menu *menu=new Menu(this,"Game Over");
  menu->addItem(new MenuItem(this,"Everybody is dead!"));
  menu->addItem(new MenuItem(this,"That's not what you were supposed to do!"));
  menu->addItem(new MenuItem(this,"Try Again",video->loadTexture("icon/ic_restart.png",true),SITTER_BTN_RESTARTLVL));
  menu->addItem(new MenuItem(this,"Skip Level",video->loadTexture("icon/ic_next.png",true),SITTER_BTN_NEXTLVL));
  menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
  menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("mainmenu");
}

void Game::openAllGoalMenu() {
  audio->playEffect("winlevel");
  Menu *menu=new Menu(this,"You Win!");
  menu->addItem(new MenuItem(this,"Next Level",video->loadTexture("icon/ic_next.png",true),SITTER_BTN_NEXTLVL));
  menu->addItem(new MenuItem(this,"Play Again",video->loadTexture("icon/ic_restart.png",true),SITTER_BTN_RESTARTLVL));
  menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
  menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("victory");
}

void Game::openMurderMenu() {
  audio->playEffect("loselevel");
  Menu *menu=new Menu(this,"You Win?");
  menu->addItem(new MenuItem(this,"No, you don't! You've killed too many children!"));
  menu->addItem(new MenuItem(this,"Try Again",video->loadTexture("icon/ic_restart.png",true),SITTER_BTN_RESTARTLVL));
  menu->addItem(new MenuItem(this,"Skip Level",video->loadTexture("icon/ic_next.png",true),SITTER_BTN_NEXTLVL));
  menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
  menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("mainmenu");
}

void Game::openPlayersDeadMenu() {
  audio->playEffect("loselevel");
  Menu *menu=new Menu(this,"Game Over");
  menu->addItem(new MenuItem(this,"Who's going to watch the kids now?"));
  menu->addItem(new MenuItem(this,"Try Again",video->loadTexture("icon/ic_restart.png",true),SITTER_BTN_RESTARTLVL));
  menu->addItem(new MenuItem(this,"Skip Level",video->loadTexture("icon/ic_next.png",true),SITTER_BTN_NEXTLVL));
  menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
  menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("mainmenu");
}

void Game::openCompetitiveWinMenu(int winnerid) {
  audio->playEffect("winlevel");
  const char *title;
  switch (winnerid) {
    case 1: title="Player One Wins!"; break;
    case 2: title="Player Two Wins!"; break;
    case 3: title="Player Three Wins!"; break;
    case 4: title="Player Four Wins!"; break;
    default: title="Somebody wins...?";
  }
  Menu *menu=new Menu(this,title);
  menu->addItem(new MenuItem(this,"Next Level",video->loadTexture("icon/ic_next.png",true),SITTER_BTN_NEXTLVL));
  menu->addItem(new MenuItem(this,"Play Again",video->loadTexture("icon/ic_restart.png",true),SITTER_BTN_RESTARTLVL));
  menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
  menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("victory");
}

void Game::openCompetitiveDrawMenu() {
  audio->playEffect("loselevel");
  Menu *menu=new Menu(this,"Draw! Nobody Wins!");
  menu->addItem(new MenuItem(this,"Try Again",video->loadTexture("icon/ic_restart.png",true),SITTER_BTN_RESTARTLVL));
  menu->addItem(new MenuItem(this,"Skip Level",video->loadTexture("icon/ic_next.png",true),SITTER_BTN_NEXTLVL));
  menu->addItem(new MenuItem(this,"Main Menu",video->loadTexture("icon/ic_mainmenu.png",true),SITTER_BTN_MAINMENU));
  menu->addItem(new MenuItem(this,"Quit",video->loadTexture("icon/ic_quit.png",true),SITTER_BTN_QUIT));
  menu->pack();
  video->pushMenu(menu);
  audio->setSong("mainmenu");
}

void Game::openEditorMenu() {
  #ifdef SITTER_WII
    Menu *menu=new Menu(this,"Editor");
    menu->addItem(new MenuItem(this,"Sorry, this feature not available for wii"));
    menu->pack();
    video->pushMenu(menu);
  #else
    if (editor_set) { free(editor_set); editor_set=NULL; }
    Menu *menu=new Menu(this,"Editor");
    menu->addItem(new MenuItem(this,"New Set",-1,SITTER_BTN_EDITORNEWSET));
  
    char **gridlist=res->listGridSets();
    for (int i=0;gridlist[i];i++) {
      menu->addItem(new MenuItem(this,gridlist[i],-1,SITTER_BTN_OPENGRIDSET));
      free(gridlist[i]);
    }
    free(gridlist);
  
    menu->pack();
    video->pushMenu(menu);
  #endif
}

void Game::openEditorSetMenu(const char *setname) {
  if (editor_set) free(editor_set);
  if (!(editor_set=strdup(setname))) sitter_throw(AllocationError,"");
  
  Menu *menu=new Menu(this,setname);
  menu->addItem(new MenuItem(this,"New Grid",-1,SITTER_BTN_EDITORNEW));
  
  char **gridlist=res->listGrids(editor_set);
  for (int i=0;gridlist[i];i++) {
    menu->addItem(new MenuItem(this,gridlist[i],-1,SITTER_BTN_OPENGRID));
    free(gridlist[i]);
  }
  free(gridlist);
  
  menu->pack();
  video->pushMenu(menu);
}

void Game::openQueryDialog(const char *prompt,char **dst,int respid,bool replace) {
  if (*dst&&replace) { free(*dst); *dst=NULL; }
  Menu *menu=new Menu(this,prompt);
  menu->addItem(new TextEntryMenuItem(this,dst,respid));
  menu->pack();
  video->pushMenu(menu);
}

void Game::openAudioMenu() {
  Menu *menu=new Menu(this,"Audio");
  menu->itemdiscolor=0xaa0000ff; // for warning message, if there is one
  if (ListMenuItem *item=new ListMenuItem(this,"Background Music: ",SITTER_BTN_CFGAUDIOOK)) {
    item->addChoice("off",video->loadTexture("icon/ic_nomusic.png",true));
    item->addChoice("on",video->loadTexture("icon/ic_music.png",true));
    item->choose(cfg->getOption_bool("music")?1:0);
    menu->addItem(item);
  }
  if (ListMenuItem *item=new ListMenuItem(this,"Sound Effects: ",SITTER_BTN_CFGAUDIOOK)) {
    item->addChoice("off",video->loadTexture("icon/ic_nosound.png",true));
    item->addChoice("on",video->loadTexture("icon/ic_sound.png",true));
    item->choose(cfg->getOption_bool("sound")?1:0);
    menu->addItem(item);
  }
  menu->addItem(new MenuItem(this,"OK",-1,SITTER_BTN_CFGAUDIOOK));
  if (!cfg->getOption_bool("audio")) {
    menu->addItem(new MenuItem(this,"Note: audio is fully disabled.",-1,0));
    menu->addItem(new MenuItem(this,"Run with option '-A' to reenable.",-1,0));
    menu->addItem(new MenuItem(this,"(when it's turned off this way,",-1,0));
    menu->addItem(new MenuItem(this,"I'm not allowed to turn it back on)",-1,0));
  }
  menu->pack();
  video->pushMenu(menu);
}

/******************************************************************************
 * video settings, wii
 *****************************************************************************/
#ifdef SITTER_WII

void Game::openVideoMenu() {
  Menu *menu=new Menu(this,"Video");
  menu->addItem(new MenuItem(this,"Overscan correction, pixels:"));
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"Horz: ",0,10000,cfg->getOption_int("wii-overscan-x"),-1,SITTER_BTN_CFGVIDEOOK,5)) {
    menu->addItem(item);
  }
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"Vert: ",0,10000,cfg->getOption_int("wii-overscan-y"),-1,SITTER_BTN_CFGVIDEOOK,5)) {
    menu->addItem(item);
  }
  menu->addItem(new MenuItem(this,"Virtual Screen Size:"));
  menu->addItem(new MenuItem(this,"(0 means use TV's size)"));
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"Horz: ",0,10000,cfg->getOption_int("fswidth"),-1,SITTER_BTN_CFGVIDEOOK,16)) {
    menu->addItem(item);
  }
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"Vert: ",0,10000,cfg->getOption_int("fsheight"),-1,SITTER_BTN_CFGVIDEOOK,16)) {
    menu->addItem(item);
  }
  if (ListMenuItem *item=new ListMenuItem(this,"Filter: ",SITTER_BTN_CFGVIDEOOK)) {
    item->addChoice("Auto");
    item->addChoice("Never");
    item->addChoice("Always");
    item->choose(cfg->getOption_int("tex-filter"));
    menu->addItem(item);
  }
  menu->addItem(new MenuItem(this,"OK",-1,SITTER_BTN_CFGVIDEOOK));
  menu->pack();
  video->pushMenu(menu);
}

void Game::closeVideoMenu() {
  if (Menu *menu=video->popMenu()) {
    cfg->setOption_int("wii-overscan-x",((IntegerMenuItem*)(menu->itemv[1]))->val);
    cfg->setOption_int("wii-overscan-y",((IntegerMenuItem*)(menu->itemv[2]))->val);
    cfg->setOption_int("fswidth",((IntegerMenuItem*)(menu->itemv[5]))->val);
    cfg->setOption_int("fsheight",((IntegerMenuItem*)(menu->itemv[6]))->val);
    cfg->setOption_int("tex-filter",((ListMenuItem*)(menu->itemv[7]))->getChoice());
    delete menu;
    video->reconfigure();
  }
}

/******************************************************************************
 * video settings, sdl/gl
 *****************************************************************************/
#else

void Game::openVideoMenu() {
  Menu *menu=new Menu(this,"Video");
  if (ListMenuItem *item=new ListMenuItem(this,"Fullscreen: ",SITTER_BTN_CFGVIDEOOK)) {
    item->addChoice("off");
    item->addChoice("on");
    item->choose(cfg->getOption_bool("fullscreen"));
    menu->addItem(item);
  }
  if (ListMenuItem *item=new ListMenuItem(this,"Allow adjustment: ",SITTER_BTN_CFGVIDEOOK)) {
    item->addChoice("yes");
    item->addChoice("no");
    item->choose(cfg->getOption_bool("fssmart"));
    menu->addItem(item);
  }
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"(full) Width: ",4,10000,cfg->getOption_int("fswidth"),-1,SITTER_BTN_CFGVIDEOOK,4)) {
    menu->addItem(item);
  }
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"(full) Height: ",4,10000,cfg->getOption_int("fsheight"),-1,SITTER_BTN_CFGVIDEOOK,4)) {
    menu->addItem(item);
  }
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"(window) Width: ",4,10000,cfg->getOption_int("winwidth"),-1,SITTER_BTN_CFGVIDEOOK,4)) {
    menu->addItem(item);
  }
  if (IntegerMenuItem *item=new IntegerMenuItem(this,"(window) Height: ",4,10000,cfg->getOption_int("winheight"),-1,SITTER_BTN_CFGVIDEOOK,4)) {
    menu->addItem(item);
  }
  if (ListMenuItem *item=new ListMenuItem(this,"Filter: ",SITTER_BTN_CFGVIDEOOK)) {
    item->addChoice("Auto");
    item->addChoice("Never");
    item->addChoice("Always");
    item->choose(cfg->getOption_int("tex-filter"));
    menu->addItem(item);
  }
  menu->pack();
  video->pushMenu(menu);
}

void Game::closeVideoMenu() {
  if (Menu *menu=video->popMenu()) {
    cfg->setOption_bool("fullscreen",((ListMenuItem*)(menu->itemv[0]))->getChoice());
    cfg->setOption_bool("fssmart",((ListMenuItem*)(menu->itemv[1]))->getChoice());
    cfg->setOption_int("fswidth",((IntegerMenuItem*)(menu->itemv[2]))->val);
    cfg->setOption_int("fsheight",((IntegerMenuItem*)(menu->itemv[3]))->val);
    cfg->setOption_int("winwidth",((IntegerMenuItem*)(menu->itemv[4]))->val);
    cfg->setOption_int("winheight",((IntegerMenuItem*)(menu->itemv[5]))->val);
    cfg->setOption_int("tex-filter",((ListMenuItem*)(menu->itemv[6]))->getChoice());
    delete menu;
    video->reconfigure();
  }
}

#endif
