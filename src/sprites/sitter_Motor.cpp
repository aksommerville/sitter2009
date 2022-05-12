#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Motor.h"

#define VELV_INCREMENT 16

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Motor::Motor() {
  memset(strmv,0,sizeof(MotorStream)*4);
  strmp=0;
  velp=0;
  timer=0;
  vel=0;
  framedelay=1;
}

Motor::~Motor() {
  for (int i=0;i<4;i++) if (strmv[i].velv) free(strmv[i].velv);
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
int Motor::update() {
  if (++timer>=framedelay) {
    timer=0;
    velp++;
    again:
    if (velp>=strmv[strmp].velc) {
      velp=0;
      switch (strmp) {
        case SITTER_MOTOR_IDLE: if (!strmv[strmp].velc) return 0; break;
        case SITTER_MOTOR_START: strmp=SITTER_MOTOR_RUN; goto again;
        case SITTER_MOTOR_RUN: if (!strmv[strmp].velc) { strmp=SITTER_MOTOR_STOP; goto again; } break;
        case SITTER_MOTOR_STOP: strmp=SITTER_MOTOR_IDLE; goto again;
      }
    }
  }
  return vel=strmv[strmp].velv[velp];
}

void Motor::start(bool force,bool smooth) {
  if (force) {
    strmp=SITTER_MOTOR_START;
    velp=0;
    timer=0;
    return;
  }
  if ((strmp==SITTER_MOTOR_START)||(strmp==SITTER_MOTOR_RUN)) return;
  if (strmv[SITTER_MOTOR_START].velc) strmp=SITTER_MOTOR_START;
  else if (strmv[SITTER_MOTOR_RUN].velc) strmp=SITTER_MOTOR_RUN;
  else return;
  velp=0;
  timer=0;
  if (smooth) while (vel>strmv[strmp].velv[velp]) {
    velp++;
    if (velp>=strmv[strmp].velc) {
      velp=0;
      if (strmv[SITTER_MOTOR_RUN].velc) strmp=SITTER_MOTOR_RUN;
      else if (strmv[SITTER_MOTOR_STOP].velc) strmp=SITTER_MOTOR_STOP;
      else strmp=SITTER_MOTOR_IDLE;
      return;
    }
  }
}

void Motor::stop(bool force,bool smooth) {
  if (force) {
    strmp=SITTER_MOTOR_IDLE;
    velp=0;
    timer=0;
    vel=0;
    return;
  }
  if ((strmp==SITTER_MOTOR_STOP)||(strmp==SITTER_MOTOR_IDLE)) return;
  if (strmv[SITTER_MOTOR_STOP].velc) strmp=SITTER_MOTOR_STOP;
  else if (strmv[SITTER_MOTOR_IDLE].velc) strmp=SITTER_MOTOR_IDLE;
  else return;
  velp=0;
  timer=0;
  if (smooth) while (vel<strmv[strmp].velv[velp]) {
    velp++;
    if (velp>=strmv[strmp].velc) {
      velp=0;
      strmp=SITTER_MOTOR_IDLE;
      return;
    }
  }
}

/******************************************************************************
 * edit
 *****************************************************************************/
 
void Motor::appendStream(int strmid,int vel) {
  if ((strmid<0)||(strmid>=4)) sitter_throw(ArgumentError,"Motor stream %d",strmid);
  if (strmv[strmid].velc>=strmv[strmid].vela) {
    strmv[strmid].vela+=VELV_INCREMENT;
    if (!(strmv[strmid].velv=(int*)realloc(strmv[strmid].velv,sizeof(int)*strmv[strmid].vela))) sitter_throw(AllocationError,"");
  }
  strmv[strmid].velv[strmv[strmid].velc++]=vel;
}

void Motor::setRampStream(int strmid,int start,int stop,int count) {
  if ((strmid<0)||(strmid>=4)) sitter_throw(ArgumentError,"Motor stream %d",strmid);
  if (count<1) { strmv[strmid].velc=0; return; }
  if (count>strmv[strmid].vela) {
    if (strmv[strmid].velv) free(strmv[strmid].velv);
    if (!(strmv[strmid].velv=(int*)malloc(sizeof(int)*count))) sitter_throw(AllocationError,"");
    strmv[strmid].vela=count;
  }
  strmv[strmid].velc=count;
  for (int i=0;i<count;i++) {
    strmv[strmid].velv[i]=start+(i*(stop-start+1))/count;
  }
}

/******************************************************************************
 * deserialise
 *****************************************************************************/
 
class NAN {}; // Not A Number (not to be confused with NaN which means "Not a Number")
 
void Motor::deserialise(const char *serial) {
  for (int i=0;i<4;i++) strmv[i].velc=0;
  int sp=0;
  while (1) {
    #define SKIPSPACE while (*serial&&isspace(*serial)) serial++; if (!*serial) return;
    #define RDN(var) { SKIPSPACE char *tail=0; \
      var=strtol(serial,&tail,0); \
      if ((tail==serial)||(*tail&&!isspace(*tail))) throw NAN(); \
      serial=(const char*)tail; }
    SKIPSPACE
    if (!strncasecmp(serial,"ramp",4)) {
      serial+=4;
      int start,stop,count;
      try { RDN(start) RDN(stop) RDN(count) }
      catch (NAN) { sitter_throw(FormatError,"Motor::deserialise: error parsing ramp (expected 'ramp START STOP COUNT')"); }
      setRampStream(sp,start,stop,count);
    } else { // normal descriptor
      while (1) {
        try {
          int n; RDN(n)
          appendStream(sp,n);
        } catch (NAN) { break; }
      }
    }
    SKIPSPACE
    if (*serial!=';') sitter_throw(FormatError,"Motor::deserialise: expected ';' before '%c'",*serial);
    serial++;
    if (++sp>=4) sitter_throw(FormatError,"Motor::deserialise: extra tokens at end");
    #undef RDN
    #undef SKIPSPACE
  }
}
