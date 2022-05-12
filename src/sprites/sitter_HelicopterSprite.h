#ifndef SITTER_HELICOPTERSPRITE_H
#define SITTER_HELICOPTERSPRITE_H

#include "sitter_Sprite.h"

class HelicopterSprite : public Sprite {
public:

  int dy;
  SpriteGroup *riders;
  
  HelicopterSprite(Game *game);
  ~HelicopterSprite();
  void update();
  
};

class HHelicopterSprite : public Sprite {
public:
  int dx;
  int dizzy;
  SpriteGroup *riders;
  HHelicopterSprite(Game *game);
  ~HHelicopterSprite();
  void update();
  void movePigeon(Sprite *spr,int dx);
};

#endif
