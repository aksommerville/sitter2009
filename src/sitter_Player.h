#ifndef SITTER_PLAYER_H
#define SITTER_PLAYER_H

class Game;
class Sprite;
class PlayerSprite;
class Motor;
class CannonSprite;

class Player {
public:

  Game *game;
  PlayerSprite *spr; // assume that this may be deleted without my knowledge. because it will. (we'll get a makeSprite() before any events)
  
  int dx,dy;
  bool xclear; // true if the last walk succeeded fully (required for "fast" jumpmotor)
  bool suspended; // gravity failed and our feet are dangling (holding on to a girder or something?). Go through the motions, but don't walk.
  Motor *walkmotor,*turnaroundmotor;
  bool wouldwalk,ducking;
  bool unduck;
  bool duck_panic; // if you're ducking and can't get up, you're allowed to walk
  bool wouldduck;
  bool wantsattention; // when true (default) and alive, camera will share focus with this player
  CannonSprite *ctl_cannon; // hey, who put the cannon's controls on the inside?
  
  /* We have four different jumping motors: normal,fast,carrying,carrying+fast
   * jumpmotor points to one of these and may be NULL; it's what you should read.
   */
  Motor *jumpmotor_n,*jumpmotor_f,*jumpmotor_c,*jumpmotor_cf;
  Motor *jumpmotor;
  
  /* NULL when we're not carrying anything.
   * carryblackout counts down when positive. While nonzero, pickup and toss are not allowed.
   * In the very likely event that you have pickup and toss mapped to the same button, this
   * prevents both from triggering from the same button press.
   */
  Sprite *carry;
  int carryblackout;
  
  /* plrid is a 1-based index of active Players.
   * It may be zero if the Player is not registered.
   */
  int plrid;

  Player(Game *game);
  ~Player();
  
  void update();
  void updateBackground();
  
  void reset();
  
  /* set face, state-aware */
  void walkFace();
  void noWalkFace();
  void carryFace();
  void noCarryFace();
  
  /* events */
  void left_on();
  void left_off();
  void right_on();
  void right_off();
  void jump_on();
  void jump_off();
  void duck_on();
  void duck_off(); // in some locales, "go_duck_yourself"
  void pickup_on();
  void pickup_off();
  void toss_on();
  void toss_off();
  
  Sprite *findPickupSprite();
  void updateCannon();
  void fireCannon();
  
};

#endif
