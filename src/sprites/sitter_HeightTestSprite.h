#ifndef SITTER_HEIGHTTESTSPRITE_H
#define SITTER_HEIGHTTESTSPRITE_H

#include "sitter_Sprite.h"

class HeightTestSprite : public Sprite {
public:

  int badfacetime;

  HeightTestSprite(Game *game);
  ~HeightTestSprite();
  void update();
  
};

#endif
