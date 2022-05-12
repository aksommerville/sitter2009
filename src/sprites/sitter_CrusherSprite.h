#ifndef SITTER_CRUSHERSPRITE_H
#define SITTER_CRUSHERSPRITE_H

#include "sitter_Sprite.h"

#define SITTER_CRUSHER_LAMPC 5 // must be at least 2

class CrusherSprite : public Sprite {

  Sprite *armspr,*bootspr;
  Sprite *lampv[SITTER_CRUSHER_LAMPC];
  uint32_t coretint[SITTER_CRUSHER_LAMPC];
  int lampstate,lampdstate;
  int crushclock;
  bool returning;
  void makeSprites();
  void updateLamps();
  void commenceCrushification();
  void updateCrushing();

public:

  CrusherSprite(Game *game);
  ~CrusherSprite();
  void update();
  
};

#endif
