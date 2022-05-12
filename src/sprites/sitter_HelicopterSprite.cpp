#include "sitter_Game.h"
#include "sitter_HelicopterSprite.h"

#define SPEED 1
#define HSPEED 2
#define DIZZY_TIME 30;

HelicopterSprite::HelicopterSprite(Game *game):Sprite(game) {
  dy=-1;
  riders=new SpriteGroup(game);
  identity=SITTER_SPRIDENT_HELICOPTER;
}

HelicopterSprite::~HelicopterSprite() {
  delete riders;
}

void HelicopterSprite::update() {
  Sprite::update();
  if (dy>0) { // execute a little extra trickery to ensure that everyone stays put
    for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
      if (spr->identity==SITTER_SPRIDENT_HELICOPTER) continue;
      if (spr->r.bottom()!=r.top()) continue;
      if (spr->r.right()<=r.left()) continue;
      if (spr->r.left()>=r.right()) continue;
      riders->addSprite(spr);
    }
    if (!move(0,dy*SPEED,true)) dy=-dy;
    else {
      for (int i=0;i<riders->sprc;i++) riders->sprv[i]->move(0,dy*SPEED);
    }
    riders->removeAllSprites();
  } else {
    if (!move(0,dy*SPEED,true)) dy=-dy;
  }
}

/* The "H" is for "Horizontal".
 * The other "H" is still for "Helicopter".
 */
 
HHelicopterSprite::HHelicopterSprite(Game *game):Sprite(game) {
  dx=1;
  riders=new SpriteGroup(game);
  dizzy=0;
  identity=SITTER_SPRIDENT_HELICOPTER;
}

HHelicopterSprite::~HHelicopterSprite() {
  delete riders;
}

void HHelicopterSprite::update() {
  if (dizzy>0) dizzy--;
  Sprite::update();
  Rect prevr(r);
  int truedx=move(dx*HSPEED,0,true);
  if (!truedx) {
    if (dizzy>0) return;
    dx=-dx;
    flop=(dx<0);
    dizzy=DIZZY_TIME;
    return;
  }
  riders->removeAllSprites();
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
    if (spr->beingcarried) continue;
    if (spr->r.bottom()!=r.top()) continue;
    if (spr->r.right()<=r.left()) continue;
    if (spr->r.left()>=r.right()) continue;
    if (spr->hasGroup(riders)) continue;
    movePigeon(spr,truedx);
  }
}

void HHelicopterSprite::movePigeon(Sprite *spr,int dx) {
  riders->addSprite(spr);
  int truedx=spr->move(dx,0,false);
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *sspr=game->grp_solid->sprv[i]) {
    if (sspr->beingcarried) continue;
    if (sspr->r.bottom()!=spr->r.top()) continue;
    if (sspr->r.right()<=spr->r.left()) continue;
    if (sspr->r.left()>=spr->r.right()) continue;
    if (sspr->hasGroup(riders)) continue;
    movePigeon(sspr,truedx);
  }
}
