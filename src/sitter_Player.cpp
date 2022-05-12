#include <string.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Sprite.h"
#include "sitter_SpriteFace.h"
#include "sitter_Game.h"
#include "sitter_VideoManager.h"
#include "sitter_Grid.h"
#include "sitter_Motor.h"
#include "sitter_PlayerSprite.h"
#include "sitter_CannonSprite.h"
#include "sitter_ResourceManager.h"
#include "sitter_AudioManager.h"
#include "sitter_Player.h"

#define CARRY_BLACKOUT_TIME 10

// gravity is suspended while jumping:
#define JUMPMOTOR_N   "0 ; 3 8 10 10 10 10 10 9 8 7 6 5 4 3 2"
#define JUMPMOTOR_F   "0 ; 3 8 10 12 12 12 12 11 10 9 8 7 6 5 4 3 2 1"
#define JUMPMOTOR_C   "0 ; 2 4 6 8 9 9 9 9 8 8 7 6 5 4 3 2 1"
#define JUMPMOTOR_CF  "0 ; 3 6 9 10 10 11 10 10 9 9 8 7 6 5 4 3 2 1"
/* mtr hgt dist dist(standstill)
 * N   3   8    7
 * F   4   9    --
 * C   3   8    7
 * CF  3   9    --
 */
 
#define TOSS_INERTIA 40 // +my velocity * TOSS_VEL_MULTIPLIER
#define TOSS_VEL_MULTIPLIER 1

#define WALKMOTOR       "0 ; ramp 1 6 30 ; 6 ; ramp 4 0 6"
#define TURNAROUNDMOTOR "0 ; -8 -7 -6 -5 -3 -2 -1"

#define PICKUP_HORZ_SLACK 4 // sprite within this tolerance can be picked up

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Player::Player(Game *game):game(game) {
  plrid=0;
  spr=NULL;
  dy=0;
  dx=1;
  carry=NULL;
  carryblackout=0;
  jumpmotor=NULL;
  xclear=false;
  suspended=false;
  wouldwalk=false;
  ducking=false;
  unduck=false;
  duck_panic=false;
  wouldduck=false;
  wantsattention=true;
  ctl_cannon=NULL;
  
  walkmotor=new Motor(); walkmotor->deserialise(WALKMOTOR);
  turnaroundmotor=new Motor(); turnaroundmotor->deserialise(TURNAROUNDMOTOR);
  jumpmotor_n =new Motor();  jumpmotor_n->deserialise(JUMPMOTOR_N);
  jumpmotor_f =new Motor();  jumpmotor_f->deserialise(JUMPMOTOR_F);
  jumpmotor_c =new Motor();  jumpmotor_c->deserialise(JUMPMOTOR_C);
  jumpmotor_cf=new Motor(); jumpmotor_cf->deserialise(JUMPMOTOR_CF);
}

Player::~Player() {
  delete walkmotor;
  delete jumpmotor_n;
  delete jumpmotor_f;
  delete jumpmotor_c;
  delete jumpmotor_cf;
}

void Player::reset() {
  carry=NULL;
  walkmotor->stop(true);
  turnaroundmotor->stop(true);
  jumpmotor=NULL;
  xclear=false;
  wouldwalk=false;
  ducking=false;
  unduck=false;
  duck_panic=false;
  wouldduck=false;
  wantsattention=true;
  ctl_cannon=NULL;
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
void Player::update() {
  if (!spr) return;
  if (spr->beingcarried&&spr->alive&&ctl_cannon&&wouldwalk) updateCannon();
  if (spr->beingcarried||!spr->alive) {
    bool pwouldwalk=wouldwalk;
    if (dx<0) left_off();
    else if (dx>0) right_off();
    wouldwalk=pwouldwalk;
    if (jumpmotor) jump_off();
    return;
  }
  if (unduck) duck_off(); // the "real" duck_off failed, we have to keep trying until it works
  if (wouldwalk&&(walkmotor->getState()==SITTER_MOTOR_IDLE)&&!suspended) {
    if (!ducking&&!wouldduck) walkmotor->start();
  }
  if (int vel=walkmotor->update()+turnaroundmotor->update()) {
    if (vel>0) {
      vel*=dx;
      xclear=(spr->move(vel,0,true)==vel);
    } else xclear=false;
  } else xclear=false;
  if (jumpmotor) {
    spr->gravityok=false;
    if (int vel=jumpmotor->update()) spr->move(0,-vel,true);
    else { jumpmotor=NULL; spr->gravityok=true; spr->gravitymotor->start(); }
  } else spr->gravityok=true;
  if (carryblackout) carryblackout--;
  if (carry) {
    carry->ongoal=spr->ongoal;
    if (!carry->alive) { // eww gross
      toss_on(); toss_off();
    } 
  } 
}

void Player::updateBackground() {
}

void Player::updateCannon() {
  if (dx<0) ctl_cannon->turnCounterClockwise();
  else if (dx>0) ctl_cannon->turnClockwise();
}

void Player::fireCannon() {
  ctl_cannon->fire();
}

/******************************************************************************
 * Find something we can pick up
 * It must share a border with our facing edge, and must be in Game::grp_carry.
 *****************************************************************************/
 
Sprite *Player::findPickupSprite() {
  for (int i=0;i<game->grp_carry->sprc;i++) if (Sprite *pks=game->grp_carry->sprv[i]) {
    if (pks->beingcarried) continue;
    if (spr->r.h==32) { // when ducking, pickup from above
      if (spr->r.bottom()!=pks->r.top()) continue;
      if (spr->r.right()<=pks->r.left()) continue;
      if (spr->r.left()>=pks->r.right()) continue;
    } else {
      if (spr->flop) {
        int diff=pks->r.right()-spr->r.left(); if (diff<0) diff=-diff;
        if (diff>PICKUP_HORZ_SLACK) continue;
      } else {
        int diff=pks->r.left()-spr->r.right(); if (diff<0) diff=-diff;
        if (diff>PICKUP_HORZ_SLACK) continue;
      }
      if (pks->r.bottom()<=spr->r.top()) continue;
      if (pks->r.top()>=spr->r.bottom()) continue;
    }
    return pks;
  }
  return NULL;
}

/******************************************************************************
 * set face
 *****************************************************************************/
 
void Player::walkFace() {
  if (!spr->facec||!spr->alive) return;
  if (spr->r.h==32) return;
  const char *currface=spr->facev[spr->facep]->name;
  bool carrying;
  if (currface[0]=='c') { carrying=true; currface++; } else carrying=false;
  if (!strcmp(currface,"walk")) return;
  char newface[6]; memcpy(newface,"cwalk",6);
  if (!carrying) spr->setFace(newface+1);
  else spr->setFace(newface);
}

void Player::noWalkFace() {
  if (!spr->facec||!spr->alive) return;
  if (spr->r.h==32) return;
  const char *currface=spr->facev[spr->facep]->name;
  bool carrying;
  if (currface[0]=='c') { carrying=true; currface++; } else carrying=false;
  if (!strcmp(currface,"idle")) return;
  char newface[6]; memcpy(newface,"cidle",6);
  if (!carrying) spr->setFace(newface+1);
  else spr->setFace(newface);
}

void Player::carryFace() {
  if (!spr->facec||!spr->alive) return;
  if (spr->r.h==32) return;
  const char *currface=spr->facev[spr->facep]->name;
  if (currface[0]=='c') return;
  int currfacelen=0; while (currface[currfacelen]) currfacelen++;
  if (currfacelen>15) sitter_throw(IdiotProgrammerError,"we're supposed to limit Bill's face names to 15 chars, remember? ('%s')",currface);
  char newface[17]; memcpy(newface+1,currface,currfacelen+1);
  newface[0]='c';
  spr->setFace(newface);
}

void Player::noCarryFace() {
  if (!spr->facec||!spr->alive) return;
  if (spr->r.h==32) return;
  const char *currface=spr->facev[spr->facep]->name;
  if (currface[0]!='c') return;
  spr->setFace(currface+1);
}

/******************************************************************************
 * receive events
 *****************************************************************************/
 
void Player::left_on() {
  if (!spr) return;
  if (!spr->alive) return;
  wouldwalk=true;
  if (!spr->flop) { spr->flop=true; if (carry) carry->flop=!carry->flop; }
  dx=-1;
  if (ducking&&!duck_panic) return;
  if (!ducking) walkFace();
  if (spr->beingcarried) return;
  if (dx>0) turnaroundmotor->start(false,false);
  if (!suspended) walkmotor->start();
}

void Player::left_off() {
  if (!spr) return;
  if (!spr->alive) return;
  if (dx<0) {
    wouldwalk=false;
    noWalkFace();
    walkmotor->stop();
  }
}

void Player::right_on() {
  if (!spr) return;
  if (!spr->alive) return;
  wouldwalk=true;
  if (spr->flop) { spr->flop=false; if (carry) carry->flop=!carry->flop; }
  dx=1;
  if (ducking&&!duck_panic) return;
  if (!ducking) walkFace();
  if (spr->beingcarried) return;
  if (dx<0) turnaroundmotor->start(false,false);
  if (!suspended) walkmotor->start();
}

void Player::right_off() {
  if (!spr) return;
  if (!spr->alive) return;
  if (dx>0) {
    wouldwalk=false;
    noWalkFace();
    walkmotor->stop();
  }
}

void Player::jump_on() {
  if (!spr) return;
  if (!spr->alive) return;
  if (spr->beingcarried) {
    if (ctl_cannon) fireCannon();
    return;
  }
  if (jumpmotor) return;
  if (spr->gravitymotor->getState()!=SITTER_MOTOR_IDLE) return;
  if (suspended) return;
  if (ducking||(carry&&wouldduck)) {
    spr->r.y++;
    if (spr->collides()) spr->r.y--;
    else {
      game->audio->playEffect("jump");
      for (int i=0;i<spr->boundsprites->sprc;i++) spr->boundsprites->sprv[i]->move(0,1,false,false);
    }
    return;
  }
  if (xclear&&(walkmotor->getState()==SITTER_MOTOR_RUN)) {
    if (carry) jumpmotor=jumpmotor_cf;
    else jumpmotor=jumpmotor_f;
  } else {
    if (carry) jumpmotor=jumpmotor_c;
    else jumpmotor=jumpmotor_n;
  }
  jumpmotor->stop(true);
  jumpmotor->start();
  game->audio->playEffect("jump");
}

void Player::jump_off() {
  if (!spr) return;
  if (!spr->alive) return;
  if (!jumpmotor) return;
  jumpmotor->stop();
}

void Player::duck_on() {
  wouldduck=true;
  if (!spr) return;
  if (!spr->alive) return;
  if (carry) return;
  if (ducking) return;
  if (spr->beingcarried) return;
  ducking=true;
  walkmotor->stop();
  int bottom=spr->r.bottom();
  spr->r.h=32;
  spr->r.bottom(bottom);
  spr->setFace("duck");
}

void Player::duck_off() {
  wouldduck=false;
  if (!spr) return;
  if (!spr->alive) return;
  if (!ducking) return;
  Rect restorer(spr->r);
  int bottom=spr->r.bottom();
  spr->r.h=48;
  spr->r.bottom(bottom);
  if (spr->collides()) {
    /* try really hard to push up any obstructions */
    int targety=spr->r.y;
    spr->r.y=restorer.y;
    for (int i=0;i<restorer.h;i++) {
      spr->move(0,-1,true);
      if (spr->r.y<=targety) break;
    }
    if (spr->r.y>targety) {
      spr->r=restorer;
      unduck=true;
      duck_panic=true;
      return;
    }
  }
  ducking=false;
  unduck=false;
  duck_panic=false;
  if (!carry) spr->setFace("idle");
  if (wouldwalk) {
    walkFace();
    walkmotor->start();
  }
}

void Player::pickup_on() {
  if (!spr) return;
  if (!spr->alive) return;
  if (carry||carryblackout) return;
  if (spr->beingcarried) return;
  if (Sprite *lumpkin=findPickupSprite()) {
    Rect restorer(lumpkin->r);
    Rect myrestorer(spr->r);
    lumpkin->r.bottom(spr->r.top());
    if (ducking) lumpkin->r.y-=16;
    lumpkin->r.centerx(spr->r.centerx());
    if (lumpkin->collides()) {
      if (ducking) { // try trading places with the lumpkin, for picking up from duck in tight spaces
        spr->r.bottom(restorer.bottom());
        spr->r.centerx(restorer.centerx());
        lumpkin->r.bottom(spr->r.top()-16);
        lumpkin->r.centerx(spr->r.centerx());
        if (!lumpkin->collides()&&!spr->collides()) goto pickup_ok;
        // ok, one more thing to try: trade vertical position with lumpkin, but keep my horizontal
        spr->r.x=myrestorer.x;
        lumpkin->r.centerx(spr->r.centerx());
        if (!lumpkin->collides()&&!spr->collides()) goto pickup_ok;
      }
      lumpkin->r=restorer;
      spr->r=myrestorer;
    } else {
      pickup_ok:
      if (ducking) duck_off();
      carryFace();
      lumpkin->turnOffGravity();
      spr->boundsprites->addSprite(lumpkin);
      lumpkin->boundsprites->addSprite(spr);
      lumpkin->beingcarried=true;
      lumpkin->rotation=180.0;
      carry=lumpkin;
      carryblackout=CARRY_BLACKOUT_TIME;
      game->grp_carry->removeSprite(lumpkin);
      game->grp_carry->removeSprite(spr);
      lumpkin->pickup(spr);
      game->audio->playEffect("pickup");
      /* if picking it up causes a collision against a KILLBOTTOM cell, kill it (and let go) */
      if (game->grid) {
        int col0=lumpkin->r.x/game->grid->colw;
        int colz=(lumpkin->r.right()-1)/game->grid->colw;
        int row0=lumpkin->r.y/game->grid->rowh;
        int rowz=(lumpkin->r.bottom()-1)/game->grid->rowh;
        for (int col=col0;col<=colz;col++) for (int row=row0;row<=rowz;row++) {
          uint32_t prop=game->grid->getCellProperties(col,row);
          if (prop&SITTER_GRIDCELL_KILLBOTTOM) {
            lumpkin->gridKill(0,-1,col,row);
            return;
          }
        }
      }
    }
  }
}

void Player::pickup_off() {
  if (!spr) return;
  if (!spr->alive) return;
}

void Player::toss_on() {
  if (!spr) return;
  if (!spr->alive) return;
  if (!carry||carryblackout) return;
  carry->gravityok=true; // don't use turnOnGravity, as that would allocate the motor
  noCarryFace();
  spr->boundsprites->removeSprite(carry);
  carry->boundsprites->removeSprite(spr);
  game->grp_carry->addSprite(carry);
  game->grp_carry->addSprite(spr);
  carry->beingcarried=false;
  if (carry->alive) {
    if (!suspended) { // when suspended, it's more "let go" than "toss"
      if (spr->flop) carry->setInertia(-TOSS_INERTIA);
      else carry->setInertia(TOSS_INERTIA);
      carry->setInertia((walkmotor->peek()+turnaroundmotor->peek())*dx*TOSS_VEL_MULTIPLIER); // inertia is additive
      game->audio->playEffect("toss");
    } else {
      game->audio->playEffect("letgo");
      if (wouldwalk) walkmotor->start();
    }
    carry->rotation=0.0;
  }
  carry=NULL;
  carryblackout=CARRY_BLACKOUT_TIME;
}

void Player::toss_off() {
  if (!spr) return;
  if (!spr->alive) return;
}
