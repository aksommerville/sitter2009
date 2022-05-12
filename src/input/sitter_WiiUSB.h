#ifdef SITTER_WII

#ifndef SITTER_WIIUSB_H
#define SITTER_WIIUSB_H

#include <stdint.h>
#include <gccore.h>

class InputManager;
class WiiUSB_Device;

/* from libogc/usb.c: */
#define USB_IOCTL_CTRLMSG          0
#define USB_IOCTL_BLKMSG           1
#define USB_IOCTL_INTRMSG          2
#define USB_IOCTL_SUSPENDDEV       5
#define USB_IOCTL_RESUMEDEV        6
#define USB_IOCTL_GETDEVLIST      12
#define USB_IOCTL_DEVREMOVALHOOK  26
#define USB_IOCTL_DEVINSERTHOOK   27

/* from USB 2.0 standard: */
#define USB_REQ_GET_STATUS              0
#define USB_REQ_CLEAR_FEATURE           1
#define USB_REQ_SET_FEATURE             2
#define USB_REQ_SET_ADDRESS             5
#define USB_REQ_GET_DESCRIPTOR          6
#define USB_REQ_SET_DESCRIPTOR          7
#define USB_REQ_GET_CONFIGURATION       8
#define USB_REQ_SET_CONFIGURATION       9
#define USB_REQ_GET_INTERFACE          10
#define USB_REQ_SET_INTERFACE          11
#define USB_REQ_SYNCH_FRAME            12

/* from USB 2.0 standard: */
#define USB_DESC_DEVICE               1
#define USB_DESC_CONFIGURATION        2
#define USB_DESC_STRING               3
#define USB_DESC_INTERFACE            4
#define USB_DESC_ENDPOINT             5
#define USB_DESC_DEV_QUAL             6 // DEVICE_QUALIFIER
#define USB_DESC_OSPEED_CFG           7 // OTHER_SPEED_CONFIGURATION
#define USB_DESC_INTERFACE_POWER      8

/* from USB 2.0 standard: (uninteresting values omitted) */
#define USB_CLASS_HID            0x03
#define USB_CLASS_PRINTER        0x07
#define USB_CLASS_STORAGE        0x08
#define USB_CLASS_HUB            0x09
#define USB_CLASS_XBOX           0x58 // nonstandard
#define USB_CLASS_WIRELESS       0xe0

/******************************************************************************
 * HCI
 * Though it appears here to be independant of the InputManager, it is very much
 * not. Only InputManager should call scan(), open(), or removeDevice().
 *****************************************************************************/

class WiiUSB_HCI {
public:

  InputManager *mgr;
  WiiUSB_Device **devv; int devc,deva;
  char *path;
  
  typedef struct {
    uint16_t vid,pid;
  } USBAddress;
  USBAddress *addrv; int addrc,addra;

  WiiUSB_HCI(InputManager *mgr,const char *path);
  ~WiiUSB_HCI();
  
  void scan();
  
  /* HCI owns its devices. Never delete a device opened with WiiUSB_HCI::open.
   * Calling removeDevice() will delete it.
   */
  WiiUSB_Device *open(uint16_t vid,uint16_t pid);
  WiiUSB_Device *findByAddress(uint16_t vid,uint16_t pid) const;
  WiiUSB_Device *findBySerialNumber(const char *serial) const; // serial = "%04x%04x:%s",vid,pid,serial
  WiiUSB_Device *findByName(const char *name) const; // name = "%04x%04x:%s %s",vid,pid,vname,pname
  void removeDevice(WiiUSB_Device *dev);
  
};

/******************************************************************************
 * Device
 *****************************************************************************/
 
class WiiUSB_Device {
friend class WiiUSB_HCI;
friend void *sitter_wiiusb_listen_thread(void*);
private:

  ioctlv *argv_ctl;
  ioctlv *argv_bulkint;
  void *buf_ctl; int buf_ctl_a;
  void *buf_bulkint; int buf_bulkint_a;
  
  lwp_t listenthd;
  volatile int *thdstatus;
  void *thdbuf,*rptbuf,*prevrptbuf;

  void dev_open();
  void dev_close();
  void read_main_descriptors();
  void read_setup_descriptors(void *buf,int buflen,int descid,int index);
  
  ~WiiUSB_Device(); // only WiiUSB_HCI is allowed to delete; you must use WiiUSB_HCI::removeDevice.
  
public:

  WiiUSB_HCI *hci;
  int fd;
  uint16_t vid,pid;
  char *vname,*pname,*serial; // UTF-8, vname and pname have leading and trailing space stripped, otherwise verbatim
  
  // we choose an interface at init and stick with it
  uint8_t cfg_id,intf_id,alt_id;
  int cfgc; bool got_cfg_id;
  uint8_t cls,subcls,proto;
  // no device we use will use more than two endpoints. actually, we only need one.
  uint8_t ep_in,ep_out;
  int ioctl_in,ioctl_out;
  int packetsize_in,packetsize_out;
  
  /* I set this to zero at init and never look at it again.
   * InputManager may use it to describe report type.
   */
  int input_driver;
  
  WiiUSB_Device(WiiUSB_HCI *hci,uint16_t vid,uint16_t pid);
  
  /* name is "VENDOR PRODUCT". Compare against my vendor and product strings, 
   * return 0 if equivalent, <0 if I sort first, >0 if it sorts first.
   * Excess space is ignored. (tab, linefeed, etc do not count as space).
   */
  int namecmp(const char *name) const;
  
  /* xfertype is USB_IOCTL_BLKMSG or USB_IOCTL_INTRMSG.
   */
  int io_control(uint8_t bmRequestType,uint8_t bRequest,uint16_t wValue,uint16_t wIndex,uint16_t wLength,void *data);
  int io_bulkint(int xfertype,uint8_t bmEndpoint,uint16_t wLength,void *data);
  
  /* High-level string descriptor access.
   * Read the descriptor, convert to UTF-8, remove leading and trailing space.
   * Return NULL for any error.
   * String is newly allocated. May return empty strings.
   */
  char *readStringDescriptor(uint8_t strid,uint16_t lang=0);
  
  /* Once beginListenThread is called, do not try any I/O.
   * Call getReport to see the last input from the listen thread. NULL if there's nothing new.
   */
  void beginListenThread();
  const void *getReport();
  const void *getPreviousReport(); // call after getReport(), it returns not the one you're looking at but the one before
  
};

#endif
#endif
