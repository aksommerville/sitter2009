#include <string.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_SpriteFace.h"
#include "sitter_Game.h"
#include "sitter_WalkingChildSprite.h"

#define TOO_SHORT_RUN 32
// time spent waiting before turning around is randomised to mitigate phase sync.
// (2 susies in a symmetrical area will not always meet in the center).
#define DIZZY_TIME_MIN 10
#define DIZZY_TIME_MAX 40

WalkingChildSprite::WalkingChildSprite(Game *game):ChildSprite(game) {
  dx=(rand()&1)?1:-1;
  setFace("walk");
  runlen=0;
  validrun=false;
  dizzy=0;
  lookfortux=-1;
  beltdelay=0;
}

WalkingChildSprite::~WalkingChildSprite() {
}

void WalkingChildSprite::decideWhetherLookForTux() {
  lookfortux=0;
  for (int i=0;i<facec;i++) if (facev[i]&&facev[i]->name&&!strcmp(facev[i]->name,"toy")) {
    lookfortux=1;
    return;
  }
}

int WalkingChildSprite::findTux() {
  int rtn=-1;
  bool foundbehind=false;
  for (int i=0;i<game->grp_solid->sprc;i++) if (Sprite *spr=game->grp_solid->sprv[i]) {
    if (spr->identity!=SITTER_SPRIDENT_TUXDOLL) continue;
    if (spr->r.bottom()<=r.top()) continue;
    if (spr->r.top()>=r.bottom()) continue;
    if ((spr->r.x-r.x<0)!=(dx<0)) { foundbehind=true; continue; }
    if ((dx<0)&&(spr->r.right()==r.left())) return 0;
    if ((dx>0)&&(spr->r.left()==r.right())) return 0;
    rtn=1;
  }
  if ((rtn<0)&&foundbehind) return 2;
  return rtn;
}

void WalkingChildSprite::update() {
  if (lookfortux<0) decideWhetherLookForTux();
  ChildSprite::update();
  if (!alive) return;
  if (beingcarried) { 
    setFace("idle"); 
    if (rotation==0.0) flop=(dx<0);
    else if (rotation==180.0) flop=(dx>0);
    else sitter_log("rotation==%f",rotation);
    runlen=0;
    validrun=false;
    beltdelay=0;
    return; 
  }
  
  if (beltdelay>0) { setFace("idle"); beltdelay--; return; }
  
  bool dontturnaround=false;
  int lookstate=-1;
  if (lookfortux) {
    switch (lookstate=findTux()) {
      case -1: if (!strcmp(facev[facep]->name,"toy")) setFace("walk"); break;
      case  0: setFace("toy"); return;
      case  1: if (!strcmp(facev[facep]->name,"toy")) setFace("walk"); dontturnaround=true; break;
      case  2: if (!strcmp(facev[facep]->name,"toy")) setFace("walk"); dx=-dx; flop=(dx<0); break;
    }
  }
    
  if (lookstate!=1) setFace("walk");
  flop=(dx<0);
  if (!move(dx,0)) {
    if (--dizzy>0) return;
    if (0&&validrun) {
      if (runlen<TOO_SHORT_RUN) {
        dizzy=DIZZY_TIME_MIN+(rand()%(DIZZY_TIME_MAX-DIZZY_TIME_MIN+1));
        setFace("idle");
      }
    } else validrun=true;
    runlen=0;
    if (!dontturnaround) {
      dizzy=DIZZY_TIME_MIN+(rand()%(DIZZY_TIME_MAX-DIZZY_TIME_MIN+1));
      dx=-dx;
      flop=(dx<0);
    }
  } else { // successful walk
    runlen++;
    dizzy=DIZZY_TIME_MIN+(rand()%(DIZZY_TIME_MAX-DIZZY_TIME_MIN+1));
  }
}

void WalkingChildSprite::beltMove(int dx) {
  beltdelay++;
  ChildSprite::beltMove(dx);
}
