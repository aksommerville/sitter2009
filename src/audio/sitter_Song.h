#ifndef SITTER_SONG_H
#define SITTER_SONG_H

class AudioManager;
class AudioInstrumentChannel;

/* song opcodes */
#define SITTER_SONGOP_NOOP              0
#define SITTER_SONGOP_GOTO              1
#define SITTER_SONGOP_SAMPLE            2
#define SITTER_SONGOP_FREQ              3
#define SITTER_SONGOP_LEVEL             4
#define SITTER_SONGOP_PAN               5
#define SITTER_SONGOP_REWIND            6
#define SITTER_SONGOP_START             7

class Song {
public:

  typedef struct {
    int beat;
    int opcode;
    union {
      struct { int p; int ct,ct_static; } op_goto;
      struct { int chid; int smplid; } op_sample;
      struct { int chid; double dest; int len; } op_slider; // FREQ,LEVEL,PAN
      struct { int chid; } op_channel; // REWIND,START
    };
  } SongCommand;
  SongCommand *cmdv; int cmdc,cmda,cmdp;
  int beatp,samplep; // timer
  int samplesperbeat;
  bool playing;
  int chanc;
  AudioManager *mgr;
  AudioInstrumentChannel **chanv; // i don't own these
  char **samplenamev; int samplenamec,samplenamea;
  int sampleloop;
  double samplefreq;
  
  char *resname;

  Song();
  ~Song();
  
  void setup(AudioManager *mgr);
  
  void start();
  void stop();
  void rewind();
  
  /* If you don't follow the proper sequence, commands may be dropped.
   * First: countSamplesToNextCommand() -- if <0, song is not playing
   * Second: update()
   * Third: run your buffers for whatever countSamplesToNextCommand returned
   * Fourth: step() by that same amount
   */
  int countSamplesToNextCommand(); // -1=no commands, 0=update me, nothing after this beat, >0=next commands (after this beat)
  void step(int samplec);
  void update(); // run commands for this very moment
  
  /* Arguments are unchecked:
   */
  int addSample(const char *name);
  void replaceSample(int smplid,const char *name);
  int allocop();
  int addop_NOOP(int beat);
  int addop_GOTO(int beat,int p,int ct);
  int addop_SAMPLE(int beat,int chid,int smplid);
  int addop_FREQ(int beat,int chid,double freq,int len); // len in samples
  int addop_LEVEL(int beat,int chid,double level,int len);
  int addop_PAN(int beat,int chid,double pan,int len);
  int addop_REWIND(int beat,int chid);
  int addop_START(int beat,int chid);
  
};

#endif
