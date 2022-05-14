#include "sitter_evdev_internal.h"

struct sitter_evdev sitter_evdev={0};
InputManager *sitter_evdev_inmgr=0;

/* Cleanup.
 */
 
static void sitter_evdev_device_cleanup(struct sitter_evdev_device *device) {
  if (device->fd>=0) close(device->fd);
  if (device->buttonv) free(device->buttonv);
}

/* Drop device.
 */
 
static void sitter_evdev_drop_device(struct sitter_evdev_device *device) {
  int p=device-sitter_evdev.devicev;
  if ((p<0)||(p>=sitter_evdev.devicec)) return;
  sitter_evdev_device_cleanup(device);
  sitter_evdev.devicec--;
  memmove(device,device+1,sizeof(struct sitter_evdev_device)*(sitter_evdev.devicec-p));
}

/* Add device.
 */
 
static struct sitter_evdev_device *sitter_evdev_add_device(int fd) {
  if (sitter_evdev.devicec>=sitter_evdev.devicea) {
    int na=sitter_evdev.devicea+8;
    if (na>INT_MAX/sizeof(struct sitter_evdev_device)) return 0;
    void *nv=realloc(sitter_evdev.devicev,sizeof(struct sitter_evdev_device)*na);
    if (!nv) return 0;
    sitter_evdev.devicev=(struct sitter_evdev_device*)nv;
    sitter_evdev.devicea=na;
  }
  struct sitter_evdev_device *device=sitter_evdev.devicev+sitter_evdev.devicec++;
  memset(device,0,sizeof(struct sitter_evdev_device));
  device->fd=fd;
  device->plrid=(sitter_evdev.devicec-1)%4+1;
  return device;
}

/* Device by fd.
 */
 
static struct sitter_evdev_device *sitter_evdev_device_by_fd(int fd) {
  struct sitter_evdev_device *device=sitter_evdev.devicev;
  int i=sitter_evdev.devicec;
  for (;i-->0;device++) {
    if (device->fd==fd) return device;
  }
  return 0;
}

/* Check capabilities and return nonzero if we will be able to map this device.
 */
 
static int sitter_evdev_device_is_usable(
  int vid,int pid,
  const uint8_t *keybit,
  const uint8_t *absbit,
  const struct input_absinfo *absinfov
) {

  // Count keys starting at 256. Below that is mostly keyboards, which we don't care for.
  int hikeyc=0;
  int code=256;
  int len=(KEY_CNT+7)>>3;
  int major=code>>3;
  for (;major<len;major++) {
    if (!keybit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(keybit[major]&(1<<minor))) continue;
      hikeyc++;
    }
  }
  // Fewer than 2 keys? Forget it.
  if (hikeyc<2) return 0;
  
  // Count axes.
  int axisc=0;
  for (major=0,len=(ABS_CNT+7)>>3;major<len;major++) {
    if (!absbit[major]) continue;
    int minor=0;
    for (;minor<8;minor++) {
      if (!(absbit[major]&(1<<minor))) continue;
      axisc++;
    }
  }
  
  // Now, counting each axis as 2 buttons, there must be at least six inputs.
  if (axisc*3+hikeyc<6) return 0;

  return 1;
}

/* Search buttons.
 */
 
static int sitter_evdev_device_search_buttons(const struct sitter_evdev_device *device,int type,int code) {
  int lo=0,hi=device->buttonc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (type<device->buttonv[ck].type) hi=ck;
    else if (type>device->buttonv[ck].type) lo=ck+1;
    else if (code<device->buttonv[ck].code) hi=ck;
    else if (code>device->buttonv[ck].code) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add a button record.
 */
 
static struct sitter_evdev_button *sitter_evdev_device_add_button(struct sitter_evdev_device *device,int type,int code) {
  int p=sitter_evdev_device_search_buttons(device,type,code);
  if (p>=0) return device->buttonv+p;
  p=-p-1;
  if (device->buttonc>=device->buttona) {
    int na=device->buttona+16;
    if (na>INT_MAX/sizeof(struct sitter_evdev_button)) return 0;
    void *nv=realloc(device->buttonv,sizeof(struct sitter_evdev_button)*na);
    if (!nv) return 0;
    device->buttonv=(struct sitter_evdev_button*)nv;
    device->buttona=na;
  }
  struct sitter_evdev_button *button=device->buttonv+p;
  memmove(button+1,button,sizeof(struct sitter_evdev_button)*(device->buttonc-p));
  memset(button,0,sizeof(struct sitter_evdev_button));
  device->buttonc++;
  button->type=type;
  button->code=code;
  return button;
}

/* Map an absolute axis.
 */
 
static int sitter_evdev_map_abs(struct sitter_evdev_device *device,int code,const struct input_absinfo *absinfo) {
  int range=absinfo->maximum+1-absinfo->minimum;
  if (range<3) return 0;
  
  // If the value is at minimum, assume it's an analogue button. Don't map.
  if (absinfo->value==absinfo->minimum) return 0;
  
  // If it's a code we know, go with it. Otherwise alternate X,Y.
  int usage=0;
  switch (code) {
    case ABS_X: case ABS_RX: usage=SITTER_USAGE_X; break;
    case ABS_Y: case ABS_RY: usage=SITTER_USAGE_Y; break;
    default: if (device->last_map_usage==SITTER_USAGE_X) usage=SITTER_USAGE_Y; else usage=SITTER_USAGE_X; break;
  }
  
  struct sitter_evdev_button *button=sitter_evdev_device_add_button(device,EV_ABS,code);
  if (!button) return -1;
  button->usage=usage;
  button->srcvalue=absinfo->value;
  
  int mid=(absinfo->maximum+absinfo->minimum)>>1;
  button->srclo=(absinfo->minimum+mid)>>1;
  button->srchi=(absinfo->maximum+mid)>>1;
  if (button->srclo>=mid) button->srclo--;
  if (button->srchi<=mid) button->srchi++;
  
  device->last_map_usage=usage;
  return 0;
}

/* Map a 2-state button.
 * Alternate among JUMP,PICKUP,PAUSE
 */
 
static int sitter_evdev_map_key(struct sitter_evdev_device *device,int code) {
  struct sitter_evdev_button *button=sitter_evdev_device_add_button(device,EV_KEY,code);
  if (!button) return -1;
  button->srclo=1;
  button->srchi=INT_MAX;
  switch (device->last_map_usage) {
    case SITTER_USAGE_JUMP: button->usage=SITTER_USAGE_PICKUP; break;
    case SITTER_USAGE_PICKUP: button->usage=SITTER_USAGE_PAUSE; break;
    default: button->usage=SITTER_USAGE_JUMP; break;
  }
  device->last_map_usage=button->usage;
  return 0;
}

/* Try to open a device.
 * No worries if it fails, lots of devices will fail.
 */
 
static int sitter_evdev_try_open(const char *path) {
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  
  // Get IDs (in particular, USB vendor and product)
  struct input_id id={0};
  if (ioctl(fd,EVIOCGID,&id)<0) {
    close(fd);
    return 0;
  }
  const struct sitter_my_device *my_device=sitter_check_my_device(id.vendor,id.product);
  
  // Try to grab it. If this fails, whatever.
  if (ioctl(fd,EVIOCGRAB,1)<0) ;
  
  // Fetch capabilities, take a quick assessment.
  uint8_t absbit[(ABS_CNT+7)>>3]={0};
  uint8_t keybit[(KEY_CNT+7)>>3]={0};
  struct input_absinfo absinfov[ABS_CNT]={0};
  if (!my_device) {
    ioctl(fd,EVIOCGBIT(EV_ABS,sizeof(absbit)),absbit);
    ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(keybit)),keybit);
    int code=0;
    for (;code<ABS_CNT;code++) {
      if (!(absbit[code>>3]&(1<<(code&7)))) continue;
      ioctl(fd,EVIOCGABS(code),absinfov+code);
    }
    if (!sitter_evdev_device_is_usable(id.vendor,id.product,keybit,absbit,absinfov)) {
      close(fd);
      return 0;
    }
  }
  
  // Add to the list.
  struct sitter_evdev_device *device=sitter_evdev_add_device(fd);
  if (!device) {
    close(fd);
    return 0;
  }
  device->vid=id.vendor;
  device->pid=id.product;
  
  // Fetch name.
  if (my_device) {
    strcpy(device->name,my_device->nickname);
  } else {
    int namec=ioctl(device->fd,EVIOCGNAME(sizeof(device->name)),device->name);
    if (namec>0) {
      if (namec>=sizeof(device->name)) namec=sizeof(device->name)-1;
      int leadspace=0;
      while ((leadspace<namec)&&((unsigned char)device->name[leadspace]<=0x20)) leadspace++;
      if (leadspace) {
        namec-=leadspace;
        memmove(device->name,device->name+leadspace,namec);
      }
      while ((namec>0)&&((unsigned char)device->name[namec-1]<=0x20)) namec--;
      int i=0; for (;i<namec;i++) {
        if ((device->name[i]<0x20)||(device->name[i]>0x7e)) device->name[i]='?';
      }
      device->name[namec]=0;
    }
  }
  
  // Generate maps.
  if (my_device) {
    if (!(device->buttonv=(struct sitter_evdev_button*)malloc(sizeof(struct sitter_evdev_button)*my_device->buttonc))) {
      sitter_evdev_drop_device(device);
      return -1;
    }
    device->buttonc=my_device->buttonc;
    memcpy(device->buttonv,my_device->buttonv,sizeof(struct sitter_evdev_button)*my_device->buttonc);
  } else {
    device->last_map_usage=0;
    int code=0;
    for (code=0;code<ABS_CNT;code++) {
      if (!(absbit[code>>3]&(1<<(code&7)))) continue;
      if (sitter_evdev_map_abs(device,code,absinfov+code)<0) {
        sitter_evdev_drop_device(device);
        return -1;
      }
    }
    device->last_map_usage=0;
    int major=0;
    for (;major<sizeof(keybit);major++) {
      if (!keybit[major]) continue;
      int minor=0;
      for (;minor<8;minor++) {
        if (!(keybit[major]&(1<<minor))) continue;
        int code=(major<<3)+minor;
        if (sitter_evdev_map_key(device,code)<0) {
          sitter_evdev_drop_device(device);
          return -1;
        }
      }
    }
  }
  
  fprintf(stderr,"%s: Using input device %04x:%04x (%s)\n",path,device->vid,device->pid,my_device?"known":"guessed");
  return 0;
}

/* Scan for devices.
 */
 
static int sitter_evdev_scan() {
  char path[1024];
  int pfxc=strlen(DEVICE_DIR);
  if (pfxc>=sizeof(path)) return -1;
  DIR *dir=opendir(DEVICE_DIR);
  if (!dir) return 0;
  memcpy(path,DEVICE_DIR,pfxc);
  struct dirent *de;
  while (de=readdir(dir)) {
    if (de->d_type!=DT_CHR) continue;
    if (memcmp(de->d_name,"event",5)) continue;
    int baselen=strlen(de->d_name);
    if (pfxc+baselen>=sizeof(path)) continue;
    memcpy(path+pfxc,de->d_name,baselen+1);
    sitter_evdev_try_open(path);
  }
  closedir(dir);
  return 0;
}

/* Restore termios canonical mode.
 */
 
static void sitter_evdev_termios_quit() {
  if (sitter_evdev.termios_restore_present) {
    tcsetattr(STDIN_FILENO,TCSANOW,&sitter_evdev.termios_restore);
  }
}

/* Init.
 */
 
int SDL_Init(int which) {
  if (which!=SDL_INIT_JOYSTICK) return -1;
  
  memset(&sitter_evdev,0,sizeof(sitter_evdev));
  
  sitter_evdev_scan();
  
  sitter_evdev.use_stdin=1;
  atexit(sitter_evdev_termios_quit);
  if (tcgetattr(STDIN_FILENO,&sitter_evdev.termios_restore)>=0) {
    struct termios termios=sitter_evdev.termios_restore;
    termios.c_lflag&=~(ICANON|ECHO);
    if (tcsetattr(STDIN_FILENO,TCSANOW,&termios)>=0) {
      sitter_evdev.termios_restore_present=1;
    }
  }

  return 0;
}

/* Simple stubs.
 */

void SDL_EnableUNICODE(int whatever) {
}

const char *SDL_GetError() {
  return "(no error message)";
}

const char *SDL_GetKeyName(int keysym) {
  return 0;
}

/* Read evdev device.
 */
 
static int sitter_evdev_read_device(SDL_Event *dst,struct sitter_evdev_device *device) {
  struct input_event eventv[32];
  int eventc=read(device->fd,eventv,sizeof(eventv));
  if (eventc<=0) return -1;
  eventc/=sizeof(struct input_event);
  struct input_event *event=eventv;
  for (;eventc-->0;event++) {
    switch (event->type) {
      case EV_ABS:
      case EV_KEY: {
        int p=sitter_evdev_device_search_buttons(device,event->type,event->code);
        if (p<0) continue;
        struct sitter_evdev_button *button=device->buttonv+p;
        if (event->value==button->srcvalue) continue;
        button->srcvalue=event->value;
        switch (button->usage) {
        
          case SITTER_USAGE_X:
          case SITTER_USAGE_Y: {
              int dstvalue=(event->value<=button->srclo)?-1:(event->value>=button->srchi)?1:0;
              if (dstvalue==button->dstvalue) continue;
              button->dstvalue=dstvalue;
              switch (button->usage) {

                case SITTER_USAGE_X: if (button->dstvalue<0) {
                    if (device->plrid) {
                      sitter_evdev_inmgr->pushEvent(InputEvent(
                        SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_LEFT|(device->plrid<<12),1
                      ));
                    }
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNDOWN,SITTER_BTN_MN_LEFT,1
                    ));
                  } else if (button->dstvalue>0) {
                    if (device->plrid) {
                      sitter_evdev_inmgr->pushEvent(InputEvent(
                        SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_RIGHT|(device->plrid<<12),1
                      ));
                    }
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNDOWN,SITTER_BTN_MN_RIGHT,1
                    ));
                  } else {
                    if (device->plrid) {
                      sitter_evdev_inmgr->pushEvent(InputEvent(
                        SITTER_EVT_BTNUP,SITTER_BTN_PLR_LEFT|(device->plrid<<12),0
                      ));
                      sitter_evdev_inmgr->pushEvent(InputEvent(
                        SITTER_EVT_BTNUP,SITTER_BTN_PLR_RIGHT|(device->plrid<<12),0
                      ));
                    }
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNUP,SITTER_BTN_MN_LEFT,0
                    ));
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNUP,SITTER_BTN_MN_RIGHT,0
                    ));
                  } break;

                case SITTER_USAGE_Y: if (button->dstvalue<0) {
                    // There is no PLR_UP
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNDOWN,SITTER_BTN_MN_UP,1
                    ));
                  } else if (button->dstvalue>0) {
                    if (device->plrid) {
                      sitter_evdev_inmgr->pushEvent(InputEvent(
                        SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_DUCK|(device->plrid<<12),1
                      ));
                    }
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNDOWN,SITTER_BTN_MN_DOWN,1
                    ));
                  } else {
                    if (device->plrid) {
                      sitter_evdev_inmgr->pushEvent(InputEvent(
                        SITTER_EVT_BTNUP,SITTER_BTN_PLR_DUCK|(device->plrid<<12),0
                      ));
                    }
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNUP,SITTER_BTN_MN_UP,0
                    ));
                    sitter_evdev_inmgr->pushEvent(InputEvent(
                      SITTER_EVT_BTNUP,SITTER_BTN_MN_DOWN,0
                    ));
                  } break;
              }
          } break;
          
        case SITTER_USAGE_LEFT:
        case SITTER_USAGE_RIGHT:
        case SITTER_USAGE_DOWN:
        case SITTER_USAGE_UP:
        case SITTER_USAGE_JUMP:
        case SITTER_USAGE_PICKUP:
        case SITTER_USAGE_PAUSE: {
            int dstvalue=((event->value>=button->srclo)&&(event->value<=button->srchi))?1:0;
            if (dstvalue==button->dstvalue) continue;
            button->dstvalue=dstvalue;
            switch (button->usage) {
            
              case SITTER_USAGE_LEFT: if (dstvalue) {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_LEFT,1));
                  if (device->plrid) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_LEFT|(device->plrid<<12),1));
                  }
                } else {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_LEFT,0));
                  if (device->plrid) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_LEFT|(device->plrid<<12),0));
                  }
                } break;
                  
              case SITTER_USAGE_RIGHT: if (dstvalue) {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_RIGHT,1));
                  if (device->plrid) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_RIGHT|(device->plrid<<12),1));
                  }
                } else {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_RIGHT,0));
                  if (device->plrid) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_RIGHT|(device->plrid<<12),0));
                  }
                } break;
                
              case SITTER_USAGE_DOWN: if (dstvalue) {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_DOWN,1));
                  if (device->plrid) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_DUCK|(device->plrid<<12),1));
                  }
                } else {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_DOWN,0));
                  if (device->plrid) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_DUCK|(device->plrid<<12),0));
                  }
                } break;
                
              case SITTER_USAGE_UP: if (dstvalue) {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_UP,1));
                } else {
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_UP,0));
                } break;
                
              case SITTER_USAGE_JUMP: if (device->plrid) {
                  if (dstvalue) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_JUMP|(device->plrid<<12),1));
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_OK,1));
                  } else {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_JUMP|(device->plrid<<12),0));
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_OK,0));
                  }
                } break;
                
              case SITTER_USAGE_PICKUP: if (device->plrid) {
                  if (dstvalue) {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_TOSS|(device->plrid<<12),1));
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_PICKUP|(device->plrid<<12),1));
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_CANCEL,1));
                  } else {
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_TOSS|(device->plrid<<12),0));
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_PICKUP|(device->plrid<<12),0));
                    sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_CANCEL,0));
                  }
                } break;
                
              case SITTER_USAGE_PAUSE: if (dstvalue) {
                  //stateless
                  sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MENU,1)); 
                } break;
            }
          } break;
        }
      }
    }
  }
  return 0;
}

/* Read stdin.
 */
 
static int sitter_evdev_read_stdin() {
  char buf[256];
  int bufc=read(STDIN_FILENO,buf,sizeof(buf));
  if (bufc<1) return -1;
  int bufp=0;
  while (bufp<bufc) {
    #define BTNONCE(tag) { \
      sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_##tag,1)); \
      sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_##tag,0)); \
    }
  
    // G0 are "UNICODE" events, eg entering name for high scores.
    if ((buf[bufp]>=0x20)&&(buf[bufp]<=0x7e)) {
      sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_UNICODE|buf[bufp],1));
      bufp++;
      continue;
    }
    
    // Enter is OK. (mind that this is stateful)
    if ((buf[bufp]==0x0a)||(buf[bufp]==0x0d)) {
      BTNONCE(MN_OK)
      bufp++;
      // Skip doubles, or more realistically, CRLF.
      if ((bufp<bufc)&&((buf[bufp]==0x0a)||(buf[bufp]==0x0d))) bufp++;
      continue;
    }
    
    // Backspace and Delete are CANCEL.
    if ((buf[bufp]==0x08)||(buf[bufp]==0x7f)) {
      BTNONCE(MN_CANCEL)
      bufp++;
      continue;
    }
    
    // Escape alone is MENU, but watch for ^[... sequences.
    if (buf[bufp]==0x1b) {
      bufp++;
      if ((bufp+2<=bufc)&&(buf[bufp]=='[')) {
        bufp+=2;
        switch (buf[bufp-1]) {
          case 'D': BTNONCE(MN_LEFT) break;
          case 'C': BTNONCE(MN_RIGHT) break;
          case 'A': BTNONCE(MN_UP) break;
          case 'B': BTNONCE(MN_DOWN) break;
          //default: fprintf(stderr,"^[ 0x%02x %c\n",buf[bufp-1],buf[bufp-1]);
        }
      } else {
        // MENU: stateless
        sitter_evdev_inmgr->pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MENU,1));
      }
      continue;
    }
    
    // Ignore anything else.
    bufp++;
    #undef BTNONCE
  }
  return 0;
}

/* Add pollfd.
 */
 
static int sitter_evdev_add_pollfd(int fd) {
  if (sitter_evdev.pollfdc>=sitter_evdev.pollfda) {
    int na=sitter_evdev.pollfda+8;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(sitter_evdev.pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    sitter_evdev.pollfdv=(struct pollfd*)nv;
    sitter_evdev.pollfda=na;
  }
  struct pollfd *pollfd=sitter_evdev.pollfdv+sitter_evdev.pollfdc++;
  memset(pollfd,0,sizeof(struct pollfd));
  pollfd->fd=fd;
  pollfd->events=POLLIN|POLLERR|POLLHUP;
  return 0;
}

/* Update.
 */
 
int SDL_PollEvent(SDL_Event *event) {

  // Rebuild pollfdv.
  sitter_evdev.pollfdc=0;
  if (sitter_evdev.use_stdin) {
    if (sitter_evdev_add_pollfd(STDIN_FILENO)<0) return -1;
  }
  struct sitter_evdev_device *device=sitter_evdev.devicev;
  int i=sitter_evdev.devicec;
  for (;i-->0;device++) {
    if (sitter_evdev_add_pollfd(device->fd)<0) return -1;
  }
  if (sitter_evdev.pollfdc<1) return 0;
  
  int err=poll(sitter_evdev.pollfdv,sitter_evdev.pollfdc,0);
  if (err<=0) return 0;
  
  struct pollfd *pollfd=sitter_evdev.pollfdv;
  i=sitter_evdev.pollfdc;
  for (;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (pollfd->fd==STDIN_FILENO) {
      if ((err=sitter_evdev_read_stdin())<0) {
        sitter_evdev.use_stdin=0;
      }
      if (err) return 1;
    } else {
      struct sitter_evdev_device *device=sitter_evdev_device_by_fd(pollfd->fd);
      if (device) {
        if ((err=sitter_evdev_read_device(event,device))<0) {
          device->enable=0;
        }
        if (err) return 1;
      }
    }
  }

  return 0;
}

/* Public access to joystick list.
 */
 
int SDL_NumJoysticks() {
  return sitter_evdev.devicec;
}

const char *SDL_JoystickName(int p) {
  if ((p<0)||(p>=sitter_evdev.devicec)) return 0;
  return sitter_evdev.devicev[p].name;
}

/* Open joystick.
 */
 
SDL_Joystick *SDL_JoystickOpen(int p) {
  if ((p<0)||(p>=sitter_evdev.devicec)) return 0;
  struct sitter_evdev_device *device=sitter_evdev.devicev+p;
  device->enable=1;
  return device;
}

/* Close joystick.
 */
 
void SDL_JoystickClose(SDL_Joystick *joy) {
  int p=joy-sitter_evdev.devicev;
  if ((p<0)||(p>=sitter_evdev.devicec)) return;
  joy->enable=0;
  // Don't actually clean it up, they might reopen it, and we want to preserve the indices.
}

/* Joystick content.
 * TODO are we doing our own mapping or what?
 */
 
int SDL_JoystickNumAxes(SDL_Joystick *joy) {
  return 0;//TODO
}

int SDL_JoystickNumButtons(SDL_Joystick *joy) {
  return 0;//TODO
}

void sitter_evdev_set_InputManager(InputManager *mgr) {
  sitter_evdev_inmgr=mgr;
}
