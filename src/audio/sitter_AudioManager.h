#ifdef SITTER_WII
  #include "sitter_AudioManager_wii.h"
#else

#ifndef SITTER_AUDIOMANAGER_H
#define SITTER_AUDIOMANAGER_H

#include <stdint.h>

class Game;
class AudioChannel;
class AudioSampleChannel;
class Sprite;
class Song;

#define SITTER_AUDIO_FREQUENCY 48000
#define SITTER_AUDIO_BUFFERSIZE 1024 // frames (samples*2)
#define SITTER_AUDIO_SAMPLE_CHANNEL_COUNT 6 // default count of sound-effects channels

class AudioManager {
public:

  Game *game;
  
  AudioChannel **chanv; int chanc,chana;
  int *mixbuf;
  int16_t *chbuf;
  Song *song;

  AudioManager() { chanv=0; chanc=0; mixbuf=0; chbuf=0; song=0; } // only used by DummyAudioManager
  AudioManager(Game *game,int samplechanc=SITTER_AUDIO_SAMPLE_CHANNEL_COUNT);
  ~AudioManager();
  
  virtual void update();
  virtual void invalidateSprites();
  
  virtual void playEffect(const char *resname,Sprite *proxtest=0);
  virtual int playLoop(const char *resname,Sprite *proxtest=0); // keep the return -- you need it for stopLoop (it's a channel index)
  virtual void stopLoop(int ref);
  
  virtual void setSong(const char *resname);
  
  /* I own added channels.
   */
  virtual void addChannel(AudioChannel *chan);
  virtual void removeUnusedChannels();//don't use
  virtual AudioSampleChannel *findUnusedSampleChannel();//don't use
  virtual AudioSampleChannel *getSampleChannel(bool interrupt=true,int *index=0); // if interrupt, return the channel closest to finishing
  virtual void flushInstrumentChannels();
  
};

class DummyAudioManager : public AudioManager {
public:
  DummyAudioManager(Game *game):AudioManager() { this->game=game; }
  void update() {}
  void invalidateSprites() {}
  void playEffect(const char *resname,Sprite *proxtest=0) {}
  int playLoop(const char *resname,Sprite *proxtest=0) { return 0; }
  void stopLoop(int ref) {}
  void setSong(const char *resname) {}
  void addChannel(AudioChannel *chan) {}
  void removeUnusedChannels() {}
  AudioSampleChannel *findUnusedSampleChannel() { return 0; }
  AudioSampleChannel *getSampleChannel(bool interrupt=true,int *index=0) { return 0; }
  void flushInstrumentChannels() {}
};

#endif
#endif
