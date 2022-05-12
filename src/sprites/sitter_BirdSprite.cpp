#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_BirdSprite.h"

#define SPEED 2

#define STATE_REST        0 // sit at nest until timer expires, face=idle
#define STATE_HUNT        1 // fly around, look for children
#define STATE_RETURN      2 // done hunting, flying back to nest
#define STATE_HUNT1       3 // found something to pick up, go after it

#define REST_TIME_MIN 120
#define REST_TIME_MAX 600

#define HUNT_TIME_MAX 1800 // limit only
#define HUNT1_GIVEUP_TIME 30
#define RETURN_GIVEUP_TIME 60

BirdSprite::BirdSprite(Game *game):Sprite(game) {
  nestx=nesty=-1;
  state=STATE_REST;
  stateclock=REST_TIME_MIN+rand()%(REST_TIME_MAX-REST_TIME_MIN+1);
  dx=1;
  history=new SpriteGroup(game);
  target=NULL;
}

BirdSprite::~BirdSprite() {
  delete history;
}

void BirdSprite::update() {
  Sprite::update();
  if (nestx<0) {
    nestx=r.centerx();
    nesty=r.bottom();
  }
  switch (state) {
    case STATE_REST: update_REST(); break;
    case STATE_HUNT: update_HUNT(); break;
    case STATE_RETURN: update_RETURN(); break;
    case STATE_HUNT1: update_HUNT1(); break;
    default: {
        state=STATE_REST;
        stateclock=REST_TIME_MIN+rand()%(REST_TIME_MAX-REST_TIME_MIN+1);
      }
  }
}

void BirdSprite::update_REST() {
  move(0,4); // in lieu of gravity...
  if (--stateclock<0) {
    state=STATE_HUNT;
    stateclock=HUNT_TIME_MAX;
    setFace("fly");
  }
}

void BirdSprite::update_HUNT() {
  if (--stateclock<0) {
    state=STATE_RETURN;
    history->removeAllSprites(); // after unsuccessful hunt, pick up anything (even things already in your nest, oh well)
    return;
  }
  move(0,-1);
  if (!move(dx*SPEED,0,true)) {
    dx=-dx;
    flop=(dx<0);
  }
  int x=(dx>0)?r.right():r.left();
  for (int i=0;i<game->grp_carry->sprc;i++) if (Sprite *spr=game->grp_carry->sprv[i]) {
    if (spr->beingcarried) continue;
    if (x<=spr->r.left()) continue;
    if (x>=spr->r.right()) continue;
    if (spr->r.y<r.bottom()) continue;
    //if ((spr->r.centerx()==nestx)&&(spr->r.bottom()==nesty)) continue;
    if (spr->hasGroup(history)) continue;
    Rect restorer(r);
    int dy=spr->r.top()-r.bottom();
    bool lineofsight=(move(0,dy,false)==dy);
    r=restorer;
    //sitter_log("line of sight = %d",lineofsight);
    if (!lineofsight) return;
    target=spr;
    state=STATE_HUNT1;
    return;
  }
}

void BirdSprite::update_HUNT1() {
  bool xok=false,yok=false;
  int ndx=target->r.centerx()-r.centerx();
  if (ndx>=SPEED) ndx=dx=1;
  else if (ndx<=-SPEED) ndx=dx=-1;
  else if (ndx>0) { xok=move(1,0,true); ndx=0; }
  else if (ndx<0) { xok=move(-1,0,true); ndx=0; }
  flop=(dx<0);
  if (ndx) { xok=move(ndx*SPEED,0); }
  if (target->r.top()>r.bottom()) { yok=move(0,1); }
  else if (target->r.top()<r.bottom()) { yok=move(0,-1); }
  else if (!ndx&&!target->beingcarried) { // if somebody picked up the target (since we first saw it), just hang around until something happens
    nesty-=target->r.h;
    boundsprites->addSprite(target);
    target->boundsprites->addSprite(this);
    target->beingcarried=true;
    target->gravityok=false;
    state=STATE_RETURN;
    history->addSprite(target);
    setFace("cfly");
    return;
  }
  if (!xok&&!yok) {
    if (!--stateclock) {
      state=STATE_HUNT;
      target=NULL;
      stateclock=HUNT_TIME_MAX;
    }
  } else stateclock=HUNT1_GIVEUP_TIME;
}

void BirdSprite::update_RETURN() {
  int ndx=0;
  if (r.centerx()>nestx) ndx=dx=-1;
  else if (r.centerx()<nestx) ndx=dx=1;
  flop=(dx<0);
  bool moved=false;
  if (ndx&&move(ndx,0,true)) moved=true;
  if (r.bottom()>nesty) { if (move(0,-1)) moved=true; }
  else if (r.bottom()<nesty) { if (move(0,1)) moved=true; }
  int xdiff=r.centerx()-nestx; if (xdiff<0) xdiff=-xdiff;
  if ((xdiff<SPEED)&&(r.bottom()==nesty)) {
    state=STATE_REST;
    stateclock=REST_TIME_MIN+rand()%(REST_TIME_MAX-REST_TIME_MIN+1);
    if (target) {
      target->boundsprites->removeSprite(this);
      boundsprites->removeSprite(target);
      target->beingcarried=false;
      nesty+=target->r.h;
      target->gravityok=true;
      // be sure that these are precise: (with history, doesn't matter)
      //target->r.centerx(nestx);
      //target->r.bottom(nesty);
      target=NULL;
    }
    setFace("idle");
  } else if (moved) stateclock=RETURN_GIVEUP_TIME;
  else if (--stateclock<0) {
    if (target) {
      target->boundsprites->removeSprite(this);
      boundsprites->removeSprite(target);
      target->beingcarried=false;
      nesty+=target->r.h;
      target->gravityok=true;
      target=NULL;
      setFace("fly");
    }
    state=STATE_HUNT;
    stateclock=HUNT_TIME_MAX;
  }
}
