#include <math.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_AudioManager.h"
#include "sitter_Player.h"
#include "sitter_PlayerSprite.h"
#include "sitter_AnimateOnceSprite.h"
#include "sitter_Motor.h"
#include "sitter_SpriteFace.h"
#include "sitter_VideoManager.h"
#include "sitter_CannonSprite.h"

#ifndef PI
  #define PI 3.1415926535897931
#endif

#define BARREL_YOFFSET 32
#define BALL_LIMIT_W 48
#define BALL_LIMIT_H 48
#define BARREL_RETURN_SPEED 1.0 // degrees, coming in
#define BARREL_MOVE_SPEED 1.0 // degress, going out
#define BARREL_ROTATION_LIMIT 90.0 // degrees, either way
#define BOREDOM_LIMIT 300 // this long with a ball and no activity, fire it (it might be the only player)
#define BREATHER_TIME 60

CannonSprite::CannonSprite(Game *game):Sprite(game) {
  layer=200;
  ball=NULL;
  boredom=0;
  breather=0;
  
  barrel=new Sprite(game);
  barrel->addGroup(game->grp_all);
  barrel->addGroup(game->grp_vis);
  barrel->addGroup(game->grp_solid);
  barrel->layer=199;
  barrel->texid=game->video->loadTexture("cannon/barrel.png");
  barrel->r.w=64;
  barrel->r.h=64;
  
}

CannonSprite::~CannonSprite() {
}

void CannonSprite::update() {
  Sprite::update();
  /* position barrel */
  barrel->r.bottom(r.bottom()-BARREL_YOFFSET);
  barrel->r.centerx(r.centerx());
  /* look for ammo */
  if (!ball) {
    if (breather>0) { breather--; return; }
    if (barrel->rotation<-BARREL_RETURN_SPEED) barrel->rotation+=BARREL_RETURN_SPEED;
    else if (barrel->rotation<0.0) barrel->rotation=0.0;
    else if (barrel->rotation>BARREL_RETURN_SPEED) barrel->rotation-=BARREL_RETURN_SPEED;
    else if (barrel->rotation>0.0) barrel->rotation=0.0;
    else {
      for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
        if (spr->r.w>BALL_LIMIT_W) continue;
        if (spr->r.h>BALL_LIMIT_H) continue;
        if (spr->beingcarried) continue; // not likely!
        if (spr->r.bottom()!=barrel->r.top()) continue;
        if (spr->r.left()<barrel->r.left()) continue; // must be *contained* horizontally
        if (spr->r.right()>barrel->r.right()) continue;
        if (spr->boundsprites->sprc) continue; // hacky, don't swallow anything with a bound sprite (eg player+carry)
        ball=spr;
        ball->removeGroup(game->grp_vis); // don't draw
        ball->r.centerx(barrel->r.centerx()); // don't participate in physics
        ball->r.centery(barrel->r.centery());
        ball->gravityok=false;
        ball->beingcarried=true;
        ball->enterCannon(this);
        setFace("active");
        boredom=0;
        return;
      }
    }
  } else {
    ball->r.centerx(barrel->r.centerx());
    ball->r.centery(barrel->r.centery());
    if (boredom++>BOREDOM_LIMIT) {
      if (onlyOnePlayerAndHesInTheCannon()) fire();
      boredom=0;
    }
  }
}

bool CannonSprite::onlyOnePlayerAndHesInTheCannon() {
  return false; // TODO this function is not relevant if player can fire himself
  if (!ball) return false;
  for (int i=0;i<4;i++) if (Player *plr=game->getPlayer(i)) {
    if (!plr->spr) continue;
    if (!plr->spr->alive) continue;
    if (plr->spr!=ball) return false;
  }
  return true;
}

/******************************************************************************
 * events from controller
 *****************************************************************************/
 
void CannonSprite::turnClockwise() {
  if (!ball) return;
  if (barrel->rotation>BARREL_ROTATION_LIMIT-BARREL_MOVE_SPEED) barrel->rotation=BARREL_ROTATION_LIMIT;
  else if (barrel->rotation<BARREL_ROTATION_LIMIT) barrel->rotation+=BARREL_MOVE_SPEED;
  boredom=0;
}

void CannonSprite::turnCounterClockwise() {
  if (!ball) return;
  if (barrel->rotation<-BARREL_ROTATION_LIMIT+BARREL_MOVE_SPEED) barrel->rotation=-BARREL_ROTATION_LIMIT;
  else if (barrel->rotation>-BARREL_ROTATION_LIMIT) barrel->rotation-=BARREL_MOVE_SPEED;
  boredom=0;
}

void CannonSprite::fire() {
  if (!ball) return;
  ball->addGroup(game->grp_vis);
  ball->gravityok=true;
  ball->beingcarried=false;
  makeRocketMotors(ball);
  ball->exitCannon(this);
  ball=NULL;
  setFace("idle");
  boredom=0;
  game->audio->playEffect("cannon",this);
  breather=BREATHER_TIME;
  if (AnimateOnceSprite *blast=new AnimateOnceSprite(game)) {
    blast->rotation=barrel->rotation;
    blast->r.w=64;
    blast->r.h=64;
    blast->layer=barrel->layer-1;
    if (SpriteFace *face=new SpriteFace(game,"idle")) {
      for (int i=0;i<6;i++) { // keep it under BREATHER_TIME (60)
        face->addFrame("cannon/blast-0.png",5);
        face->addFrame("cannon/blast-1.png",5);
      }
      blast->addFace(face);
      blast->setFace("idle");
    }
    blast->addGroup(game->grp_all);
    blast->addGroup(game->grp_vis);
    blast->addGroup(game->grp_upd);
    blast->r.centerx(barrel->r.centerx()+sin((barrel->rotation*PI)/180)*32);
    blast->r.centery(barrel->r.centery()-cos((barrel->rotation*PI)/180)*32);
  }
}

#define INITIAL_BALL_DISTANCE 32
#define ROCKET_T 26.0
#define ROCKET_DECAY 0.99
#define ROCKET_CLIP 3.0 // stop delivering velocities when t<=clip

void CannonSprite::makeRocketMotors(Sprite *spr) {
  double fdx=sin((barrel->rotation*PI)/180.0);
  double fdy=-cos((barrel->rotation*PI)/180.0);
  spr->r.centerx(barrel->r.centerx()+fdx*INITIAL_BALL_DISTANCE);
  spr->r.centery(barrel->r.centery()+fdy*INITIAL_BALL_DISTANCE);
  if (ball->rocketmotorx) delete ball->rocketmotorx;
  if (ball->rocketmotory) delete ball->rocketmotory;
  spr->rocketmotorx=new Motor();
  spr->rocketmotory=new Motor();
  double t=ROCKET_T; // initial velocity, pixels
  double decay=ROCKET_DECAY;
  while (1) {
    int dx=lround(fdx*t);
    int dy=lround(fdy*t);
    if (!dx&&!dy) break;
    if (dx) spr->rocketmotorx->appendStream(SITTER_MOTOR_START,dx);
    if (dy) spr->rocketmotory->appendStream(SITTER_MOTOR_START,dy);
    t*=decay;
    if (t<=ROCKET_CLIP) break;
  }
  spr->rocketmotorx->start(true);
  spr->rocketmotory->start(true);
  //spr->gravityok=false;
}

/******************************************************************************
 * controller
 *****************************************************************************/
 
#define BUTTON_PRESS_DELAY 10
#define BUTTON_ON_HEIGHT   16
#define BUTTON_OFF_HEIGHT  24 // must agree with sprite definition
 
CannonButtonSprite::CannonButtonSprite(Game *game):Sprite(game) {
  cannon=NULL;
  active=false;
  pressdelay=0;
  sprite_collisions=true;
}

CannonButtonSprite::~CannonButtonSprite() {
}

void CannonButtonSprite::update() {
  if (!cannon) findCannon();
  Sprite::update();
  bool foot=false;
  if (pressdelay>0) { pressdelay--; goto actuate; }
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
    if (spr->r.bottom()!=r.top()) continue;
    if (spr->r.right()<=r.left()) continue;
    if (spr->r.left()>=r.right()) continue;
    /* make sure it will not be a collision when we're depressed */
    if (!active) {
      //sitter_log("testing");
      Rect srestorer(spr->r);
      Rect mrestorer(r);
      r.h=BUTTON_ON_HEIGHT;
      r.bottom(mrestorer.bottom());
      //sitter_log("move: %d => %d",BUTTON_OFF_HEIGHT-BUTTON_ON_HEIGHT,spr->move(0,BUTTON_OFF_HEIGHT-BUTTON_ON_HEIGHT,false));
      bool coll=(spr->move(0,BUTTON_OFF_HEIGHT-BUTTON_ON_HEIGHT,false,false)!=BUTTON_OFF_HEIGHT-BUTTON_ON_HEIGHT);
      r=mrestorer;
      spr->r=srestorer;
      if (coll) continue;
    }
    foot=true;
    break;
  }
  //sitter_log("foot=%d",foot);
  if (foot&&!active) {
    active=true;
    int bottom=r.bottom();
    r.h=BUTTON_ON_HEIGHT;
    r.bottom(bottom);
    setFace("on");
    pressdelay=BUTTON_PRESS_DELAY;
    if (cannon&&(identity==SITTER_SPRIDENT_CBTN_FIRE)) cannon->fire();
  } else if (!foot&&active) {
    int bottom=r.bottom();
    r.h=BUTTON_OFF_HEIGHT;
    r.bottom(bottom);
    if (collides()) { // can't turn off
      r.h=BUTTON_ON_HEIGHT;
      r.bottom(bottom);
    } else {
      active=false;
      setFace("off");
    }
  }
  actuate:
  if (cannon&&active) switch (identity) {
    case SITTER_SPRIDENT_CBTN_CLK: cannon->turnClockwise(); break;
    case SITTER_SPRIDENT_CBTN_CTR: cannon->turnCounterClockwise(); break;
  }
}

void CannonButtonSprite::findCannon() {
  int bestdist=0x7fffffff;
  for (int i=0;i<game->grp_all->sprc;i++) if (Sprite *spr=game->grp_all->sprv[i]) {
    if (spr->identity!=SITTER_SPRIDENT_CANNON) continue;
    int dx=spr->r.centerx()-r.centerx(); if (dx<0) dx=-dx;
    int dy=spr->r.centery()-r.centery(); if (dy<0) dy=-dy;
    if (dx+dy<bestdist) {
      bestdist=dx+dy;
      cannon=(CannonSprite*)spr;
    }
  }
}
