#ifndef SITTER_MOTOR_H
#define SITTER_MOTOR_H

#define SITTER_MOTOR_IDLE  0
#define SITTER_MOTOR_START 1
#define SITTER_MOTOR_RUN   2
#define SITTER_MOTOR_STOP  3

class Motor {

  typedef struct {
    int *velv; int velc,vela;
  } MotorStream;
  MotorStream strmv[4];
  int strmp,velp,timer;
  int vel;

public:

  int framedelay;
  
  Motor();
  ~Motor();
  
  int update();
  
  void start(bool force=false,bool smooth=true);
  void stop(bool force=false,bool smooth=true);
  int peek() const { return vel; }
  int getState() const { return strmp; }
  
  void appendStream(int strmid,int vel);
  void setRampStream(int strmid,int start,int stop,int count);
  
  /* Serialised Motor is a string, 4 stream descriptors separated by semicolons: IDLE;START;RUN;STOP
   * Each stream descriptor is space-delimited. May be:
   *   ramp START STOP COUNT
   * ...or a sequence of numbers, strtol(,,0) format
   * Spurious whitespace is ignored.
   * OK to omit trailing empty descriptors, ie if there's no STOP you only need 2 semicolons.
   * Any existing data is wiped first.
   */
  void deserialise(const char *serial);
  
};

#endif
