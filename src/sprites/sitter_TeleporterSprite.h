#ifndef SITTER_TELEPORTERSPRITE_H
#define SITTER_TELEPORTERSPRITE_H

#include "sitter_Sprite.h"

class TeleporterRecSprite;

class TeleporterSprite : public Sprite {
public:

  TeleporterRecSprite *receptacle;
  int arockin_counter;
  Motor *arockin_motor;

  TeleporterSprite(Game *game);
  void update();
  
  void teleport(Sprite *spr);
};

class TeleporterRecSprite : public Sprite {
public:

  Sprite *fx;
  TeleporterSprite *teleporter;
  bool fxvis;
  
  TeleporterRecSprite(Game *game);
  void update();
  
  bool ready(); // has 32x32 clearance above
};

#endif
