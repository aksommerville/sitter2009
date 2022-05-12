#ifndef SITTER_ACROBATCHILDSPRITE_H
#define SITTER_ACROBATCHILDSPRITE_H

#include "sitter_ChildSprite.h"

class Motor;

class AcrobatChildSprite : public ChildSprite {

  int state,stateclock;
  int dx;
  Motor *jumpmotor;
  int beltdelay;
  
  int exploreWorld();
  void setState(int st,int clk,const char *face);

public:

  AcrobatChildSprite(Game *game);
  ~AcrobatChildSprite();
  
  void update();
  void updateBackground();
  void beltMove(int dx);
  
};

#endif
