#include <malloc.h>
#include <string.h>
#include <unistd.h>
#ifdef SITTER_WIN32
  #include <windows.h>
#endif
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_Configuration.h"
#include "sitter_ResourceManager.h"

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
ResourceManager::ResourceManager(Game *game):game(game) {
  gfxpfx=gridpfx=sfxpfx=bgmpfx=sprpfx=NULL;
  gfxpfxlen=gridpfxlen=sfxpfxlen=bgmpfxlen=sprpfxlen=0;
  soundcachev=NULL; soundcachec=soundcachea=0;
  locked=false;
}

ResourceManager::~ResourceManager() {
  lock();
  if (gfxpfx) free(gfxpfx);
  if (gridpfx) free(gridpfx);
  if (sfxpfx) free(sfxpfx);
  if (bgmpfx) free(bgmpfx);
  if (sprpfx) free(sprpfx);
  for (int i=0;i<soundcachec;i++) {
    if (soundcachev[i].resname) free(soundcachev[i].resname);
    if (soundcachev[i].data) free(soundcachev[i].data);
  }
  if (soundcachev) free(soundcachev);
}

void ResourceManager::lock() {
  int panic=2000;
  while (locked) {
    if (--panic<0) {
      sitter_log("PANIC! ResourceManager mutex has been locked for too long. Forcing unlocked.");
      break;
    }
    #ifdef SITTER_WIN32
      Sleep(1);
    #else
      usleep(500);
    #endif
  }
  locked=true;
}

void ResourceManager::unlock() {
  locked=false;
}

/******************************************************************************
 * path mangling
 *****************************************************************************/
 
const char *ResourceManager::resnameToPath(const char *resname,char **pfx,int *pfxlen) {
  if (!*pfx||!*pfxlen) refreshPrefixes();
  if (!*pfx||!*pfxlen) sitter_throw(FileNotFoundError,"res='%s', bad prefix",resname);
  int rnlen=0; while (resname[rnlen]) rnlen++;
  if ((*pfxlen)+rnlen>=SITTER_RESMGR_PATH_MAX) sitter_throw(FileNotFoundError,"path too long",resname);
  if (!rnlen) sitter_throw(FileNotFoundError,"empty resource name");
  memcpy(pathbuf,*pfx,*pfxlen);
  memcpy(pathbuf+(*pfxlen),resname,rnlen);
  pathbuf[(*pfxlen)+rnlen]=0;
  return pathbuf;
}

void ResourceManager::refreshPrefixes() {
  if (gfxpfx) free(gfxpfx); gfxpfx=NULL;
  if (gridpfx) free(gridpfx); gridpfx=NULL;
  if (sfxpfx) free(sfxpfx); sfxpfx=NULL;
  if (bgmpfx) free(bgmpfx); bgmpfx=NULL;
  if (sprpfx) free(sprpfx); sprpfx=NULL;
  int pfxlen=0,pathlen=0;
  #define APPEND(optname) \
    if (const char *opt=game->cfg->getOption_str(optname)) { \
      int slen=0; while (opt[slen]) { \
        slen++; \
        if (pfxlen+slen>=SITTER_RESMGR_PATH_MAX) sitter_throw(FileNotFoundError,"resource prefix too long"); \
      }  \
      memcpy(pathbuf+pfxlen,opt,slen); \
      pathlen=pfxlen+slen; \
      pathbuf[pathlen]=0; \
    }
  #define CP(lbl) { \
      if (!(lbl=(char*)malloc(pathlen+1))) sitter_throw(AllocationError,""); \
      memcpy(lbl,pathbuf,pathlen+1); \
      lbl##len=pathlen; \
    }
  APPEND("resprefix") pfxlen=pathlen;
  APPEND("dataset")   pfxlen=pathlen;
  APPEND("gfxpfx")    CP(gfxpfx)
  APPEND("gridpfx")   CP(gridpfx)
  APPEND("sfxpfx")    CP(sfxpfx)
  APPEND("bgmpfx")    CP(bgmpfx)
  APPEND("sprpfx")    CP(sprpfx)
  #undef APPEND
  #undef CP
}
