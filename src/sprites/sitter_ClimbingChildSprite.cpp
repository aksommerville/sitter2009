#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_ClimbingChildSprite.h"

#define CLIMB_SPEED 1
#define MOUNT_DX  8
#define MOUNT_DY 10 // move up by this much to test mounting (to avoid a climbing-on air effect around corners)

ClimbingChildSprite::ClimbingChildSprite(Game *game):WalkingChildSprite(game) {
  climbing=false;
}

ClimbingChildSprite::~ClimbingChildSprite() {
}

void ClimbingChildSprite::update() {
  if (beingcarried) {
    Sprite::update();
    setFace("idle");
    climbing=false;
    //gravityok=true;
  } else if (climbing) {
    ChildSprite::update();
    if (!move(0,-CLIMB_SPEED)) {
      setFace("walk");
      climbing=false;
      gravityok=true;
      dx=-dx;
      flop=(dx<0);
    } else {
      Rect restorer(r);
      r.x+=dx*MOUNT_DX;
      r.y-=MOUNT_DY;
      //sitter_log("testing %d,%d (from %d,%d) : %d",r.x,r.y,restorer.x,restorer.y,collides(true));
      if (collides(true)) {
        r=restorer;
        ongoal=false;
        for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
          if (r.y>=spr->r.bottom()) continue;
          if (r.bottom()<=spr->r.top()) continue;
          if ((dx>0)&&(r.right()!=spr->r.left())) continue;
          if ((dx<0)&&(r.left()!=spr->r.right())) continue;
          ongoal=spr->ongoal;
        }
      } else {
        setFace("walk");
        climbing=false;
        gravityok=true;
        dizzy=0;
      }
    }
  } else {
    int predx=dx;
    WalkingChildSprite::update();
    if ((dx!=predx)&&canClimb()) {
      dx=predx;
      flop=(dx<0);
      setFace("climb");
      climbing=true;
      gravityok=false;
    }
  }
}

bool ClimbingChildSprite::canClimb() {
  return true;
}
