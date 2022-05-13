#include <string.h>
#ifndef SITTER_WII
  #ifdef SITTER_WIN32
    typedef int iconv_t; // wtf, ms?
  #endif
  //#include <SDL/SDL.h>
#endif
#include "sitter_Error.h"
#include "sitter_AudioManager.h"
#include "sitter_Game.h"
#include "sitter_Sprite.h"
#include "sitter_Player.h"
#include "sitter_PlayerSprite.h"
#include "sitter_ResourceManager.h"
#include "sitter_VideoManager.h"
#include "sitter_AudioChannel.h"

/******************************************************************************
 * generic channel
 *****************************************************************************/
 
AudioChannel::AudioChannel(AudioManager *mgr):mgr(mgr) {
  pan=0;
  level=0xff;
  playing=true;
  chtype=SITTER_AUCHAN_NONE;
}

AudioChannel::~AudioChannel() {
}

/******************************************************************************
 * sample channel
 *****************************************************************************/
 
AudioSampleChannel::AudioSampleChannel(AudioManager *mgr):AudioChannel(mgr) {
  samplev=0;
  samplec=samplep=0;
  loopstart=-1;
  chtype=SITTER_AUCHAN_SAMPLE;
  proxtest=0;
}

AudioSampleChannel::~AudioSampleChannel() {
  if (samplev) mgr->game->res->unloadSoundEffectByBuffer(samplev);
}

void AudioSampleChannel::updateProximityTest() {
  /* as far away as the longer screen axis means silence */
  int lolimit=32; // typical sprite width
  int limit=mgr->game->video->getScreenWidth();
  if (limit<=lolimit) { level=0xff; return; } // ???
  if (mgr->game->video->getScreenHeight()>limit) limit=mgr->game->video->getScreenHeight();
  /* check all the players */
  int distance=limit;
  for (int plrid=1;plrid<=4;plrid++) if (Player *plr=mgr->game->getPlayer(plrid)) {
    if (!plr->spr) continue;
    if (!plr->spr->alive) continue;
    int dx=(plr->spr->r.x>proxtest->r.x)?(plr->spr->r.x-proxtest->r.x):(proxtest->r.x-plr->spr->r.x);
    int dy=(plr->spr->r.y>proxtest->r.y)?(plr->spr->r.y-proxtest->r.y):(proxtest->r.y-plr->spr->r.y);
    int thisdistance=dx+dy;
    if (thisdistance<distance) distance=thisdistance;
  }
  if (distance>=limit) { level=0; return; }
  if (distance<=lolimit) { level=0xff; return; }
  level=((limit-distance)*0xff)/(limit-lolimit);
}

void AudioSampleChannel::update(int16_t *dst,int len) {
  if (!playing||!samplec) return;
  if (proxtest) updateProximityTest();
  while (len>0) {
    if (samplep>=samplec) {
      if (loopstart<0) { playing=false; memset(dst,0,len<<1); return; }
      samplep=loopstart;
    }
    *dst=samplev[samplep];
    samplep++;
    len--;
    dst++;
  }
}

void AudioSampleChannel::setSample(const void *src,int srcsamplec,int loop) {
  if (loop>=srcsamplec) sitter_throw(ArgumentError,"AudioSampleChannel: loop=%d len=%d",loop,srcsamplec);
  if (((src==NULL)!=(srcsamplec==0))||(srcsamplec<0)) sitter_throw(ArgumentError,"AudioSampleChannel::setSample(%p,%d)",src,srcsamplec);
  if (samplev) mgr->game->res->unloadSoundEffectByBuffer(samplev);
  #ifndef SITTER_WII
    mgr->lock();
  #endif
    samplev=(int16_t*)src;
    samplec=srcsamplec;
    samplep=0;
    loopstart=loop;
    playing=samplev?true:false;
    level=0xff;
  #ifndef SITTER_WII
    mgr->unlock();
  #endif
}

/******************************************************************************
 * AudioInstrumentChannel
 *****************************************************************************/
 
AudioInstrumentChannel::AudioInstrumentChannel(AudioManager *mgr):AudioChannel(mgr) {
  samplev=NULL;
  samplec=0;
  loopstart=-1;
  samplep=0.0;
  level=0;
  chtype=SITTER_AUCHAN_INSTRUMENT;
  defaultSliders();
}

AudioInstrumentChannel::~AudioInstrumentChannel() {
  if (samplev) mgr->game->res->unloadSoundEffectByBuffer(samplev);
}

void AudioInstrumentChannel::update(int16_t *dst,int len) {
  if (!playing||!samplec) return;
  //sitter_log("%p AIC:update lvl=%d pan=%d",this,level,pan);
  while (len-->0) {
  
    /* push the next sample */
    if (samplep>=samplec) {
      if (loopstart>=0) samplep=loopstart+(samplep-samplec);
      else { playing=false; memset(dst,0,len<<1); return; }
    }
    *dst=samplev[(int)samplep];
    //sitter_log("%d/%d %d",(int)samplep,samplec,*dst);
    samplep+=stepslider.curr;
    dst++;
    
    /* update sliders */
    #define UPDSLIDER(sld) \
      sld.curr+=sld.rate; \
      if (sld.rate>0.0) { if (sld.curr>=sld.dest) { sld.curr=sld.dest; sld.rate=0.0; } } \
      else if (sld.rate<0.0) { if (sld.curr<=sld.dest) { sld.curr=sld.dest; sld.rate=0.0; } }
    UPDSLIDER(stepslider)
    UPDSLIDER(levelslider)
    UPDSLIDER(panslider)
    #undef UPDSLIDER
    
  }
  /* level and pan only *actually* get updated at the buffer-refill interval */
  level=(uint8_t)levelslider.curr;
  pan=(int8_t)panslider.curr;
}

void AudioInstrumentChannel::defaultSliders() {
  stepslider.curr=1.0; stepslider.dest=1.0; stepslider.rate=0.0;
  levelslider.curr=255.0; levelslider.dest=255.0; levelslider.rate=0.0;
  panslider.curr=0.0; panslider.dest=0.0; panslider.rate=0.0;
}

void AudioInstrumentChannel::setSample(const void *src,int srcsamplec,int loop,double basefreq) {
  if (loop>=srcsamplec) sitter_throw(ArgumentError,"AudioInstrumentChannel: loop=%d len=%d",loop,srcsamplec);
  if (((src==NULL)!=(srcsamplec==0))||(srcsamplec<0)) sitter_throw(ArgumentError,"AudioInstrumentChannel::setSample(%p,%d)",src,srcsamplec);
  if (basefreq<1.0) sitter_throw(ArgumentError,"AudioInstrumentChannel, basefreq=%f",basefreq);
  if (samplev) mgr->game->res->unloadSoundEffectByBuffer(samplev);
  //SDL_LockAudio(); // this *only* gets called from inside the callback
    samplev=(int16_t*)src;
    samplec=srcsamplec;
    samplep=0.0;
    loopstart=loop;
    playing=samplev?true:false;
    //level=0xff;
    //pan=0;
    this->basefreq=basefreq;
    //defaultSliders();
  //SDL_UnlockAudio();
}

void AudioInstrumentChannel::setSlider(AudioInstrumentChannel::Slider *slider,double dest,int len) {
  slider->dest=dest;
  slider->rate=(slider->dest-slider->curr)/len;
}

void AudioInstrumentChannel::setFrequency(double freq,int len) {
  if (freq<1.0) freq=1.0;
  if (len<1) len=1;
  stepslider.dest=freq/basefreq;
  stepslider.rate=(stepslider.dest-stepslider.curr)/len;
}
