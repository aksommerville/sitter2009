#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_AudioManager.h"
#include "sitter_VideoManager.h"
#include "sitter_CrusherSprite.h"

#define LAMPW  8
#define LAMPH  8
#define LAMP_YOFFSET 6
#define BOOTW 64
#define BOOTH 16
#define ARMW  32
#define CRUSH_TIME_MIN 400
#define CRUSH_TIME_MAX 600
#define CRUSH_WARNING_TIME  200 // lights rotate faster this close to crushification
#define CRUSH_PANIC_TIME    120 // lights stop rotating, start blinking
#define PANIC_FRAMEDELAY      8 // frames per blink

#define BOOT_CRUSH_SPEED  8
#define BOOT_RETURN_SPEED 1 // unused, always 1

CrusherSprite::CrusherSprite(Game *game):Sprite(game) {
  layer=100;
  bootspr=armspr=0;
  for (int i=0;i<SITTER_CRUSHER_LAMPC;i++) lampv[i]=0;
  lampstate=0;
  lampdstate=1;
  crushclock=CRUSH_TIME_MIN+(rand()%(CRUSH_TIME_MAX-CRUSH_TIME_MIN));
  makeSprites();
}

CrusherSprite::~CrusherSprite() {
  // don't delete sub-sprites; they are in Game::grp_all
}

void CrusherSprite::update() {
  Sprite::update();
  updateLamps();
  if (crushclock<0) { updateCrushing(); return; }
  if (--crushclock<0) commenceCrushification();
  /* reposition sprites (do this every frame, in case we make crushers moveable somehow) */
  bootspr->r.top(r.bottom());
  bootspr->r.centerx(r.centerx());
  armspr->r.top(r.bottom());
  armspr->r.centerx(r.centerx());
  for (int i=0;i<SITTER_CRUSHER_LAMPC;i++) {
    lampv[i]->r.y=r.y+LAMP_YOFFSET;
    lampv[i]->r.centerx(r.x+((i+1)*r.w)/(SITTER_CRUSHER_LAMPC+1));
  }
}

void CrusherSprite::updateLamps() {
  if (crushclock<CRUSH_PANIC_TIME) {
    uint8_t tintamt=(((crushclock/PANIC_FRAMEDELAY)&1)?0x00:0xff);
    if (!(crushclock%(PANIC_FRAMEDELAY*2))) game->audio->playEffect("crusher-warn",this);
    for (int i=0;i<SITTER_CRUSHER_LAMPC;i++) lampv[i]->tint=coretint[i]|tintamt;
  } else {
    int multiplier=1;
    if (crushclock<CRUSH_WARNING_TIME) multiplier=2;
    lampstate+=lampdstate*multiplier; 
    if (lampstate<0) { lampstate=0; lampdstate=-lampdstate; } 
    else if (lampstate>255) { lampstate=255; lampdstate=-lampdstate; }
    for (int i=0;i<SITTER_CRUSHER_LAMPC;i++) {
      int peak=(i*255)/(SITTER_CRUSHER_LAMPC-1);
      int displacement=lampstate-peak; if (displacement<0) displacement=-displacement;
      lampv[i]->tint=coretint[i]|(255-displacement);
    }
  }
}

void CrusherSprite::updateCrushing() {
  armspr->r.top(r.bottom());
  armspr->r.centerx(r.centerx());
  bootspr->r.centerx(r.centerx());
  if (returning) {
    if (bootspr->r.y==r.bottom()) {
      crushclock=CRUSH_TIME_MIN+(rand()%(CRUSH_TIME_MAX-CRUSH_TIME_MIN));
      armspr->removeGroup(game->grp_vis);
    } else bootspr->r.y--;
  } else {
    if (!bootspr->move(0,BOOT_CRUSH_SPEED)) {
      returning=true;
      game->audio->playEffect("crusher-land",this);
    } else {
      for (int i=0;i<game->grp_crushable->sprc;i++) if (bootspr->collides_sprite(game->grp_crushable->sprv[i])) {
        game->grp_crushable->sprv[i]->crush();
        i=-1; // Sprite::crush will likely remove this sprite from grp_crushable and cause us to skip one
      }
      for (int i=0;i<game->grp_solid->sprc;i++) if (bootspr->collides_sprite(game->grp_solid->sprv[i])) {
        bootspr->r.y-=BOOT_CRUSH_SPEED;
        returning=true;
        game->audio->playEffect("crusher-land",this);
        break;
      }
    }
  }
  armspr->r.h=bootspr->r.y-r.bottom();
}

void CrusherSprite::commenceCrushification() {
  returning=false;
  armspr->addGroup(game->grp_vis);
  armspr->r.h=0;
}

void CrusherSprite::makeSprites() {
  if (bootspr) delete bootspr;
  if (armspr) delete armspr;
  for (int i=0;i<SITTER_CRUSHER_LAMPC;i++) delete lampv[i];
  
  bootspr=new Sprite(game);
  bootspr->addGroup(game->grp_all);
  bootspr->addGroup(game->grp_vis);
  bootspr->addGroup(game->grp_upd);
  bootspr->addGroup(game->grp_solid);
  bootspr->r.w=BOOTW;
  bootspr->r.h=BOOTH;
  bootspr->r.top(r.bottom());
  bootspr->r.centerx(r.centerx());
  bootspr->texid=game->video->loadTexture("crusher/boot.png");
  bootspr->grid_collisions=true;
  bootspr->edge_collisions=true;
  bootspr->layer=101;
  
  armspr=new Sprite(game);
  armspr->addGroup(game->grp_all);
  armspr->addGroup(game->grp_solid);
  armspr->r.w=ARMW;
  armspr->r.h=1;
  armspr->r.top(r.bottom());
  armspr->r.centerx(r.centerx());
  armspr->texid=game->video->loadTexture("crusher/arm.png",false,0); // don't filter; we're going to stretch vertically and there may be artifacts
  armspr->layer=101;
  
  for (int i=0;i<SITTER_CRUSHER_LAMPC;i++) {
    coretint[i]=rand()<<8;
    switch (rand()%6) {
      case 0: coretint[i]=0xff000000; break;
      case 1: coretint[i]=0xffff0000; break;
      case 2: coretint[i]=0xff00ff00; break;
      case 3: coretint[i]=0x00ffff00; break;
      case 4: coretint[i]=0x00ff0000; break;
      case 5: coretint[i]=0x8080ff00; break;
    }
    lampv[i]=new Sprite(game);
    lampv[i]->layer=101;
    lampv[i]->addGroup(game->grp_all);
    lampv[i]->addGroup(game->grp_vis);
    lampv[i]->r.w=LAMPW;
    lampv[i]->r.h=LAMPH;
    lampv[i]->r.y=r.y+LAMP_YOFFSET;
    lampv[i]->r.centerx(((i+1)*r.w)/(SITTER_CRUSHER_LAMPC+1));
    lampv[i]->texid=game->video->loadTexture("crusher/lamp.png");
  }
}
