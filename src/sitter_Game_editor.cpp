#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_VideoManager.h"
#include "sitter_InputManager.h"
#include "sitter_Menu.h"
#include "sitter_Grid.h"
#include "sitter_Player.h"
#include "sitter_Sprite.h"
#include "sitter_ResourceManager.h"
#include "sitter_Game.h"

#define EDITOR_CAMERA_SPEED 4

#define DEFAULT_GRID_WIDTH 20
#define DEFAULT_GRID_HEIGHT 15

/******************************************************************************
 * map editor
 *****************************************************************************/
 
void Game::update_editor() {

  /* animation */
  editor_orn_op+=editor_orn_dop;
  if (editor_orn_op>1.0) { editor_orn_op=1.0; editor_orn_dop=-0.01; }
  if (editor_orn_op<0.2) { editor_orn_op=0.2; editor_orn_dop= 0.01; }
  for (int i=0;i<grp_vis->sprc;i++) {
    grp_vis->sprv[i]->update_animation();
    grp_vis->sprv[i]->opacity=editor_orn_op; // make it obvious that they aren't part of the grid
  }

  /* scroll */
  int gridw=grid->colc*grid->colw;
  int gridh=grid->rowc*grid->rowh;
  if (video->getScreenWidth()>=gridw) camerax=(gridw>>1)-(video->getScreenWidth()>>1);
  else {
    camerax+=cameradx; 
    if (camerax<0) camerax=0; 
    else if (camerax+video->getScreenWidth()>=gridw) camerax=gridw-video->getScreenWidth();
  }
  if (video->getScreenHeight()>=gridh) cameray=(gridh>>1)-(video->getScreenHeight()>>1);
  else {
    cameray+=camerady;
    if (cameray<0) cameray=0; 
    else if (cameray+video->getScreenHeight()>=gridh) cameray=gridh-video->getScreenHeight();
  }
  grp_vis->scrollx=grid->scrollx=camerax;
  grp_vis->scrolly=grid->scrolly=cameray;
  
  /* pen */
  if (!editor_palette) {
    if (editor_mdown&&(editor_selx>=0)&&(editor_sely>=0)&&grid&&(editor_selx<grid->colc)&&(editor_sely<grid->rowc)) {
      int x,y;
      if (input->getPointer(&x,&y)) {
        x+=camerax; y+=cameray;
        x/=grid->colw;
        y/=grid->rowh;
        if ((x>=0)&&(y>=0)&&(x<grid->colc)&&(y<=grid->rowc)) {
          grid->cellv[y*grid->colc+x]=grid->cellv[editor_sely*grid->colc+editor_selx];
        }
      }
    }
  }
  
}

void Game::editorEvent(InputEvent &evt) {
  switch (evt.type) {
    case SITTER_EVT_UNICODE: switch (evt.btnid) {
        case ' ': editor_palette=!editor_palette; editor_mdown=false; break;
        case 's': res->saveGrid(grid,editor_set,editor_grid); break;
        case 'r': drawradar=!drawradar; break;
        case 'i': grid->rotate( 0,-1); grp_all->killAllSprites(); grid->makeSprites(0); break;
        case 'j': grid->rotate(-1, 0); grp_all->killAllSprites(); grid->makeSprites(0); break;
        case 'k': grid->rotate( 0, 1); grp_all->killAllSprites(); grid->makeSprites(0); break;
        case 'l': grid->rotate( 1, 0); grp_all->killAllSprites(); grid->makeSprites(0); break;
        case 'p': { // macro, add 4 players
              if (editor_selx+4>grid->colc) sitter_log("make-players macro ignored, not enough room to the right of selection");
              else {
                grid->addSprite("bill",editor_selx,editor_sely,0x11);
                grid->addSprite("bill",editor_selx,editor_sely,0x21);
                grid->addSprite("bill",editor_selx,editor_sely,0x31);
                grid->addSprite("bill",editor_selx,editor_sely,0x41);
                grid->addSprite("bill",editor_selx+1,editor_sely,0x22);
                grid->addSprite("bill",editor_selx+1,editor_sely,0x32);
                grid->addSprite("bill",editor_selx+1,editor_sely,0x42);
                grid->addSprite("bill",editor_selx+2,editor_sely,0x33);
                grid->addSprite("bill",editor_selx+2,editor_sely,0x43);
                grid->addSprite("bill",editor_selx+3,editor_sely,0x44);
                grp_all->killAllSprites();
                grid->makeSprites(0);
              }
          } break;
        case 'a': { // add sprite
            Menu *menu=new Menu(this,"Add Sprite");
            if (editor_instr) { free(editor_instr); editor_instr=NULL; }
            if (TextEntryMenuItem *item=new TextEntryMenuItem(this,&editor_instr,SITTER_BTN_EDITADDSPR)) {
              menu->addItem(item);
            }
            if (ListMenuItem *item=new ListMenuItem(this,"Player ID: ",SITTER_BTN_EDITADDSPR)) {
              item->addChoice("n/a");
              item->addChoice("One");
              item->addChoice("Two");
              item->addChoice("Three");
              item->addChoice("Four");
              item->choose(0);
              menu->addItem(item);
            }
            if (ListMenuItem *item=new ListMenuItem(this,"Mode: ",SITTER_BTN_EDITADDSPR)) {
              item->addChoice("always");
              item->addChoice("one-player",video->loadTexture("icon/ic_1up.png"));
              item->addChoice("two-player",video->loadTexture("icon/ic_2up.png"));
              item->addChoice("three-player",video->loadTexture("icon/ic_3up.png"));
              item->addChoice("four-player",video->loadTexture("icon/ic_4up.png"));
              item->choose(0);
              menu->addItem(item);
            }
            menu->pack();
            video->pushMenu(menu);
          } break;
        case 'z': { // remove sprite
            Menu *menu=new Menu(this,"Remove Sprite");
            char sprnamebuf[256];
            for (int i=0;i<grid->spawnc;i++) if ((grid->spawnv[i].col==editor_selx)&&(grid->spawnv[i].row==editor_sely)) {
              snprintf(sprnamebuf,256,"%d(%x): %s",i,grid->spawnv[i].plrid,grid->spawnv[i].sprresname);
              sprnamebuf[255]=0;
              menu->addItem(new MenuItem(this,sprnamebuf,-1,SITTER_BTN_EDITRMSPR));
            }
            menu->pack();
            video->pushMenu(menu);
          } break;
        case 'h': { // grid header
            Menu *menu=new Menu(this,"Grid Header");
            menu->addItem(new IntegerMenuItem(this,"Width",1,65536,grid->colc,-1,SITTER_BTN_EDITHDR));
            menu->addItem(new IntegerMenuItem(this,"Height",1,65536,grid->rowc,-1,SITTER_BTN_EDITHDR));
            menu->addItem(new IntegerMenuItem(this,"Column Width",1,256,grid->colw,-1,SITTER_BTN_EDITHDR));
            menu->addItem(new IntegerMenuItem(this,"Row Height",1,256,grid->rowh,-1,SITTER_BTN_EDITHDR));
            if (ListMenuItem *item=new ListMenuItem(this,"Mode: ",SITTER_BTN_EDITHDR)) {
              item->addChoice("Cooperative");
              item->addChoice("Most-Children");
              item->addChoice("Death Match");
              item->choose(grid->play_mode-1);
              menu->addItem(item);
            }
            menu->addItem(new IntegerMenuItem(this,"Kill Tolerance",0,65535,grid->murderlimit,-1,SITTER_BTN_EDITHDR));
            menu->addItem(new IntegerMenuItem(this,"Time Limit (s*60)",0,65535,grid->timelimit,-1,SITTER_BTN_EDITHDR,60));
            if (editor_instr) free(editor_instr);
            if (grid->bgname) { if (!(editor_instr=strdup(grid->bgname))) sitter_throw(AllocationError,""); }
            else editor_instr=NULL;
            menu->addItem(new MenuItem(this,"Background Image or Color:"));
            menu->addItem(new TextEntryMenuItem(this,&editor_instr,SITTER_BTN_EDITHDR));
            menu->addItem(new MenuItem(this,"Song name:"));
            menu->addItem(new TextEntryMenuItem(this,&grid->songname,SITTER_BTN_EDITHDR));
            menu->pack();
            video->pushMenu(menu);
          } break;
      } break;
    case SITTER_EVT_BTNDOWN: switch (evt.btnid) {
        case SITTER_BTN_EDITADDSPR: {
            if (Menu *menu=video->popMenu()) {
              int pid=((ListMenuItem*)(menu->itemv[1]))->getChoice();
              int mid=((ListMenuItem*)(menu->itemv[2]))->getChoice();
              int sprid=grid->addSprite(editor_instr,editor_selx,editor_sely,(mid<<4)|pid);
              grp_all->killAllSprites();
              try {
                grid->makeSprites(0);
              } catch (FileNotFoundError) {
                sitter_log("Sprite '%s' not found, entry not added",editor_instr);
                grid->removeSpawn(sprid);
              }
              delete menu;
            }
            while (Menu *menu=video->popMenu()) delete menu;
          } break;
        case SITTER_BTN_EDITRMSPR: {
            if (Menu *menu=video->popMenu()) {
              if ((menu->selection>=0)&&(menu->selection<menu->itemc)&&menu->itemv[menu->selection]->label) {
                char *tail=0;
                int sprid=strtol(menu->itemv[menu->selection]->label,&tail,0);
                if (tail&&tail[0]=='(') {
                  grid->removeSpawn(sprid);
                  grp_all->killAllSprites();
                  grid->makeSprites(0);
                }
              }
              delete menu;
            }
            while (Menu *menu=video->popMenu()) delete menu;
          } break;
        case SITTER_BTN_EDITHDR: {
            if (Menu *menu=video->popMenu()) {
              if (menu->itemc==11) {
                int colc=((IntegerMenuItem*)(menu->itemv[0]))->val;
                int rowc=((IntegerMenuItem*)(menu->itemv[1]))->val;
                grid->colw=((IntegerMenuItem*)(menu->itemv[2]))->val;
                grid->rowh=((IntegerMenuItem*)(menu->itemv[3]))->val;
                grid->safeResize(colc,rowc);
                grid->play_mode=((ListMenuItem*)(menu->itemv[4]))->getChoice()+1;
                grid->murderlimit=((IntegerMenuItem*)(menu->itemv[5]))->val;
                grid->timelimit=((IntegerMenuItem*)(menu->itemv[6]))->val;
                grid->setBackground(editor_instr);
              } else sitter_log("%s:%d:EDITHDR: expected 11 items in menu, found %d, ignoring response",__FILE__,__LINE__,menu->itemc);
            }
            while (Menu *menu=video->popMenu()) delete menu;
          } break;
        case SITTER_BTN_EDITORPENT: {
            if (Menu *menu=video->popMenu()) {
              if (menu->itemc==12) {
                if (const char *resname=menu->itemv[0]->label) {
                  try {
                    grid->cellpropv[editor_pent_cell]=0;
                    if (((ListMenuItem*)(menu->itemv[1]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_SOLID;
                    if (((ListMenuItem*)(menu->itemv[2]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_GOAL;
                    if (((ListMenuItem*)(menu->itemv[3]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_KILLTOP;
                    if (((ListMenuItem*)(menu->itemv[4]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_KILLBOTTOM;
                    if (((ListMenuItem*)(menu->itemv[5]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_KILLLEFT;
                    if (((ListMenuItem*)(menu->itemv[6]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_KILLRIGHT;
                    if (((ListMenuItem*)(menu->itemv[7]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_GOAL1;
                    if (((ListMenuItem*)(menu->itemv[8]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_GOAL2;
                    if (((ListMenuItem*)(menu->itemv[9]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_GOAL3;
                    if (((ListMenuItem*)(menu->itemv[10]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_GOAL4;
                    if (((ListMenuItem*)(menu->itemv[11]))->getChoice()) grid->cellpropv[editor_pent_cell]|=SITTER_GRIDCELL_SEMISOLID;
                    try {
                      int tid=video->loadTexture(resname,false,0);
                      if (grid->tidv[editor_pent_cell]>=0) video->unloadTexture(grid->tidv[editor_pent_cell]);
                      grid->tidv[editor_pent_cell]=tid;
                    } catch (...) {
                      grid->loadMultiTexture(resname,editor_pent_cell);
                    }
                  } catch (...) { // only loadTexture can throw
                    if (resname[0]) { // empty tile name? they probably meant to clear it; don't print the exception log
                      sitter_log("Error replacing Grid Palette Entry 0x%02x (cell cleared):",editor_pent_cell);
                      sitter_printError();
                    }
                    grid->cellpropv[editor_pent_cell]=0;
                    if (grid->tidv[editor_pent_cell]>=0) video->unloadTexture(grid->tidv[editor_pent_cell]);
                    grid->tidv[editor_pent_cell]=-1;
                  }
                }
              } else sitter_log("%s:%d:EDITORPENT: expected 12 items in menu, found %d, ignoring response",__FILE__,__LINE__,menu->itemc);
              delete menu;
            }
            while (Menu *menu=video->popMenu()) delete menu;
          } break;
        case SITTER_BTN_MSLEFT: {
            if (video->getActiveMenu()) break;
            if (editor_palette) {
              int x,y;
              if (input->getPointer(&x,&y)) {
                x-=(video->getScreenWidth()>>1)-(grid->colw*8);
                y-=(video->getScreenHeight()>>1)-(grid->rowh*8);
                x/=grid->colw;
                y/=grid->rowh;
                if ((x>=0)&&(y>=0)&&(x<16)&&(y<16)) {
                  grid->cellv[editor_sely*grid->colc+editor_selx]=y*16+x;
                }
              }
            } else editor_mdown=true; 
          } break;
        case SITTER_BTN_MSRIGHT: {
            if (video->getActiveMenu()) break;
            if (editor_palette) {
              /* edit palette entry */
              int x,y;
              if (!input->getPointer(&x,&y)) break;
              x-=(video->getScreenWidth()>>1)-(grid->colw*8);
              y-=(video->getScreenHeight()>>1)-(grid->rowh*8);
              x/=grid->colw;
              y/=grid->rowh;
              if ((x<0)||(y<0)||(x>=16)||(y>=16)) break;
              editor_pent_cell=y*16+x;
              char menuhdr[256];
              sprintf(menuhdr,"Cell 0x%02x",y*16+x);
              if (Menu *menu=new Menu(this,menuhdr)) {
                if (editor_instr) free(editor_instr);
                if (const char *texname=video->texidToResname(grid->tidv[y*16+x])) {
                  if (!(editor_instr=strdup(texname))) sitter_throw(AllocationError,"");
                } else editor_instr=NULL;
                menu->addItem(new TextEntryMenuItem(this,&editor_instr,SITTER_BTN_EDITORPENT));
                uint32_t prop=grid->cellpropv[y*16+x];
                #define ADDBOOL(lbl,propid) if (ListMenuItem *item=new ListMenuItem(this,lbl": ",SITTER_BTN_EDITORPENT)) { \
                  item->addChoice("FALSE"); item->addChoice("TRUE"); \
                  item->choose((prop&SITTER_GRIDCELL_##propid)?1:0); \
                  menu->addItem(item); \
                }
                ADDBOOL("Solid",SOLID)
                ADDBOOL("Goal",GOAL)
                ADDBOOL("Kill-top",KILLTOP)
                ADDBOOL("Kill-bottom",KILLBOTTOM)
                ADDBOOL("Kill-left",KILLLEFT)
                ADDBOOL("Kill-right",KILLRIGHT)
                ADDBOOL("Goal-1",GOAL1)
                ADDBOOL("Goal-2",GOAL2)
                ADDBOOL("Goal-3",GOAL3)
                ADDBOOL("Goal-4",GOAL4)
                ADDBOOL("Semi-solid",SEMISOLID)
                #undef ADDBOOL
                menu->pack();
                video->pushMenu(menu);
              }
            } else {
              int x,y;
              if (input->getPointer(&x,&y)) {
                x+=camerax; y+=cameray;
                editor_selx=x/grid->colw; if (editor_selx<0) editor_selx=0; else if (editor_selx>=grid->colc) editor_selx=grid->colc-1;
                editor_sely=y/grid->rowh; if (editor_sely<0) editor_sely=0; else if (editor_sely>=grid->rowc) editor_sely=grid->rowc-1;
              }
            }
          } break;
        default: {
            if (video->getActiveMenu()) break;
            if (SITTER_BTN_ISPLR_JUMP(evt.btnid)) camerady=-EDITOR_CAMERA_SPEED;
            else if (SITTER_BTN_ISPLR_DUCK(evt.btnid)) camerady=EDITOR_CAMERA_SPEED;
            else if (SITTER_BTN_ISPLR_LEFT(evt.btnid)) cameradx=-EDITOR_CAMERA_SPEED;
            else if (SITTER_BTN_ISPLR_RIGHT(evt.btnid)) cameradx=EDITOR_CAMERA_SPEED;
          }
      } break;
    case SITTER_EVT_BTNUP: switch (evt.btnid) {
        case SITTER_BTN_MSLEFT: editor_mdown=false; break;
        default: {
            if (video->getActiveMenu()) break;
            if (SITTER_BTN_ISPLR_JUMP(evt.btnid)) { if (camerady<0) camerady=0; }
            else if (SITTER_BTN_ISPLR_DUCK(evt.btnid)) { if (camerady>0) camerady=0; }
            else if (SITTER_BTN_ISPLR_LEFT(evt.btnid)) { if (cameradx<0) cameradx=0; }
            else if (SITTER_BTN_ISPLR_RIGHT(evt.btnid)) { if (cameradx>0) cameradx=0; }
          }
      } break;
  }
}

void Game::beginEditor(const char *gridresname) {
  if (editor_grid) free(editor_grid);
  if (!(editor_grid=strdup(gridresname))) sitter_throw(AllocationError,"");
  editing_map=true;
  /* must load grid *before* deleting menu! */
  if (grid) delete grid;
  grid=res->loadGrid(editor_set,editor_grid,0);
  while (Menu *menu=video->popMenu()) delete menu;
  for (int i=1;i<=4;i++) removePlayer(i);
  grp_all->killAllSprites();
  for (int i=0;i<plrc;i++) plrv[i]->spr=NULL;
  grid->makeSprites(0);
  lvlplayerc=0;
  camerax=cameray=0;
  cameradx=camerady=0;
  editor_mdown=false;
  editor_selx=editor_sely=0;
  editor_orn_op=1.0;
  editor_orn_dop=-0.01;
  editor_palette=false;
}

void Game::editorNew() {
  editing_map=true;
  if (grid) delete grid;
  grid=new Grid(this,DEFAULT_GRID_WIDTH,DEFAULT_GRID_HEIGHT);
  res->saveGrid(grid,editor_set,editor_grid);
  while (Menu *menu=video->popMenu()) delete menu;
  for (int i=1;i<=4;i++) removePlayer(i);
  grp_all->killAllSprites();
  for (int i=0;i<plrc;i++) plrv[i]->spr=NULL;
  grid->makeSprites(0);
  lvlplayerc=0;
  camerax=cameray=0;
  cameradx=camerady=0;
  editor_mdown=false;
  editor_selx=editor_sely=0;
  editor_orn_op=1.0;
  editor_orn_dop=-0.01;
  editor_palette=false;
}

void Game::editorNewSet() {
  sitter_log("TODO: Game::editorNewSet");
}
