#ifndef SITTER_CLIMBINGCHILDSPRITE_H
#define SITTER_CLIMBINGCHILDSPRITE_H

#include "sitter_WalkingChildSprite.h"

class ClimbingChildSprite : public WalkingChildSprite {
public:

  bool climbing;

  ClimbingChildSprite(Game *game);
  ~ClimbingChildSprite();
  
  void update();
  
  bool canClimb();
  
};

#endif
