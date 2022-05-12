#ifndef SITTER_ANIMATEONCESPRITE_H
#define SITTER_ANIMATEONCESPRITE_H

#include "sitter_Sprite.h"

class AnimateOnceSprite : public Sprite {
public:
  AnimateOnceSprite(Game *game);
  void update();
  void updateBackground();
};

class TrivialMovingSprite : public Sprite {
public:
  int dx,dy;
  TrivialMovingSprite(Game *game,int dx,int dy);
  void update();
};

/* safe to ignore return:
 */
Sprite *sitter_makeExplosion(Game *game,const Rect &r);

#endif
