#ifndef SITTER_CLOCK_H
#define SITTER_CLOCK_H

#include <stdint.h>

#define DEFAULT_CLOCKRATE 60
#define DEFAULT_CLOCKSKIP  3

typedef struct {
  double drawrate,execrate;
  int64_t drawct,execct,elapsedus;
} ClockReport;

class Timer {
public:

  int targetrate,maxskip;
  int64_t frametime;
  int64_t lasttick;
  int64_t backlog,backloglimit;
  int64_t drawct,execct,firsttick;
  void *mainref;
  
  int64_t currentTime() const;
  void delay(int64_t ct) const;

  Timer(int targetrate=DEFAULT_CLOCKRATE,int maxskip=DEFAULT_CLOCKSKIP);
  ~Timer();
  int tick();
  void *startTracking() const;
  void getReport(ClockReport *rpt,void *ref=0) const;
  void resetMaster();
  
};

#endif
