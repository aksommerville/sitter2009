#ifdef SITTER_WII

#include <stdio.h>
#include "sitter_Game.h"
#include "sitter_Menu.h"
#include "sitter_VideoManager.h"
#include "sitter_InputManager.h"
#include "sitter_WiiUSB.h"
#include "sitter_InputConfigurator.h"

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
InputConfigurator::InputConfigurator(Game *game):game(game) {

  if (Menu *menu=new Menu(game,"Input")) {
    char pfx[256];
    for (int i=0;i<4;i++) {
      if (game->input->gcpad_connected[i]) {
        sprintf(pfx,"GC Pad #%d: ",i+1);
        if (ListMenuItem *item=new ListMenuItem(game,pfx,SITTER_BTN_MN_OK)) {
          item->addChoice("unused");
          item->addChoice("Player One",game->video->loadTexture("icon/ic_1up.png",true));
          item->addChoice("Player Two",game->video->loadTexture("icon/ic_2up.png",true));
          item->addChoice("Player Three",game->video->loadTexture("icon/ic_3up.png",true));
          item->addChoice("Player Four",game->video->loadTexture("icon/ic_4up.png",true));
          item->choose(game->input->gcpad_plrid[i]);
          menu->addItem(item);
        }
      } else {
        sprintf(pfx,"GC Pad #%d not connected",i+1);
        menu->addItem(new MenuItem(game,pfx));
      }
    }
    for (int i=0;i<4;i++) {
      if (game->input->wiimote_connected[i]) {
        sprintf(pfx,"Wiimote #%d: ",i+1);
        if (ListMenuItem *item=new ListMenuItem(game,pfx,SITTER_BTN_MN_OK)) {
          item->addChoice("unused");
          item->addChoice("Player One",game->video->loadTexture("icon/ic_1up.png",true));
          item->addChoice("Player Two",game->video->loadTexture("icon/ic_2up.png",true));
          item->addChoice("Player Three",game->video->loadTexture("icon/ic_3up.png",true));
          item->addChoice("Player Four",game->video->loadTexture("icon/ic_4up.png",true));
          item->choose(game->input->wiimote_plrid[i]);
          menu->addItem(item);
        }
      } else {
        sprintf(pfx,"Wiimote #%d not connected",i+1);
        menu->addItem(new MenuItem(game,pfx));
      }
    }
    if (game->input->usb) {
      for (int i=0;i<game->input->usb->devc;i++) if (WiiUSB_Device *dev=game->input->usb->devv[i]) {
        if (dev->pname) snprintf(pfx,256,"USB '%s': ",dev->pname);
        else if (dev->vname) snprintf(pfx,256,"USB '%s': ",dev->vname);
        else switch (dev->input_driver) {
          case 1: sprintf(pfx,"Keyboard: "); break;
          case 2: sprintf(pfx,"Mouse: "); break;
          case 3: sprintf(pfx,"USB-HID %x/%x: ",dev->vid,dev->pid); break;
          case 4: sprintf(pfx,"Xbox Joystick: "); break;
          default: sprintf(pfx,"USB %x/%x: ",dev->vid,dev->pid);
        }
        pfx[255]=0;
        if (ListMenuItem *item=new ListMenuItem(game,pfx,SITTER_BTN_MN_OK)) {
          item->addChoice("unused");
          item->addChoice("Player One",game->video->loadTexture("icon/ic_1up.png",true));
          item->addChoice("Player Two",game->video->loadTexture("icon/ic_2up.png",true));
          item->addChoice("Player Three",game->video->loadTexture("icon/ic_3up.png",true));
          item->addChoice("Player Four",game->video->loadTexture("icon/ic_4up.png",true));
          item->choose(game->input->usb_plrid[i]);
          menu->addItem(item);
        }
      }
    }
    menu->pack();
    game->video->pushMenu(menu);
  }
  
}

InputConfigurator::~InputConfigurator() {
  if (game->inputcfg==this) game->inputcfg=NULL;
}

/******************************************************************************
 * event
 *****************************************************************************/
 
bool InputConfigurator::receiveEvent(InputEvent &evt) {
  if (evt.type==SITTER_EVT_BTNDOWN) switch (evt.btnid) {
    case SITTER_BTN_MN_OK: {
        finalise();
        if (Menu *menu=game->video->popMenu()) delete menu;
        delete this;
        return true;
      }
    case SITTER_BTN_MN_CANCEL: {
        finalise();
        if (Menu *menu=game->video->popMenu()) delete menu;
        delete this;
        return true;
      }
  }
  return false;
}

void InputConfigurator::finalise() {
  Menu *menu=game->video->getActiveMenu();
  if (!menu) return;
  for (int i=0;i<4;i++) {
    if (game->input->gcpad_connected[i]) {
      game->input->gcpad_plrid[i]=((ListMenuItem*)(menu->itemv[i]))->getChoice();
    } else game->input->gcpad_plrid[i]=0;
    if (game->input->wiimote_connected[i]) {
      game->input->wiimote_plrid[i]=((ListMenuItem*)(menu->itemv[i+4]))->getChoice();
    } else game->input->wiimote_plrid[i]=0;
  }
  if (game->input->usb) for (int i=0;i<game->input->usb->devc;i++) 
    game->input->usb_plrid[i]=((ListMenuItem*)(menu->itemv[i+8]))->getChoice();
}

#endif
