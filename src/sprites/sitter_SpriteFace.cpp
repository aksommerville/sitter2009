#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_VideoManager.h"
#include "sitter_SpriteFace.h"

#define FRAMEV_INCREMENT 4

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
SpriteFace::SpriteFace(Game *game,const char *name):game(game) {
  if (!(this->name=strdup(name))) sitter_throw(AllocationError,"");
  framev=NULL; framec=framea=framep=0;
  timer=delay=0;
  loopc=loopp=0;
  donefacename=NULL;
}

SpriteFace::~SpriteFace() {
  free(name);
  for (int i=0;i<framec;i++) game->video->unloadTexture(framev[i].texid);
  if (framev) free(framev);
  if (donefacename) free(donefacename);
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
int SpriteFace::update() {
  if (!framec) return -1;
  if (++timer>=delay) {
    timer=0;
    if (++framep>=framec) {
      if ((loopc>0)&&(++loopp>=loopc)) throw SpriteFaceDone(donefacename);
      framep=0;
    }
    if (framev[framep].delaymin>=framev[framep].delaymax) delay=framev[framep].delaymin;
    else delay=framev[framep].delaymin+rand()%(framev[framep].delaymax-framev[framep].delaymin+1);
  }
  return framev[framep].texid;
}

void SpriteFace::reset() {
  timer=0;
  loopp=0;
  framep=0;
  if (!framec) return;
  if (framev[framep].delaymin>=framev[framep].delaymax) delay=framev[framep].delaymin;
  else delay=framev[framep].delaymin+rand()%(framev[framep].delaymax-framev[framep].delaymin+1);
}

/******************************************************************************
 * edit
 *****************************************************************************/
 
void SpriteFace::setLoop(int ct,const char *donefacename) {
  loopc=ct;
  if (this->donefacename) free(this->donefacename);
  if (donefacename) {
    if (!(this->donefacename=strdup(donefacename))) sitter_throw(AllocationError,"");
  } else this->donefacename=NULL;
}

void SpriteFace::addFrame(const char *texname,int delaymin,int delaymax,uint32_t flags) {
  if (delaymin<1) sitter_throw(ArgumentError,"SpriteFace::addFrame, delay=%d",delaymin);
  if (framec>=framea) {
    framea+=FRAMEV_INCREMENT;
    if (!(framev=(Frame*)realloc(framev,sizeof(Frame)*framea))) sitter_throw(AllocationError,"");
  }
  framev[framec].texid=game->video->loadTexture(texname,false,flags);
  framev[framec].delaymin=delaymin;
  framev[framec].delaymax=delaymax;
  framec++;
}
