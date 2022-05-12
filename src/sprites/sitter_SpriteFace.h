#ifndef SITTER_SPRITEFACE_H
#define SITTER_SPRITEFACE_H

/* A SpriteFace is a set of texture IDs each with an associated time.
 * Call update() every frame, it returns the current texture ID.
 * By default, a face will loop forever.
 * You can call setLoop with loop count and 'donefacename', after
 * so many loops, will throw SpriteFaceDone with the name of the face
 * to switch to.
 * Frames may have a range of delays; actual delay is chosen randomly
 * within that range.
 */
 
#include "sitter_VideoManager.h"
 
class Game;
 
class SpriteFaceDone {
public:
  const char *nextfacename;
  SpriteFaceDone(const char *s):nextfacename(s) { if (!nextfacename) nextfacename=""; }
};

class SpriteFace {

  Game *game;

  typedef struct {
    int texid;
    int delaymin,delaymax;
  } Frame;
  Frame *framev; int framec,framea,framep;
  int timer,delay;
  int loopp;

public:

  char *name;
  char *donefacename;
  int loopc;

  SpriteFace(Game *game,const char *name);
  ~SpriteFace();
  
  int update();
  void reset();
  bool getSync() const { return ((timer==0)&&(framep==0)); }
  
  /* 0,0 means loop forever. (default)
   */
  void setLoop(int ct=0,const char *donefacename=0);
  void addFrame(const char *texname,int delaymin,int delaymax=-1,uint32_t flags=SITTER_TEX_FILTER); // if delaymax<delaymin, ignore delaymax
  
};

#endif
