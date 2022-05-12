#ifndef SITTER_WII

#include <malloc.h>
#include <string.h>
#ifdef SITTER_WIN32
  typedef int iconv_t; // wtf, ms?
#endif
#include <SDL/SDL.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_Configuration.h"
#include "sitter_ResourceManager.h"
#include "sitter_AudioChannel.h"
#include "sitter_Song.h"
#include "sitter_AudioManager.h"

#define CHANV_INCREMENT 4

/******************************************************************************
 * callback
 *****************************************************************************/
 
static void sitter_audio_callback1(AudioManager *audio,int16_t *dst,int dstsamplec) {
  memset(audio->mixbuf,0,dstsamplec<<3); // 4 bytes per channel
  /* read input channels */
  for (int chi=0;chi<audio->chanc;chi++) {
    if (!audio->chanv[chi]->playing) continue;
    int leftweight=(audio->chanv[chi]->pan<=0)?128:(128-audio->chanv[chi]->pan);
    int rightweight=(audio->chanv[chi]->pan>=0)?128:(128+audio->chanv[chi]->pan);
    audio->chanv[chi]->update(audio->chbuf,dstsamplec);
    int *mixdst=audio->mixbuf;
    int16_t *mixsrc=audio->chbuf;
    for (int si=0;si<dstsamplec;si++) {
      (*mixdst)+=(((*mixsrc)*leftweight)>>7)*audio->chanv[chi]->level;
      mixdst++;
      (*mixdst)+=(((*mixsrc)*rightweight)>>7)*audio->chanv[chi]->level;
      mixdst++;
      mixsrc++;
    }
  }
  /* divide by calculated weight */
  int weight=audio->chanc<<8;
  int *src=audio->mixbuf;
  for (int i=0;i<dstsamplec;i++) {
    int sample=(*src)/weight; if (sample<-32768) sample=-32768; else if (sample>32767) sample=32767;
    *dst=sample; dst++; src++;
    sample=(*src)/weight; if (sample<-32768) sample=-32768; else if (sample>32767) sample=32767;
    *dst=sample; dst++; src++;
  }
}
 
static void sitter_audio_callback(AudioManager *audio,int16_t *dst,int len) {
  int dstsamplec=(len>>2); // 2 bytes per channel, 2 channels
  if (!audio->chanc) return;
  if (!audio->song) { sitter_audio_callback1(audio,dst,dstsamplec); return; }
  while (dstsamplec>0) {
    int songframesamplec=audio->song->countSamplesToNextCommand();
    if (songframesamplec<0) {
      sitter_audio_callback1(audio,dst,dstsamplec);
      return;
    } else if (!songframesamplec) songframesamplec=dstsamplec;
    if (songframesamplec>dstsamplec) songframesamplec=dstsamplec;
    audio->song->update();
    sitter_audio_callback1(audio,dst,songframesamplec);
    audio->song->step(songframesamplec);
    dstsamplec-=songframesamplec;
    dst+=(songframesamplec<<2);
  }
}

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
AudioManager::AudioManager(Game *game,int samplechanc):game(game) {
  chanv=NULL; chanc=chana=0;
  mixbuf=NULL;
  chbuf=NULL;
  song=NULL;
  if (SDL_Init(SDL_INIT_AUDIO)) sitter_throw(SDLError,"SDL_Init(SDL_INIT_AUDIO): %s",SDL_GetError());
  
  SDL_AudioSpec desired;
  memset(&desired,0,sizeof(SDL_AudioSpec));
  desired.freq=SITTER_AUDIO_FREQUENCY;
  desired.channels=2;
  #ifdef SITTER_LITTLE_ENDIAN
    desired.format=AUDIO_S16LSB;
  #elif defined(SITTER_BIG_ENDIAN)
    desired.format=AUDIO_S16MSB;
  #else
    #error "Unsure of local byte order, assuming little-endian. Define SITTER_LITTLE_ENDIAN or SITTER_BIG_ENDIAN."
    desired.format=AUDIO_S16LSB;
  #endif
  desired.samples=SITTER_AUDIO_BUFFERSIZE;
  desired.callback=(void(*)(void*,Uint8*,int))sitter_audio_callback;
  desired.userdata=this;
  if (SDL_OpenAudio(&desired,NULL)) sitter_throw(SDLError,"SDL_OpenAudio: %s",SDL_GetError());
  
  chbuf=(int16_t*)malloc(desired.size>>1); // mono, 16-bit
  mixbuf=(int*)malloc(desired.size<<1); // stereo, 32-bit
  if (!chbuf||!mixbuf) sitter_throw(AllocationError,"audio buffers, main size = %d b",desired.size);
  
  SDL_PauseAudio(0);
  for (int i=0;i<samplechanc;i++) {
    AudioSampleChannel *chan=new AudioSampleChannel(this);
    chan->playing=false;
    addChannel(chan);
  }
}

AudioManager::~AudioManager() {
  SDL_PauseAudio(1);
  SDL_CloseAudio();
  if (song) delete song;
  for (int i=0;i<chanc;i++) delete chanv[i];
  if (chanv) free(chanv);
  if (mixbuf) free(mixbuf);
  if (chbuf) free(chbuf);
}

/******************************************************************************
 * high-level interface
 *****************************************************************************/
 
void AudioManager::update() {
}

void AudioManager::invalidateSprites() {
  for (int i=0;i<chanc;i++) if (chanv[i]->chtype==SITTER_AUCHAN_SAMPLE) if (((AudioSampleChannel*)(chanv[i]))->proxtest) {
    ((AudioSampleChannel*)(chanv[i]))->setSample(NULL,0);
    ((AudioSampleChannel*)(chanv[i]))->proxtest=NULL;
  }
}

void AudioManager::playEffect(const char *resname,Sprite *proxtest) {
  //sitter_log("playEffect...");
  if (!game->cfg->getOption_bool("sound")) return;
  AudioSampleChannel *chan=getSampleChannel();
  if (!chan) { sitter_log("not playing sound effect '%s', as no channel was found",resname); return; }
  int16_t *samplev; int samplec;
  game->res->loadSoundEffect(resname,&samplev,&samplec);
  chan->proxtest=proxtest;
  chan->setSample(samplev,samplec);
  //sitter_log("...playEffect");
}

int AudioManager::playLoop(const char *resname,Sprite *proxtest) {
  if (!game->cfg->getOption_bool("sound")) return -1;
  int rtn;
  AudioSampleChannel *chan=getSampleChannel(false,&rtn);
  if (!chan) { sitter_log("not playing loop '%s', as no channel was found",resname); return -1; }
  int16_t *samplev; int samplec;
  game->res->loadSoundEffect(resname,&samplev,&samplec);
  chan->proxtest=proxtest;
  chan->setSample(samplev,samplec,0);
  return rtn;
}

void AudioManager::stopLoop(int ref) {
  if ((ref<0)||(ref>=chanc)) return;
  if (chanv[ref]->chtype!=SITTER_AUCHAN_SAMPLE) return;
  ((AudioSampleChannel*)(chanv[ref]))->setSample(NULL,0,-1);
}

void AudioManager::setSong(const char *resname) {
  if (!game->cfg->getOption_bool("music")) { // still allow it to stop music; option may change during play
    if (song) {
      delete song;
      song=NULL;
    }
    return;
  }
  SDL_LockAudio();
  try {
    if (song) {
      if (song->resname&&resname&&!strcmp(song->resname,resname)) { // ok, it's already playing
        SDL_UnlockAudio();
        return;
      }
      delete song; song=NULL;
    }
    if (!resname||!resname[0]) { SDL_UnlockAudio(); return; }
    song=game->res->loadSong(resname);
    if (!(song->resname=strdup(resname))) sitter_throw(AllocationError,"");
    song->setup(this);
    song->start();
  } catch (...) { SDL_UnlockAudio(); throw; }
  SDL_UnlockAudio();
}

/******************************************************************************
 * channel list
 *****************************************************************************/
  
void AudioManager::addChannel(AudioChannel *chan) {
  if (chanc>=chana) {
    chana+=CHANV_INCREMENT;
    if (!(chanv=(AudioChannel**)realloc(chanv,sizeof(void*)*chana))) sitter_throw(AllocationError,"");
  }
  chanv[chanc]=chan;
  chanc++;
}

void AudioManager::removeUnusedChannels() {
  for (int i=0;i<chanc;) {
    if (chanv[i]->playing) i++;
    else {
      chanc--;
      delete chanv[i];
      if (i<chanc) memmove(chanv+i,chanv+i+1,sizeof(void*)*(chanc-i));
    }
  }
}

void AudioManager::flushInstrumentChannels() {
  for (int i=0;i<chanc;) {
    if (chanv[i]->chtype!=SITTER_AUCHAN_INSTRUMENT) i++;
    else {
      chanc--;
      delete chanv[i];
      if (i<chanc) memmove(chanv+i,chanv+i+1,sizeof(void*)*(chanc-i));
    }
  }
}

AudioSampleChannel *AudioManager::findUnusedSampleChannel() {
  for (int i=0;i<chanc;i++) if (!chanv[i]->playing&&(chanv[i]->chtype==SITTER_AUCHAN_SAMPLE)) return (AudioSampleChannel*)(chanv[i]);
  return NULL;
}

AudioSampleChannel *AudioManager::getSampleChannel(bool interrupt,int *index) {
  int besti=-1,bestlen=0x7fffffff;
  for (int i=0;i<chanc;i++) {
    if (chanv[i]->chtype!=SITTER_AUCHAN_SAMPLE) continue;
    if (!chanv[i]->playing) return (AudioSampleChannel*)(chanv[i]);
    if (!interrupt||(((AudioSampleChannel*)(chanv[i]))->loopstart>=0)) continue;
    int remaining=(((AudioSampleChannel*)(chanv[i]))->samplec-((AudioSampleChannel*)(chanv[i]))->samplep);
    if (remaining<bestlen) {
      besti=i;
      bestlen=remaining;
    }
  }
  if (besti<0) return NULL;
  if (index) *index=besti;
  return ((AudioSampleChannel*)(chanv[besti]));
}

#endif
