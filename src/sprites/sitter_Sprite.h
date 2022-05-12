#ifndef SITTER_SPRITES_H
#define SITTER_SPRITES_H

#include <stdint.h>
#include "sitter_Rect.h"

class Game;
class SpriteGroup;
class SpriteFace;
class Motor;
class Grid;

/* Sprite::identity */
#define SITTER_SPRIDENT_NONE            0
#define SITTER_SPRIDENT_BELT            1
#define SITTER_SPRIDENT_CHAINSAW        2
#define SITTER_SPRIDENT_TUXDOLL         3
#define SITTER_SPRIDENT_CANNON          4
#define SITTER_SPRIDENT_CBTN_FIRE       5
#define SITTER_SPRIDENT_CBTN_CLK        6
#define SITTER_SPRIDENT_CBTN_CTR        7
#define SITTER_SPRIDENT_TELEPORTER      8
#define SITTER_SPRIDENT_TELEPORTERREC   9
#define SITTER_SPRIDENT_PLAYER         10
#define SITTER_SPRIDENT_HELICOPTER     11

class Sprite {
public:

  Game *game;
  
  SpriteGroup **grpv; int grpc,grpa;
  bool autokill; // delete sprite when removing last group, default true
  int layer; // for sorting, higher is more visible
  
  Rect r;
  int texid;
  float opacity;
  uint32_t tint;
  float rotation;
  bool flop,neverflop; // flop ignored if neverflop
  
  // sorted by name
  SpriteFace **facev; int facec,facea,facep;
  
  /* Collision detection enable.
   * All default to false.
   */
  bool edge_collisions,grid_collisions,sprite_collisions;
  bool collide_unslicable; // hack for the chainsaw; use sprite_collisions but not slicable sprites
  bool pushable; // only relevant with sprite_collisions
  SpriteGroup *standingongrp; // sprites that stopped my last +y move (I can't push them). TODO -- unused, can we remove it?
  bool ongoal;
  int goalid; // 0,1,2,3,4; who owns the goal I'm on?
  bool alive;
  bool slicer; // i slice other sprites
  int identity; // subclasses may use this to identify each other
  
  /* Don't just set ongoal if the sprite you're standing on has it.
   * Geez, if every other sprite went and jumped off a cliff, would you be ongoal?
   */
  bool couldConceivablyBeOnGoal();
  
  Motor *gravitymotor; bool gravityok;
  void turnOnGravity(); // allocates the motor, update() takes care of the rest (make sure grid_collisions is set)
  void turnOffGravity(); // temporarily disable gravity, eg when i'm being carried
  
  /* inertia counts to zero (from positive or negative).
   * Its sign multiplies by inertiamotor->update().
   */
  Motor *inertiamotor; int inertia;
  void setInertia(int dx);
  void resetInertia();
  Motor *rocketmotorx,*rocketmotory; // extra inertia, set by seesaw or cannon
  bool belted; // ugly hack to keep from multiple conveyor belts moving one sprite
  
  /* Everything in this group moves as a single unit.
   * This is a unique object, but all Sprites in a binding group should have the same contents.
   * In particular, when one sprite moves, its bound-sprites do not examine their binding lists (as that would create an infinite loop).
   * So, unlike pushing, binding is not recursive.
   */
  SpriteGroup *boundsprites;
  bool beingcarried;
  
  Sprite(Game *game);
  virtual ~Sprite();
  
  /* One frame (updateBackground when menu is running).
   * If you override, be sure to call back here for face animation and gravity.
   */
  virtual void update();
  virtual void updateBackground();
  // normally you will ignore the returns:
  bool update_gravity(); // true if i moved
  bool update_inertia(); // true if i moved
  bool update_animation(); // true if my frame changed
  bool update_slicer(); // true if i killed anybody
  
  /* Override this if you want to get killed by spikes and such.
   * dx,dy is how you were moving at the time.
   * col,row is the coordinates of the offending cell.
   * Entirely possible to be called more than once for the same incident.
   */
  virtual void gridKill(int dx,int dy,int col,int row) {}
  virtual void crush() {}
  virtual void pickup(Sprite *picker) {} // players must call as they pick up a sprite
  virtual void slice() {}
  virtual void mutate() {} // called when something unnatural (eg teleporting) happens. if you want to give yourself cancer or something.
  virtual void beltMove(int dx) {} // called for every successful movement by conveyor belt
  virtual void enterCannon(Sprite *cannon) {} // all of the necessary changes are committed by the cannon itself
  virtual void exitCannon(Sprite *cannon) {}
  
  /* Moves axes independantly, but you can give both together. Will return most significant.
   */
  int move(int dx,int dy,bool push=false,bool movebindees=true);
  
  /* Check collision with <spr>.
   * If dx||dy, look at the new space I will occupy (assume my 'r' has not yet moved).
   * !dx&&!dy, use my full 'r'.
   * One of dx,dy should be zero.
   * outdx,outdy are the amount i *can* move without colliding. Will always be zero, or the same sign as dx,dy.
   * outdx,outdy are not populated if dx,dy are both 0, or if the return is false.
   */
  bool collides_sprite(const Sprite *spr,int dx=0,int dy=0,int *outdx=0,int *outdy=0) const;
  bool collides_grid(const Grid *grid,int dx=0,int dy=0,int *outdx=0,int *outdy=0,int *colcol=0,int *colrow=0,bool countsemi=false);
  bool collides_edge(int dx=0,int dy=0,int *outdx=0,int *outdy=0) const;
  bool collides(bool countsemi=false); // check all possible collisions for where i am right now (const, but it calls collides_grid)
  
  bool hasGroup(const SpriteGroup *grp) const { for (int i=0;i<grpc;i++) if (grpv[i]==grp) return true; return false; }
  void addGroup(SpriteGroup *grp,bool recip=true);
  void removeGroup(SpriteGroup *grp,bool recip=true); // may delete if autokill
  void removeAllGroups(); // watch out, this *will* delete me if autokill
  
  /* -1 = me first
   *  1 = it first
   *  0 = equivalent
   */
  int cmp(const Sprite *spr) const { 
    if (layer<spr->layer) return -1; 
    if (layer>spr->layer) return 1; 
    if (r.bottom()<spr->r.bottom()) return -1;
    if (r.bottom()>spr->r.bottom()) return 1;
    return 0;
  }
  
  /* Faces are sorted alphabetically, so you must setFace *after* you're done adding Faces.
   * If you add a Face after, you must call setFace again.
   */
  void addFace(SpriteFace *face);
  bool setFace(const char *name); // do nothing if i don't have this face
  const char *getFace() const;
  
};

class SpriteGroup {
public:

  Game *game;
  Sprite **sprv; int sprc,spra;
  bool addsorted;
  int sortdir;
  int scrollx,scrolly; // only relevant when drawing; normally synced to grid
  
  SpriteGroup(Game *game);
  ~SpriteGroup();
  
  void update() { for (int i=0;i<sprc;i++) sprv[i]->update(); }
  void updateBackground() { for (int i=0;i<sprc;i++) sprv[i]->updateBackground(); }
  
  bool hasSprite(const Sprite *spr) const { for (int i=0;i<sprc;i++) if (sprv[i]==spr) return true; return false; }
  void addSprite(Sprite *spr,bool recip=true);
  void removeSprite(Sprite *spr,bool recip=true);
  void removeAllSprites() { for (int i=0;i<sprc;i++) sprv[i]->removeGroup(this,false); sprc=0; }
  void killAllSprites() { while (sprc) sprv[0]->removeAllGroups(); }
  
  /* sort() sorts the list completely.
   * sort1() performs one pass of a bubble sort, alternating direction.
   * Normally, you can tolerate a few frames out of order.
   * After calling sort1(), any Sprites added will be added at the correct sort position.
   */
  bool sort1();
  void sort() { while (sort1()) ; }
  
};

#endif
