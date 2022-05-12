#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_AudioManager.h"
#include "sitter_Grid.h"
#include "sitter_Motor.h"
#include "sitter_SpriteFace.h"
#include "sitter_AcrobatChildSprite.h"

#define JUMP_MOTOR " 0 ; 6 6 6 6 6 5 5 5 5 4 4 3 3 2 2 1 1 1 "

/* state */
#define STATE_UNKNOWN         0 // wait until no one is carrying me, then decide a course of action
#define STATE_WALK            1 // walk in <dx> direction until wall or cliff
#define STATE_LOOKX           2 // look out over a cliff, decide to jump or turn around
#define STATE_LOOKY           3 // look up over a wall, decide to jump or turn around
#define STATE_JUMP            4 // in the air

/* timing */
#define STATE_CLOCK_INIT     20
#define STATE_CLOCK_CARRY    60 // delay this long after being tossed (don't make decisions in the air)
#define STATE_CLOCK_LOOKX    60
#define STATE_CLOCK_LOOKY    60
#define STATE_CLOCK_BORED    30 // having decided not to jump, turn around and delay so long before doing anything else
#define STATE_CLOCK_JUMP     20 // at least as long as JUMP_MOTOR

/* result of exploreWorld */
#define EXPLORE_CLIFF     1 // I will fall if I walk forward
#define EXPLORE_PLATFORM  2 // I can walk forward
#define EXPLORE_WALL      3 // I will bump a wall (sprite, edge, ...) if I walk forward
#define EXPLORE_JCLIFF    4 // cliff, but I want to jump off
#define EXPLORE_JWALL     5 // wall, but I want to jump over it

#define JUMP_OFF_CLIFF_LIMIT 3 // if it's so many times taller than I am, don't jump
#define WALK_SPEED 2
#define JUMP_WALK_SPEED 4
#define CAN_PUSH false

AcrobatChildSprite::AcrobatChildSprite(Game *game):ChildSprite(game) {
  state=STATE_UNKNOWN;
  stateclock=STATE_CLOCK_INIT;
  dx=((rand()&1)?1:-1);
  flop=(dx<0);
  jumpmotor=new Motor(); jumpmotor->deserialise(JUMP_MOTOR);
  beltdelay=0;
}

AcrobatChildSprite::~AcrobatChildSprite() {
  delete jumpmotor;
}

void AcrobatChildSprite::setState(int st,int clk,const char *face) {
  /*
  switch (st) {
    case STATE_UNKNOWN: sitter_log("setState UNKNOWN %d %s",clk,face); break;
    case STATE_WALK: sitter_log("setState WALK %d %s",clk,face); break;
    case STATE_JUMP: sitter_log("setState JUMP %d %s",clk,face); break;
    case STATE_LOOKX: sitter_log("setState LOOKX %d %s",clk,face); break;
    case STATE_LOOKY: sitter_log("setState LOOKY %d %s",clk,face); break;
    default: sitter_log("setState %d %d %s",st,clk,face);
  }*/
  setFace(face);
  state=st;
  stateclock=clk;
}

// TEMP:
#include <stdio.h>
static char state_name_buffer[256];
static const char *stateName(int state) {
  switch (state) {
    case STATE_UNKNOWN: return "UNKNOWN";
    case STATE_WALK: return "WALK";
    case STATE_JUMP: return "JUMP";
    case STATE_LOOKX: return "LOOKX";
    case STATE_LOOKY: return "LOOKY";
    default: snprintf(state_name_buffer,256,"%d?",state); state_name_buffer[255]=0; return state_name_buffer;
  }
}

void AcrobatChildSprite::update() {
  ChildSprite::update();
  if (!alive) return; // being dead precludes most activity
  if (beingcarried) { // being carried cancels everything
    setState(STATE_UNKNOWN,STATE_CLOCK_CARRY,"idle");
    return; 
  } 
  if (beltdelay>0) {
    beltdelay--;
    setState(STATE_UNKNOWN,1,"idle");
    return;
  }
  //sitter_log("state=%-8s %-8s %d",stateName(state),facev[facep]->name,game->play_clock);
  if (gravitymotor->getState()!=SITTER_MOTOR_IDLE) setState(STATE_WALK,0,"walk");//state=STATE_WALK;
  switch (state) {
    case STATE_UNKNOWN: {
        if (--stateclock>0) return;
        switch (exploreWorld()) {
          case EXPLORE_CLIFF: dx=-dx; flop=(dx<0); setState(STATE_WALK,0,"walk"); break;
          case EXPLORE_PLATFORM: setState(STATE_WALK,0,"walk"); break;
          case EXPLORE_WALL: dx=-dx; flop=(dx<0); state=STATE_WALK; setFace("walk"); break;
          case EXPLORE_JCLIFF: {
              setState(STATE_JUMP,STATE_CLOCK_JUMP,"walk"); 
              jumpmotor->start(true); 
              gravityok=false; 
              game->audio->playEffect("jump",this);
            } break;
          case EXPLORE_JWALL: {
              setState(STATE_JUMP,STATE_CLOCK_JUMP,"walk"); 
              jumpmotor->start(true); 
              gravityok=false; 
              game->audio->playEffect("jump",this);
            } break;
        }
      } break;
    case STATE_WALK: {
        flop=(dx<0);
        int leadingedge=((dx>0)?r.right():r.left());
        if (!(leadingedge%game->grid->colw)) { // at a column border, look around
          switch (exploreWorld()) {
            case EXPLORE_JCLIFF:
            case EXPLORE_CLIFF: setState(STATE_LOOKX,STATE_CLOCK_LOOKX,"lookx"); return;
            case EXPLORE_JWALL:
            case EXPLORE_WALL: setState(STATE_LOOKY,STATE_CLOCK_LOOKY,"looky"); return;
          }
        }
        int speed=dx;
        if (!(leadingedge%WALK_SPEED)) speed*=WALK_SPEED; // align to WALK_SPEED
        if (!move(speed,0,CAN_PUSH)) {
          if (gravitymotor->getState()!=SITTER_MOTOR_IDLE) return;
          setState(STATE_LOOKY,STATE_CLOCK_LOOKY,"looky");
        }
      } break;
    case STATE_LOOKX: {
        if (--stateclock>0) return;
        switch (exploreWorld()) {
          case EXPLORE_JWALL:
          case EXPLORE_JCLIFF: {
              setState(STATE_JUMP,STATE_CLOCK_JUMP,"walk"); 
              jumpmotor->start(true); 
              gravityok=false; 
              game->audio->playEffect("jump",this);
            } break;
          default: dx=-dx; flop=(dx<0); setState(STATE_UNKNOWN,STATE_CLOCK_BORED,"idle"); break;
        }
      } break;
    case STATE_LOOKY: {
        if (--stateclock>0) return;
        switch (exploreWorld()) {
          case EXPLORE_JCLIFF:
          case EXPLORE_JWALL: {
              setState(STATE_JUMP,STATE_CLOCK_JUMP,"walk"); 
              jumpmotor->start(true); 
              gravityok=false; 
              game->audio->playEffect("jump",this);
            } break;
          default: dx=-dx; flop=(dx<0); setState(STATE_UNKNOWN,STATE_CLOCK_BORED,"idle"); break;
        }
      } break;
    case STATE_JUMP: {
        if ((--stateclock<=0)&&gravityok&&(gravitymotor->getState()==SITTER_MOTOR_IDLE)) {
          setState(STATE_WALK,0,"walk");
          return;
        }
        flop=(dx<0);
        move(0,-jumpmotor->update(),CAN_PUSH);
        if (jumpmotor->getState()==SITTER_MOTOR_IDLE) gravityok=true;
        if (!move(dx*JUMP_WALK_SPEED,0,CAN_PUSH)) {
          // JUMP also includes some walking afterwards; here's where that stops:
          if (!gravityok) return;
          if (gravitymotor->getState()!=SITTER_MOTOR_IDLE) return;
          setState(STATE_LOOKX,STATE_CLOCK_LOOKX,"lookx");
          gravityok=true;
        }
      } break;
  }
}

void AcrobatChildSprite::updateBackground() {
  ChildSprite::updateBackground();
}

int AcrobatChildSprite::exploreWorld() {
  Rect restorer(r);
  r.x+=dx*r.w;
  if (collides()) { // obstruction directly ahead
    r.y-=r.h;
    if (collides()) { // wall is at least as tall as I am
      r=restorer;
      return EXPLORE_WALL;
    } else { // short wall, no taller than me
      r=restorer;
      return EXPLORE_JWALL;
    }
  } else { // clear ahead
    r.y+=r.h;
    if (collides(true)) { // path appears walkable
      r=restorer;
      return EXPLORE_PLATFORM;
    } else { // cliff
      int depth=1; // i'm not afraid of heights, but depths terrify me :)
      while (1) {
        r.y+=r.h;
        if (collides(true)) break;
        if (++depth>=JUMP_OFF_CLIFF_LIMIT) break;
      }
      r=restorer;
      return (depth<JUMP_OFF_CLIFF_LIMIT)?EXPLORE_JCLIFF:EXPLORE_CLIFF;
    }
  }
}

void AcrobatChildSprite::beltMove(int dx) {
  beltdelay++;
  ChildSprite::beltMove(dx);
}
