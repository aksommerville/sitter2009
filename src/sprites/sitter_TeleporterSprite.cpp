#include <math.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_AudioManager.h"
#include "sitter_VideoManager.h"
#include "sitter_Grid.h"
#include "sitter_SpriteFace.h"
#include "sitter_Motor.h"
#include "sitter_TeleporterSprite.h"

#define AROCKIN_TIME 60 // does not need to be the same as "on" face time, but it's pretty that way
#define AROCKIN_PERIOD 15 // period of sine wave, frames
#define AROCKIN_DISP 8 // displacement of sine wave, degrees
#ifndef PI
  #define PI 3.1415926535897931
#endif

/******************************************************************************
 * teleporter (input half)
 *****************************************************************************/
 
TeleporterSprite::TeleporterSprite(Game *game):Sprite(game) {
  layer=201;
  receptacle=NULL;
  arockin_counter=0;
  arockin_motor=new Motor();
  for (int i=0;i<AROCKIN_PERIOD;i++) {
    double s=sin((i*PI*2)/AROCKIN_PERIOD)*AROCKIN_DISP;
    arockin_motor->appendStream(SITTER_MOTOR_RUN,lround(s));
  }
  arockin_motor->start();
}

void TeleporterSprite::update() {
  Sprite::update();
  if (!receptacle) { // find the nearest receptacle
    int dist=0x7fffffff;
    for (int i=0;i<game->grp_all->sprc;i++) if (Sprite *spr=game->grp_all->sprv[i]) {
      if (spr->identity!=SITTER_SPRIDENT_TELEPORTERREC) continue;
      int dx=spr->r.centerx()-r.centerx(); if (dx<0) dx=-dx;
      int dy=spr->r.centery()-r.centery(); if (dy<0) dy=-dy;
      if (dx+dy<dist) { dist=dx+dy; receptacle=(TeleporterRecSprite*)spr; }
    }
  }
  if (arockin_counter>0) {
    arockin_counter--;
    rotation=arockin_motor->update();
  } else rotation=0.0;
  /* teleport anything in grp_carry if its left,right,bottom match mine and receptacle is available */
  if (!arockin_counter&&receptacle&&receptacle->ready()) {
    for (int i=0;i<game->grp_carry->sprc;i++) if (Sprite *spr=game->grp_carry->sprv[i]) {
      if (spr->r.x!=r.x) continue;
      if (spr->r.right()!=r.right()) continue;
      if (spr->r.bottom()!=r.bottom()) continue;
      teleport(spr);
    }
  }
}

void TeleporterSprite::teleport(Sprite *spr) {
  game->audio->playEffect("teleport",this);
  setFace("on");
  arockin_counter=AROCKIN_TIME;
  arockin_motor->start(true);
  spr->mutate();
  spr->r.centerx(receptacle->r.centerx());
  spr->r.bottom(receptacle->r.top());
  /* impale */
  if (game->grid) {
    int col0=spr->r.x/game->grid->colw;
    int colz=(spr->r.right()-1)/game->grid->colw;
    int row0=spr->r.y/game->grid->rowh;
    int rowz=(spr->r.bottom()-1)/game->grid->rowh;
    for (int col=col0;col<=colz;col++) for (int row=row0;row<=rowz;row++) {
      uint32_t prop=game->grid->getCellProperties(col,row);
      if (prop&SITTER_GRIDCELL_KILLBOTTOM) { spr->gridKill( 0,-1,col,row); return; }
      if (prop&SITTER_GRIDCELL_KILLTOP   ) { spr->gridKill( 0, 1,col,row); return; }
      if (prop&SITTER_GRIDCELL_KILLLEFT  ) { spr->gridKill(-1, 0,col,row); return; }
      if (prop&SITTER_GRIDCELL_KILLRIGHT ) { spr->gridKill( 1, 0,col,row); return; }
    }
  }
}

/******************************************************************************
 * teleporter receptacle (output half)
 *****************************************************************************/
 
TeleporterRecSprite::TeleporterRecSprite(Game *game):Sprite(game) {
  teleporter=NULL;

  fx=new Sprite(game);
  fx->addGroup(game->grp_all);
  fx->addGroup(game->grp_upd);
  fx->layer=201;
  fx->r.w=32;
  fx->r.h=32;
  if (SpriteFace *face=new SpriteFace(game,"idle")) {
    char namebuf[64];
    for (int i=0;i<12;i++) {
      sprintf(namebuf,"teleporter/recfx-%d.png",i);
      face->addFrame(namebuf,4);
    }
    fx->addFace(face);
    fx->setFace("idle");
  }
  fxvis=false;
  
}

void TeleporterRecSprite::update() {
  Sprite::update();
  if (!teleporter) { // find the nearest teleporter
    int dist=0x7fffffff;
    for (int i=0;i<game->grp_all->sprc;i++) if (Sprite *spr=game->grp_all->sprv[i]) {
      if (spr->identity!=SITTER_SPRIDENT_TELEPORTER) continue;
      int dx=spr->r.centerx()-r.centerx(); if (dx<0) dx=-dx;
      int dy=spr->r.centery()-r.centery(); if (dy<0) dy=-dy;
      if (dx+dy<dist) { dist=dx+dy; teleporter=(TeleporterSprite*)spr; break; }
    }
  }
  if (fxvis&&beingcarried) { fx->removeGroup(game->grp_vis); fxvis=false; }
  else if (!fxvis&&!beingcarried) { fx->addGroup(game->grp_vis); fxvis=true; }
  fx->r.centerx(r.centerx());
  fx->r.bottom(r.top());
}

bool TeleporterRecSprite::ready() {
  if (beingcarried) return false;
  Rect restorer(r);
  r.set(r.x,r.y-32,32,32);
  bool rtn=!collides();
  r=restorer;
  return rtn;
}
