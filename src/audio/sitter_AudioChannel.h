#ifndef SITTER_AUDIOCHANNEL_H
#define SITTER_AUDIOCHANNEL_H

#include <stdint.h>

class AudioManager;
class Sprite;

/* channel type */
#define SITTER_AUCHAN_NONE            0
#define SITTER_AUCHAN_SAMPLE          1
#define SITTER_AUCHAN_INSTRUMENT      2

/******************************************************************************
 * Generic channel interface
 *****************************************************************************/

class AudioChannel {
public:

  AudioManager *mgr;
  int8_t pan;
  uint8_t level;
  bool playing; // once false, update will not be called and the channel may be deleted
  int chtype;

  AudioChannel(AudioManager *mgr);
  virtual ~AudioChannel();
  
  /* Populate <dst> with <len> samples (not bytes). Samples are in the native byte order and
   * should fill the available range. Scaling for pan and level will be performed by the caller.
   */
  virtual void update(int16_t *dst,int len) =0;
  
};

/******************************************************************************
 * AudioSampleChannel
 * Play recorded samples, optionally looping.
 *****************************************************************************/
 
class AudioSampleChannel : public AudioChannel {
public:

  const int16_t *samplev; int samplec,samplep;
  int loopstart;
  Sprite *proxtest;

  AudioSampleChannel(AudioManager *mgr);
  ~AudioSampleChannel();
  void update(int16_t *dst,int len);
  void updateProximityTest(); // set level based on <proxtest>'s proximity to any player
  
  /* <src> must remain in scope during the life of this channel. It will be read
   * during the callback, so be careful with it.
   */
  void setSample(const void *src,int srcsamplec,int loop=-1);
  
};

/******************************************************************************
 * AudioInstrumentChannel
 * The more sublime SampledChannel -- allow transmogrification of rate, level,
 * and pan via sliders. It's a very bad idea, from a human-beings-with-ears
 * perspective, to change rate or level instantly. This class simplifies the
 * gradual changes we should make, even when a change sounds instantaneous to
 * the ear.
 *****************************************************************************/
 
class AudioInstrumentChannel : public AudioChannel {
public:

  typedef struct {
    double curr,dest,rate;
  } Slider;

  const int16_t *samplev; int samplec;
  int loopstart;
  double samplep;
  double basefreq; // tonal frequency of sample, when played at 48 kHz
  Slider stepslider;
  Slider levelslider;
  Slider panslider;
  
  AudioInstrumentChannel(AudioManager *mgr);
  ~AudioInstrumentChannel();
  void update(int16_t *dst,int len);
  
  /* Same as for AudioSampleChannel.
   */
  void setSample(const void *src,int srcsamplec,int loop=-1,double basefreq=440.0);
  
  /* <len> when setting a slider is the number of samples until the change is complete.
   */
  void defaultSliders();
  void setSlider(Slider *slider,double dest,int len);
  void setRate(double rate,int len) { if (rate<=0.0) rate=0.001; if (len<1) len=1; setSlider(&stepslider,rate,len); }
  void setPan(double pan,int len) { if (pan<-128.0) pan=-128.0; else if (pan>127.0) pan=127.0; if (len<1) len=1; setSlider(&panslider,pan,len); }
  void setLevel(double level,int len) { if (level<0.0) level=0.0; else if (level>255.0) level=255.0; if (len<1) len=1; setSlider(&levelslider,level,len); }
  void setFrequency(double freq,int len); // compound, examines basefreq and calls setRate
  
};

#endif
