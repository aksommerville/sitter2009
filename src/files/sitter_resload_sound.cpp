#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_File.h"
#include "sitter_ResourceManager.h"

/******************************************************************************
 * sound effects
 *****************************************************************************/
 
#define SOUNDCACHEV_INCREMENT  8
#define SOUNDCACHEV_LIMIT     32 // maximum number of loaded samples -- if exceeded, it's an error
 
void ResourceManager::loadSoundEffect(const char *resname,int16_t **data,int *samplec,bool cacheonly) {
  if (!resname||!resname[0]) return;
  try {
  /* read from cache if possible */
  int cacheavailable=-1;
  for (int i=0;i<soundcachec;i++) {
    if (!soundcachev[i].refc) {
      cacheavailable=i;
    }
    if (!soundcachev[i].resname||!soundcachev[i].data) continue;
    if (!strcmp(resname,soundcachev[i].resname)) {
      soundcachev[i].refc++;
      *data=(int16_t*)(soundcachev[i].data);
      *samplec=soundcachev[i].samplec;
      //sitter_log("...was cached");
      //unlock();
      return;
    }
  }
  if (cacheonly) { *data=0; *samplec=0; return; }
  lock();
  /* read file */
  const char *path=resnameToPath(resname,&sfxpfx,&sfxpfxlen);
  //sitter_log("...from file '%s'...",path);
  if (File *f=sitter_open(path,"rb")) {
    try {
      int flen=f->seek(0,SEEK_END);
      //sitter_log("...len=%d...",flen);
      if (!(*data=(int16_t*)malloc(flen))) sitter_throw(AllocationError,"");
      f->seek(0);
      if (f->read(*data,flen)!=flen) sitter_throw(ShortIOError,"%s",path);
      *samplec=flen>>1;
    } catch (...) { delete f; throw; }
    delete f;
  } else sitter_throw(IOError,"sound '%s': open failed",resname);
  /* fix format */
  #if defined(SITTER_BIG_ENDIAN) // we're presuming that files are all little-endian
    //sitter_log("...byte-swapping...");
    uint8_t *swapbuf=(uint8_t*)*data;
    for (int i=0;i<*samplec;i++) {
      uint8_t tmp=swapbuf[0];
      swapbuf[0]=swapbuf[1];
      swapbuf[1]=tmp;
      swapbuf+=2;
    }
  #endif
  /* find cache space */
  //sitter_log("...cache search...");
  /*if (cacheavailable>=0) {
    if (soundcachev[cacheavailable].data) free(soundcachev[cacheavailable].data);
    if (soundcachev[cacheavailable].resname) free(soundcachev[cacheavailable].resname);
  } else*/ if (soundcachec<soundcachea) {
    cacheavailable=soundcachec++;
  } else if (soundcachea<SOUNDCACHEV_LIMIT) {
    soundcachea+=SOUNDCACHEV_INCREMENT;
    if (!(soundcachev=(SoundEffectCacheEntry*)realloc(soundcachev,sizeof(SoundEffectCacheEntry)*soundcachea))) sitter_throw(AllocationError,"");
    cacheavailable=soundcachec++;
  } else if (cacheavailable>=0) { // overwriting is last resort
    if (soundcachev[cacheavailable].data) free(soundcachev[cacheavailable].data);
    if (soundcachev[cacheavailable].resname) free(soundcachev[cacheavailable].resname);
  } else { // can't cache -- this is a problem, as we are supposed to be responsible for the buffer
    sitter_throw(IdiotProgrammerError,"ResourceManager can't find cache space for sample '%s'",resname);
  }
  //sitter_log("%s in cache slot %d (c=%d a=%d)...",resname,cacheavailable,soundcachec,soundcachea);
  /* add to cache */
  if (!(soundcachev[cacheavailable].resname=strdup(resname))) sitter_throw(AllocationError,"");
  soundcachev[cacheavailable].data=*data;
  soundcachev[cacheavailable].samplec=*samplec;
  soundcachev[cacheavailable].refc=1;
  //sitter_log("...loadSoundEffect");
  } catch (...) { unlock(); throw; }
  unlock();
}

void ResourceManager::unloadSoundEffectByName(const char *resname) {
  if (!resname||!resname[0]) return;
  lock();
  for (int i=0;i<soundcachec;i++) {
    if (!strcmp(soundcachev[i].resname,resname)) {
      soundcachev[i].refc--;
      unlock();
      return;
    }
  }
  unlock();
  sitter_log("WARNING: unloadSoundEffectByName '%s' -- effect not found",resname);
}

void ResourceManager::unloadSoundEffectByBuffer(const void *data) {
  lock();
  for (int i=0;i<soundcachec;i++) {
    if (data==(void*)soundcachev[i].data) {
      soundcachev[i].refc--;
      unlock();
      return;
    }
  }
  unlock();
  sitter_log("WARNING: unloadSoundEffectByBuffer %p -- effect not found",data);
}
