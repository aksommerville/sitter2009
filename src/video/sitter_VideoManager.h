#ifdef SITTER_WII
  #include "sitter_VideoManager_wii.h"
#else

#ifndef SITTER_VIDEOMANAGER_H
#define SITTER_VIDEOMANAGER_H

/* For portability's sake, please don't make any SDL or OpenGL calls outside VideoManager.
 * (except for input, audio.... stuff that SDL *is* doing).
 */

#include <stdint.h>
#ifdef SITTER_WIN32
  typedef int iconv_t; // wtf, ms?
  #include <SDL/SDL.h>
  #define GLAPI __stdcall
  #include <GL/gl.h>
#elif defined SITTER_LINUX_DRM
  #include "drm_alsa_evdev/sitter_drm.h"
  #include <GL/gl.h>
#else
  #include <SDL/SDL.h>
  #include <GL/gl.h>
#endif
#include "sitter_Rect.h"

class Game;
class Grid;
class SpriteGroup;
class Menu;
//class EdgeMap;

/* init flags */
#define SITTER_VIDEO_FULLSCREEN    0x00000001
#define SITTER_VIDEO_CHOOSEDIMS    0x00000002

/* texture flags */
#define SITTER_TEX_FILTER     0x00000001

class VideoManager {

  Game *game;
  #if defined(SITTER_LINUX_DRM)
    struct sitter_drm *drm;
  #else
    SDL_Surface *screen;
  #endif
  
  bool debug_goals;
  
  typedef struct {
    uint32_t  resnamehash; // assuming that all hashes will be unique... maybe not safe
    char *resname;
    int resnamelen;
    int texid,refc;
  } TextureReference;
  TextureReference *texv; int texc,texa; // sorted by hash
  
  void *gxtexbuf; int gxtexbuflen;
  bool force_power_of_two; // if a texture has not power-of-two dimensions, scale it up (expensive, only use if you need it)
  void *scalebuf; int scalebuflen;
  void scaleTexture(const void *src,int fmt,int w,int h,int dstw,int dsth); // output to scalebuf
  
  #define SITTER_HIGHSCORE_FORMAT_BUFFER_LEN 8192
  char highscore_format_buffer[SITTER_HIGHSCORE_FORMAT_BUFFER_LEN];
  
  int _loadTexture(int fmt,int w,int h,const void *pixels,int flags=SITTER_TEX_FILTER,int texid=-1);
  
public:

  SpriteGroup *spr_vis; // not own
  Menu **menuv; int menuc,menua; // own list, not menus; draw in order, ie menuv[menuc-1] is on top
  bool highscore_dirty;

  VideoManager(Game *game,int w,int h,int flags);
  ~VideoManager();
  
  void reconfigure();
  
  #ifdef SITTER_LINUX_DRM
    #define SITTER_W_LIMIT 800
    #define SITTER_H_LIMIT 600
    int getScreenWidth() const {
      int realw=sitter_drm_get_width(drm);
      if (realw<SITTER_W_LIMIT) return realw;
      return SITTER_W_LIMIT;
    }
    int getScreenHeight() const {
      int realh=sitter_drm_get_height(drm);
      if (realh<SITTER_H_LIMIT) return realh;
      return SITTER_H_LIMIT;
    }
    Rect getBounds() const {
      int realw=sitter_drm_get_width(drm);
      int realh=sitter_drm_get_height(drm);
      int w=(realw>=SITTER_W_LIMIT)?SITTER_W_LIMIT:realw;
      int h=(realh>=SITTER_H_LIMIT)?SITTER_H_LIMIT:realh;
      return Rect((realw>>1)-(w>>1),(realh>>1)-(h>>1),w,h);
    }
  #else
    int getScreenWidth() const { return screen->w; }
    int getScreenHeight() const { return screen->h; }
    Rect getBounds() const { return Rect(screen->w,screen->h); }
  #endif
  
  void update();
  void draw();
  
  void pushMenu(Menu *menu);
  Menu *popMenu() { if (!menuc) return 0; return menuv[--menuc]; }
  Menu *getActiveMenu() const { if (!menuc) return 0; return menuv[menuc-1]; }
  
  /* <safe> means ignore missing files -- they'll get logged by the error-thrower.
   * It does not protect against allocation or other failures; those are *real* problems.
   */
  int loadTexture(const char *resname,bool safe=false,int flags=SITTER_TEX_FILTER);
  bool isTexture(int texid) { return glIsTexture(texid); }
  void unloadTexture(int texid); // guaranteed not to throw exceptions
  const char *texidToResname(int texid) const { for (int i=0;i<texc;i++) if (texid==texv[i].texid) return texv[i].resname; return NULL; }
  
  /* draw, second level */
  void drawGrid(Grid *grid);
  void drawSprites(SpriteGroup *grp);
  void drawControl();
  void drawEditorDecorations();
  void drawHighScores();
  void drawMenu(Menu *menu);
  void drawMenuItems(Menu *menu,int itemp,int itemc,int bx,int by,int bw);
  void drawScrollBar(int texid,int x,int y,int w,int h,int btnh,int pos,int limit,int range);
  void drawBlotter(uint32_t rgba);
  void drawRadar();
  void drawHAKeyboard();
  
  /* draw, third level
   * Centering a string with drawBoundedString only centers the block -- lines are still left-justified.
   */
  void drawString(int x,int y,const char *s,int texid,uint32_t fgclr,int chw,int chh);
  bool drawBoundedString(int x,int y,int w,int h,const char *s,int texid,uint32_t fgclr,int chw,int chh,bool xctr,bool yctr);
  void measureString(const char *s,int *maxw,int *h);
  
};

#endif
#endif
