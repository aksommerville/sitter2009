#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_ResourceManager.h"
#include "sitter_AudioManager.h"
#include "sitter_AudioChannel.h"
#include "sitter_Song.h"

#define CMDV_INCREMENT 64
#define SAMPLENAMEV_INCREMENT 4

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Song::Song() {
  cmdv=NULL;
  cmdc=cmda=cmdp=0;
  beatp=samplep=0;
  samplesperbeat=1;
  playing=false;
  chanc=0;
  chanv=NULL;
  samplenamev=NULL; samplenamec=samplenamea=0;
  mgr=NULL;
  sampleloop=0;
  samplefreq=440.0;
  resname=NULL;
}

Song::~Song() {
  if (cmdv) free(cmdv);
  if (chanv) free(chanv);
  for (int i=0;i<samplenamec;i++) {
    mgr->game->res->unloadSoundEffectByName(samplenamev[i]);
    free(samplenamev[i]);
  }
  if (samplenamev) free(samplenamev);
  if (mgr) mgr->flushInstrumentChannels();
  if (resname) free(resname);
}

void Song::setup(AudioManager *mgr) {
  this->mgr=mgr;
  mgr->flushInstrumentChannels();
  if (chanv) free(chanv);
  if (!chanc) { chanv=NULL; playing=false; return; }
  if (!(chanv=(AudioInstrumentChannel**)malloc(sizeof(void*)*chanc))) sitter_throw(AllocationError,"");
  for (int i=0;i<chanc;i++) {
    chanv[i]=new AudioInstrumentChannel(mgr);
    //sitter_log("chan %d = %p",i,chanv[i]);
    mgr->addChannel(chanv[i]);
  }
}

/******************************************************************************
 * high-level interface
 *****************************************************************************/
 
void Song::start() {
  if (cmdc) {
    playing=true;
  }
}

void Song::stop() {
  playing=false;
}

void Song::rewind() {
  // TODO -- must reset all GOTO commands
  cmdp=0;
  beatp=samplep=0;
}

/******************************************************************************
 * low-level interface (from main audio callback)
 *****************************************************************************/
  
int Song::countSamplesToNextCommand() {
  if (!playing||(cmdp>=cmdc)) return -1;
  int cmdck=cmdp;
  while ((cmdck<cmdc)&&(cmdv[cmdck].beat==beatp)) cmdck++;
  if (cmdck==cmdp) return 0;
  if (cmdck>=cmdc) return 0;
  return (cmdv[cmdck].beat-beatp)*samplesperbeat;
}

void Song::step(int samplec) {
  samplep+=samplec;
  beatp+=(samplep/samplesperbeat);
  samplep%=samplesperbeat;
}

// warnings disabled as they may kill the wii
void Song::update() {
  //sitter_log("Song::update playing=%d mgr=%p cmdc=%d cmdp=%d beatp=%d nextbeat=%d",playing,mgr,cmdc,cmdp,beatp,(cmdp<cmdc)?cmdv[cmdp].beat:-1);
  if (!playing||!mgr||!cmdc) return;
  while ((cmdp<cmdc)&&(cmdv[cmdp].beat==beatp)) {
    switch (cmdv[cmdp].opcode) {
      case SITTER_SONGOP_NOOP: break;
      case SITTER_SONGOP_GOTO: { 
          if ((cmdv[cmdp].op_goto.p>=0)&&(cmdv[cmdp].op_goto.p<cmdc)) {
            //sitter_log("GOTO %d ct_static=%d ct=%d",cmdv[cmdp].op_goto.p,cmdv[cmdp].op_goto.ct_static,cmdv[cmdp].op_goto.ct);
            if ((cmdv[cmdp].op_goto.ct_static>=0)&&(cmdv[cmdp].op_goto.ct>=cmdv[cmdp].op_goto.ct_static)) {
              cmdv[cmdp].op_goto.ct=0;
              cmdp++;
            } else {
              cmdv[cmdp].op_goto.ct++;
              cmdp=cmdv[cmdp].op_goto.p;
              beatp=cmdv[cmdp].beat;
            }
          } else {
            //sitter_log("WARNING: song @ %d: GOTO %d (command count=%d)",beatp,cmdv[cmdp].op_goto.p,cmdc);
            cmdp++;
          }
        } continue; // don't break; cmdp is where we want it
      case SITTER_SONGOP_SAMPLE: {
          int chid=cmdv[cmdp].op_sample.chid;
          if ((chid<0)||(chid>=chanc)) ;//sitter_log("WARNING: song @ %d: SAMPLE chid=%d (channel count=%d)",beatp,chid,chanc);
          else {
            int smplid=cmdv[cmdp].op_sample.smplid;
            if ((smplid<0)||(smplid>=samplenamec)) ;//sitter_log("WARNING: song @ %d: SAMPLE smplid=%d (sample count=%d)",beatp,smplid,samplenamec);
            else {
              int16_t *buf=0; int len=0;
              mgr->game->res->loadSoundEffect(samplenamev[smplid],&buf,&len,true);
              if (smplid==0) { // main instrument
                chanv[chid]->setSample(buf,len,sampleloop,samplefreq);
              } else { // drum
                chanv[chid]->setSample(buf,len,-1,440.0);
              }
            }
          }
        } break;
      case SITTER_SONGOP_FREQ: {
          int chid=cmdv[cmdp].op_slider.chid;
          if ((chid<0)||(chid>=chanc)) ;//sitter_log("WARNING: song @ %d: FREQ chid=%d (channel count=%d)",beatp,chid,chanc);
          else chanv[chid]->setFrequency(cmdv[cmdp].op_slider.dest,cmdv[cmdp].op_slider.len);
        } break;
      case SITTER_SONGOP_LEVEL: {
          int chid=cmdv[cmdp].op_slider.chid;
          if ((chid<0)||(chid>=chanc)) ;//sitter_log("WARNING: song @ %d: LEVEL chid=%d (channel count=%d)",beatp,chid,chanc);
          else chanv[chid]->setLevel(cmdv[cmdp].op_slider.dest,cmdv[cmdp].op_slider.len);
        } break;
      case SITTER_SONGOP_PAN: {
          int chid=cmdv[cmdp].op_slider.chid;
          if ((chid<0)||(chid>=chanc)) ;//sitter_log("WARNING: song @ %d: PAN chid=%d (channel count=%d)",beatp,chid,chanc);
          else chanv[chid]->setPan(cmdv[cmdp].op_slider.dest,cmdv[cmdp].op_slider.len);
        } break;
      case SITTER_SONGOP_REWIND: {
          int chid=cmdv[cmdp].op_channel.chid;
          if ((chid<0)||(chid>=chanc)) ;//sitter_log("WARNING: song @ %d: REWIND chid=%d (channel count=%d)",beatp,chid,chanc);
          else {
            //chanv[chid]->defaultSliders();
            chanv[chid]->samplep=0.0;
          }
        } break;
      case SITTER_SONGOP_START: {
          int chid=cmdv[cmdp].op_channel.chid;
          if ((chid<0)||(chid>=chanc)) ;//sitter_log("WARNING: song @ %d: START chid=%d (channel count=%d)",beatp,chid,chanc);
          else chanv[chid]->playing=true;
        } break;
      default: ;//sitter_log("WARNING: song @ %d: unknown opcode %d",beatp,cmdv[cmdp].opcode);
    }
    cmdp++;
  }
}

/******************************************************************************
 * editing
 *****************************************************************************/

int Song::addSample(const char *name) {
  if (samplenamec>=samplenamea) {
    samplenamea+=SAMPLENAMEV_INCREMENT;
    if (!(samplenamev=(char**)realloc(samplenamev,sizeof(void*)*samplenamea))) sitter_throw(AllocationError,"");
  }
  if (!(samplenamev[samplenamec]=strdup(name))) sitter_throw(AllocationError,"");
  int16_t *data; int samplec;
  mgr->game->res->loadSoundEffect(name,&data,&samplec); // force ResourceManager to cache it
  return samplenamec++;
}

void Song::replaceSample(int smplid,const char *name) {
  if ((smplid<0)||(smplid>=samplenamec)) return;
  mgr->game->res->unloadSoundEffectByName(samplenamev[smplid]);
  free(samplenamev[smplid]);
  if (!(samplenamev[smplid]=strdup(name))) sitter_throw(AllocationError,"");
  int16_t *data; int samplec;
  mgr->game->res->loadSoundEffect(name,&data,&samplec); // force ResourceManager to cache it
}

int Song::allocop() {
  if (cmdc>=cmda) {
    cmda+=CMDV_INCREMENT;
    if (!(cmdv=(SongCommand*)realloc(cmdv,sizeof(SongCommand)*cmda))) sitter_throw(AllocationError,"");
  }
  cmdv[cmdc].beat=0;
  cmdv[cmdc].opcode=SITTER_SONGOP_NOOP;
  return cmdc++;
}

int Song::addop_NOOP(int beat) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  return cmdid;
}

int Song::addop_GOTO(int beat,int p,int ct) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  cmdv[cmdid].opcode=SITTER_SONGOP_GOTO;
  cmdv[cmdid].op_goto.p=p;
  cmdv[cmdid].op_goto.ct_static=ct;
  cmdv[cmdid].op_goto.ct=0;
  return cmdid;
}

int Song::addop_SAMPLE(int beat,int chid,int smplid) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  cmdv[cmdid].opcode=SITTER_SONGOP_SAMPLE;
  cmdv[cmdid].op_sample.chid=chid;
  cmdv[cmdid].op_sample.smplid=smplid;
  return cmdid;
}

int Song::addop_FREQ(int beat,int chid,double freq,int len) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  cmdv[cmdid].opcode=SITTER_SONGOP_FREQ;
  cmdv[cmdid].op_slider.chid=chid;
  cmdv[cmdid].op_slider.dest=freq;
  cmdv[cmdid].op_slider.len=len;
  return cmdid;
}

int Song::addop_LEVEL(int beat,int chid,double level,int len) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  cmdv[cmdid].opcode=SITTER_SONGOP_LEVEL;
  cmdv[cmdid].op_slider.chid=chid;
  cmdv[cmdid].op_slider.dest=level;
  cmdv[cmdid].op_slider.len=len;
  return cmdid;
}

int Song::addop_PAN(int beat,int chid,double pan,int len) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  cmdv[cmdid].opcode=SITTER_SONGOP_PAN;
  cmdv[cmdid].op_slider.chid=chid;
  cmdv[cmdid].op_slider.dest=pan;
  cmdv[cmdid].op_slider.len=len;
  return cmdid;
}

int Song::addop_REWIND(int beat,int chid) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  cmdv[cmdid].opcode=SITTER_SONGOP_REWIND;
  cmdv[cmdid].op_channel.chid=chid;
  return cmdid;
}

int Song::addop_START(int beat,int chid) {
  int cmdid=allocop();
  cmdv[cmdid].beat=beat;
  cmdv[cmdid].opcode=SITTER_SONGOP_START;
  cmdv[cmdid].op_channel.chid=chid;
  return cmdid;
}
