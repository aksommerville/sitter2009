#include <malloc.h>
#include <string.h>
#include <limits.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_AudioManager.h"
#include "sitter_VideoManager.h"
#include "sitter_SpriteFace.h"
#include "sitter_Grid.h"
#include "sitter_Motor.h"
#include "sitter_Sprite.h"

#include "sitter_PlayerSprite.h"
#include "sitter_Player.h"

#define GRPV_INCREMENT 4
#define SPRV_INCREMENT 16
#define FACEV_INCREMENT 4

#define GRAVITY_MOTOR "0 ; 1 2 2 2 2 3 3 3 4 4 4 4 5 5 5 6 6 6 6 7 7 7 ; 8"
#define INERTIA_MOTOR "0 ; 6 6 5 5 5 5 5 5 5 4 4 4 4 4 4 4 4 3 3 3 3 3 3 3 3 2 2 2 2 2 2 2 2 ; 1"

#ifndef MIN
  #define MIN(a,b) (((a)<(b)):(a):(b))
  #define MAX(a,b) (((a)>(b)):(a):(b))
#endif

/******************************************************************************
 * Sprite init / kill
 *****************************************************************************/
 
Sprite::Sprite(Game *game):game(game),r(1,1) {
  grpv=NULL; grpc=grpa=0;
  autokill=true;
  layer=0;
  texid=-1;
  opacity=1.0f;
  tint=0;
  rotation=0.0f;
  flop=false;
  neverflop=false;
  facev=NULL; facec=facea=facep=0;
  edge_collisions=grid_collisions=sprite_collisions=false;
  collide_unslicable=false;
  gravitymotor=NULL;
  pushable=false;
  standingongrp=NULL;
  boundsprites=new SpriteGroup(game);
  gravityok=true;
  inertiamotor=NULL;
  inertia=0;
  beingcarried=false;
  ongoal=false;
  goalid=0;
  alive=true;
  slicer=false;
  rocketmotorx=rocketmotory=NULL;
  identity=SITTER_SPRIDENT_NONE;
  belted=false;
}

Sprite::~Sprite() {
  for (int i=0;i<grpc;i++) grpv[i]->removeSprite(this,false);
  if (grpv) free(grpv);
  // if there are faces, we don't own texid
  if (facev) {
    for (int i=0;i<facec;i++) delete facev[i];
    free(facev);
  } else if (texid>=0) game->video->unloadTexture(texid);
  if (gravitymotor) delete gravitymotor;
  if (standingongrp) delete standingongrp;
  delete boundsprites;
  if (inertiamotor) delete inertiamotor;
  if (rocketmotorx) delete rocketmotorx;
  if (rocketmotory) delete rocketmotory;
}

/******************************************************************************
 * Sprite: Group list
 *****************************************************************************/
 
void Sprite::addGroup(SpriteGroup *grp,bool recip) {
  for (int i=0;i<grpc;i++) if (grpv[i]==grp) return;
  if (grpc>=grpa) {
    grpa+=GRPV_INCREMENT;
    if (!(grpv=(SpriteGroup**)realloc(grpv,sizeof(void*)*grpa))) sitter_throw(AllocationError,"");
  }
  grpv[grpc++]=grp;
  if (recip) grp->addSprite(this,false);
}

void Sprite::removeGroup(SpriteGroup *grp,bool recip) {
  for (int i=0;i<grpc;i++) if (grpv[i]==grp) {
    grpc--;
    if (i<grpc) memmove(grpv+i,grpv+i+1,sizeof(void*)*(grpc-i));
    if (recip) grp->removeSprite(this,false);
    break;
  }
  if (!grpc&&autokill) delete this;
}

void Sprite::removeAllGroups() {
  if (autokill) delete this;
  else {
    for (int i=0;i<grpc;i++) grpv[i]->removeSprite(this,false);
    grpc=0;
  }
}

/******************************************************************************
 * Sprite: maintenance
 *****************************************************************************/
 
void Sprite::update() {
  update_gravity();
  update_inertia();
  update_animation();
  if (slicer) update_slicer();
}

void Sprite::updateBackground() {
  update_animation();
}

bool Sprite::update_gravity() {
  int prvy=r.y;
  if (gravitymotor&&gravityok) {
    if (int dy=gravitymotor->update()) {
      if (!move(0,dy)) {
        if (game->play_clock>1) game->audio->playEffect("land",this); // play_clock? ugly hack so we don't hear a huge "thump" at start
        gravitymotor->stop(true);
      }
    } else if (move(0,1)) gravitymotor->start();
  }
  return (r.y!=prvy);
}

bool Sprite::update_inertia() {
  int prvx=r.x;
  int prvy=r.y;
  if (inertiamotor&&inertia) {
    int sign=((inertia<0)?-1:1);
    int vel=inertiamotor->update();
    if (!beingcarried) move(vel*sign,0);
    if (!(inertia-=sign)) inertiamotor->stop(true);
  }
  if (rocketmotorx) rocketmotorx->update();
  if (rocketmotory) rocketmotory->update();
  if (rocketmotorx) if (!move(rocketmotorx->update(),0,true)) rocketmotorx->stop();
  if (rocketmotory) if (!move(0,rocketmotory->update(),true)) rocketmotory->stop();
  return ((prvx!=r.x)||(prvy!=r.y));
}

bool Sprite::update_animation() {
  if ((facep<0)||(facep>=facec)) return false;
  try {
    int prvtexid=texid;
    texid=facev[facep]->update();
    return (texid!=prvtexid);
  } catch (SpriteFaceDone &e) {
    setFace(e.nextfacename);
    return true;
  }
}

bool Sprite::update_slicer() {
  bool rtn=false;
  for (int i=0;i<game->grp_slicable->sprc;i++) if (collides_sprite(game->grp_slicable->sprv[i])) {
    game->grp_slicable->sprv[i]->slice();
    rtn=true;
  }
  return rtn;
}

void Sprite::turnOnGravity() {
  if (!standingongrp) standingongrp=new SpriteGroup(game);
  gravityok=true;
  if (gravitymotor) return;
  gravitymotor=new Motor(); gravitymotor->deserialise(GRAVITY_MOTOR);
}

void Sprite::turnOffGravity() {
  gravityok=false;
}

void Sprite::setInertia(int dx) {
  inertia+=dx;
  if (inertia&&!inertiamotor) {
    inertiamotor=new Motor(); inertiamotor->deserialise(INERTIA_MOTOR);
  }
  inertiamotor->start();
}

void Sprite::resetInertia() {
  inertia=0;
  if (inertiamotor) inertiamotor->stop(true);
}

/******************************************************************************
 * Sprite: face list
 *****************************************************************************/
 
void Sprite::addFace(SpriteFace *face) {
  if (facec>=facea) {
    facea+=FACEV_INCREMENT;
    if (!(facev=(SpriteFace**)realloc(facev,sizeof(void*)*facea))) sitter_throw(AllocationError,"");
  }
  int before=facec;
  for (int i=0;i<facec;i++) if (strcmp(face->name,facev[i]->name)<=0) { before=i; break; }
  if (before<facec) memmove(facev+before+1,facev+before,sizeof(void*)*(facec-before));
  facec++;
  facev[before]=face;
}

bool Sprite::setFace(const char *name) {
  if (!facec) return false;
  if (!strcmp(name,facev[facep]->name)) return false;
  int lo=0,hi=facec,ck=facec>>1;
  while (lo<hi) {
    int cmp=strcmp(name,facev[ck]->name);
    if (cmp<0) hi=ck;
    else if (cmp>0) lo=ck+1;
    else { facep=ck; facev[facep]->reset(); return true; }
    ck=(lo+hi)>>1;
  }
  return false;
}

const char *Sprite::getFace() const {
  if ((facep<0)||(facep>=facec)) return "";
  return facev[facep]->name;
}

/******************************************************************************
 * Sprite: physics
 *****************************************************************************/

int Sprite::move(int dx,int dy,bool push,bool movebindees) {
  if (!dx&&!dy) return 0;
  if (dx&&dy) {
    dx=move(dx,0,push); int adx=(dx<0)?-dx:dx;
    dy=move(0,dy,push); int ady=(dy<0)?-dy:dy;
    if (adx>ady) return dx;
    return dy;
  }
  
  // exactly one of dx,dy is zero... (but both may be zero by the end, watch out)
  bool horz=dx;
  if (dy>0) ongoal=false;
  if (movebindees) {
    for (int i=0;i<boundsprites->sprc;i++) {
      int bsmv=boundsprites->sprv[i]->move(dx,dy,push,false);
      if (bsmv!=dx+dy) {
        for (int j=0;j<i;j++) {
          if (horz) boundsprites->sprv[j]->r.x+=bsmv-dx;
          else boundsprites->sprv[j]->r.y+=bsmv-dy;
        }
        if (horz) dx=bsmv; else dy=bsmv;
      }
    }
    if (!dx&&!dy) {
      return 0;
    }
  }
  int ndx=dx,ndy=dy;
  int xrtnx=0,xrtny=0; // extra return (to report hacky corrections)
  
  #define POST_COLLISION_RESET { \
      if (((dx<0)!=(ndx<0))||((dy<0)!=(ndy<0))) { \
        if (movebindees) for (int ii=0;ii<boundsprites->sprc;ii++) boundsprites->sprv[ii]->r.move(-dx,-dy); \
        return xrtnx+xrtny; \
      } \
      if (movebindees) for (int ii=0;ii<boundsprites->sprc;ii++) { \
        if (horz) boundsprites->sprv[ii]->r.x+=ndx-dx; \
        else boundsprites->sprv[ii]->r.y+=ndy-dy; \
      } \
      dx=ndx; dy=ndy; \
      if (!dx&&!dy) { return xrtnx+xrtny; } \
    }
  
  if (edge_collisions) if (collides_edge(dx,dy,&ndx,&ndy)) POST_COLLISION_RESET
  
  if (grid_collisions&&game->grid) {
    int colcol,colrow;
    ndx=dx; ndy=dy;
    if (collides_grid(game->grid,dx,dy,&ndx,&ndy,&colcol,&colrow)) {
      if (game->grid->getCellProperties(colcol,colrow)&SITTER_GRIDCELL_ALLGOALS) {
        if (dy>0) {
          ongoal=true;
          switch (game->grid->getCellProperties(colcol,colrow)&SITTER_GRIDCELL_ALLGOALS) {
            case SITTER_GRIDCELL_GOAL1: goalid=1; break;
            case SITTER_GRIDCELL_GOAL2: goalid=2; break;
            case SITTER_GRIDCELL_GOAL3: goalid=3; break;
            case SITTER_GRIDCELL_GOAL4: goalid=4; break;
            default: goalid=0;
          }
          for (int i=0;i<boundsprites->sprc;i++) {
            boundsprites->sprv[i]->ongoal=true;
            boundsprites->sprv[i]->goalid=goalid;
          }
        }
      }
      POST_COLLISION_RESET
    }
  }
  
  if (sprite_collisions) {
    /* history helps to prevent stop-and-go artifacts when you push two things
     * sitting on one top of the other. It's not a great system. It's possible
     * to still get this artifact in recursive operations.
     * It is aesthetic only -- we could remove it altogether and the game would still work.
     */
    #define HISTORY_LIMIT 32 // evil hard-coded push limit. (but you'd never push this many things anyway)
    struct { Sprite *spr; int dx,dy; } history[HISTORY_LIMIT];
    int historyc=0;
    for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
      if (spr==this) continue; // impossible for collision, but we can save a hair of time anyway
      if (boundsprites->hasSprite(spr)) continue;
      if (collide_unslicable&&spr->hasGroup(game->grp_slicable)) continue;
      if (push) {
        if (collides_sprite(spr,dx,dy,&ndx,&ndy)) {
          if (spr->pushable&&!ndx&&!ndy) { // !ndx&&!ndy == only push when we are flush against the edge
            push_it:
            // make sure we pass along at least one, preferring half of input force
            int rdx=dx>>1; if (dx&&!rdx) rdx=dx;
            int rdy=dy>>1; if (dy&&!rdy) rdy=dy;
            if (horz) ndx=spr->move(rdx,0,true);
            else ndy=spr->move(0,rdy,true);
            if ((dx!=ndx)||(dy!=ndy)) { // must update history
              for (int hi=0;hi<historyc;hi++) {
                history[hi].spr->r.x+=(ndx-history[hi].dx);
                history[hi].spr->r.y+=(ndy-history[hi].dy);
                for (int bi=0;bi<history[hi].spr->boundsprites->sprc;bi++) if (Sprite *bspr=history[hi].spr->boundsprites->sprv[bi]) {
                  bspr->r.x+=(ndx-history[hi].dx);
                  bspr->r.y+=(ndy-history[hi].dy);
                }
                history[hi].dx=ndx;
                history[hi].dy=ndy;
              }
            }
            if (historyc<HISTORY_LIMIT) {
              history[historyc].spr=spr;
              history[historyc].dx=ndx;
              history[historyc].dy=ndy;
              historyc++;
            }
          } else if (spr->pushable) { // HACK -- don't abandon pushable-in-the-future blockages
            // even hackier -- don't do this if there will be another sprite collision
            bool abort_test=false;
            for (int j=i+1;j<game->grp_solid->sprc;j++) if (Sprite *jspr=game->grp_solid->sprv[j]) {
              if (jspr==this) continue;
              if (boundsprites->hasSprite(jspr)) continue;
              if (collide_unslicable&&jspr->hasGroup(game->grp_slicable)) continue;
              int ondx=ndx,ondy=ndy;
              if (collides_sprite(jspr,dx,dy,&ndx,&ndy)) {
                abort_test=true;
                ndx=ondx; ndy=ondy;
                break;
              }
              ndx=ondx; ndy=ondy;
            }
            if (!abort_test) {
              xrtnx+=ndx;
              xrtny+=ndy;
              r.x+=ndx;
              r.y+=ndy;
              dx-=ndx;
              dy-=ndy;
              for (int hi=0;hi<historyc;hi++) {
                history[hi].dx-=ndx;
                history[hi].dy-=ndy;
              }
              ndx=ndy=0;
              goto push_it;
            }
          }
          if (dy>0) {
            ongoal=(spr->ongoal&&couldConceivablyBeOnGoal());
            goalid=spr->goalid;
            for (int i=0;i<boundsprites->sprc;i++) { boundsprites->sprv[i]->ongoal=ongoal; boundsprites->sprv[i]->goalid=goalid; }
          }
          POST_COLLISION_RESET
        }
      } else {
        if (spr==this) continue;
        if (collides_sprite(spr,dx,dy,&ndx,&ndy)) {
          if (dy>0) {
            ongoal=(spr->ongoal&&couldConceivablyBeOnGoal());
            goalid=spr->goalid;
            for (int i=0;i<boundsprites->sprc;i++) { boundsprites->sprv[i]->ongoal=ongoal; boundsprites->sprv[i]->goalid=goalid; }
          }
          POST_COLLISION_RESET
        }
      }
    }
    #undef HISTORY_LIMIT
  }
  
  if (horz) { r.x+=dx; return dx+xrtnx; }
  else { r.y+=dy; return dy+xrtny; }
  #undef POST_COLLISION_RESET
}

bool Sprite::collides(bool countsemi) {
  if ((edge_collisions&&collides_edge())||(game->grid&&grid_collisions&&collides_grid(game->grid,0,0,0,0,0,0,countsemi))) return true;
  if (!sprite_collisions) return false;
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
    if (spr==this) continue;
    if (boundsprites->hasSprite(spr)) continue;
    if (collides_sprite(spr)) {
      return true;
    }
  }
  return false;
}

bool Sprite::collides_sprite(const Sprite *spr,int dx,int dy,int *outdx,int *outdy) const {
  if (spr==this) return false;
  if (spr->slicer&&hasGroup(game->grp_slicable)) return false;
  if (dx) {
    if ((r.y>=spr->r.y+spr->r.h)||(r.y+r.h<=spr->r.y)) return false;
    if (dx<0) {
      if (r.x<spr->r.x+spr->r.w) return false;
      if (r.x+dx>=spr->r.x+spr->r.w) return false;
      if (outdx) *outdx=(spr->r.x+spr->r.w)-r.x;
      if (outdy) *outdy=0;
      return true;
    } else {
      if ((identity!=SITTER_SPRIDENT_PLAYER)&&(spr->identity==SITTER_SPRIDENT_TELEPORTER)
        &&(r.h<=32)&&(r.bottom()==spr->r.bottom())) { // teleporter entrance
        if (r.right()>spr->r.right()) return false;
        if (r.right()+dx<=spr->r.right()) return false;
        if (outdx) *outdx=spr->r.right()-r.right();
        if (outdy) *outdy=0;
        return true;
      }
      if (r.x+r.w>spr->r.x) return false;
      if (r.x+r.w+dx<=spr->r.x) return false;
      if (outdx) *outdx=spr->r.x-(r.x+r.w);
      if (outdy) *outdy=0;
      return true;
    }
  } else if (dy) {
    if ((r.x>=spr->r.x+spr->r.w)||(r.x+r.w<=spr->r.x)) return false;
    if (dy<0) {
      if (r.y<spr->r.y+spr->r.h) return false;
      if (r.y+dy>=spr->r.y+spr->r.h) return false;
      if (outdx) *outdx=0;
      if (outdy) *outdy=(spr->r.y+spr->r.h)-r.y;
      return true;
    } else {
      if (r.y+r.h>spr->r.y) return false;
      if (r.y+r.h+dy<=spr->r.y) return false;
      if (outdx) *outdx=0;
      if (outdy) *outdy=spr->r.y-(r.y+r.h);
      return true;
    }
  } else return r.collides(spr->r);
}

bool Sprite::collides_grid(const Grid *grid,int dx,int dy,int *outdx,int *outdy,int *colcol,int *colrow,bool countsemi) {
  #define STDX \
    int lcol=r.x/grid->colw; if (lcol<0) lcol=0; \
    int rcol=(r.x+r.w-1)/grid->colw; if (rcol>=grid->colc) rcol=grid->colc-1; \
    if (rcol<lcol) return false;
  #define STDY \
    int trow=r.y/grid->rowh; if (trow<0) trow=0; \
    int brow=(r.y+r.h-1)/grid->rowh; if (brow>=grid->rowc) brow=grid->rowc-1; \
    if (brow<trow) return false;
  if (dx<0) {
    int lcol=(r.x+dx)/grid->colw; if (lcol<0) lcol=0;
    int rcol=r.x/grid->colw-1; if (rcol>=grid->colc) rcol=grid->colc-1;
    if (rcol<lcol) return false;
    STDY
    for (int col=rcol;col>=lcol;col--) {
      bool cankill=false; int kcol,krow;
      for (int row=trow;row<=brow;row++) {
        uint32_t prop=grid->getCellProperties(col,row);
        if (prop&SITTER_GRIDCELL_SOLID) {
          if (colcol) *colcol=col;
          if (colrow) *colrow=row;
          if (outdx) *outdx=(col+1)*grid->colw-r.x;
          if (outdy) *outdy=0;
          return true;
        } else if (prop&SITTER_GRIDCELL_KILLRIGHT) { //gridKill(dx,dy,col,row);
          cankill=true;
          kcol=col;
          krow=row;
        }
      }
      if (cankill) gridKill(dx,dy,kcol,krow);
    }
  } else if (dx>0) {
    int lcol=(r.x+r.w-1)/grid->colw+1; if (lcol<0) lcol=0;
    int rcol=(r.x+r.w+dx-1)/grid->colw; if (rcol>=grid->colc) rcol=grid->colc-1;
    if (rcol<lcol) return false;
    STDY
    for (int col=lcol;col<=rcol;col++) {
      bool cankill=false; int kcol,krow;
      for (int row=trow;row<=brow;row++) {
        uint32_t prop=grid->getCellProperties(col,row);
        if (prop&SITTER_GRIDCELL_SOLID) {
          if (colcol) *colcol=col;
          if (colrow) *colrow=row;
          if (outdx) *outdx=col*grid->colw-(r.x+r.w);
          if (outdy) *outdy=0;
          return true;
        } else if (prop&SITTER_GRIDCELL_KILLLEFT) { //gridKill(dx,dy,col,row);
          cankill=true;
          kcol=col;
          krow=row;
        }
      }
      if (cankill) gridKill(dx,dy,kcol,krow);
    }
  } else if (dy<0) {
    int trow=(r.y+dy)/grid->rowh; if (trow<0) trow=0;
    int brow=r.y/grid->rowh-1; if (brow>=grid->rowc) brow=grid->rowc-1;
    if (brow<trow) return false;
    STDX
    for (int row=brow;row>=trow;row--) {
      bool cankill=false; int kcol,krow;
      for (int col=lcol;col<=rcol;col++) {
        uint32_t prop=grid->getCellProperties(col,row);
        if (prop&SITTER_GRIDCELL_SOLID) {
          if (colcol) *colcol=col;
          if (colrow) *colrow=row;
          if (outdx) *outdx=0;
          if (outdy) *outdy=(row+1)*grid->rowh-r.y;
          return true;
        } else if (prop&SITTER_GRIDCELL_KILLBOTTOM) { //gridKill(dx,dy,col,row);
          cankill=true;
          kcol=col;
          krow=row;
        }
      }
      if (cankill) gridKill(dx,dy,kcol,krow);
    }
  } else if (dy>0) {
    int trow=(r.y+r.h-1)/grid->rowh+1; if (trow<0) trow=0;
    int brow=(r.y+r.h+dy-1)/grid->rowh; if (brow>=grid->rowc) brow=grid->rowc-1;
    if (brow<trow) return false;
    STDX
    for (int row=trow;row<=brow;row++) {
      bool cankill=false; int kcol,krow;
      for (int col=lcol;col<=rcol;col++) {
        uint32_t prop=grid->getCellProperties(col,row);
        // ugly hack to keep carried objects from catching on semi-solid platforms (but allow wide objects to do so):
        if ((prop&SITTER_GRIDCELL_SEMISOLID)&&beingcarried&&(r.w<=32)) {
          continue;
        }
        if (prop&(SITTER_GRIDCELL_SOLID|SITTER_GRIDCELL_SEMISOLID)) {
          if (colcol) *colcol=col;
          if (colrow) *colrow=row;
          if (outdx) *outdx=0;
          if (outdy) *outdy=row*grid->rowh-(r.y+r.h);
          return true;
        } else if (prop&SITTER_GRIDCELL_KILLTOP) { //gridKill(dx,dy,col,row);
          cankill=true;
          kcol=col;
          krow=row;
        }
      }
      if (cankill) gridKill(dx,dy,kcol,krow);
    }
  } else {
    STDX STDY
    for (int col=lcol;col<=rcol;col++) for (int row=trow;row<=brow;row++) {
      uint32_t prop=grid->getCellProperties(col,row);
      if ((prop&SITTER_GRIDCELL_SOLID)||((prop&SITTER_GRIDCELL_SEMISOLID)&&countsemi)) {
        if (colcol) *colcol=col;
        if (colrow) *colrow=row;
        return true;
      }
    }
  }
  return false;
  #undef STDX
  #undef STDY
}

bool Sprite::collides_edge(int dx,int dy,int *outdx,int *outdy) const {
  Rect collr;
       if (dx<0) collr.set(r.x+dx,r.y,-dx,r.h);
  else if (dx>0) collr.set(r.x+r.w,r.y,dx,r.h);
  else if (dy<0) collr.set(r.x,r.y+dy,r.w,-dy);
  else if (dy>0) collr.set(r.x,r.y+r.h,r.w,dy);
  else collr=r;
  Rect bounds;
  if (game->grid) bounds=game->grid->getBounds();
  else bounds=game->video->getBounds();
  if (bounds.contains(collr)) {
    if (outdx) *outdx=dx;
    if (outdy) *outdy=dy;
    return false;
  } else {
         if ((dx<0)&&outdx) *outdx=-r.x;
    else if ((dx>0)&&outdx) *outdx=bounds.right()-r.right();
    else if ((dy<0)&&outdx) *outdy=-r.y;
    else if ((dy>0)&&outdx) *outdy=bounds.bottom()-r.bottom();
    return true;
  }
}

/* There is one case not handled properly by this: if you are above a goal, and the sprite you're
 * standing on rests on *another* goal -- it returns true. I guess that's ok, since it's never
 * going to happen anyway.
 */
bool Sprite::couldConceivablyBeOnGoal() {
  if (!game->grid) return true; // if a tree falls in the forest, and there's no one there to hear it....
  int col0=r.x/game->grid->colw;
  int colz=(r.right()-1)/game->grid->colw;
  int row0=r.bottom()/game->grid->rowh;
  for (int row=row0;row<game->grid->rowc;row++) {
    for (int col=col0;col<=colz;col++) {
      if (game->grid->getCellProperties(col,row)&SITTER_GRIDCELL_ALLGOALS) return true;
    }
  }
  return false;
}

/******************************************************************************
 * Group init / kill
 *****************************************************************************/
 
SpriteGroup::SpriteGroup(Game *game):game(game) {
  sprv=NULL; sprc=spra=0;
  sortdir=1;
  addsorted=false;
  scrollx=scrolly=0;
}

SpriteGroup::~SpriteGroup() {
  for (int i=0;i<sprc;i++) sprv[i]->removeGroup(this,false);
  if (sprv) free(sprv);
}

void SpriteGroup::addSprite(Sprite *spr,bool recip) {
  int before=sprc;
  if (addsorted) {
    for (int i=0;i<sprc;i++) if (sprv[i]==spr) return; else if ((before==sprc)&&(spr->cmp(sprv[i])<0)) before=i;
  } else {
    for (int i=0;i<sprc;i++) if (sprv[i]==spr) return;
  }
  if (sprc>=spra) {
    spra+=SPRV_INCREMENT;
    if (!(sprv=(Sprite**)realloc(sprv,sizeof(void*)*spra))) sitter_throw(AllocationError,"");
  }
  if (before<sprc) memmove(sprv+before+1,sprv+before,sizeof(void*)*(sprc-before));
  sprv[before]=spr;
  sprc++;
  if (recip) spr->addGroup(this,false);
}

void SpriteGroup::removeSprite(Sprite *spr,bool recip) {
  for (int i=0;i<sprc;i++) if (sprv[i]==spr) {
    sprc--;
    if (i<sprc) memmove(sprv+i,sprv+i+1,sizeof(void*)*(sprc-i));
    if (recip) spr->removeGroup(this,false);
    return;
  }
}

bool SpriteGroup::sort1() {
  addsorted=true;
  if (!sprc) return false;
  int first,last;
  if (sortdir==1) {
    first=0;
    last=sprc-1;
  } else {
    first=sprc-1;
    last=0;
  }
  bool did=false;
  for (int i=first;i!=last;i+=sortdir) {
    if (sprv[i]->cmp(sprv[i+sortdir])==sortdir) {
      did=true;
      Sprite *swap=sprv[i];
      sprv[i]=sprv[i+sortdir];
      sprv[i+sortdir]=swap;
    }
  }
  sortdir=-sortdir;
  return did;
}
