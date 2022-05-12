#include <math.h>
#include <stdio.h>
#include "sitter_Grid.h"
#include "sitter_VideoManager.h"
#include "sitter_SpriteFace.h"
#include "sitter_Game.h"
#include "sitter_AnimateOnceSprite.h"

#ifndef PI
  #define PI 3.1415926535897931
#endif

AnimateOnceSprite::AnimateOnceSprite(Game *game):Sprite(game) {
}

void AnimateOnceSprite::update() {
  Sprite::update();
  if (!facec) { delete this; return; }
  if (facev[facep]->getSync()) { delete this; return; }
}

void AnimateOnceSprite::updateBackground() {
  // don't animate
}

#define FIREWORKS_COUNT 10
#define FIREWORKS_SPEED 4
#define FIREWORKS_FRAMEC 8

Sprite *sitter_makeExplosion(Game *game,const Rect &refr) {
  Sprite *spr=new AnimateOnceSprite(game);
  spr->addGroup(game->grp_all);
  spr->addGroup(game->grp_vis);
  spr->addGroup(game->grp_upd);
  spr->r.w=64;
  spr->r.h=64;
  spr->r.centerx(refr.centerx());
  spr->r.centery(refr.centery());
  spr->layer=600;
  if (SpriteFace *face=new SpriteFace(game,"expl")) {
    face->addFrame("explode/0.png",4);
    face->addFrame("explode/1.png",4);
    face->addFrame("explode/2.png",4);
    face->addFrame("explode/3.png",4);
    face->addFrame("explode/4.png",4);
    face->addFrame("explode/5.png",4);
    face->addFrame("explode/6.png",4);
    spr->addFace(face);
    spr->setFace("expl");
  }
  
  for (int i=0;i<FIREWORKS_COUNT;i++) {
    double t=(i*PI*2)/FIREWORKS_COUNT;
    int dx=lround(sin(t)*FIREWORKS_SPEED);
    int dy=lround(cos(t)*-FIREWORKS_SPEED);
    if (TrivialMovingSprite *tms=new TrivialMovingSprite(game,dx,dy)) {
      //sitter_log("TMS at %d,%d",dx,dy);
      tms->addGroup(game->grp_all);
      tms->addGroup(game->grp_vis);
      tms->addGroup(game->grp_upd);
      tms->layer=1000;
      tms->r.w=16;
      tms->r.h=16;
      tms->r.centerx(refr.centerx());
      tms->r.centery(refr.centery());
      if (SpriteFace *face=new SpriteFace(game,"idle")) {
        char framenamebuf[20];
        for (int i=0;i<FIREWORKS_FRAMEC;i++) {
          sprintf(framenamebuf,"fireworks/%d.png",i);
          face->addFrame(framenamebuf,5);
        }
        tms->addFace(face);
        tms->setFace("idle");
      }
    }
  }
  
  return spr;
}

TrivialMovingSprite::TrivialMovingSprite(Game *game,int dx,int dy):Sprite(game),dx(dx),dy(dy) {
}

void TrivialMovingSprite::update() {
  //sitter_log("tms update");
  Sprite::update();
  r.x+=dx;
  r.y+=dy;
  if (r.right()<0) { delete this; return; }
  if (r.top()<0) { delete this; return; }
  if (game->grid) {
    if (r.left()>=game->grid->colc*game->grid->colw) { delete this; return; }
    if (r.top()>=game->grid->rowc*game->grid->rowh) { delete this; return; }
  } else {
    if (r.left()>=game->video->getScreenWidth()) { delete this; return; }
    if (r.top()>=game->video->getScreenHeight()) { delete this; return; }
  }
}
