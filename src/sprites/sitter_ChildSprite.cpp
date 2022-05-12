#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_Grid.h"
#include "sitter_Motor.h"
#include "sitter_AudioManager.h"
#include "sitter_SpriteFace.h"
#include "sitter_AnimateOnceSprite.h"
#include "sitter_ChildSprite.h"

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
ChildSprite::ChildSprite(Game *game):Sprite(game) {
  crushed=false;
  ismutant=false;
}

ChildSprite::~ChildSprite() {
}

/******************************************************************************
 * events
 *****************************************************************************/
 
void ChildSprite::update() {
  if (alive) {
    update_gravity();
    update_inertia();
  } else if (crushed) {
    //move(0,4);
  }
  update_animation();
}

void ChildSprite::updateBackground() {
  update_animation();
}
 
void ChildSprite::gridKill(int dx,int dy,int col,int row) {
  if (!alive) return;
  alive=false;
  pushable=false;
  gravityok=false;
  removeGroup(game->grp_carry);
  removeGroup(game->grp_crushable);
  removeGroup(game->grp_slicable);
  // ...but definitely stay in grp_solid
  if (hasGroup(game->grp_quarry)) game->murderc++;
  if (ismutant) setFace("mimpale"); else setFace("impale");
  if (rocketmotorx) rocketmotorx->stop(true);
  if (rocketmotory) rocketmotory->stop(true);
  if (dx<0) {
    rotation=90.0;
    r.centerx((col+1)*game->grid->colw-dx-1);
  } else if (dx>0) {
    rotation=270.0;
    r.centerx(col*game->grid->colw-dx);
  } else if (dy<0) {
    rotation=180.0;
    r.centery((row+1)*game->grid->rowh-dy-1);
  } else {
    rotation=0.0;
    r.centery(row*game->grid->rowh-dy);
  }
  sitter_makeExplosion(game,r);
  game->audio->playEffect("impale",this);
}

void ChildSprite::crush() {
  alive=false;
  crushed=true;
  if (hasGroup(game->grp_quarry)) game->murderc++;
  removeGroup(game->grp_crushable);
  removeGroup(game->grp_slicable);
  removeGroup(game->grp_solid);
  removeGroup(game->grp_carry);
  sprite_collisions=false;
  if (rocketmotorx) rocketmotorx->stop(true);
  if (rocketmotory) rocketmotory->stop(true);
  while (move(0,32)>0) ;
  r.y+=(r.h>>1);
  layer=-1;
  setFace("splatter");
  if (beingcarried) removeGroup(game->grp_vis); // just disappear (don't make splatter pile in mid-air)
  sitter_makeExplosion(game,r);
  game->audio->playEffect("crush",this);
}

void ChildSprite::slice() {
  alive=false;
  if (hasGroup(game->grp_quarry)) game->murderc++;
  removeGroup(game->grp_crushable);
  removeGroup(game->grp_slicable);
  removeGroup(game->grp_solid);
  removeGroup(game->grp_carry);
  removeGroup(game->grp_vis); 
  if (rocketmotorx) rocketmotorx->stop(true);
  if (rocketmotory) rocketmotory->stop(true);
  sitter_makeExplosion(game,r);
  game->audio->playEffect("slice",this);
}

void ChildSprite::mutate() {
  if (ismutant||!alive) return;
  ismutant=setFace("midle");
}
