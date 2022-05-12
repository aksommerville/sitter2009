#ifndef SITTER_BIRDSPRITE_H
#define SITTER_BIRDSPRITE_H

#include "sitter_Sprite.h"

class BirdSprite : public Sprite {
public:

  int nestx,nesty;
  int state;
  int stateclock;
  int dx;
  Sprite *target;
  SpriteGroup *history;

  BirdSprite(Game *game);
  ~BirdSprite();
  
  void update();
  void update_REST();
  void update_HUNT();
  void update_RETURN();
  void update_HUNT1();
  
};

#endif
