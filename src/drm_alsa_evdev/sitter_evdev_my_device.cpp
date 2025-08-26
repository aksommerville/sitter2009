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

/* Atari VCS (the stick one).
 */
static const struct sitter_my_device sitter_my_device_ataristick={
  .vid=0x3250,
  .pid=0x1001,
  .nickname="Atari Stick",
  .buttonc=6,
  .buttonv={
    //{EV_KEY,139,SITTER_USAGE_,0,0,1,INT_MAX},//hamburger
    {EV_KEY,158,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//back
    {EV_KEY,172,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},//atari
    {EV_KEY,304,SITTER_USAGE_JUMP,0,0,1,INT_MAX},//primary
    {EV_KEY,305,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//secondary
    {EV_ABS,16,SITTER_USAGE_X,0,0,-1,1},
    {EV_ABS,17,SITTER_USAGE_Y,0,0,-1,1},
  },
};

/* 8bitdo Pro 2.
 */
static const struct sitter_my_device sitter_my_device_pro2={
  .vid=0x2dc8,
  .pid=0x3010,
  .nickname="Pro 2",
  .buttonc=7,
  .buttonv={
    // type code        usage         0,0 srclo srchi
    {EV_KEY,0x0131,SITTER_USAGE_JUMP,  0,0,1,INT_MAX},//start
    {EV_KEY,0x0134,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},//south
    {EV_KEY,0x013b,SITTER_USAGE_PAUSE, 0,0,1,INT_MAX},//unknown
    {EV_ABS, 0,SITTER_USAGE_X,0,0,64,192},//lx
    {EV_ABS, 1,SITTER_USAGE_Y,0,0,64,192},//ly
    {EV_ABS,16,SITTER_USAGE_X,0,0,-1,1},//dpad x
    {EV_ABS,17,SITTER_USAGE_Y,0,0,-1,1},//dpad y
    /*2dc8:3010
      01:0130: 0..0..2    # A (east)
      01:0131: 0..0..2    # B (south)
      01:0132: 0..0..2    # unknown
      01:0133: 0..0..2    # X (north)
      01:0134: 0..0..2    # Y (west)
      01:0135: 0..0..2    # unknown
      01:0136: 0..0..2    # L1
      01:0137: 0..0..2    # R1
      01:0138: 0..0..2    # L2
      01:0139: 0..0..2    # R2
      01:013a: 0..0..2    # select
      01:013b: 0..0..2    # start
      01:013c: 0..0..2    # heart (right aux); the other two auxes don't report
      01:013d: 0..0..2    # lp
      01:013e: 0..0..2    # rp
      03:0000: 0..0..255  # lx
      03:0001: 0..0..255  # ly (+down)
      03:0002: 0..0..255  # rx
      03:0005: 0..0..255  # ry (+down)
      03:0009: 0..0..255  # r2a ; associated button triggers around v=43
      03:000a: 0..0..255  # l2a
      03:0010: -1..0..1   # dpad x
      03:0011: -1..0..1   # dpad y
      */
  },
};

/* Xinmotek (arcade cabinet).
 */
static const struct sitter_my_device sitter_my_device_xinmotek={
  .vid=0x16c0,
  .pid=0x05e1,
  .nickname="Xinmotek",
  .buttonc=6,
  .buttonv={
     {EV_ABS,0,SITTER_USAGE_X,128,0,63,191},
     {EV_ABS,1,SITTER_USAGE_Y,128,0,63,191},
     {EV_KEY,289,SITTER_USAGE_JUMP,0,0,1,INT_MAX}, // green
     {EV_KEY,290,SITTER_USAGE_PICKUP,0,0,1,INT_MAX}, // blue
     //{EV_KEY,291,SITTER_USAGE_,0,0,1,INT_MAX}, // red
     //{EV_KEY,292,SITTER_USAGE_,0,0,1,INT_MAX}, // yellow
     //{EV_KEY,293,SITTER_USAGE_,0,0,1,INT_MAX}, // upper white
     {EV_KEY,294,SITTER_USAGE_PAUSE,0,0,1,INT_MAX}, // lower white
     {EV_KEY,298,SITTER_USAGE_QUIT,0,0,1,INT_MAX}, // front panel
  },
};

static const struct sitter_my_device *sitter_my_devicev[]={
  &sitter_my_device_xbox,
  &sitter_my_device_zelda,
  &sitter_my_device_ps2,
  &sitter_my_device_powera,
  &sitter_my_device_vcs,
  &sitter_my_device_ataristick,
  &sitter_my_device_pro2,
  &sitter_my_device_xinmotek,
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
