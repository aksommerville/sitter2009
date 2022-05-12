#ifndef SITTER_CANNONSPRITE_H
#define SITTER_CANNONSPRITE_H

#include "sitter_Sprite.h"

class CannonSprite : public Sprite {
public:

  Sprite *barrel;
  Sprite *ball;
  int boredom;
  int breather;

  CannonSprite(Game *game);
  ~CannonSprite();
  void update();
  
  /* our controller-button-sprites will call these */
  void turnCounterClockwise();
  void turnClockwise();
  void fire();
  
  void makeRocketMotors(Sprite *spr);
  bool onlyOnePlayerAndHesInTheCannon();
  
};

class CannonButtonSprite : public Sprite {
public:

  CannonSprite *cannon;
  bool active;
  int pressdelay; // after first pressing, how long to begin collision detection again? (time to let gravity work)
  
  CannonButtonSprite(Game *game);
  ~CannonButtonSprite();
  void update();
  
  void findCannon();
  
};

#endif
