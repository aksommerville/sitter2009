#include "sitter_evdev_internal.h"

/* Xbox 360 joystick.
 */
static const struct sitter_my_device sitter_my_device_xbox={
  .vid=0x045e,
  .pid=0x028e,
  .nickname="Xbox 360",
  .buttonc=5,
  .buttonv={
    // type code        usage         0,0 srclo srchi
    {EV_KEY,BTN_SOUTH,SITTER_USAGE_JUMP,0,0,1,INT_MAX},
    {EV_KEY,BTN_NORTH,SITTER_USAGE_PICKUP,0,0,1,INT_MAX},
    {EV_KEY,BTN_START,SITTER_USAGE_PAUSE,0,0,1,INT_MAX},
    {EV_ABS,ABS_HAT0X,SITTER_USAGE_X,0,0,-1,1},
    {EV_ABS,ABS_HAT0Y,SITTER_USAGE_Y,0,0,-1,1},
    //BTN_WEST=north
    //BTN_EAST=east
  },
};

/* Zelda-branded BDA joystick.
 */
static const struct sitter_my_device sitter_my_device_zelda={
  .vid=0,//TODO
  .pid=0,//TODO
  .nickname="Zelda",
  .buttonc=1,
  .buttonv={
    // type code        usage         0,0 srclo srchi
  },
};

/* Cheapy PS2 knockoffs.
 */
static const struct sitter_my_device sitter_my_device_ps2={
  .vid=0,//TODO
  .pid=0,//TODO
  .nickname="PS2",
  .buttonc=1,
  .buttonv={
    // type code        usage         0,0 srclo srchi
  },
};

/* Nice heavy PS2 knockoff.
 */
static const struct sitter_my_device sitter_my_device_powera={
  .vid=0,//TODO
  .pid=0,//TODO
  .nickname="PowerA",
  .buttonc=1,
  .buttonv={
    // type code        usage         0,0 srclo srchi
  },
};

/* Atari VCS (standard form, not the stick one).
 */
static const struct sitter_my_device sitter_my_device_vcs={
  .vid=0,//TODO
  .pid=0,//TODO
  .nickname="VCS Gamepad",
  .buttonc=1,
  .buttonv={
    // type code        usage         0,0 srclo srchi
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
