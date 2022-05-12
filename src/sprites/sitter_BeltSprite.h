#ifndef SITTER_BELTSPRITE_H
#define SITTER_BELTSPRITE_H

#include "sitter_Sprite.h"

#define SITTER_BELT_GEARC 4

class BeltSprite : public Sprite {

  Sprite *gearv[SITTER_BELT_GEARC];
  double geardt;
  int dx;

public:

  BeltSprite(Game *game);
  void update();
  void updateBackground();
  
};

#endif
