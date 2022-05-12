#ifndef SITTER_CHILDSPRITE_H
#define SITTER_CHILDSPRITE_H

#include "sitter_Sprite.h"

class ChildSprite : public Sprite {
public:

  bool crushed;
  bool ismutant;

  void gridKill(int dx,int dy,int col,int row);
  void update();
  void updateBackground();
  
  ChildSprite(Game *game);//,const char *name,int col,int row);
  ~ChildSprite();
  
  void crush();
  void slice();
  void mutate();
  
};

#endif
