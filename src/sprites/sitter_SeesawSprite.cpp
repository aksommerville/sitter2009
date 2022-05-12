#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_Grid.h"
#include "sitter_Motor.h"
#include "sitter_AudioManager.h"
#include "sitter_VideoManager.h"
#include "sitter_SeesawSprite.h"

#define YOFFSET_UP      16
#define YOFFSET_MID     30
#define YOFFSET_DOWN    42

#define ROCKET_MOTOR "0 ; -16 -16 -16 -14 -14 -13 -13 -12 -12 -11 -11 -10 -10 -9 -9 " \
                     "-8 -8 -8 -8 -8 -8 -8 -8 -8 -8 -8 -7 -7 -7 -7 -7 -6 -6 -6 -6 -5 -5 -5 -4 -4 -3 -3 -2 -1 "

SeesawSprite::SeesawSprite(Game *game):Sprite(game) {
  pltleft=new Sprite(game);
  pltleft->addGroup(game->grp_all);
  pltleft->addGroup(game->grp_vis);
  pltleft->addGroup(game->grp_solid);
  pltleft->r.w=32;
  pltleft->r.h=20;
  pltleft->texid=game->video->loadTexture("seesaw/platform.png");
  
  pltright=new Sprite(game);
  pltright->addGroup(game->grp_all);
  pltright->addGroup(game->grp_vis);
  pltright->addGroup(game->grp_solid);
  pltright->r.w=32;
  pltright->r.h=20;
  pltright->texid=game->video->loadTexture("seesaw/platform.png");
  
  state=0;
}

SeesawSprite::~SeesawSprite() {
}

class AbortLaunch {};

void SeesawSprite::update() {
  Sprite::update();
  
  /* look for new activity */
  switch (state) {
    case -1: if (platformActuated(pltright)) {
        state=1;
        setFace("right");
        pltright->r.y=r.y+YOFFSET_DOWN;
        pltleft->r.y=r.y+YOFFSET_UP;
        try { launch(pltleft,YOFFSET_UP-YOFFSET_DOWN); }
        catch (AbortLaunch) {
          pltright->r.y=r.y+YOFFSET_UP;
          pltleft->r.y=r.y+YOFFSET_DOWN;
          state=-1;
          setFace("left");
        }
      } break;
    case 1: if (platformActuated(pltleft)) {
        state=-1;
        setFace("left");
        pltright->r.y=r.y+YOFFSET_UP;
        pltleft->r.y=r.y+YOFFSET_DOWN;
        try { launch(pltright,YOFFSET_UP-YOFFSET_DOWN); }
        catch (AbortLaunch) {
          state=1;
          setFace("right");
          pltright->r.y=r.y+YOFFSET_DOWN;
          pltleft->r.y=r.y+YOFFSET_UP;
        }
      } break;
    case 0: {
        bool al=platformActuated(pltleft);
        bool ar=platformActuated(pltright);
        if (al!=ar) {
          if (al) {
            state=-1;
            setFace("left");
            pltright->r.y=r.y+YOFFSET_UP;
            pltleft->r.y=r.y+YOFFSET_DOWN;
            try { launch(pltright,YOFFSET_UP-YOFFSET_MID); }
            catch (AbortLaunch) {
              state=0;
              setFace("mid");
              pltright->r.y=r.y+YOFFSET_MID;
              pltleft->r.y=r.y+YOFFSET_MID;
            }
          } else if (ar) {
            state=1;
            setFace("right");
            pltright->r.y=r.y+YOFFSET_DOWN;
            pltleft->r.y=r.y+YOFFSET_UP;
            try { launch(pltleft,YOFFSET_UP-YOFFSET_MID); }
            catch (AbortLaunch) {
              state=0;
              setFace("mid");
              pltright->r.y=r.y+YOFFSET_MID;
              pltleft->r.y=r.y+YOFFSET_MID;
            }
          }
        }
      } break;
  }
  
  /* position platforms */
  pltleft->r.x=r.x-32;
  pltright->r.x=r.right();
  switch (state) {
    case -1: pltleft->r.y=r.y+YOFFSET_DOWN; pltright->r.y=r.y+YOFFSET_UP; break;
    case  1: pltleft->r.y=r.y+YOFFSET_UP; pltright->r.y=r.y+YOFFSET_DOWN; break;
    case  0: pltleft->r.y=r.y+YOFFSET_MID; pltright->r.y=r.y+YOFFSET_MID; break;
  }
}

bool SeesawSprite::platformActuated(Sprite *plt) {
  int row0=plt->r.top()/game->grid->rowh;
  int rowz=(r.bottom()-1)/game->grid->rowh; // yes, fulcrum's r.bottom()
  if (row0<0) row0=0;
  if (rowz>=game->grid->rowc) rowz=game->grid->rowc-1;
  
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
    if (spr->r.bottom()!=plt->r.top()) continue;
    if (spr->r.right()<=plt->r.left()) continue;
    if (spr->r.left()>=plt->r.right()) continue;
    int col0,colz;
    if (plt==pltleft) {
      col0=spr->r.left()/game->grid->colw;
      colz=(plt->r.left()-1)/game->grid->colw;
    } else {
      col0=plt->r.right()/game->grid->colw;
      colz=(spr->r.right()-1)/game->grid->colw;
    }
    if (col0<0) col0=0;
    if (colz>=game->grid->colc) colz=game->grid->colc-1;
    for (int row=row0;row<=rowz;row++) for (int col=col0;col<=colz;col++) 
      if (game->grid->cellpropv[game->grid->cellv[row*game->grid->colc+col]]&SITTER_GRIDCELL_SOLID) continue;
    return true;
  }
  return false;
}

void SeesawSprite::launch(Sprite *plt,int dy,SpriteGroup *grp,bool edge) {
  bool master=(!grp);
  if (!grp) grp=new SpriteGroup(game);
  try {
    for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
      if (edge) { // test the top edge
        if (spr->r.bottom()!=plt->r.top()) continue;
        if (spr->r.right()<=plt->r.left()) continue;
        if (spr->r.left()>=plt->r.right()) continue;
      } else { // test body collisions
        if (!plt->collides_sprite(spr)) continue;
      }
      if (grp->hasSprite(spr)) continue;
      int restorey=spr->r.y;
      int target=spr->r.y+dy;
      if (!spr->move(0,dy,true)) throw AbortLaunch();
      if (!edge) { // doing full-body collisions, and it still collides, must abort
        int giveup=8;
        while (spr->r.y>target) {
          spr->move(0,dy,true);
          if (--giveup<0) { spr->r.y=restorey; throw AbortLaunch(); }
        }
      }
      /* ok, add it */
      if (!spr->beingcarried) {
        if (!game->grp_solid->sprv[i]->rocketmotory) game->grp_solid->sprv[i]->rocketmotory=new Motor();
        game->grp_solid->sprv[i]->rocketmotory->deserialise(ROCKET_MOTOR);
        game->grp_solid->sprv[i]->rocketmotory->start(true);
      }
      grp->addSprite(spr);
      launch(spr,dy,grp,true);
    }
  } catch (AbortLaunch) {
    if (master) delete grp;
    throw;
  }
  if (master) {
    game->audio->playEffect("seesaw",this);
    delete grp;
  }
}
