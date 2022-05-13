#ifndef SITTER_EVDEV_INTERNAL_H
#define SITTER_EVDEV_INTERNAL_H

#include "sitter_evdev.h"
#include "input/sitter_InputManager.h"
#include <sys/poll.h>
#include <dirent.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <termio.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Button mappings.
#define SITTER_USAGE_NONE       0
#define SITTER_USAGE_X          1
#define SITTER_USAGE_Y          2
#define SITTER_USAGE_LEFT       3
#define SITTER_USAGE_RIGHT      4
#define SITTER_USAGE_UP         5
#define SITTER_USAGE_DOWN       6
#define SITTER_USAGE_JUMP       7
#define SITTER_USAGE_PICKUP     8
#define SITTER_USAGE_PAUSE      9

struct sitter_evdev_button {
  int type,code;
  int usage;
  int srcvalue;
  int dstvalue;
  int srclo,srchi;
};

struct sitter_evdev_device {
  int fd;
  int vid,pid;
  char name[64];
  int enable; // We have to be told via SDL functions to actually start using it.
  struct sitter_evdev_button *buttonv;
  int buttonc,buttona;
  int last_map_usage;
  int plrid;
};

extern InputManager *sitter_evdev_inmgr;

extern struct sitter_evdev {

  struct pollfd *pollfdv;
  int pollfdc,pollfda;
  int use_stdin;
  
  struct sitter_evdev_device *devicev;
  int devicec,devicea;
  
  struct termios termios_restore;
  int termios_restore_present;

} sitter_evdev;

#define DEVICE_DIR "/dev/input/"

#endif
