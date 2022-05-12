#ifdef SITTER_WII

#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include "sitter_byteorder.h"
#include "sitter_Error.h"
#include "sitter_string.h"
#include "sitter_Game.h"
#include "sitter_Configuration.h"
#include "sitter_WiiUSB.h"
#include "sitter_InputManager.h"

/* WiiUSB_Device::input_driver */
#define USBINPUT_NONE       0
#define USBINPUT_KEYBOARD   1
#define USBINPUT_MOUSE      2
#define USBINPUT_HID        3
#define USBINPUT_XBOX       4

#define BTNMAP_LIMIT 16

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
InputManager::InputManager(Game *game,int evtqlen):game(game) {
  if (evtqlen<1) sitter_throw(ArgumentError,"InputManager, evtqlen=%d",evtqlen);
  promiscuous=false;
  evta=evtqlen;
  evtc=evtp=0;
  if (!(evtq=(InputEvent*)malloc(sizeof(InputEvent)*evta))) sitter_throw(AllocationError,"");
  mousex=mousey=-1;
  usb=NULL;
  usb_plrid=NULL;
  never_remap_keys=false;
  
  PAD_Init();
  WPAD_Init();
  
  for (int i=0;i<4;i++) { // these will be overwritten by config, but set them to something logical anyway
    gcpad_plrid[i]=i+1;
    wiimote_plrid[i]=i+1;
  }
  memset(gcpad_connected,0,sizeof(bool)*4);
  memset(wiimote_connected,0,sizeof(bool)*4);
  
  try { usb=new WiiUSB_HCI(this,"/dev/usb/oh0"); }
  catch (...) {
    sitter_printError();
    sitter_log("InputManager: open USB HCI '/dev/usb/oh0' failed, USB input is not available");
    sitter_clearError();
    usb=NULL;
  }
  refreshUSBDevices();
}

InputManager::~InputManager() {
  if (usb) delete usb;
  if (usb_plrid) free(usb_plrid);
  free(evtq);
}

/******************************************************************************
 * button mapping
 *****************************************************************************/
 
static const int _hid_to_unicode[256]={
  0,0,0,0,
  'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
  '1','2','3','4','5','6','7','8','9','0',
  0x0d,0x1b,0x08,0x09,
  ' ','-','=','[',']','\\','#',';','\'','`',',','.','/',
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0x7f,
  0,0,0,0,0,0,0,
  '/','*','-','+',0x0d,
  '1','2','3','4','5','6','7','8','9','0','.',
  '\\',0,0,
  '=',
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 7f=mute...
  ',','=',
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  ',','.',0xa4,0,
  '(',')','[',']',0x09,0x08,'a','b','c','d','e','f','^','^','%','<','>','&',0,'|',0,':','#',' ','@','!',
  0,0,0,0,0,0,0,0xb1,0,0,0,0,0,0,
  0,
};

static const int _hid_to_unicode_shift[256]={
  0,0,0,0,
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
  '!','@','#','$','%','^','&','*','(',')',
  0x0d,0x1b,0x08,0x09,
  ' ','_','+','{','}','|','~',':','"','~','<','>','?',
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0x7f,
  0,0,0,0,0,0,0,
  '/','*','-','+',0x0d,
  '1','2','3','4','5','6','7','8','9','0','.',
  '\\',0,0,
  '=',
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 7f=mute...
  ',','=',
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  ',','.',0xa4,0,
  '(',')','[',']',0x09,0x08,'a','b','c','d','e','f','^','^','%','<','>','&',0,'|',0,':','#',' ','@','!',
  0,0,0,0,0,0,0,0xb1,0,0,0,0,0,0,
  0,
};
 
int InputManager::resolveKeyboardButton(uint8_t hidbtnid,uint8_t hidmods,int plrid,int *btnidv) {
  int btnc=0;
  // player controls:
  if (plrid&&!never_remap_keys) switch (hidbtnid) {
    case 0x2c:/* SP    */ btnidv[btnc++]=SITTER_BTN_PLR_TOSS|plrid;
                          btnidv[btnc++]=SITTER_BTN_PLR_PICKUP|plrid; break;
    case 0x4f:/* RIGHT */ btnidv[btnc++]=SITTER_BTN_PLR_RIGHT|plrid; break;
    case 0x50:/* LEFT  */ btnidv[btnc++]=SITTER_BTN_PLR_LEFT|plrid; break;
    case 0x51:/* DOWN  */ btnidv[btnc++]=SITTER_BTN_PLR_DUCK|plrid; break;
    case 0x52:/* UP    */ btnidv[btnc++]=SITTER_BTN_PLR_JUMP|plrid; break;
    case 0x56:/* KP-   */ btnidv[btnc++]=SITTER_BTN_PLR_ATN|plrid; break;
    case 0x57:/* KP+   */ btnidv[btnc++]=SITTER_BTN_PLR_RADAR|plrid; break;
    case 0x5a:/* KP2   */ btnidv[btnc++]=SITTER_BTN_PLR_DUCK|plrid; break;
    case 0x5c:/* KP4   */ btnidv[btnc++]=SITTER_BTN_PLR_LEFT|plrid; break;
    case 0x5d:/* KP5   */ btnidv[btnc++]=SITTER_BTN_PLR_DUCK|plrid; break;
    case 0x5e:/* KP6   */ btnidv[btnc++]=SITTER_BTN_PLR_RIGHT|plrid; break;
    case 0x60:/* KP8   */ btnidv[btnc++]=SITTER_BTN_PLR_JUMP|plrid; break;
    case 0x62:/* KP0   */ btnidv[btnc++]=SITTER_BTN_PLR_TOSS|plrid;
                          btnidv[btnc++]=SITTER_BTN_PLR_PICKUP|plrid; break;
  }
  // menu controls:
  if (!never_remap_keys) switch (hidbtnid) {
    case 0x28:/* ENTER */ btnidv[btnc++]=SITTER_BTN_MN_OK; break;
    case 0x29:/* ESC   */ btnidv[btnc++]=SITTER_BTN_MN_CANCEL;
                          btnidv[btnc++]=SITTER_BTN_MENU; break;
    case 0x2c:/* SP    */ btnidv[btnc++]=SITTER_BTN_MN_OK; break;
    case 0x4f:/* RIGHT */ btnidv[btnc++]=SITTER_BTN_MN_RIGHT; break;
    case 0x50:/* LEFT  */ btnidv[btnc++]=SITTER_BTN_MN_LEFT; break;
    case 0x51:/* DOWN  */ btnidv[btnc++]=SITTER_BTN_MN_DOWN; break;
    case 0x52:/* UP    */ btnidv[btnc++]=SITTER_BTN_MN_UP; break;
    case 0x58:/* KPENT */ btnidv[btnc++]=SITTER_BTN_MN_OK; break;
    case 0x5a:/* KP2   */ btnidv[btnc++]=SITTER_BTN_MN_DOWN; break;
    case 0x5c:/* KP4   */ btnidv[btnc++]=SITTER_BTN_MN_LEFT; break;
    case 0x5d:/* KP5   */ btnidv[btnc++]=SITTER_BTN_MN_DOWN; break;
    case 0x5e:/* KP6   */ btnidv[btnc++]=SITTER_BTN_MN_RIGHT; break;
    case 0x60:/* KP8   */ btnidv[btnc++]=SITTER_BTN_MN_UP; break;
    case 0x62:/* KP0   */ btnidv[btnc++]=SITTER_BTN_MN_OK; break;
  }
  // Unicode
  const int *unicode_tb;
  if (hidmods&0x22) unicode_tb=_hid_to_unicode_shift;
  else unicode_tb=_hid_to_unicode;
  if (unicode_tb[hidbtnid]) btnidv[btnc++]=unicode_tb[hidbtnid];
  return btnc;
}

/******************************************************************************
 * USB
 *****************************************************************************/
 
void InputManager::refreshUSBDevices() {
  sitter_log("InputManager::refreshUSBDevices hci=%p",usb);
  if (!usb) return;
  usb->scan();
  for (int i=0;i<usb->addrc;i++) {
    try {
      WiiUSB_Device *dev=usb->open(usb->addrv[i].vid,usb->addrv[i].pid);
      bool isinput=false;
      try {
        switch (dev->cls) {
          case USB_CLASS_HID: {
              isinput=true;
              if (dev->subcls==1) switch (dev->proto) {
                case 0x01: {
                    if (dev->packetsize_in<8) isinput=false; 
                    else {
                      dev->input_driver=USBINPUT_KEYBOARD; 
                      if (dev->io_control(0x21,0x0b/*SET_PROTOCOL*/,0,dev->intf_id,0,NULL)<0) {
                        sitter_log("SET_PROTOCOL(boot) failed, not using keyboard");
                        isinput=false;
                      }
                    }
                  } break;
                case 0x02: dev->input_driver=USBINPUT_MOUSE; break;
                default: dev->input_driver=USBINPUT_HID;
              } else dev->input_driver=USBINPUT_HID;
            } break;
          case USB_CLASS_XBOX: {
              //sitter_log("detected xbox joystick");
              isinput=true;
              dev->input_driver=USBINPUT_XBOX;
              if (dev->packetsize_in>20) dev->packetsize_in=20;
            } break;
        }
      } catch (...) { usb->removeDevice(dev); throw; }
      if (isinput) {
        //sitter_log("%p begin listening...",dev);
        dev->beginListenThread();
      } else {
        //sitter_log("InputManager: USB device %x/%x is not input device",dev->vid,dev->pid);
        usb->removeDevice(dev);
      }
    } catch (...) {
      sitter_printError();
      sitter_log("InputManager: error involving USB device %x/%x (%d of %d)",usb->addrv[i].vid,usb->addrv[i].pid,i+1,usb->addrc);
      sitter_clearError();
    }
  }
  /* assign player ids */
  /* TODO mapping these is a complicated proposition....
   *      For now, they're assigned in order. Is it worth storing that in Configuration somehow?
   */
  if (usb_plrid) free(usb_plrid);
  if (!usb->devc) { usb_plrid=NULL; return; }
  if (!(usb_plrid=(int*)malloc(sizeof(int)*usb->devc))) sitter_throw(AllocationError,"");
  for (int i=0;i<usb->devc;i++) {
    usb_plrid[i]=(i&3)+1;
  }
  sitter_log("...refreshUSBDevices");
}

/******************************************************************************
 * USB, report reception
 *****************************************************************************/

void InputManager::receiveKeyboardReport(const void *rpt,const void *prev,int devid) {
  //sitter_log("receiveKeyboardReport %02x %02x %02x %02x %02x %02x %02x %02x",
  //  sitter_get8(rpt,0),sitter_get8(rpt,1),sitter_get8(rpt,2),sitter_get8(rpt,3),
  //  sitter_get8(rpt,4),sitter_get8(rpt,5),sitter_get8(rpt,6),sitter_get8(rpt,7));
  uint8_t mods=sitter_get8(rpt,0);
  int btnbuf[BTNMAP_LIMIT];
  /* down */
  for (int ini=2;ini<8;ini++) {
    uint8_t inch=sitter_get8(rpt,ini);
    if ((inch==0)||(inch==0xff)) continue;
    bool already_got=false;
    for (int pri=2;pri<8;pri++) {
      if (sitter_get8(prev,pri)==inch) { already_got=true; break; }
    }
    if (already_got) continue;
    int btnc=resolveKeyboardButton(inch,mods,usb_plrid[devid]<<12,btnbuf);
    for (int btni=0;btni<btnc;btni++) {
      if (SITTER_BTN_ISUNICODE(btnbuf[btni])) pushEvent(InputEvent(SITTER_EVT_UNICODE,btnbuf[btni],1));
      else pushEvent(InputEvent(SITTER_EVT_BTNDOWN,btnbuf[btni],1));
    }
    #if 0
    switch (inch) { // TODO -- keycode lookup table
      case 0x28: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_OK,1)); break; // enter
      case 0x29: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_CANCEL,1)); break; // escape
      case 0x4f: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_RIGHT,1)); break; // arrows...
      case 0x50: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_LEFT,1)); break;
      case 0x51: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_DOWN,1)); break;
      case 0x52: pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_UP,1)); break;
    }
    #endif
  }
  /* up */
  mods=sitter_get8(prev,0);
  for (int pri=2;pri<8;pri++) {
    uint8_t prch=sitter_get8(prev,pri);
    if ((prch==0)||(prch==0xff)) continue;
    bool still_got=false;
    for (int ini=2;ini<8;ini++) {
      if (sitter_get8(rpt,ini)==prch) { still_got=true; break; }
    }
    if (still_got) continue;
    int btnc=resolveKeyboardButton(prch,mods,usb_plrid[devid]<<12,btnbuf);
    for (int btni=0;btni<btnc;btni++) {
      if (SITTER_BTN_ISUNICODE(btnbuf[btni])) ;
      else pushEvent(InputEvent(SITTER_EVT_BTNUP,btnbuf[btni],0));
    }
    #if 0
    switch (prch) { // TODO -- keycode lookup table
      case 0x28: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_OK,0)); break; // enter
      case 0x29: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_CANCEL,0)); break; // escape
      case 0x4f: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_RIGHT,0)); break; // arrows...
      case 0x50: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_LEFT,0)); break;
      case 0x51: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_DOWN,0)); break;
      case 0x52: pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_UP,0)); break;
    }
    #endif
  }
}

void InputManager::receiveMouseReport(const void *rpt,const void *prev,int devid) {
  // TODO
}

void InputManager::receiveHIDReport(const void *rpt,const void *prev,WiiUSB_Device *dev,int devid) {
  // TODO -- will require more knowledge from device at startup
}

void InputManager::receiveXboxReport(const void *rpt,const void *prev,int devid) {
  #pragma pack(push,1)
  typedef struct {
    uint16_t k1; // constant, unused (i think, report ID and length. but we don't use it)
    uint8_t btns; // (80)RPLUNGE LPLUNGE BACK START RIGHT LEFT DOWN (01)UP
    uint8_t k2; // always zero
    uint8_t btn_a;
    uint8_t btn_b;
    uint8_t btn_x;
    uint8_t btn_y;
    uint8_t btn_black;
    uint8_t btn_white;
    uint8_t btn_l;
    uint8_t btn_r;
    int16_t lx; // little-endian
    int16_t ly; // -down +up
    int16_t rx;
    int16_t ry;
  } XboxReport;
  #pragma pack(pop)
  int plrid=usb_plrid[devid]<<12;
  XboxReport *rn=(XboxReport*)rpt;
  XboxReport *rp=(XboxReport*)prev;
  /* check 2-state buttons */
  if (rn->btns!=rp->btns) {
    uint8_t btns_new=rn->btns&~rp->btns;
    uint8_t btns_old=rp->btns&~rn->btns;
    for (int i=1;i<256;i<<=1) {
      int btnid_menu=0,btnid_player=0,btnid_other=0;
      switch (i) {
        case 0x01: btnid_menu=SITTER_BTN_MN_UP; break; // up
        case 0x02: btnid_menu=SITTER_BTN_MN_DOWN; btnid_player=SITTER_BTN_PLR_DUCK; break; // down
        case 0x04: btnid_menu=SITTER_BTN_MN_LEFT; btnid_player=SITTER_BTN_PLR_LEFT; break; // left
        case 0x08: btnid_menu=SITTER_BTN_MN_RIGHT; btnid_player=SITTER_BTN_PLR_RIGHT; break; // right
        case 0x10: btnid_menu=SITTER_BTN_MN_CANCEL; btnid_other=SITTER_BTN_MENU; break; // start
      } 
      if (btns_new&i) {
        if (btnid_menu) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,btnid_menu,1));
        if (btnid_other) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,btnid_other,1));
        if (btnid_player) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,btnid_player|plrid,1));
      }
      if (btns_old&i) {
        if (btnid_menu) pushEvent(InputEvent(SITTER_EVT_BTNUP,btnid_menu,0));
        if (btnid_other) pushEvent(InputEvent(SITTER_EVT_BTNUP,btnid_other,0));
        if (btnid_player) pushEvent(InputEvent(SITTER_EVT_BTNUP,btnid_player|plrid,0));
      }
    }
  }
  /* check analogue buttons */
  #define BTN_THRESH 0x40
  if ((rn->btn_a>=BTN_THRESH)!=(rp->btn_a>=BTN_THRESH)) {
    if (rn->btn_a>=BTN_THRESH) { // A = ok, jump
      pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_OK,1));
      pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_JUMP|plrid,1));
    } else {
      pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_OK,0));
      pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_JUMP|plrid,0));
    }
  }
  if ((rn->btn_x>=BTN_THRESH)!=(rp->btn_x>=BTN_THRESH)) {
    if (rn->btn_x>=BTN_THRESH) { // X = cancel, pickup, toss
      pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_CANCEL,1));
      pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_TOSS|plrid,1));
      pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_PICKUP|plrid,1));
    } else {
      pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_CANCEL,0));
      pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_TOSS|plrid,0));
      pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_PICKUP|plrid,0));
    }
  }
  if ((rn->btn_black>=BTN_THRESH)!=(rp->btn_black>=BTN_THRESH)) {
    if (rn->btn_black>=BTN_THRESH) { // black = focus
      pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_ATN|plrid,1));
    } else {
      pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_ATN|plrid,0));
    }
  }
  if ((rn->btn_white>=BTN_THRESH)!=(rp->btn_white>=BTN_THRESH)) {
    if (rn->btn_white>=BTN_THRESH) { // white=radar
      pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_RADAR|plrid,1));
    } else {
      pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_RADAR|plrid,0));
    }
  }
  #undef BTN_THRESH
  /* analogue sticks are not used at this time. if we do, make sure to byte-swap them */
}

void InputManager::pollUSB() {
  if (!usb) return;
  for (int i=0;i<usb->devc;i++) if (WiiUSB_Device *dev=usb->devv[i]) {
    int slack_counter=10; // if a device updates itself *really* fast, we still keep sync (up to about 600 fps)
    while (const void *rpt=dev->getReport()) {
      switch (dev->input_driver) {
        //case USBINPUT_KEYBOARD: receiveKeyboardReport(rpt,dev->getPreviousReport(),i); break;
        case USBINPUT_MOUSE: receiveMouseReport(rpt,dev->getPreviousReport(),i); break;
        case USBINPUT_HID: receiveHIDReport(rpt,dev->getPreviousReport(),dev,i); break;
        case USBINPUT_XBOX: receiveXboxReport(rpt,dev->getPreviousReport(),i); break;
      }
      if (--slack_counter<10) break;
    }
  }
}

/******************************************************************************
 * reconfigure
 *****************************************************************************/
 
void InputManager::reconfigure() {
  if (!game||!game->cfg) return;
  /* gamecube joysticks */
  const char *gcpadmap=game->cfg->getOption_str("gcpadmap");
  if (gcpadmap&&gcpadmap[0]) {
    char **mapv=sitter_strsplit_selfDescribing(gcpadmap);
    bool listshort=false;
    for (int i=0;i<4;i++) {
      if (!mapv[i]) {
        sitter_log("invalid list 'gcpadmap', expected 4 items, found %d.",i);
        listshort=true;
        break;
      }
      char *tail=0;
      gcpad_plrid[i]=strtol(mapv[i],&tail,0);
      if (!tail||tail[0]) {
        sitter_log("skipping gcpadmap[%d], expected integer",i);
        continue;
      }
      if ((gcpad_plrid[i]<0)||(gcpad_plrid[i]>4)) {
        sitter_log("bad value %d for gcpadmap[%d], reset to zero",gcpad_plrid[i],i);
        gcpad_plrid[i]=0;
      }
    }
    if (!listshort&&mapv[4]) sitter_log("ignoring spurious items in 'gcpadmap'");
    for (char **mapi=mapv;*mapi;mapi++) free(*mapi);
    free(mapv);
  }
  /* wiimotes */
  const char *wiimotemap=game->cfg->getOption_str("wiimotemap");
  if (wiimotemap&&wiimotemap[0]) {
    char **mapv=sitter_strsplit_selfDescribing(wiimotemap);
    bool listshort=false;
    for (int i=0;i<4;i++) {
      if (!mapv[i]) {
        sitter_log("invalid list 'wiimotemap', expected 4 items, found %d.",i);
        listshort=true;
        break;
      }
      char *tail=0;
      wiimote_plrid[i]=strtol(mapv[i],&tail,0);
      if (!tail||tail[0]) {
        sitter_log("skipping wiimotemap[%d], expected integer",i);
        continue;
      }
      if ((wiimote_plrid[i]<0)||(wiimote_plrid[i]>4)) {
        sitter_log("bad value %d for wiimotemap[%d], reset to zero",wiimote_plrid[i],i);
        wiimote_plrid[i]=0;
      }
    }
    if (!listshort&&mapv[4]) sitter_log("ignoring spurious items in 'wiimotemap'");
    for (char **mapi=mapv;*mapi;mapi++) free(*mapi);
    free(mapv);
  }
}

/******************************************************************************
 * write config
 *****************************************************************************/
 
void InputManager::writeConfiguration() {
  char mapbuf[9];
  mapbuf[0]=mapbuf[2]=mapbuf[4]=mapbuf[6]=',';
  mapbuf[8]=0;
  for (int i=0;i<4;i++) mapbuf[(i<<1)+1]=0x30+gcpad_plrid[i];
  game->cfg->setOption_str("gcpadmap",mapbuf);
  for (int i=0;i<4;i++) mapbuf[(i<<1)+1]=0x30+wiimote_plrid[i];
  game->cfg->setOption_str("wiimotemap",mapbuf);
  // ...awesome
}

void InputManager::generaliseMappings() {
  // no op
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
void InputManager::update() {
  memset(gcpad_connected,0,sizeof(bool)*4);
  memset(wiimote_connected,0,sizeof(bool)*4);

  /* gamecube pads */
  uint32_t padbusstate=PAD_ScanPads();
  for (int i=0;i<4;i++) {
    if (padbusstate&(1<<i)) gcpad_connected[i]=true;
    else continue;
  
    uint16_t padstate=PAD_ButtonsDown(i);
    /* menu controls */
    if (padstate&PAD_BUTTON_LEFT)  pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_LEFT,1));
    if (padstate&PAD_BUTTON_RIGHT) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_RIGHT,1));
    if (padstate&PAD_BUTTON_UP)    pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_UP,1));
    if (padstate&PAD_BUTTON_DOWN)  pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_DOWN,1));
    if (padstate&PAD_BUTTON_A)     pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_OK,1));
    if (padstate&PAD_BUTTON_B)     pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_CANCEL,1));
    if (padstate&PAD_BUTTON_START) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MENU,1));
    /* player controls */
    if (gcpad_plrid[i]) {
      if (padstate&PAD_BUTTON_LEFT)  pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_LEFT|(gcpad_plrid[i]<<12),1));
      if (padstate&PAD_BUTTON_RIGHT) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_RIGHT|(gcpad_plrid[i]<<12),1));
      if (padstate&PAD_BUTTON_A)     pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_JUMP|(gcpad_plrid[i]<<12),1));
      if (padstate&PAD_BUTTON_DOWN)  pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_DUCK|(gcpad_plrid[i]<<12),1));
      if (padstate&PAD_BUTTON_B) {
        pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_PICKUP|(gcpad_plrid[i]<<12),1));
        pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_TOSS|(gcpad_plrid[i]<<12),1));
      }
      if (padstate&PAD_BUTTON_X)     pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_ATN|(gcpad_plrid[i]<<12),1));
      if (padstate&PAD_BUTTON_Y)     pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_RADAR|(gcpad_plrid[i]<<12),1));
    }
    
    padstate=PAD_ButtonsUp(i);
    /* menu controls */
    if (padstate&PAD_BUTTON_LEFT)  pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_LEFT,0));
    if (padstate&PAD_BUTTON_RIGHT) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_RIGHT,0));
    if (padstate&PAD_BUTTON_UP)    pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_UP,0));
    if (padstate&PAD_BUTTON_DOWN)  pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_DOWN,0));
    if (padstate&PAD_BUTTON_A)     pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_OK,0));
    if (padstate&PAD_BUTTON_B)     pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_CANCEL,0));
    if (padstate&PAD_BUTTON_START) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MENU,0));
    /* player controls */
    if (gcpad_plrid[i]) {
      if (padstate&PAD_BUTTON_LEFT)  pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_LEFT|(gcpad_plrid[i]<<12),0));
      if (padstate&PAD_BUTTON_RIGHT) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_RIGHT|(gcpad_plrid[i]<<12),0));
      if (padstate&PAD_BUTTON_A)     pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_JUMP|(gcpad_plrid[i]<<12),0));
      if (padstate&PAD_BUTTON_DOWN)  pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_DUCK|(gcpad_plrid[i]<<12),0));
      if (padstate&PAD_BUTTON_B) {
        pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_PICKUP|(gcpad_plrid[i]<<12),0));
        pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_TOSS|(gcpad_plrid[i]<<12),0));
      }
      if (padstate&PAD_BUTTON_X)     pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_ATN|(gcpad_plrid[i]<<12),0));
      if (padstate&PAD_BUTTON_Y)     pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_RADAR|(gcpad_plrid[i]<<12),0));
    }
  }
  
  /* wiimotes */
  for (int i=WPAD_ScanPads();i-->0;) if (WPADData *wd=WPAD_Data(i)) {
    wiimote_connected[i]=true;
    if (wd->data_present&WPAD_DATA_BUTTONS) {
    
      /* menu, down */
      if (wd->btns_d&WPAD_BUTTON_UP) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_LEFT,1));
      if (wd->btns_d&WPAD_BUTTON_DOWN) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_RIGHT,1));
      if (wd->btns_d&WPAD_BUTTON_LEFT) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_DOWN,1));
      if (wd->btns_d&WPAD_BUTTON_RIGHT) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_UP,1));
      if (wd->btns_d&WPAD_BUTTON_2) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_OK,1));
      if (wd->btns_d&WPAD_BUTTON_1) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MN_CANCEL,1));
      if (wd->btns_d&WPAD_BUTTON_HOME) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_MENU,1));
      /* player, down */
      if (wiimote_plrid[i]) {
        if (wd->btns_d&WPAD_BUTTON_UP) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_LEFT|(wiimote_plrid[i]<<12),1));
        if (wd->btns_d&WPAD_BUTTON_DOWN) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_RIGHT|(wiimote_plrid[i]<<12),1));
        if (wd->btns_d&WPAD_BUTTON_LEFT) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_DUCK|(wiimote_plrid[i]<<12),1));
        if (wd->btns_d&WPAD_BUTTON_2) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_JUMP|(wiimote_plrid[i]<<12),1));
        if (wd->btns_d&WPAD_BUTTON_1) {
          pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_PICKUP|(wiimote_plrid[i]<<12),1));
          pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_TOSS|(wiimote_plrid[i]<<12),1));
        }
        if (wd->btns_d&WPAD_BUTTON_PLUS) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_ATN|(wiimote_plrid[i]<<12),1));
        if (wd->btns_d&WPAD_BUTTON_MINUS) pushEvent(InputEvent(SITTER_EVT_BTNDOWN,SITTER_BTN_PLR_RADAR|(wiimote_plrid[i]<<12),1));
      }
    
      /* menu, up */
      if (wd->btns_u&WPAD_BUTTON_UP) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_LEFT,0));
      if (wd->btns_u&WPAD_BUTTON_DOWN) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_RIGHT,0));
      if (wd->btns_u&WPAD_BUTTON_LEFT) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_DOWN,0));
      if (wd->btns_u&WPAD_BUTTON_RIGHT) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_UP,0));
      if (wd->btns_u&WPAD_BUTTON_2) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_OK,0));
      if (wd->btns_u&WPAD_BUTTON_1) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MN_CANCEL,0));
      if (wd->btns_u&WPAD_BUTTON_HOME) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_MENU,0));
      /* player, down */
      if (wiimote_plrid[i]) {
        if (wd->btns_u&WPAD_BUTTON_UP) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_LEFT|(wiimote_plrid[i]<<12),0));
        if (wd->btns_u&WPAD_BUTTON_DOWN) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_RIGHT|(wiimote_plrid[i]<<12),0));
        if (wd->btns_u&WPAD_BUTTON_LEFT) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_DUCK|(wiimote_plrid[i]<<12),0));
        if (wd->btns_u&WPAD_BUTTON_2) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_JUMP|(wiimote_plrid[i]<<12),0));
        if (wd->btns_u&WPAD_BUTTON_1) {
          pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_PICKUP|(wiimote_plrid[i]<<12),0));
          pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_TOSS|(wiimote_plrid[i]<<12),0));
        }
        if (wd->btns_u&WPAD_BUTTON_PLUS) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_ATN|(wiimote_plrid[i]<<12),0));
        if (wd->btns_u&WPAD_BUTTON_MINUS) pushEvent(InputEvent(SITTER_EVT_BTNUP,SITTER_BTN_PLR_RADAR|(wiimote_plrid[i]<<12),0));
      }
      
    }
  }
  
  pollUSB();
  
}

#endif
