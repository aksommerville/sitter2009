#ifdef SITTER_WII

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "sitter_byteorder.h"
#include "sitter_Error.h"
#include "sitter_WiiUSB.h"

#define ADDRV_INCREMENT 16
#define DEVV_INCREMENT   4

/******************************************************************************
 * misc
 *****************************************************************************/
 
static inline int rdhex(char c) {
  if ((c>=0x30)&&(c<=0x39)) return c-0x30;
  if ((c>=0x41)&&(c<=0x46)) return c-0x37;
  if ((c>=0x61)&&(c<=0x66)) return c-0x57;
  sitter_throw(ArgumentError,"expected hexadecimal digit, found 0x%02x",c);
}

/******************************************************************************
 * HCI init / kill
 *****************************************************************************/
 
WiiUSB_HCI::WiiUSB_HCI(InputManager *mgr,const char *path):mgr(mgr) {
  //sitter_log("%p WiiUSB_HCI '%s'",this,path);
  devv=NULL; devc=deva=0;
  addrv=NULL; addrc=addra=0;
  if (!(this->path=strdup(path))) sitter_throw(AllocationError,"");
}

WiiUSB_HCI::~WiiUSB_HCI() {
  //sitter_log("%p ~WiiUSB_HCI",this);
  for (int i=0;i<devc;i++) delete devv[i];
  if (devv) free(devv);
  if (path) free(path);
  if (addrv) free(addrv);
}

/******************************************************************************
 * HCI scan
 *****************************************************************************/
 
void WiiUSB_HCI::scan() {
  //sitter_log("%p WiiUSB_HCI::scan",this);
  int fd=IOS_Open(path,IPC_OPEN_NONE);
  if (fd<0) sitter_throw(IOError,"USB: open '%s' failed (%d)",path,fd);
  try {
    
    /* from QQ, 20091003: */
    #define DEVLIMIT 128
    ioctlv *argv=(ioctlv*)memalign(32,sizeof(ioctlv)*4);
    if (!argv) sitter_throw(AllocationError,"");
    try {
      argv[0].data=memalign(32,1); argv[0].len=1;
      argv[1].data=memalign(32,1); argv[1].len=1;
      argv[2].data=memalign(32,1); argv[2].len=1;
      argv[3].data=memalign(32,8*DEVLIMIT); argv[3].len=8*DEVLIMIT;
      if (!argv[0].data||!argv[1].data||!argv[2].data||!argv[3].data) sitter_throw(AllocationError,""); // slight leak here
      try {
        sitter_set8(argv[0].data,0,DEVLIMIT);
        sitter_set8(argv[1].data,0,0);
        sitter_set8(argv[2].data,0,0);
        memset(argv[3].data,0,8*DEVLIMIT);
      
        int result=IOS_Ioctlv(fd,USB_IOCTL_GETDEVLIST,2,2,argv);
        if (result<0) sitter_throw(IOError,"usb scan failed, %d",result);
        
        addrc=sitter_get8(argv[2].data,0);
        if (addrc>addra) {
          addra=addrc;
          addrv=(USBAddress*)realloc(addrv,sizeof(USBAddress)*addra);
          if (!addrv) sitter_throw(AllocationError,"");
        }
        for (int i=0;i<addrc;i++) {
          addrv[i].vid=sitter_get16be(argv[3].data,8*i+4);
          addrv[i].pid=sitter_get16be(argv[3].data,8*i+6);
        }
      
      } catch (...) { free(argv[0].data); free(argv[1].data); free(argv[2].data); free(argv[3].data); throw; }
      free(argv[0].data); free(argv[1].data); free(argv[2].data); free(argv[3].data);
    } catch (...) { free(argv); throw; }
    free(argv);
    #undef DEVLIMIT
    
  } catch (...) { IOS_Close(fd); throw; }
  IOS_Close(fd);
}

/******************************************************************************
 * HCI open
 *****************************************************************************/
 
WiiUSB_Device *WiiUSB_HCI::open(uint16_t vid,uint16_t pid) {
  //sitter_log("%p WiiUSB_HCI::open(%x,%x)",this,vid,pid);
  WiiUSB_Device *dev=new WiiUSB_Device(this,vid,pid);
  if (devc>=deva) {
    deva+=DEVV_INCREMENT;
    if (!(devv=(WiiUSB_Device**)realloc(devv,sizeof(void*)*deva))) sitter_throw(AllocationError,"");
  }
  devv[devc++]=dev;
  return dev;
}

/******************************************************************************
 * HCI list access
 *****************************************************************************/
 
WiiUSB_Device *WiiUSB_HCI::findByAddress(uint16_t vid,uint16_t pid) const {
  for (int i=0;i<devc;i++) if ((vid==devv[i]->vid)&&(pid==devv[i]->pid)) return devv[i];
  return NULL;
}

WiiUSB_Device *WiiUSB_HCI::findBySerialNumber(const char *serial) const {
  uint16_t vid=0,pid=0;
  int seriallen=0; while (serial[seriallen]) seriallen++;
  if ((seriallen<9)||(serial[8]!=':')) sitter_throw(ArgumentError,"WiiUSB_HCI::findBySerialNumber: expected 'VID_PID_:SERIAL'");
  int shift=12; for (int i=0;i<4;i++) {
    vid|=(rdhex(serial[i  ])<<shift);
    pid|=(rdhex(serial[i+4])<<shift);
    shift-=4;
  }
  serial+=9;
  for (int i=0;i<devc;i++) {
    if ((vid!=devv[i]->vid)||(pid!=devv[i]->pid)) continue;
    if (!devv[i]->serial||!devv[i]->serial[0]) {
      if (!serial[0]) return devv[i];
      continue;
    }
    if (strcmp(devv[i]->serial,serial)) continue;
    return devv[i];
  }
  return NULL;
}

WiiUSB_Device *WiiUSB_HCI::findByName(const char *name) const {
  uint16_t vid=0,pid=0;
  int namelen=0; while (name[namelen]) namelen++;
  if ((namelen<9)||(name[8]!=':')) sitter_throw(ArgumentError,"WiiUSB_HCI::findByName: expected 'VID_PID_:VENDOR PRODUCT'");
  int shift=12; for (int i=0;i<4;i++) {
    vid|=(rdhex(name[i  ])<<shift);
    pid|=(rdhex(name[i+4])<<shift);
    shift-=4;
  }
  name+=9;
  for (int i=0;i<devc;i++) {
    if ((vid!=devv[i]->vid)||(pid!=devv[i]->pid)) continue;
    if (devv[i]->namecmp(name)) continue;
    return devv[i];
  }
  return NULL;
}

void WiiUSB_HCI::removeDevice(WiiUSB_Device *dev) {
  if (!dev) return;
  delete dev;
  for (int i=0;i<devc;i++) if (devv[i]==dev) {
    devc--;
    if (i<devc) memmove(devv+i,devv+i+1,sizeof(void*)*(devc-i));
    return;
  }
}

/******************************************************************************
 * Device init / kill
 *****************************************************************************/
 
#define ARGC_CTL     7
#define ARGC_BULKINT 3

/* listen thread status */
#define THREAD_NONE       0 // thread not initialised
#define THREAD_WAIT       1 // waiting for input
#define THREAD_READY      2 // input received
#define THREAD_KILL       3 // terminate thread

void *sitter_wiiusb_listen_thread(void *arg) {
  WiiUSB_Device *dev=(WiiUSB_Device*)arg;
  volatile int *thdstatus=dev->thdstatus;
  void *buf=dev->thdbuf;
  //sitter_log("%p listen thread...",arg);
  while (1) {
    while (*thdstatus==THREAD_READY) {
      usleep(1000);
    }
    if (*thdstatus==THREAD_KILL) break;
    if (dev->io_bulkint(dev->ioctl_in,dev->ep_in,dev->packetsize_in,buf)>0) {
      if (*thdstatus!=THREAD_KILL) *thdstatus=THREAD_READY;
    }
    if (*thdstatus==THREAD_KILL) break;
  }
  //sitter_log("%p ...listen thread",arg);
  free((int*)thdstatus);
  free(buf);
  return NULL;
}
 
WiiUSB_Device::WiiUSB_Device(WiiUSB_HCI *hci,uint16_t vid,uint16_t pid):hci(hci),vid(vid),pid(pid) {
  //sitter_log("%p WiiUSB_Device %x/%x",this,vid,pid);
  vname=pname=serial=NULL;
  cfg_id=intf_id=alt_id=0;
  cfgc=0;
  got_cfg_id=false;
  cls=subcls=proto=0;
  ep_in=ep_out=0;
  ioctl_in=ioctl_out=-1;
  packetsize_in=packetsize_out=0;
  fd=-1;
  argv_ctl=NULL; buf_ctl=NULL; buf_ctl_a=0;
  argv_bulkint=NULL; buf_bulkint=NULL; buf_bulkint_a=0;
  thdbuf=NULL;
  thdstatus=NULL;
  listenthd=-1;
  rptbuf=prevrptbuf=NULL;
  input_driver=0;
  dev_open();
  read_main_descriptors();
}

WiiUSB_Device::~WiiUSB_Device() {
  //sitter_log("%p ~WiiUSB_Device %x/%x",this,vid,pid);
  dev_close();
  if (vname) free(vname);
  if (pname) free(pname);
  if (serial) free(serial);
  if (rptbuf) free(rptbuf);
  if (prevrptbuf) free(prevrptbuf);
  if (buf_ctl) free(buf_ctl);
  if (buf_bulkint) free(buf_bulkint);
  // the last element of the argvs is either its 'buf_', or user data. either way, don't free it
  if (argv_ctl) {
    for (int i=0;i<ARGC_CTL-1;i++) if (argv_ctl[i].data) free(argv_ctl[i].data);
    free(argv_ctl);
  }
  if (argv_bulkint) {
    for (int i=0;i<ARGC_BULKINT-1;i++) if (argv_bulkint[i].data) free(argv_bulkint[i].data);
    free(argv_bulkint);
  }
}

/******************************************************************************
 * Device misc
 *****************************************************************************/
 
int WiiUSB_Device::namecmp(const char *name) const {
  if (!name) {
    if ((vname&&vname[0])||(pname&&pname[0])) return 1;
    return 0;
  }
  while (name[0]==' ') name++;
  if (vname) {
    for (int i=0;vname[i]||name[0];i++) {
      if (vname[i]!=name[0]) {
        if (!vname[i]) { // ok, matched vendor
          if (name[0]!=' ') return -1; // ...or not
          break;
        } else if (!name[0]) {
          return 1;
        } else {
          if (vname[i]<name[0]) return -1;
          return 1;
        }
      }
      name++;
    }
  }
  while (name[0]==' ') name++;
  if (pname) {
    for (int i=0;pname[i]||name[0];i++) {
      if (pname[i]!=name[0]) {
        if (!pname[i]) {
          break;
        } else if (!name[0]) {
          return 1;
        } else {
          if (pname[i]<name[0]) return -1;
          return 1;
        }
      }
      name++;
    }
  }
  while (name[0]==' ') name++;
  if (name[0]) return -1;
  return 0;
}

/******************************************************************************
 * Device open / close
 *****************************************************************************/
 
void WiiUSB_Device::dev_open() {
  if (fd>=0) dev_close();
  char path[256];
  if (snprintf(path,256,"%s/%x/%x",hci->path,vid,pid)>=256) 
    sitter_throw(FileNotFoundError,"USB Device %x/%x, path too long",vid,pid);
  fd=IOS_Open(path,IPC_OPEN_NONE);
  if (fd<0) switch (fd) {
    default: sitter_throw(IOError,"IOS_Open(%s): %d",path,fd);
  }
}

void WiiUSB_Device::dev_close() {
  if (thdstatus) *thdstatus=THREAD_KILL;
  thdstatus=NULL;
  thdbuf=NULL;
  listenthd=-1; // TODO is this safe, or do we need to rejoin?
  if (rptbuf) { free(rptbuf); rptbuf=NULL; }
  if (prevrptbuf) { free(prevrptbuf); prevrptbuf=NULL; }
  if (fd<0) return;
  IOS_Close(fd);
  fd=-1;
  // keep vid,pid,hci so we can reopen if necessary. flush everything else
  if (vname) { free(vname); vname=NULL; }
  if (pname) { free(pname); pname=NULL; }
  if (serial) { free(serial); serial=NULL; }
  cfg_id=intf_id=alt_id=0;
  cls=subcls=proto=0;
  cfgc=0;
  got_cfg_id=false;
  ep_in=ep_out=0;
  ioctl_in=ioctl_out=-1;
  packetsize_in=packetsize_out=0;
}

void WiiUSB_Device::beginListenThread() {
  if (thdstatus) *thdstatus=THREAD_KILL;
  if (!ep_in||!packetsize_in||(ioctl_in<0)) {
    thdstatus=NULL;
    thdbuf=NULL;
    listenthd=-1;
    return;
  }
  if (!(thdstatus=(volatile int*)malloc(sizeof(int)))) sitter_throw(AllocationError,"");
  *thdstatus=THREAD_WAIT;
  if (!(thdbuf=memalign(32,packetsize_in))) sitter_throw(AllocationError,"");
  if (rptbuf) free(rptbuf);
  if (!(rptbuf=malloc(packetsize_in))) sitter_throw(AllocationError,"");
  memset(rptbuf,0,packetsize_in);
  if (prevrptbuf) free(prevrptbuf);
  if (!(prevrptbuf=malloc(packetsize_in))) sitter_throw(AllocationError,"");
  memset(prevrptbuf,0,packetsize_in);
  if (LWP_CreateThread(&listenthd,sitter_wiiusb_listen_thread,this,NULL,0,1)<0) // TODO prio=50?
    sitter_throw(IOError,"create listen thread failed");
}

const void *WiiUSB_Device::getReport() {
  if (!thdstatus) return NULL;
  if (*thdstatus!=THREAD_READY) return NULL;
  memcpy(prevrptbuf,rptbuf,packetsize_in);
  memcpy(rptbuf,thdbuf,packetsize_in);
  *thdstatus=THREAD_WAIT;
  return rptbuf;
}

const void *WiiUSB_Device::getPreviousReport() {
  return prevrptbuf;
}

void WiiUSB_Device::read_main_descriptors() {
  #define DESC_BUF_LIMIT 1024
  int err;
  void *descbuf=memalign(32,DESC_BUF_LIMIT);
  if (!descbuf) sitter_throw(AllocationError,"");
  try {
  
    read_setup_descriptors(descbuf,DESC_BUF_LIMIT,USB_DESC_DEVICE,0);
    if ((cls==USB_CLASS_HUB)||(cls==USB_CLASS_WIRELESS)) { // don't mess around with hubs, just get out now
      free(descbuf);
      return;
    }
      
    read_setup_descriptors(descbuf,DESC_BUF_LIMIT,USB_DESC_CONFIGURATION,cfg_id);
    if (!got_cfg_id) { // something's wrong, don't worry about it
      free(descbuf);
      return;
    }
    if ((err=io_control(0x00,USB_REQ_SET_CONFIGURATION,cfg_id,0,0,NULL))<0) 
      sitter_throw(IOError,"USB device %x/%x: SET_CONFIGURATION(%d) failed (%d)",vid,pid,cfg_id,err);
    if ((err=io_control(0x01,USB_REQ_SET_INTERFACE,alt_id,intf_id,0,NULL))<0)
      sitter_throw(IOError,"USB device %x/%x: SET_INTERFACE(%d,%d) failed (%d)",vid,pid,intf_id,alt_id,err);
      
  } catch (...) { free(descbuf); throw; }
  free(descbuf);
  #undef DESC_BUF_LIMIT
}

void WiiUSB_Device::read_setup_descriptors(void *buf,int bufa,int descid,int descindex) {
  //sitter_log("%p WiiUSB_Device::read_setup_descriptors(%p,%d,%d,%d)...",this,buf,bufa,descid,descindex);
  int bufc=io_control(0x80,USB_REQ_GET_DESCRIPTOR,(descid<<8)|descindex,0,bufa,buf);
  if (bufc<0) sitter_throw(IOError,"USB device %x/%x: GET_DESCRIPTOR(%d,%d) failed (%d)",vid,pid,descid,descindex,bufc);
  int offset=0;
  bool rdep=false;
  while (offset+2<=bufc) {
    uint8_t bLength        =sitter_get8(buf,offset);
    uint8_t bDescriptorType=sitter_get8(buf,offset+1);
    if (offset+bLength>bufc) {
      //sitter_log("  impossible length 0x%02x for descriptor at 0x%x (total=0x%x). abort",bLength,offset,bufc);
      break;
    }
    switch (bDescriptorType) {
    
      case USB_DESC_DEVICE: {
          //sitter_log("  DEVICE descriptor len=0x%x @ 0x%x",bLength,offset);
          uint16_t bcdUSB=0,idVendor=0,idProduct=0,bcdDevice=0;
          uint8_t bDeviceClass=0,bDeviceSubClass=0,bDeviceProtocol=0;
          uint8_t bMaxPacketSize0=0,iManufacturer=0,iProduct=0,iSerialNumber=0,bNumConfigurations=0;
          if (bLength>=4) {  bcdUSB            =sitter_get16le(buf,offset+2);
          if (bLength>=5) {  bDeviceClass      =sitter_get8   (buf,offset+4);
          if (bLength>=6) {  bDeviceSubClass   =sitter_get8   (buf,offset+5);
          if (bLength>=7) {  bDeviceProtocol   =sitter_get8   (buf,offset+6);
          if (bLength>=8) {  bMaxPacketSize0   =sitter_get8   (buf,offset+7);
          if (bLength>=10) { idVendor          =sitter_get16le(buf,offset+8);
          if (bLength>=12) { idProduct         =sitter_get16le(buf,offset+10);
          if (bLength>=14) { bcdDevice         =sitter_get16le(buf,offset+12);
          if (bLength>=15) { iManufacturer     =sitter_get8   (buf,offset+14);
          if (bLength>=16) { iProduct          =sitter_get8   (buf,offset+15);
          if (bLength>=17) { iSerialNumber     =sitter_get8   (buf,offset+16);
          if (bLength>=18) { bNumConfigurations=sitter_get8   (buf,offset+17);
          }}}}}}}}}}}}
          if (bDeviceClass) {
            cls=bDeviceClass;
            subcls=bDeviceSubClass;
            proto=bDeviceProtocol;
          }
          if (bNumConfigurations) cfgc=bNumConfigurations;
          else cfgc=1; // go out on a limb, assume there's at least one configuration
          if (iManufacturer) { if (vname) free(vname); vname=readStringDescriptor(iManufacturer); }
          if (iProduct) { if (pname) free(pname); pname=readStringDescriptor(iProduct); }
          if (iSerialNumber) { if (serial) free(serial); serial=readStringDescriptor(iSerialNumber); }
          rdep=false;
          // TEMP dump the whole descriptor:
          #if 0
            sitter_log("    bcdUSB: %04x",bcdUSB);
            sitter_log("    class: %02x/%02x/%02x",bDeviceClass,bDeviceSubClass,bDeviceProtocol);
            sitter_log("    bMaxPacketSize0: %d",bMaxPacketSize0);
            sitter_log("    vid/pid: %04x/%04x",idVendor,idProduct);
            sitter_log("    bcdDevice: %04x",bcdDevice);
            sitter_log("    iManufacturer: %02x '%s'",iManufacturer,vname?vname:"");
            sitter_log("    iProduct: %02x '%s'",iProduct,pname?pname:"");
            sitter_log("    iSerialNumber: %02x '%s'",iSerialNumber,serial?serial:"");
            sitter_log("    bNumConfigurations: %d",bNumConfigurations);
          #endif
        } break;
        
      case USB_DESC_CONFIGURATION: {
          //sitter_log("  CONFIGURATION descriptor len=0x%x @ 0x%x",bLength,offset);
          // the only thing we care about is bConfigurationValue
          if ((bLength>=6)&&!got_cfg_id) {
            cfg_id=sitter_get8(buf,offset+5);
            got_cfg_id=true;
          }
          rdep=false;
        } break;
        
      case USB_DESC_INTERFACE: {
          //sitter_log("  INTERFACE descriptor len=0x%x @ 0x%x",bLength,offset);
          uint8_t bInterfaceNumber=0,bAlternateSetting=0,bNumEndpoints=0;
          uint8_t bInterfaceClass=0,bInterfaceSubClass=0,bInterfaceProtocol=0;
          uint8_t iInterface=0;
          if (bLength>=3) { bInterfaceNumber   =sitter_get8(buf,offset+2);
          if (bLength>=4) { bAlternateSetting  =sitter_get8(buf,offset+3);
          if (bLength>=5) { bNumEndpoints      =sitter_get8(buf,offset+4);
          if (bLength>=6) { bInterfaceClass    =sitter_get8(buf,offset+5);
          if (bLength>=7) { bInterfaceSubClass =sitter_get8(buf,offset+6);
          if (bLength>=8) { bInterfaceProtocol =sitter_get8(buf,offset+7);
          if (bLength>=9) { iInterface         =sitter_get8(buf,offset+8);
          }}}}}}}
          if (!cls|| // take this interface if we don't have one yet
              ((bInterfaceClass==USB_CLASS_HID)&&(bInterfaceSubClass==0x01))|| // take it if it's boot protocol HID
              0) {
            cls=bInterfaceClass;
            subcls=bInterfaceSubClass;
            proto=bInterfaceProtocol;
            intf_id=bInterfaceNumber;
            alt_id=bAlternateSetting;
            ep_in=ep_out=0;
            rdep=true;
            //sitter_log("    (using this interface)");
          } else rdep=false;
          // TEMP dump:
          #if 0
            char *intfname=iInterface?readStringDescriptor(iInterface):NULL;
            sitter_log("    bInterfaceNumber: 0x%02x",bInterfaceNumber);
            sitter_log("    bAlternateSetting: 0x%02x",bAlternateSetting);
            sitter_log("    bNumEndpoints: %d",bNumEndpoints);
            sitter_log("    class: %02x/%02x/%02x",bInterfaceClass,bInterfaceSubClass,bInterfaceProtocol);
            sitter_log("    iInterface: %02x '%s'",iInterface,intfname?intfname:"");
            if (intfname) free(intfname);
          #endif
        } break;
        
      case USB_DESC_ENDPOINT: {
          //sitter_log("  ENDPOINT descriptor len=0x%x @ 0x%x",bLength,offset);
          if (!rdep) break;
          uint8_t bEndpointAddress=0,bmAttributes=0,bInterval=0;
          uint16_t wMaxPacketSize=0;
          if (bLength>=3) { bEndpointAddress=sitter_get8   (buf,offset+2);
          if (bLength>=4) { bmAttributes    =sitter_get8   (buf,offset+3);
          if (bLength>=6) { wMaxPacketSize  =sitter_get16le(buf,offset+4);
          if (bLength>=7) { bInterval       =sitter_get8   (buf,offset+6);
          }}}}
          if (bEndpointAddress&0x80) { // input endpoint
            if (!ep_in) {
              switch (bmAttributes&0x03) {
                case 0x00: break; // CONTROL
                case 0x01: break; // ISOCHRONOUS
                case 0x02: { // BULK
                    ep_in=bEndpointAddress;
                    packetsize_in=wMaxPacketSize;
                    ioctl_in=USB_IOCTL_BLKMSG;
                  } break;
                case 0x03: { // INTERRUPT
                    ep_in=bEndpointAddress;
                    packetsize_in=wMaxPacketSize;
                    ioctl_in=USB_IOCTL_INTRMSG;
                  } break;
              }
            }
          } else { // output endpoint
            if (!ep_out) {
              switch (bmAttributes&0x03) {
                case 0x00: break; // CONTROL
                case 0x01: break; // ISOCHRONOUS
                case 0x02: { // BULK
                    ep_out=bEndpointAddress;
                    packetsize_out=wMaxPacketSize;
                    ioctl_out=USB_IOCTL_BLKMSG;
                  } break;
                case 0x03: { // INTERRUPT
                    ep_out=bEndpointAddress;
                    packetsize_out=wMaxPacketSize;
                    ioctl_out=USB_IOCTL_INTRMSG;
                  } break;
              }
            }
          }
          // TEMP dump:
          #if 0
            const char *xfertype,*synctype,*usagetype;
            switch (bmAttributes&0x03) {
              case 0x00: xfertype="CONTROL"; break;
              case 0x01: xfertype="ISOCHRONOUS"; break;
              case 0x02: xfertype="BULK"; break;
              case 0x03: xfertype="INTERRUPT"; break;
            }
            switch (bmAttributes&0x0c) {
              case 0x00: synctype="NO-SYNC"; break;
              case 0x04: synctype="ASYNCH"; break;
              case 0x08: synctype="ADAPTIVE"; break;
              case 0x0c: synctype="SYNCH"; break;
            }
            switch (bmAttributes&0x30) {
              case 0x00: usagetype="DATA"; break;
              case 0x10: usagetype="FEEDBACK"; break;
              case 0x20: usagetype="IMPLICIT-FEEDBACK-DATA"; break;
              case 0x30: usagetype="RESERVED-USAGE"; break;
            }
            sitter_log("    bEndpointAddress: 0x%02x",bEndpointAddress);
            sitter_log("    bmAttributes: 0x%02x %s %s %s",bmAttributes,xfertype,synctype,usagetype);
            sitter_log("    wMaxPacketSize: %d",wMaxPacketSize);
            sitter_log("    bInterval: %d",bInterval);
          #endif
        } break;
        
      default: ;//sitter_log("  ignoring descriptor type=0x%02x len=0x%02x offset=0x%x",bDescriptorType,bLength,offset);
    }
    if (!bLength) {
      //sitter_log("  bLength=0, offset=0x%x total=0x%x, terminate early",offset,bufc);
      break;
    }
    offset+=bLength;
  }
}

/******************************************************************************
 * Device read string
 *****************************************************************************/
 
char *WiiUSB_Device::readStringDescriptor(uint8_t strid,uint16_t lang) {
  //sitter_log("%p readStringDescriptor(0x%02x,0x%04x)...",this,strid,lang);
  uint8_t rawbuf[256];
  int desclen=io_control(0x80,USB_REQ_GET_DESCRIPTOR,(USB_DESC_STRING<<8)|strid,lang,256,rawbuf);
  //sitter_log("    string descriptor length = %d",desclen);
  //if (desclen>=2) sitter_log("    string descriptor [:2] = 0x%02x 0x%02x",rawbuf[0],rawbuf[1]);
  if (rawbuf[0]<desclen) desclen=rawbuf[0]; // when IOS and the device disagree about descriptor's length, trust the device
  if ((desclen<4)||(rawbuf[0]!=desclen)||(rawbuf[1]!=USB_DESC_STRING)) return NULL;
  /* examine string, determine length in UTF-8 (but don't actually convert yet) */
  bool lead=true;
  int offset=2;
  int utf8len=0;
  int utf8start=2;
  int utf16start=2;
  bool expect_surrogate=false;
  #define ILLEGAL { \
        sitter_log("%p WiiUSB_Device: illegal UTF-16 in string 0x%02x:0x%04x. ignoring whole string",this,strid,lang); \
        return NULL; \
      }
  while (offset<desclen) {
    uint8_t blo=rawbuf[offset++];
    uint8_t bhi=rawbuf[offset++];
    if ((bhi==0x00)&&(blo==0x20)) {
      if (expect_surrogate) ILLEGAL
      if (lead) {
        utf8start+=2;
        continue;
      }
    } else lead=false;
    if (expect_surrogate) {
      if ((bhi<0xdc)||(bhi>=0xe0)) ILLEGAL
      utf8len+=4;
      expect_surrogate=false;
    } else {
      if ((bhi>=0xd8)&&(bhi<0xdc)) expect_surrogate=true;
      else if ((bhi>=0xdc)&&(bhi<0xe0)) ILLEGAL
      else {
        if (!bhi) {
          if (blo&0x80) utf8len+=2;
          else utf8len+=1;
        } else {
          if (blo&0xf8) utf8len+=3;
          else utf8len+=2;
        }
      }
    }
  }
  #undef ILLEGAL
  /* convert to UTF-8 */
  //sitter_log("    UTF-8 length = %d",utf8len);
  uint8_t *utf8=(uint8_t*)malloc(utf8len+1);
  if (!utf8) sitter_throw(AllocationError,"");
  int utf8p=0;
  offset=utf8start;
  while (offset<desclen) {
    uint8_t blo=rawbuf[offset++];
    uint8_t bhi=rawbuf[offset++];
    uint32_t ch;
    if ((bhi>=0xd8)&&(bhi<0xdc)) {
      uint8_t blo2=rawbuf[offset++];
      uint8_t bhi2=rawbuf[offset++];
      ch=0x10000+(blo2|((bhi2&3)<<8)|(blo<<10)|((bhi&3)<<18));
    } else ch=blo|(bhi<<8);
    if (ch&0xffff0000) {
      utf8[utf8p++]=0xf0|(ch>>18);
      utf8[utf8p++]=0x80|((ch>>12)&0x3f);
      utf8[utf8p++]=0x80|((ch>>6)&0x3f);
      utf8[utf8p++]=0x80|(ch&0x3f);
    } else if (ch&0x0000f800) {
      utf8[utf8p++]=0xe0|(ch>>12);
      utf8[utf8p++]=0x80|((ch>>6)&0x3f);
      utf8[utf8p++]=0x80|(ch&0x3f);
    } else if (ch&0x00000780) {
      utf8[utf8p++]=0xc0|(ch>>6);
      utf8[utf8p++]=0x80|(ch&0x3f);
    } else utf8[utf8p++]=ch;
  }
  while (utf8len&&(utf8[utf8len-1]==' ')) utf8len--;
  utf8[utf8len]=0;
  return (char*)utf8;
}

/******************************************************************************
 * Device Control I/O
 *****************************************************************************/
 
int WiiUSB_Device::io_control(uint8_t bmRequestType,uint8_t bRequest,uint16_t wValue,uint16_t wIndex,uint16_t wLength,void *data) {
  if (fd<0) sitter_throw(IOError,"WiiUSB_Device::io_control, file not open (fd=%d,vid=%x,pid=%x)",fd,vid,pid);
  
  /* create argv if necessary */
  if (!argv_ctl) {
    if (!(argv_ctl=(ioctlv*)memalign(32,sizeof(ioctlv)*ARGC_CTL))) sitter_throw(AllocationError,"");
    #define ARG(i,sz) { \
      if (!(argv_ctl[i].data=memalign(32,sz))) sitter_throw(AllocationError,""); \
      argv_ctl[i].len=sz; }
    ARG(0,1) // bmRequestType
    ARG(1,1) // bRequest
    ARG(2,2) // wValue
    ARG(3,2) // wIndex
    ARG(4,2) // wLength
    ARG(5,1) // mystery byte
    ((uint8_t*)(argv_ctl[5].data))[0]=0; // constant
    #undef ARG
  }
  
  /* populate argv (header) */
  sitter_set8   (argv_ctl[0].data,0,bmRequestType);
  sitter_set8   (argv_ctl[1].data,0,bRequest);
  sitter_set16le(argv_ctl[2].data,0,wValue);
  sitter_set16le(argv_ctl[3].data,0,wIndex);
  sitter_set16le(argv_ctl[4].data,0,wLength);
  
  /* populate argv (payload) */
  bool mybuffer=false;
  void *usebuffer=data;
  if ((((uint32_t)data)&0x1f)||(wLength&0x1f)) { // addr or len not multiple of 32, use my aligned buffer
    mybuffer=true;
    int reqlen=(wLength+31)&0x7fffffe0; // next multiple of 32
    if (reqlen>buf_ctl_a) {
      if (buf_ctl) free(buf_ctl);
      if (!(buf_ctl=memalign(32,reqlen))) sitter_throw(AllocationError,"");
      buf_ctl_a=reqlen;
    }
    usebuffer=buf_ctl;
    if (wLength) memcpy(buf_ctl,data,wLength);
  }
  argv_ctl[6].data=usebuffer;
  argv_ctl[6].len=wLength;
  
  /* push to IOS, sleep until it returns */
  for (int i=0;i<ARGC_CTL;i++) DCFlushRange(argv_ctl[i].data,argv_ctl[i].len); // TODO necessary?
  int rtn=IOS_Ioctlv(fd,USB_IOCTL_CTRLMSG,6,1,argv_ctl);
  if (rtn>0) DCInvalidateRange(usebuffer,rtn); // TODO necessary?
  
  /* transfer back to user's buffer if necessary */
  if ((rtn>0)&&mybuffer) {
    int cplen=rtn; if (rtn>wLength) cplen=wLength;
    if (cplen>0) memcpy(data,buf_ctl,cplen);
  }
  
  // TODO -- should we throw exceptions for <0 result?
  return rtn;
  
}
 
/******************************************************************************
 * Device Bulk/Interrupt I/O
 *****************************************************************************/
 
int WiiUSB_Device::io_bulkint(int xfertype,uint8_t bmEndpoint,uint16_t wLength,void *data) {
  //sitter_log("%p io_bulkint(%d,0x%02x,%d,%p)... fd=%d",this,xfertype,bmEndpoint,wLength,data,fd);
  if (fd<0) sitter_throw(IOError,"WiiUSB_Device::io_bulkint, file not open (fd=%d,vid=%x,pid=%x)",fd,vid,pid);
  if ((xfertype!=USB_IOCTL_BLKMSG)&&(xfertype!=USB_IOCTL_INTRMSG))
    sitter_throw(ArgumentError,"USB bulk/interrupt I/O: bad ioctl %d (bulk=%d,interrupt=%d)",
    xfertype,USB_IOCTL_BLKMSG,USB_IOCTL_INTRMSG);
  
  /* create argv if necessary */
  if (!argv_bulkint) {
    if (!(argv_bulkint=(ioctlv*)memalign(32,sizeof(ioctlv)*ARGC_BULKINT))) sitter_throw(AllocationError,"");
    #define ARG(i,sz) { \
      if (!(argv_bulkint[i].data=memalign(32,sz))) sitter_throw(AllocationError,""); \
      argv_bulkint[i].len=sz; }
    ARG(0,1) // bmEndpoint
    ARG(1,2) // wLength
    #undef ARG
  }
  
  /* populate argv (header) */
  sitter_set8   (argv_bulkint[0].data,0,bmEndpoint);
  sitter_set16be(argv_bulkint[1].data,0,wLength); // big-endian. weird, right?
  
  /* populate argv (payload) */
  bool mybuffer=false;
  void *usebuffer=data;
  if ((((uint32_t)data)&0x1f)||(wLength&0x1f)) { // addr or len not multiple of 32, use my aligned buffer
    mybuffer=true;
    int reqlen=(wLength+31)&0x7fffffe0; // next multiple of 32
    if (reqlen>buf_bulkint_a) {
      if (buf_bulkint) free(buf_bulkint);
      if (!(buf_bulkint=memalign(32,reqlen))) sitter_throw(AllocationError,"");
      buf_bulkint_a=reqlen;
    }
    usebuffer=buf_bulkint;
    if (wLength) memcpy(buf_bulkint,data,wLength);
  }
  argv_bulkint[2].data=usebuffer;
  argv_bulkint[2].len=wLength;
  
  /* push to IOS, sleep until it returns */
  //for (int i=0;i<ARGC_BULKINT;i++) DCFlushRange(argv_bulkint[i].data,argv_bulkint[i].len); // TODO necessary?
  DCFlushRange(argv_bulkint,sizeof(ioctlv)*ARGC_BULKINT); // TODO necessary?
  int rtn=IOS_Ioctlv(fd,xfertype,2,1,argv_bulkint);
  //if (rtn>0) DCInvalidateRange(usebuffer,rtn); // TODO necessary?
  
  /* transfer back to user's buffer if necessary */
  if ((rtn>0)&&mybuffer) {
    int cplen=rtn; if (rtn>wLength) cplen=wLength;
    if (cplen>0) memcpy(data,buf_bulkint,cplen);
  }
  
  // TODO -- should we throw exceptions for <0 result?
  //sitter_log("...io_bulkint = %d",rtn);
  return rtn;
}

#endif
