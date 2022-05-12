#ifndef SITTER_PLAYERSPRITE_H
#define SITTER_PLAYERSPRITE_H

/* Most of the work is managed by Player, which is not a Sprite.
 */

#include "sitter_Sprite.h"

class Player;

class PlayerSprite : public Sprite {
public:

  Player *plr;
  bool crushed;

  PlayerSprite(Game *game);
  ~PlayerSprite();
  
  void update();
  void updateBackground();
  void gridKill(int dx,int dy,int col,int row);
  void crush();
  void slice();
  void enterCannon(Sprite *cannon);
  void exitCannon(Sprite *cannon);
  
};

#endif
