#ifndef SITTER_WALKINGCHILDSPRITE_H
#define SITTER_WALKINGCHILDSPRITE_H

#include "sitter_ChildSprite.h"

class WalkingChildSprite : public ChildSprite {
public:

  int dx;
  int dizzy; // stop walking in very tight spaces
  int runlen;
  bool validrun;
  int lookfortux; // 1 if this activity is allowed; ie if we have a "toy" face (constant; -1=unset)
  int beltdelay;

  WalkingChildSprite(Game *game);
  ~WalkingChildSprite();
  
  void update();
  void beltMove(int dx);
  
  void decideWhetherLookForTux(); // call once
  int findTux(); // return -1=nothing, 0=found right here, 1=keep walking (found), 2=found behind me
  
};

#endif
