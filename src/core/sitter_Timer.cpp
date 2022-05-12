#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#if defined(SITTER_WIN32)
  #include <windows.h>
  #include <time.h>
#elif defined(SITTER_WII)
  #include <unistd.h>
  #include <time.h>
  #include <sys/time.h>
#else
  #include <time.h>
  #include <sys/time.h>
#endif
#include "sitter_Error.h"
#include "sitter_Timer.h"

#define DEFAULT_BACKLOG_LIMIT 1000000 // 1 second == ancient history, forget about it

/******************************************************************************
 * driver
 * (i copied this from qq 20091118, had ifdefs for wii, win32...)
 *****************************************************************************/
 
int64_t Timer::currentTime() const {
  #ifdef SITTER_WII
    return 0;
  #elif defined SITTER_WIN32
    return ((int64_t)GetCurrentTime())*1000;
  #else
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return ((int64_t)tv.tv_sec)*1000000+tv.tv_usec;
  #endif
}

void Timer::delay(int64_t ct) const {
  #if defined(SITTER_WIN32)
    Sleep(ct/1000);
  #elif defined(SITTER_WII)
    //usleep(ct); // timing is managed by VideoManager
  #else
    struct timespec tv;
    tv.tv_sec=0;
    tv.tv_nsec=ct*1000;
    nanosleep(&tv,NULL);
  #endif
}

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Timer::Timer(int targetrate,int maxskip) {
  if (targetrate<1) sitter_throw(ArgumentError,"Clock: %d fps",targetrate);
  this->targetrate=targetrate;
  this->maxskip=maxskip;
  frametime=1000000ll/targetrate;
  lasttick=currentTime();
  backlog=0;
  backloglimit=DEFAULT_BACKLOG_LIMIT;
  drawct=0;
  execct=0;
  mainref=startTracking();
}

Timer::~Timer() {
  ClockReport rpt;
  getReport(&rpt);
  free(mainref);
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
int Timer::tick() {
  #ifdef SITTER_WII
    return 1; // timing handled by VideoManager
  #else
  drawct++;
  int rtn=0;
  while ((backlog>0)&&(rtn<maxskip)) {
    backlog-=frametime;
    rtn++;
  }
  int64_t now=currentTime();
  if (lasttick+frametime>now) {
    delay(lasttick+frametime-now);
    now=currentTime();
    backlog+=now-(lasttick+frametime);
    lasttick=now;
    rtn++;
    execct+=rtn;
    return rtn;
  }
  backlog+=now-(lasttick+frametime);
  if (backlog>=backloglimit) backlog=backloglimit;
  while ((backlog>=frametime)&&(rtn<maxskip)) {
    backlog-=frametime;
    rtn++;
  }
  rtn++;
  lasttick=currentTime();
  execct+=rtn;
  return rtn;
  #endif
}

/******************************************************************************
 * reporting
 *****************************************************************************/
 
void Timer::resetMaster() {
  if (mainref) free(mainref);
  mainref=startTracking();
}
 
typedef struct {
  int64_t indrawct,inexecct,intick;
} ClockReferencePoint;
 
void *Timer::startTracking() const {
  ClockReferencePoint *ref=(ClockReferencePoint*)malloc(sizeof(ClockReferencePoint));
  if (!ref) sitter_throw(AllocationError,"");
  ref->indrawct=drawct;
  ref->inexecct=execct;
  ref->intick=lasttick;
  return ref;
}

void Timer::getReport(ClockReport *rpt,void *ref) const {
  if (!ref) ref=mainref;
  #define r ((ClockReferencePoint*)ref)
  rpt->drawct=this->drawct-r->indrawct;
  rpt->execct=this->execct-r->inexecct;
  rpt->elapsedus=this->lasttick-r->intick;
  if (rpt->elapsedus) {
    rpt->drawrate=(rpt->drawct*1000000.0)/rpt->elapsedus;
    rpt->execrate=(rpt->execct*1000000.0)/rpt->elapsedus;
  } else {
    rpt->drawrate=0;
    rpt->execrate=0;
  }
  #undef r
}
