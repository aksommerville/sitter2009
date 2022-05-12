#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_VideoManager.h"
#include "sitter_BeltSprite.h"

static struct {
  int offx,offy;
  int w,h;
  double initt;
  double opacity;
} gear_data[SITTER_BELT_GEARC]={
  {4,4,24,24,0.0,0.5},
  {0,12,12,12,0.0,1.0},
  {20,6,8,8,15.0,1.0},
  {12,16,12,12,45.0,1.0},
};

#define GEAR_SPEED 3.0 // degrees

BeltSprite::BeltSprite(Game *game):Sprite(game) {
  for (int i=0;i<SITTER_BELT_GEARC;i++) {
    gearv[i]=new Sprite(game);
    gearv[i]->addGroup(game->grp_all);
    gearv[i]->addGroup(game->grp_vis);
    gearv[i]->texid=game->video->loadTexture("belt/gear.png",true,SITTER_TEX_FILTER);
    gearv[i]->rotation=gear_data[i].initt;
    gearv[i]->r.w=gear_data[i].w;
    gearv[i]->r.h=gear_data[i].h;
    gearv[i]->opacity=gear_data[i].opacity;
    gearv[i]->layer=101;
  }
  geardt=GEAR_SPEED;
  layer=100;
  dx=1;
  identity=SITTER_SPRIDENT_BELT;
}

void BeltSprite::update() {
  Sprite::update();
  /* move solid+pushable sprites which are standing on me */
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
    if (!spr->pushable) continue;
    if (spr->belted) continue;
    if (spr->r.bottom()!=r.top()) continue;
    if (spr->r.right()<=r.left()) continue;
    if (spr->r.left()>=r.right()) continue;
    if (int truedx=spr->move(dx,0,false)) spr->beltMove(truedx);
    spr->belted=true;
  }
  /* look for neighbors */
  bool hasleft=false,hasright=false;
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
    if (spr->identity!=SITTER_SPRIDENT_BELT) continue;
    if (spr->r.y!=r.y) continue;
    if (spr->r.right()==r.left()) hasleft=true;
    if (spr->r.left()==r.right()) hasright=true;
    if (hasleft&&hasright) break;
  }
  if (hasleft==hasright) { // XOR -- we don't have a "no-neighors" image, so use "both neighbors" in that case
    if (dx>0) setFace("mr");
    else setFace("ml");
    flop=false;
  } else {
    if (hasleft) { // right edge
      if (dx>0) setFace("el");
      else setFace("er");
      flop=true;
    } else {
      if (dx>0) setFace("er");
      else setFace("er");
      flop=false;
    }
  }
  /* rotate and reposition gears */
  for (int i=0;i<SITTER_BELT_GEARC;i++) {
    gearv[i]->r.x=r.x+gear_data[i].offx;
    gearv[i]->r.y=r.y+gear_data[i].offy;
    if ((gearv[i]->rotation+=geardt)>=360.0) gearv[i]->rotation-=360.0;
    else if (gearv[i]->rotation<0.0) gearv[i]->rotation+=360.0;
  }
}

void BeltSprite::updateBackground() {
  Sprite::updateBackground();
  /* rotate and reposition gears */
  for (int i=0;i<SITTER_BELT_GEARC;i++) {
    gearv[i]->r.x=r.x+gear_data[i].offx;
    gearv[i]->r.y=r.y+gear_data[i].offy;
    if ((gearv[i]->rotation+=geardt)>=360.0) gearv[i]->rotation-=360.0;
    else if (gearv[i]->rotation<0.0) gearv[i]->rotation+=360.0;
  }
}
