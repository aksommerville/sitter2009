#ifndef SITTER_RESOURCEMANAGER_H
#define SITTER_RESOURCEMANAGER_H

/* Manage loading, parsing, and caching data files.
 * If the user can pick different data sets, eg hi-res or lo-res graphics, that should
 * all be handled here.
 */
 
#include <stdint.h>

class Game;
class File;
class Grid;
class Sprite;
class Song;
class TabLabelTable;

#define SITTER_RESMGR_PATH_MAX 1024

/* Thrown when a grid does not support the requested player count (try the next one).
 */
class DontUseThisGrid {};

class ResourceManager {

  Game *game;
  char *gfxpfx,*gridpfx,*sfxpfx,*bgmpfx,*sprpfx;
  int gfxpfxlen,gridpfxlen,sfxpfxlen,bgmpfxlen,sprpfxlen;
  char pathbuf[SITTER_RESMGR_PATH_MAX];
  volatile bool locked;
  
  typedef struct {
    char *resname;
    void *data;
    int samplec;
    int refc;
  } SoundEffectCacheEntry;
  SoundEffectCacheEntry *soundcachev; int soundcachec,soundcachea;
  
  /* pfx and pfxlen must point to one of my: (gfxpfx,gfxpfxlen), etc.
   * Returns pathbuf always.
   */
  const char *resnameToPath(const char *resname,char **pfx,int *pfxlen);
  void refreshPrefixes();
  
  /* We have two grid file formats. (I might remove the text format later...)
   */
  void loadGrid_text(Grid *grid,File *f,const char *resname,int playerc);
  void loadGrid_binary(Grid *grid,File *f,int playerc);
  
  /* Fooling around with song formats.
   */
  void loadSong_tab(Song *song,File *f,const char *resname);
  void loadSong_tab_line(Song *song,char **notesbuf,int *beatno,int drumtrackc,TabLabelTable &lbltab);
  void loadSong_asm(Song *song,File *f,const char *resname);
  void loadSong_temp(Song *song,File *f,const char *resname);
  
public:

  ResourceManager(Game *game);
  ~ResourceManager();
  
  /* I allocate pixels, you free them */
  bool loadImage(const char *resname,int *fmt,int *w,int *h,void **pixels);
  
  Grid *loadGrid(const char *gridset,const char *resname,int playerc);
  void saveGrid(const Grid *grid,const char *gridset,const char *resname);
  Sprite *loadSprite(const char *resname,int *xoffset=0,int *yoffset=0);
  
  /* Return a NULL-terminated list of all available grids.
   */
  char **listGrids(const char *gridset);
  char **listGridSets();
  
  /* For now, sound effect files are presumed to be headerless s16le PCM 48 kHz.
   */
  void loadSoundEffect(const char *resname,int16_t **data,int *samplec,bool cacheonly=false);
  void unloadSoundEffectByName(const char *resname);
  void unloadSoundEffectByBuffer(const void *data);
  
  Song *loadSong(const char *resname);
  
  /* Mutex.
   */
  void lock();
  void unlock();
  
  static void loadImage_PNG(File *f,int *fmt,int *w,int *h,void **pixels);
  static void loadImage_STR0TEX0(File *f,int *fmt,int *w,int *h,void **pixels); // a private format allowing GX textures (pixels will be aligned)
  
};

#endif
