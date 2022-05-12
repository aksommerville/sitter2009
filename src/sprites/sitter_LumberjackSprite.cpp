#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_Motor.h"
#include "sitter_VideoManager.h"
#include "sitter_SpriteFace.h"
#include "sitter_AudioManager.h"
#include "sitter_LumberjackSprite.h"

/******************************************************************************
 * ChainsawSprite (created by Lumberjack when he gets picked up)
 *****************************************************************************/
 
class ChainsawSprite : public Sprite {
public:

  Sprite *blade;
  bool carry;
  Sprite *carrier;
  int audioloopref;

  ChainsawSprite(Game *game,Sprite *blade):Sprite(game) {
    this->blade=blade;
    addGroup(game->grp_all);
    addGroup(game->grp_vis);
    addGroup(game->grp_upd);
    addGroup(game->grp_solid);
    addGroup(game->grp_carry);
    turnOnGravity();
    edge_collisions=true;
    grid_collisions=true;
    sprite_collisions=true;
    texid=game->video->loadTexture("jack/sawbase.png",false);
    pushable=true;
    r.w=16;
    r.h=32;
    r.y=blade->r.y;
    if (blade->flop) {
      flop=true;
      r.x=blade->r.right();
    } else {
      flop=false;
      r.right(blade->r.x);
    }
    blade->boundsprites->addSprite(this);
    boundsprites->addSprite(blade);
    carry=false;
    identity=SITTER_SPRIDENT_CHAINSAW;
    audioloopref=-1;
  }
  
  ~ChainsawSprite() {
    // stopLoop is safe for negative argument. just make sure that only one of us has the "real" reference.
    game->audio->stopLoop(audioloopref);
  }
  
  void pickup(Sprite *picker) {
    r.w=32;
    r.h=16;
    r.y+=16;
    r.centerx(picker->r.centerx());
    rotation=0.0;
    game->video->unloadTexture(texid);
    texid=game->video->loadTexture("jack/sawbaseup.png",false);
    blade->flop=false;
    blade->rotation=270.0;
    blade->r.x=r.x;
    blade->r.bottom(r.y);
    picker->boundsprites->addSprite(this);
    picker->boundsprites->addSprite(blade);
    blade->boundsprites->removeSprite(this);
    carry=true;
    carrier=picker;
  }
  
  void update() {
    Sprite::update();
    if (beingcarried) {
      if (!hasGroup(game->grp_vis)) { // in cannon
        if (blade->hasGroup(game->grp_vis)) {
          beingcarried=false;
          addGroup(game->grp_vis);
          addGroup(game->grp_solid);
          Motor *temp_motor;
          temp_motor=rocketmotorx; rocketmotorx=blade->rocketmotorx; blade->rocketmotorx=temp_motor;
          temp_motor=rocketmotory; rocketmotory=blade->rocketmotory; blade->rocketmotory=temp_motor;
          if (rocketmotorx) {
            blade->flop=flop=(rocketmotorx->peek()<0);
            if (flop) r.left(blade->r.right());
            else r.right(blade->r.left());
            r.y=blade->r.y;
          }
          boundsprites->addSprite(blade);
          blade->boundsprites->addSprite(this);
          carry=false;
          carrier=NULL;
          return;
        }
      }
      //flop=false;
      //blade->flop=false;
      //r.centerx(carrier->r.centerx());
      //r.bottom(carrier->r.top());
      blade->r.x=r.x;
      blade->r.bottom(r.y);
      blade->beingcarried=true;
    } else {
      if (blade->beingcarried&&!blade->hasGroup(game->grp_vis)) { // hacky; it's in the cannon
        beingcarried=true;
        removeGroup(game->grp_vis);
        removeGroup(game->grp_solid);
        return;
      }
      blade->beingcarried=false;
      if (carry) {
        flop=carrier->flop; // always toss blade-side out. trust me, that's how chainsaws work. ;)
        rotation=0.0;
        r.w=16;
        r.h=32;
        r.y-=16;
        game->video->unloadTexture(texid);
        texid=game->video->loadTexture("jack/sawbase.png",false);
        carry=false;
        blade->rotation=0.0;
        carrier->boundsprites->removeSprite(blade);
        blade->boundsprites->removeSprite(carrier);
      }
      if (flop) {
        blade->r.right(r.x);
        blade->r.y=r.y;
      } else {
        blade->r.x=r.right();
        blade->r.y=r.y;
      }
      blade->flop=flop;
    }
  }
  
};

/******************************************************************************
 * Lumberjack (Jack the chainsaw kid)
 *****************************************************************************/

#define SPEED 1
#define HAPPY_TIME 60 // how long after rediscovering the chainsaw do we sit and look thrilled?
#define TURN_DELAY_TIME 40

LumberjackSprite::LumberjackSprite(Game *game):WalkingChildSprite(game) {
  flop=false;

  chainsaw=new Sprite(game);
  chainsaw->addGroup(game->grp_all);
  chainsaw->addGroup(game->grp_upd);
  chainsaw->addGroup(game->grp_vis);
  chainsaw->addGroup(game->grp_solid);
  chainsaw->r.x=r.right();
  chainsaw->r.y=r.y;
  chainsaw->r.w=32;
  chainsaw->r.h=32;
  chainsaw->slicer=true;
  chainsaw->grid_collisions=true;
  chainsaw->sprite_collisions=true;
  chainsaw->edge_collisions=true;
  chainsaw->collide_unslicable=true;
  if (SpriteFace *face=new SpriteFace(game,"idle")) {
    face->addFrame("jack/blade-0.png",2);
    face->addFrame("jack/blade-1.png",2);
    face->addFrame("jack/blade-2.png",2);
    face->addFrame("jack/blade-3.png",2);
    chainsaw->addFace(face);
    chainsaw->setFace("idle");
  }
  boundsprites->addSprite(chainsaw);
  chainsaw->boundsprites->addSprite(this);

  dx=1;
  happytime=0;
  turndelay=0;
  audioloopref=game->audio->playLoop("chainsaw",chainsaw);
}

LumberjackSprite::~LumberjackSprite() {
  game->audio->stopLoop(audioloopref);
}

void LumberjackSprite::update() {
  if (!alive) {
    Sprite::update();
    return;
  }
  if (!beingcarried&&!chainsaw) { 
    if (!dx) { setFace("walk"); dx=((rand()&1)?1:-1); }
    int predx=dx;
    if (happytime) {
      ChildSprite::update(); // not WalkingSprite
      Sprite *foundsaw=NULL;
      for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
        if (spr->identity!=SITTER_SPRIDENT_CHAINSAW) continue;
        if (spr->beingcarried) continue; // no fighting over the chainsaw...
        if ((spr->r.bottom()<=r.top())||(spr->r.top()>=r.bottom())) continue;
        if ((spr->r.left()==r.right())||(spr->r.right()==r.left())) { // don't bother checking direction
          foundsaw=spr;
          break;
        }
      }
      if (!foundsaw) { happytime=0; setFace("idle"); return; }
      if (--happytime<=0) {
        chainsaw=foundsaw->boundsprites->sprv[0];
        boundsprites->addSprite(chainsaw);
        chainsaw->boundsprites->removeAllSprites();
        chainsaw->boundsprites->addSprite(this);
        setFace("wsaw");
        audioloopref=((ChainsawSprite*)foundsaw)->audioloopref;
        ((ChainsawSprite*)foundsaw)->audioloopref=-1;
        delete foundsaw;
        return;
      }
      return;
    } else {
      WalkingChildSprite::update(); 
      if (dx!=predx) { // something stopped us walking, is it.... a chainsaw?
        for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
          if (spr->identity!=SITTER_SPRIDENT_CHAINSAW) continue;
          if (spr->beingcarried) continue; // no fighting over the chainsaw...
          if ((spr->r.bottom()<=r.top())||(spr->r.top()>=r.bottom())) continue;
          if ((spr->r.left()==r.right())||(spr->r.right()==r.left())) { // don't bother checking direction
            if (spr->boundsprites->sprc!=1) continue;
            dx=-dx;
            flop=(dx<0);
            happytime=HAPPY_TIME;
            setFace("happy");
            return;
          }
        }
      }
    }
    if (happytime) { happytime=0; setFace("idle"); }
    return; 
  }
  // not carried, has chainsaw...
  if (happytime) { happytime=0; setFace("idle"); }
  ChildSprite::update();
  if (!beingcarried) {
    if (!move(SPEED*dx,0)) {
      if (!turndelay) {
        turndelay=TURN_DELAY_TIME;
      } else if (!--turndelay) {
        dx=-dx;
        // trade places with blade:
        Rect swap(r);
        r=chainsaw->r;
        chainsaw->r=swap;
      }
    }
    flop=(dx<0);
  }
  if (chainsaw) {
    if (flop) {
      chainsaw->r.right(r.x);
      chainsaw->r.y=r.y;
      chainsaw->flop=true;
    } else {
      chainsaw->r.x=r.right();
      chainsaw->r.y=r.y;
      chainsaw->flop=false;
    }
  }
}

void LumberjackSprite::pickup(Sprite *picker) {
  setFace("idle");
  dx=0;
  if (chainsaw) {
    boundsprites->removeSprite(chainsaw);
    chainsaw->boundsprites->removeSprite(this);
    ChainsawSprite *goodbyesaw=new ChainsawSprite(game,chainsaw);
    goodbyesaw->audioloopref=audioloopref;
    audioloopref=-1;
    chainsaw=0;
  }
}

void LumberjackSprite::gridKill(int dx,int dy,int col,int row) {
  if (!alive) return;
  pickup(NULL);
  WalkingChildSprite::gridKill(dx,dy,col,row);
}

void LumberjackSprite::crush() {
  if (!alive) return;
  pickup(NULL);
  WalkingChildSprite::crush();
}

void LumberjackSprite::slice() {
  if (!alive) return;
  pickup(NULL);
  WalkingChildSprite::slice();
}
