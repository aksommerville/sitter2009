#ifndef SITTER_SEESAWSPRITE_H
#define SITTER_SEESAWSPRITE_H

#include "sitter_Sprite.h"

class SeesawSprite : public Sprite {

  Sprite *pltleft,*pltright;
  int state;
  bool platformActuated(Sprite *plt);
  void launch(Sprite *plt,int dy,SpriteGroup *grp=0,bool edge=false);

public:

  SeesawSprite(Game *game);
  ~SeesawSprite();
  
  void update();
  
};

#endif
