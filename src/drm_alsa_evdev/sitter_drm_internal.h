#ifndef SITTER_DRM_INTERNAL_H
#define SITTER_DRM_INTERNAL_H

#include "sitter_drm.h"
#include <cstdlib>
#include <cstdio>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct sitter_drm_fb {
  uint32_t fbid;
  int handle;
  int size;
};

struct sitter_drm {
  int fd;
  
  int mmw,mmh; // monitor's physical size
  int w,h; // monitor's logical size in pixels
  int rate; // monitor's refresh rate in hertz
  drmModeModeInfo mode; // ...and more in that line
  uint32_t connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  
  int flip_pending;
  struct sitter_drm_fb fbv[2];
  int fbp;
  struct gbm_bo *bo_pending;
  int crtcunset;
  
  struct gbm_device *gbmdevice;
  struct gbm_surface *gbmsurface;
  EGLDisplay egldisplay;
  EGLContext eglcontext;
  EGLSurface eglsurface;
};

int sitter_drm_open_file(struct sitter_drm *driver,const char *path);
int sitter_drm_configure(struct sitter_drm *driver);
int sitter_drm_init_gx(struct sitter_drm *driver);

#endif
