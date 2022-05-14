#include "sitter_drm_internal.h"

/* Quit.
 */
 
static void sitter_drm_fb_cleanup(struct sitter_drm *driver,struct sitter_drm_fb *fb) {
  if (fb->fbid) {
    drmModeRmFB(driver->fd,fb->fbid);
  }
}

void sitter_drm_quit(struct sitter_drm *driver) {
  if (!driver) return;

  // If waiting for a page flip, we must let it finish first.
  if ((driver->fd>=0)&&driver->flip_pending) {
    struct pollfd pollfd={.fd=driver->fd,.events=POLLIN|POLLERR|POLLHUP};
    if (poll(&pollfd,1,500)>0) {
      char dummy[64];
      read(driver->fd,dummy,sizeof(dummy));
    }
  }
  
  if (driver->eglcontext) eglDestroyContext(driver->egldisplay,driver->eglcontext);
  if (driver->eglsurface) eglDestroySurface(driver->egldisplay,driver->eglsurface);
  if (driver->egldisplay) eglTerminate(driver->egldisplay);
  
  sitter_drm_fb_cleanup(driver,driver->fbv+0);
  sitter_drm_fb_cleanup(driver,driver->fbv+1);
  
  if (driver->crtc_restore&&(driver->fd>=0)) {
    drmModeCrtcPtr crtc=driver->crtc_restore;
    drmModeSetCrtc(
      driver->fd,crtc->crtc_id,crtc->buffer_id,
      crtc->x,crtc->y,&driver->connid,1,&crtc->mode
    );
    drmModeFreeCrtc(driver->crtc_restore);
  }
  
  if (driver->fd>=0) close(driver->fd);
  
  free(driver);
}

/* Init.
 */

struct sitter_drm *sitter_drm_init() {
  struct sitter_drm *driver=(struct sitter_drm*)calloc(1,sizeof(struct sitter_drm));
  if (!driver) return 0;
  
  driver->fd=-1;
  driver->crtcunset=1;

  if (!drmAvailable()) {
    fprintf(stderr,"DRM not available.\n");
    sitter_drm_quit(driver);
    return 0;
  }
  
  if (
    (sitter_drm_open_file(driver)<0)||
    (sitter_drm_configure(driver)<0)||
    (sitter_drm_init_gx(driver)<0)||
  0) {
    sitter_drm_quit(driver);
    return 0;
  }

  return driver;
}

/* Poll file.
 */
 
static void drm_cb_vblank(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {}
 
static void drm_cb_page1(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,void *userdata
) {
  struct sitter_drm *driver=(struct sitter_drm*)userdata;
  driver->flip_pending=0;
}
 
static void drm_cb_page2(
  int fd,unsigned int seq,unsigned int times,unsigned int timeus,unsigned int ctrcid,void *userdata
) {
  drm_cb_page1(fd,seq,times,timeus,userdata);
}
 
static void drm_cb_seq(
  int fd,uint64_t seq,uint64_t timeus,uint64_t userdata
) {}
 
static int sitter_drm_poll_file(struct sitter_drm *driver,int to_ms) {
  struct pollfd pollfd={.fd=driver->fd,.events=POLLIN};
  if (poll(&pollfd,1,to_ms)<=0) return 0;
  drmEventContext ctx={
    .version=DRM_EVENT_CONTEXT_VERSION,
    .vblank_handler=drm_cb_vblank,
    .page_flip_handler=drm_cb_page1,
    .page_flip_handler2=drm_cb_page2,
    .sequence_handler=drm_cb_seq,
  };
  int err=drmHandleEvent(driver->fd,&ctx);
  if (err<0) return -1;
  return 0;
}

/* Swap, the EGL part.
 */
 
static int sitter_drm_swap_egl(uint32_t *fbid,struct sitter_drm *driver) { 
  eglSwapBuffers(driver->egldisplay,driver->eglsurface);
  
  struct gbm_bo *bo=gbm_surface_lock_front_buffer(driver->gbmsurface);
  if (!bo) return -1;
  
  int handle=gbm_bo_get_handle(bo).u32;
  struct sitter_drm_fb *fb;
  if (!driver->fbv[0].handle) {
    fb=driver->fbv;
  } else if (handle==driver->fbv[0].handle) {
    fb=driver->fbv;
  } else {
    fb=driver->fbv+1;
  }
  
  if (!fb->fbid) {
    int width=gbm_bo_get_width(bo);
    int height=gbm_bo_get_height(bo);
    int stride=gbm_bo_get_stride(bo);
    fb->handle=handle;
    if (drmModeAddFB(driver->fd,width,height,24,32,stride,fb->handle,&fb->fbid)<0) return -1;
    
    if (driver->crtcunset) {
      if (drmModeSetCrtc(
        driver->fd,driver->crtcid,fb->fbid,0,0,
        &driver->connid,1,&driver->mode
      )<0) {
        fprintf(stderr,"drmModeSetCrtc: %m\n");
        return -1;
      }
      driver->crtcunset=0;
    }
  }
  
  *fbid=fb->fbid;
  if (driver->bo_pending) {
    gbm_surface_release_buffer(driver->gbmsurface,driver->bo_pending);
  }
  driver->bo_pending=bo;
  
  return 0;
}

/* Let the current swap finish.
 */
 
int sitter_drm_drain(struct sitter_drm *driver) {
  if (driver->flip_pending) {
    if (sitter_drm_poll_file(driver,20)<0) return -1;
  }
  return 0;
}

/* Swap.
 */

int sitter_drm_swap(struct sitter_drm *driver) {

  // There must be no more than one page flip in flight at a time.
  // If one is pending -- likely -- give it a chance to finish.
  //...update: Actually not likely anymore, since I added drain(). But do this anyway to be safe.
  if (driver->flip_pending) {
    if (sitter_drm_poll_file(driver,20)<0) return -1;
    if (driver->flip_pending) {
      // Page flip didn't complete after a 20 ms delay? Drop the frame, no worries.
      return 0;
    }
  }
  driver->flip_pending=1;
  
  uint32_t fbid=0;
  if (sitter_drm_swap_egl(&fbid,driver)<0) {
    driver->flip_pending=0;
    return -1;
  }
  
  if (drmModePageFlip(driver->fd,driver->crtcid,fbid,DRM_MODE_PAGE_FLIP_EVENT,driver)<0) {
    fprintf(stderr,"drmModePageFlip: %m\n");
    driver->flip_pending=0;
    return -1;
  }
  
  return 0;
}

/* Trivial accessors.
 */

int sitter_drm_get_width(const struct sitter_drm *driver) {
  return driver->w;
}

int sitter_drm_get_height(const struct sitter_drm *driver) {
  return driver->h;
}
