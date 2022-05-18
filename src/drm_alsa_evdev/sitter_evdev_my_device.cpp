#include "sitter_evdev_internal.h"

/* Xbox 360 joystick, and also SN30/N30 (grumble, grumble)
 */
static const struct sitter_my_device sitter_my_device_xbox={
  .vid=0x045e,
  .pid=0x028e,
  .nickname="Xbox 360",
  .buttonc=9,
  .buttonv={
    // type code        usage         0,0 srclo srchi
    {EV_KEY,BTN_SOUTH,SITTER_USAGE_JUMP,0,0,1,INT_MAX},
    {EV_KEY,BTN_EAST, SITTER_USAGE_PICKUP,0,0,1,INT_MAX},
    {EV_KEY,BTN_NORTH,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},
    {EV_KEY,BTN_WEST, SITTER_USAGE_PICKUP,0,0,1,INT_MAX},
    {EV_KEY,BTN_START,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},
    {EV_ABS,ABS_X,    SITTER_USAGE_X,0,0,-10000,10000},
    {EV_ABS,ABS_Y,    SITTER_USAGE_Y,0,0,-10000,10000},
    {EV_ABS,ABS_HAT0X,SITTER_USAGE_X,0,0,-1,1},
    {EV_ABS,ABS_HAT0Y,SITTER_USAGE_Y,0,0,-1,1},
    //BTN_WEST=north
    //BTN_EAST=east
  },
};

/* Zelda-branded BDA joystick.
 */
static const struct sitter_my_device sitter_my_device_zelda={
  .vid=0x20d6,
  .pid=0xa711,
  .nickname="Zelda",
  .buttonc=6,
  .buttonv={
    // type code        usage         0,0 srclo srchi
    {EV_KEY,304,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//west
    {EV_KEY,305,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//south
    //{EV_KEY,306,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//east
    //{EV_KEY,307,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//north
    //{EV_KEY,308,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//l1
    //{EV_KEY,309,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//r1
    //{EV_KEY,310,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//l2
    //{EV_KEY,311,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//r2
    //{EV_KEY,312,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//minus(upper select)
    {EV_KEY,313,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//plus(upper start)
    //{EV_KEY,314,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//lp
    //{EV_KEY,315,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//rp
    {EV_KEY,316,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//home(lower start
    //{EV_KEY,317,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//circle(lower select)
    {EV_ABS,16,SITTER_USAGE_X,0,0,-1,1},
    {EV_ABS,17,SITTER_USAGE_Y,0,0,-1,1},
  },
  /*
JOYSTICK: 20d6:a711: Bensussen Deutsch & Associates,Inc.(BDA) Core (Plus) Wired Cont
    {EV_ABS,0,SITTER_USAGE_,0,0,63,191},
    {EV_ABS,1,SITTER_USAGE_,0,0,63,191},
    {EV_ABS,2,SITTER_USAGE_,0,0,63,191},
    {EV_ABS,5,SITTER_USAGE_,0,0,63,191},
/**/
};

/* Cheapy PS2 knockoffs.
 */
static const struct sitter_my_device sitter_my_device_ps2={
  .vid=0x0e8f,
  .pid=0x0003,
  .nickname="PS2",
  .buttonc=5,
  .buttonv={
    // type code        usage         0,0 srclo srchi
    //{EV_KEY,288,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//north
    //{EV_KEY,289,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//east
    {EV_KEY,290,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//south
    {EV_KEY,291,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//west
    //{EV_KEY,292,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//l1
    //{EV_KEY,296,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//select
    {EV_KEY,297,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},
    /*
    {EV_KEY,293,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,294,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,295,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,298,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,299,SITTER_USAGE_,0,0,1,INT_MAX},
    /**/
    // EV_ABS (0,1,2,5:63..191),(16,17:-1..1) presumably the sticks (i don't want them)
    // Dpad is (0,1) when the light is off; (16,17) when it's on
    {EV_ABS,0,SITTER_USAGE_X,0,0,63,191},
    {EV_ABS,1,SITTER_USAGE_Y,0,0,63,191},
  },
};

/* Nice heavy PS2 knockoff.
 */
static const struct sitter_my_device sitter_my_device_powera={
  .vid=0x20d6,
  .pid=0xca6d,
  .nickname="PowerA",
  .buttonc=5,
  .buttonv={
    // type code        usage         0,0 srclo srchi
    {EV_KEY,304,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//west
    {EV_KEY,305,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//south
    //{EV_KEY,306,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//east
    //{EV_KEY,307,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//north
    //{EV_KEY,308,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//l1
    //{EV_KEY,309,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//r1
    //{EV_KEY,310,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//l2
    //{EV_KEY,311,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//r2
    //{EV_KEY,312,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//select
    {EV_KEY,313,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//start
    {EV_ABS,16,SITTER_USAGE_X,0,0,-1,1},
    {EV_ABS,17,SITTER_USAGE_Y,0,0,-1,1},
  },
  /*
JOYSTICK: 20d6:ca6d: BDA Pro Ex
    {EV_KEY,314,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,315,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,316,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_ABS,0,SITTER_USAGE_,0,0,63,191},
    {EV_ABS,1,SITTER_USAGE_,0,0,63,191},
    {EV_ABS,2,SITTER_USAGE_,0,0,63,191},
    {EV_ABS,5,SITTER_USAGE_,0,0,63,191},
/**/
};

/* Atari VCS (standard form, not the stick one).
 */
static const struct sitter_my_device sitter_my_device_vcs={
  .vid=0x3250,
  .pid=0x1002,
  .nickname="VCS Gamepad",
  .buttonc=5,
  .buttonv={
    // type code        usage         0,0 srclo srchi
    {EV_KEY,139,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//start
    {EV_KEY,304,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//south
    {EV_KEY,307,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//unknown
    {EV_ABS,16,SITTER_USAGE_X,0,0,-1,1},//dpad x
    {EV_ABS,17,SITTER_USAGE_Y,0,0,-1,1},//dpad y
    /*
    {EV_KEY,158,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//select
    {EV_KEY,172,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//heart
    {EV_KEY,256,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//unknown
    {EV_KEY,305,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//east
    {EV_KEY,308,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//unknown
    {EV_KEY,310,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//unknown
    {EV_KEY,311,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,317,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_KEY,318,SITTER_USAGE_,0,0,1,INT_MAX},
    {EV_ABS,0,SITTER_USAGE_X,0,0,-16384,16383},
    {EV_ABS,1,SITTER_USAGE_Y,0,0,-16384,16383},
    {EV_ABS,2,SITTER_USAGE_X,0,0,-16384,16383},
    {EV_ABS,5,SITTER_USAGE_Y,0,0,-16384,16383},
    {EV_ABS,9,SITTER_USAGE_X,0,0,255,767},
    {EV_ABS,10,SITTER_USAGE_Y,0,0,255,767},
    {EV_ABS,40,SITTER_USAGE_,0,0,63,191},
    /**/
  },
};

//TODO the 3 new 8bitdo devices, once they arrive

static const struct sitter_my_device *sitter_my_devicev[]={
  &sitter_my_device_xbox,
  &sitter_my_device_zelda,
  &sitter_my_device_ps2,
  &sitter_my_device_powera,
  &sitter_my_device_vcs,
};

const struct sitter_my_device *sitter_check_my_device(int vid,int pid) {
  const struct sitter_my_device **p=sitter_my_devicev;
  int i=sizeof(sitter_my_devicev)/sizeof(void*);
  for (;i-->0;p++) {
    if ((*p)->vid!=vid) continue;
    if ((*p)->pid!=pid) continue;
    return *p;
  }
  return 0;
}
