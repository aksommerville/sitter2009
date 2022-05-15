#ifndef SITTER_WII

#include <malloc.h>
#include <string.h>
#ifdef SITTER_WIN32
  typedef int iconv_t; // wtf, ms?
#endif

#ifdef SITTER_LINUX_DRM
  #include "drm_alsa_evdev/sitter_alsa.h"
#else
  #include <SDL/SDL.h>
#endif

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
 
static void sitter_audio_callback1(AudioManager *audio,int16_t *dst,int dstframec) {
  int reqintc=dstframec<<1;
  if (reqintc>audio->mixbufa) {
    void *nv=realloc((void*)audio->mixbuf,sizeof(int)*reqintc);
    if (!nv) return;
    audio->mixbuf=(int*)nv;
    nv=realloc((void*)audio->chbuf,sizeof(int16_t)*reqintc);
    if (!nv) return;
    audio->chbuf=(int16_t*)nv;
    audio->mixbufa=reqintc;
  }
  memset(audio->mixbuf,0,dstframec<<3); // 4 bytes per channel
  /* read input channels */
  for (int chi=0;chi<audio->chanc;chi++) {
    if (!audio->chanv[chi]->playing) continue;
    int leftweight=(audio->chanv[chi]->pan<=0)?128:(128-audio->chanv[chi]->pan);
    int rightweight=(audio->chanv[chi]->pan>=0)?128:(128+audio->chanv[chi]->pan);
    audio->chanv[chi]->update(audio->chbuf,dstframec);
    int *mixdst=audio->mixbuf;
    int16_t *mixsrc=audio->chbuf;
    for (int si=0;si<dstframec;si++) {
      (*mixdst)+=(((*mixsrc)*leftweight)>>7)*audio->chanv[chi]->level;
      mixdst++;
      (*mixdst)+=(((*mixsrc)*rightweight)>>7)*audio->chanv[chi]->level;
      mixdst++;
      mixsrc++;
    }
  }
  /* divide by calculated weight */
  //int weight=audio->chanc<<8;
  int weight=512;
  int *src=audio->mixbuf;
  for (int i=0;i<dstframec;i++) {
    int sample=(*src)/weight; if (sample<-32768) sample=-32768; else if (sample>32767) sample=32767;
    *dst=sample; dst++; src++;
    sample=(*src)/weight; if (sample<-32768) sample=-32768; else if (sample>32767) sample=32767;
    *dst=sample; dst++; src++;
  }
}
 
static void sitter_audio_callback(AudioManager *audio,int16_t *dst,int len) {
  int dstframec=(len>>2); // 2 bytes per channel, 2 channels
  if (!audio->chanc) return;
  if (!audio->song) { sitter_audio_callback1(audio,dst,dstframec); return; }
  while (dstframec>0) {
    int songframesamplec=audio->song->countSamplesToNextCommand();
    if (songframesamplec<0) {
      sitter_audio_callback1(audio,dst,dstframec);
      return;
    } else if (!songframesamplec) songframesamplec=dstframec;
    if (songframesamplec>dstframec) songframesamplec=dstframec;
    audio->song->update();
    sitter_audio_callback1(audio,dst,songframesamplec);
    audio->song->step(songframesamplec);
    dstframec-=songframesamplec;
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
  
  #ifdef SITTER_LINUX_DRM
    if (sitter_alsa_init(
      SITTER_AUDIO_FREQUENCY,2,
      sitter_audio_callback,this
    )<0) sitter_throw(Error,"sitter_alsa_init failed");
    
  #else
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
    mixbufa=desired.size>>1;
    if (!chbuf||!mixbuf) sitter_throw(AllocationError,"audio buffers, main size = %d b",desired.size);
  
    SDL_PauseAudio(0);
  #endif
  
  for (int i=0;i<samplechanc;i++) {
    AudioSampleChannel *chan=new AudioSampleChannel(this);
    chan->playing=false;
    addChannel(chan);
  }
}

AudioManager::~AudioManager() {
  #ifdef SITTER_LINUX_DRM
    sitter_alsa_quit();
  #else
    SDL_PauseAudio(1);
    SDL_CloseAudio();
  #endif
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
  lock();
  AudioSampleChannel *chan=getSampleChannel();
  if (!chan) {
    sitter_log("not playing sound effect '%s', as no channel was found",resname);
    unlock();
    return;
  }
  int16_t *samplev; int samplec;
  game->res->loadSoundEffect(resname,&samplev,&samplec);
  chan->proxtest=proxtest;
  chan->setSample(samplev,samplec);
  unlock();
  //sitter_log("...playEffect");
}

int AudioManager::playLoop(const char *resname,Sprite *proxtest) {
  if (!game->cfg->getOption_bool("sound")) return -1;
  lock();
  int rtn;
  AudioSampleChannel *chan=getSampleChannel(false,&rtn);
  if (!chan) {
    sitter_log("not playing loop '%s', as no channel was found",resname);
    unlock();
    return -1;
  }
  int16_t *samplev; int samplec;
  game->res->loadSoundEffect(resname,&samplev,&samplec);
  chan->proxtest=proxtest;
  chan->setSample(samplev,samplec,0);
  unlock();
  return rtn;
}

void AudioManager::stopLoop(int ref) {
  if ((ref<0)||(ref>=chanc)) return;
  lock();
  if (chanv[ref]->chtype==SITTER_AUCHAN_SAMPLE) {
    ((AudioSampleChannel*)(chanv[ref]))->setSample(NULL,0,-1);
  }
  unlock();
}

void AudioManager::setSong(const char *resname) {
  if (!game->cfg->getOption_bool("music")) { // still allow it to stop music; option may change during play
    if (song) {
      delete song;
      song=NULL;
    }
    return;
  }
  lock();
  try {
    if (song) {
      if (song->resname&&resname&&!strcmp(song->resname,resname)) { // ok, it's already playing
        unlock();
        return;
      }
      delete song; song=NULL;
    }
    if (!resname||!resname[0]) { unlock(); return; }
    song=game->res->loadSong(resname);
    if (!(song->resname=strdup(resname))) sitter_throw(AllocationError,"");
    song->setup(this);
    song->start();
  } catch (...) { unlock(); throw; }
  unlock();
}

/* Lock.
 */
 
#if SITTER_LINUX_DRM
  void AudioManager::lock() {
    sitter_alsa_lock();
  }
  void AudioManager::unlock() {
    sitter_alsa_unlock();
  }
#else
  void AudioManager::lock() {
    SDL_LockAudio();
  }
  void AudioManager::unlock() {
    SDL_UnlockAudio();
  }
#endif

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
