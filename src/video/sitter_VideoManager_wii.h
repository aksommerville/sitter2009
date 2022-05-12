#ifdef SITTER_WII
#ifndef SITTER_VIDEOMANAGER_H
#define SITTER_VIDEOMANAGER_H

#include <stdint.h>
#include <gccore.h>
#include "sitter_Rect.h"

class Game;
class Grid;
class SpriteGroup;
class Menu;

/* init flags */
#define SITTER_VIDEO_FULLSCREEN    0x00000001
#define SITTER_VIDEO_CHOOSEDIMS    0x00000002

/* texture flags */
#define SITTER_TEX_FILTER     0x00000001

class VideoManager {

  Game *game;
  GXRModeObj *rmode;
  void *xfb[2];
  int whichfb;
  void *gxfifo;
  
  bool debug_goals;
  
  #if 0 // texture, first attempt
  typedef struct {
    uint32_t resnamehash; // assuming that all hashes will be unique... maybe not safe
    char *resname;
    int resnamelen;
    int texid,refc;
    GXTexObj gxobj;
    void *pixels;
    int pixelslen;
  } TextureReference;
  TextureReference *texv; int texc,texa; // sorted by hash
  int nexttexid;
  int _loadTexture(int fmt,int w,int h,const void *pixels,int flags=SITTER_TEX_FILTER,int texid=-1);
  void bindTexture(int texid);
  int allocateTexture(int *ti=0); // ti holds texv index
  int findTexture(int texid); // return index into texv, or -1
  #endif
  
  typedef struct {
    bool valid;
    uint32_t resnamehash;
    char *resname;
    void *pixels;
    int pixelslen;
    int fmt,w,h;
    int refc;
    GXTexObj gxobj;
  } TextureReference;
  TextureReference *texv; int texc; // sort by name, texc is alloc count, not all slots are necessarily used
  bool findTextureByName(const char *resname,int *texid) const;
  void insertTextureReference(int texid,const char *resname); // texid comes from findTextureByName
  void bindTexture(int texid) {
    if ((texid>=0)&&(texid<texc)&&texv[texid].valid) GX_LoadTexObj(&(texv[texid].gxobj),GX_TEXMAP0); 
  }
  
  // The actual screen dimensions are in rmode.
  // These are the dimensions of the projection, and the dimensions reported outward.
  int vscreenw,vscreenh;
  
  void tevTextureOnly();
  void tevRasterOnly();
  void tevBlend(); // default, multiply texture by rasteriser color
  void tevSprite(); // tint texture towards rasteriser color, multiply alpha by tev register 0
  void tevTextureSegment(); // blend with fractional texcoord
  
  #define SITTER_HIGHSCORE_FORMAT_BUFFER_LEN 8192
  char highscore_format_buffer[SITTER_HIGHSCORE_FORMAT_BUFFER_LEN];
  
  void viewportFull();
  
public:

  SpriteGroup *spr_vis; // not own
  Menu **menuv; int menuc,menua; // own list, not menus; draw in order, ie menuv[menuc-1] is on top
  bool highscore_dirty;

  VideoManager(Game *game,int w,int h,int flags);
  ~VideoManager();
  
  void reconfigure();
  
  int getScreenWidth() const { return vscreenw; } //rmode->fbWidth; }
  int getScreenHeight() const { return vscreenh; } //rmode->efbHeight; }
  Rect getBounds() const { return Rect(vscreenw,vscreenh); } //Rect(rmode->fbWidth,rmode->efbHeight); }
  
  void update();
  void draw();
  
  void pushMenu(Menu *menu);
  Menu *popMenu() { if (!menuc) return 0; return menuv[--menuc]; }
  Menu *getActiveMenu() const { if (!menuc) return 0; return menuv[menuc-1]; }
  
  /* <safe> means ignore missing files -- they'll get logged by the error-thrower.
   * It does not protect against allocation or other failures; those are *real* problems.
   */
  int loadTexture(const char *resname,bool safe=false,int flags=SITTER_TEX_FILTER);
  bool isTexture(int texid) const { return ((texid>=0)&&(texid<texc)&&(texv[texid].refc>0)); }
  void unloadTexture(int texid); // guaranteed not to throw exceptions
  const char *texidToResname(int texid) const { if ((texid>=0)&&(texid<texc)) return texv[texid].resname; return NULL; }
  
  /* draw, second level */
  void drawGrid(Grid *grid);
  void drawSprites(SpriteGroup *grp);
  void drawControl();
  void drawEditorDecorations();
  void drawMenu(Menu *menu);
  void drawMenuItems(Menu *menu,int itemp,int itemc,int bx,int by,int bw);
  void drawScrollBar(int texid,int x,int y,int w,int h,int btnh,int pos,int limit,int range);
  void drawBlotter(uint32_t rgba);
  void drawRadar();
  void drawHighScores();
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
