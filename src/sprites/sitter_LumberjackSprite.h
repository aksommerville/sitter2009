#ifndef SITTER_LUMBERJACKSPRITE_H
#define SITTER_LUMBERJACKSPRITE_H

#include "sitter_WalkingChildSprite.h"

class LumberjackSprite : public WalkingChildSprite {

  Sprite *chainsaw;
  int happytime;
  int audioloopref;
  int turndelay; // prevent seizure-inducing every-frame-floppery

public:

  LumberjackSprite(Game *game);
  ~LumberjackSprite();
  
  void update();
  void pickup(Sprite *picker);
  void gridKill(int dx,int dy,int col,int row);
  void crush();
  void slice();
  
};

#endif
