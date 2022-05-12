#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_string.h"
#include "sitter_Game.h"
#include "sitter_VideoManager.h"
#include "sitter_File.h"
#include "sitter_Grid.h"
#include "sitter_ResourceManager.h"

/******************************************************************************
 * grid load
 *****************************************************************************/
 
class GridSymbolTable {
public:

  Grid *grid;
  struct {
    char *k;
  } symv[256];
  int symc;
  
  GridSymbolTable(Grid *grid):grid(grid) {
    symc=0;
  }
  
  ~GridSymbolTable() {
    for (int i=0;i<symc;i++) free(symv[i].k);
  }
  
  #define HEXSTR(s) ((((s[0]>=0x30)&&(s[0]<=0x39))||((s[0]>=0x41)&&(s[0]<=46)))&&\
                     (((s[1]>=0x30)&&(s[1]<=0x39))||((s[1]>=0x41)&&(s[1]<=46))))
  
  void addSymbol(const char *k) {
    if (symc>=256) sitter_throw(FormatError,"Grid: too many symbols");
    if (!k[0]||!k[1]||k[2]) sitter_throw(FormatError,"grid symbol '%s' must be 2 characters",k);
    if (HEXSTR(k)) sitter_throw(FormatError,"illegal grid symbol '%s' (would be read as hexadecimal byte)",k);
    if (!(symv[symc].k=strdup(k))) sitter_throw(AllocationError,"");
    symc++;
  }
  
  void qualifySymbol(const char *arg) {
    if (!symc) sitter_throw(IdiotProgrammerError,"GridSymbolTable::qualifySymbol, symc==0");
    if (!strcasecmp(arg,"solid")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_SOLID;
    else if (!strcasecmp(arg,"goal")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_GOAL;
    else if (!strcasecmp(arg,"killtop")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_KILLTOP;
    else if (!strcasecmp(arg,"killbottom")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_KILLBOTTOM;
    else if (!strcasecmp(arg,"killleft")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_KILLLEFT;
    else if (!strcasecmp(arg,"killright")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_KILLRIGHT;
    else if (!strcasecmp(arg,"goal1")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_GOAL1;
    else if (!strcasecmp(arg,"goal2")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_GOAL2;
    else if (!strcasecmp(arg,"goal3")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_GOAL3;
    else if (!strcasecmp(arg,"goal4")) grid->cellpropv[symc-1]|=SITTER_GRIDCELL_GOAL4;
    else sitter_throw(FormatError,"unknown grid symbol qualifier '%s'",arg);
  }
  
  /* throw exceptions if necessary */
  uint8_t lookup(const char *src) const {
    if (!src[0]||!src[1]) sitter_throw(FormatError,"unexpected end of line");
    if (HEXSTR(src)) {
      uint8_t cell;
      if ((src[0]>=0x30)&&(src[0]<=0x39)) cell=(src[0]-0x30)<<4;
      else cell=(src[0]-0x41+10)<<4;
      if ((src[1]>=0x30)&&(src[1]<=0x39)) cell|=(src[1]-0x30);
      else cell|=(src[1]-0x41+10);
      return cell;
    }
    for (int i=0;i<symc;i++) if (!memcmp(src,symv[i].k,2)) return i;
    sitter_throw(FormatError,"expected grid symbol or hexadecimal byte before '%c%c'",src[0],src[1]);
  }
  
  #undef HEXSTR
  
};

Grid *ResourceManager::loadGrid(const char *gridset,const char *resname,int playerc) {
  lock();
  Grid *grid=new Grid(game);
  try {
  try {
    /* make qualified resource name and path */
    int setlen=0; while (gridset[setlen]) setlen++;
    int reslen=0; while (resname[reslen]) reslen++;
    if (setlen+reslen+2>256) sitter_throw(ArgumentError,"grid name too long");
    char qresname[256];
    memcpy(qresname,gridset,setlen);
    memcpy(qresname+setlen+1,resname,reslen);
    qresname[setlen]='/';
    qresname[setlen+1+reslen]=0;
    const char *path=resnameToPath(qresname,&gridpfx,&gridpfxlen);
  
    if (File *f=sitter_open(path,"rb")) {
      try {
        uint8_t signature[8];
        if (f->read(signature,8)<8) sitter_throw(FormatError,"grid '%s': failed to read signature",qresname);
        if (!memcmp(signature,"BIN_GRID",8)) loadGrid_binary(grid,f,playerc);
        else { f->seek(0); loadGrid_text(grid,f,qresname,playerc); }
      } catch (...) { delete f; throw; }
      delete f;
    } else sitter_throw(FileNotFoundError,"%s",path);
  } catch (...) { delete grid; throw; }
  } catch (...) { unlock(); throw; }
  unlock();
  return grid;
}
 
void ResourceManager::loadGrid_text(Grid *grid,File *f,const char *resname,int playerc) {
  #define READCOUPLET(word,a,b) { \
      char *tail=0; \
      a=strtol(word,&tail,0); \
      if (!tail||(tail[0]!=',')) sitter_throw(FormatError,"grid '%s': expected couplet, found '%s'",resname,word); \
      b=strtol(tail+1,&tail,0); \
      if (!tail||tail[0]) sitter_throw(FormatError,"grid '%s': expected couplet, found '%s'",resname,word); \
    }
  
  struct { int p1x,p1y,p2x,p2y,p3x,p3y,p4x,p4y; } startpositions[4];
  bool got1starts=false,got2starts=false,got3starts=false,got4starts=false,donestarts=false;
  int rowa=0;
  
  GridSymbolTable stab(grid);
  bool strip=true,skip=true,cont=true,comment=true; int lineno=0;
  while (char *line=f->readLine(strip,skip,cont,comment,&lineno)) {
    if (rowa) { // reading cell data
      
        if (grid->rowc>=rowa) {
          rowa+=32;
          if (!(grid->cellv=(uint8_t*)realloc(grid->cellv,grid->colc*rowa))) sitter_throw(AllocationError,"");
        }
        
        const char *src=line;
        uint8_t *rowcells=grid->cellv+grid->rowc*grid->colc;
        for (int col=0;col<grid->colc;col++) {
          try {
            rowcells[col]=stab.lookup(src);
          } catch (FormatError) {
            sitter_prependErrorMessage("grid %s:%d: ",resname,lineno);
            throw;
          }
          src+=2;
        }
        if (*src) sitter_throw(FormatError,"grid '%s':%d: extra tokens after data line",resname,lineno);
        grid->rowc++;
      
      } else { // reading header
        char **wordv=sitter_strsplit_white(line);
        int wordc=0; while (wordv[wordc]) wordc++;
        
        if ((wordc>=2)&&!strncasecmp(wordv[0],"start",5)&&!wordv[0][6]&&((wordv[0][5]>='1')&&(wordv[0][5]<='4'))) { // start positions
          if (donestarts) sitter_throw(FormatError,"grid '%s': start positions must appear first",resname);
          switch (wordv[0][5]) {
            case '1': {
                if (got1starts) sitter_throw(FormatError,"grid '%s': multiple '%s'",resname,"start1");
                got1starts=true;
                if (wordc!=2) sitter_throw(FormatError,"grid '%s': '%s' takes %s argument%c",resname,"start1","one",' ');
                READCOUPLET(wordv[1],startpositions[0].p1x,startpositions[0].p1y)
              } break;
            case '2': {
                if (got2starts) sitter_throw(FormatError,"grid '%s': multiple '%s'",resname,"start2");
                got2starts=true;
                if (wordc!=3) sitter_throw(FormatError,"grid '%s': '%s' takes %s argument%c",resname,"start2","two",'s');
                READCOUPLET(wordv[1],startpositions[1].p1x,startpositions[1].p1y)
                READCOUPLET(wordv[2],startpositions[1].p2x,startpositions[1].p2y)
              } break;
            case '3': {
                if (got3starts) sitter_throw(FormatError,"grid '%s': multiple '%s'",resname,"start3");
                got3starts=true;
                if (wordc!=4) sitter_throw(FormatError,"grid '%s': '%s' takes %s argument%c",resname,"start3","three",'s');
                READCOUPLET(wordv[1],startpositions[2].p1x,startpositions[2].p1y)
                READCOUPLET(wordv[2],startpositions[2].p2x,startpositions[2].p2y)
                READCOUPLET(wordv[3],startpositions[2].p3x,startpositions[2].p3y)
              } break;
            case '4': {
                if (got4starts) sitter_throw(FormatError,"grid '%s': multiple '%s'",resname,"start4");
                got4starts=true;
                if (wordc!=5) sitter_throw(FormatError,"grid '%s': '%s' takes %s argument%c",resname,"start4","four",'s');
                READCOUPLET(wordv[1],startpositions[3].p1x,startpositions[3].p1y)
                READCOUPLET(wordv[2],startpositions[3].p2x,startpositions[3].p2y)
                READCOUPLET(wordv[3],startpositions[3].p3x,startpositions[3].p3y)
                READCOUPLET(wordv[4],startpositions[3].p4x,startpositions[3].p4y)
              } break;
          }
        
        } else {
          if (!donestarts) {
            donestarts=true;
            switch (playerc) {
              case 0: break;
              case 1: { 
                  if (!got1starts) throw DontUseThisGrid();
                  grid->addSprite("bill",startpositions[0].p1x,startpositions[0].p1y,0x10|1);
                } break;
              case 2: {
                  if (!got2starts) throw DontUseThisGrid();
                  grid->addSprite("bill",startpositions[1].p1x,startpositions[1].p1y,0x20|1);
                  grid->addSprite("bill",startpositions[1].p2x,startpositions[1].p2y,0x20|2);
                } break;
              case 3: {
                  if (!got3starts) throw DontUseThisGrid();
                  grid->addSprite("bill",startpositions[2].p1x,startpositions[2].p1y,0x30|1);
                  grid->addSprite("bill",startpositions[2].p2x,startpositions[2].p2y,0x30|2);
                  grid->addSprite("bill",startpositions[2].p3x,startpositions[2].p3y,0x30|3);
                } break;
              case 4: {
                  if (!got4starts) throw DontUseThisGrid();
                  grid->addSprite("bill",startpositions[3].p1x,startpositions[3].p1y,0x40|1);
                  grid->addSprite("bill",startpositions[3].p2x,startpositions[3].p2y,0x40|2);
                  grid->addSprite("bill",startpositions[3].p3x,startpositions[3].p3y,0x40|3);
                  grid->addSprite("bill",startpositions[3].p4x,startpositions[3].p4y,0x40|4);
                } break;
              default: sitter_throw(IdiotProgrammerError,"loadGrid '%s': playerc=%d",resname,playerc);
            }
          }
            
          if ((wordc==2)&&!strcasecmp(wordv[0],"begin")&&!strcasecmp(wordv[1],"cells")) {
            if (!grid->colc) sitter_throw(FormatError,"grid '%s': missing column count",resname);
            strip=skip=cont=comment=false;
            rowa=32;
            grid->cellv=(uint8_t*)malloc(grid->colc*rowa);
            if (!grid->cellv) sitter_throw(AllocationError,"");
            
          } else if ((wordc==3)&&!strcasecmp(wordv[0],"sprite")) {
            int col,row; READCOUPLET(wordv[2],col,row)
            grid->addSprite(wordv[1],col,row);
          
          } else if ((wordc==2)&&!strcasecmp(wordv[0],"colc")) {
            char *tail=0; grid->colc=strtol(wordv[1],&tail,0);
            if (!tail||tail[0]||(grid->colc<1)) sitter_throw(FormatError,"grid '%s': bad column count '%s'",resname,wordv[1]);
          
          } else if ((wordc==2)&&!strcasecmp(wordv[0],"colw")) {
            char *tail=0; grid->colw=strtol(wordv[1],&tail,0);
            if (!tail||tail[0]||(grid->colw<1)) sitter_throw(FormatError,"grid '%s': bad column width '%s'",resname,wordv[1]);
          
          } else if ((wordc==2)&&!strcasecmp(wordv[0],"rowh")) {
            char *tail=0; grid->rowh=strtol(wordv[1],&tail,0);
            if (!tail||tail[0]||(grid->rowh<1)) sitter_throw(FormatError,"grid '%s': bad row height '%s'",resname,wordv[1]);
           
          } else if ((wordc>2)&&!strcasecmp(wordv[0],"sym")) {
            stab.addSymbol(wordv[1]);
            grid->tidv[stab.symc-1]=game->video->loadTexture(wordv[2],false,0);
            for (int i=3;i<wordc;i++) stab.qualifySymbol(wordv[i]);
           
          } else sitter_throw(FormatError,"grid '%s': failed to parse line %d: '%s'",resname,lineno,line);
        
        }
        for (int i=0;i<wordc;i++) free(wordv[i]); free(wordv);
      }
      free(line);
    }
  #undef READCOUPLET
}

void ResourceManager::loadGrid_binary(Grid *grid,File *f,int playerc) {
  /* f is positioned just past the signature.
   * Binary Grid Format:
   *  Integers are big-endian, unsigned unless noted.
   *  0000    8 signature "BIN_GRID"
   *  0008    2 column count - 1
   *  000a    2 row count - 1
   *  000c    1 column width - 1
   *  000d    1 row height - 1
   *  000e    4 additional header length (for future expansion)
   *  0012  ... additional header:
   *  0012    1 mode (SITTER_GRIDMODE_*)
   *  0013    2 murder limit
   *  0015    2 time limit
   *  0017    1 background name length
   *  0018  ... background name (texture,RRGGBB,RRGGBBAA)
   *  ....    1 song name length
   *  ....  ... song name
   *  t     ... cell property table, array of:
   *              1 texture resource name length, 0=table terminator
   *              . texture resource name
   *              4 cellprop, see sitter_Grid.h
   *             Index in this table is the cell value.
   *             Only 256 entries are permitted.
    *            List must be terminated, even when all 256 slots are used.
   *  s     ... sprite table, array of:
   *              1 resource name length, 0=table terminator
   *              . resource name
   *              1 0=always load, or (playercount<<4)|playerid
   *              2 column
   *              2 row
   *  c     ... cell data, colc*rowc bytes.
   * All possible values are permitted for column count, row count, column width, column height.
   * Data after cells is ignored.
   */
  try {
    /* header */
    grid->colc=f->read16be()+1;
    grid->rowc=f->read16be()+1;
    grid->colw=f->read8()+1;
    grid->rowh=f->read8()+1;
    uint32_t addlhdrlen=f->read32be();
    uint32_t posthdr=0x12+addlhdrlen;
    if (addlhdrlen>=1) grid->play_mode=f->read8();
    if (addlhdrlen>=3) grid->murderlimit=f->read16be();
    if (addlhdrlen>=5) grid->timelimit=f->read16be();
    if (addlhdrlen>=6) {
      uint8_t bgnamelen=f->read8();
      if (bgnamelen) {
        if (6+bgnamelen>addlhdrlen) sitter_throw(FormatError,"grid: impossible background-name length %d, addl-hdr length=%d",bgnamelen,addlhdrlen);
        char bgname[256];
        if (f->read(bgname,bgnamelen)!=bgnamelen) throw ShortIOError();
        bgname[bgnamelen]=0;
        unlock();
        grid->setBackground(bgname);
        lock();
      }
      if (addlhdrlen>=7+bgnamelen) {
        uint8_t songnamelen=f->read8();
        if (songnamelen) {
          if (7+bgnamelen+songnamelen>addlhdrlen) sitter_throw(FormatError,"grid: impossible song-name length %d, addl-hdr length=%d",songnamelen,addlhdrlen);
          char songname[256];
          if (f->read(songname,songnamelen)!=songnamelen) throw ShortIOError();
          songname[songnamelen]=0;
          if (!(grid->songname=strdup(songname))) sitter_throw(AllocationError,"");
        }
      }
    }
    if (f->seek(posthdr)!=posthdr) throw ShortIOError();
    /* property table */
    int cellid=0;
    char resname[256]; resname[255]=0;
    while (1) {
      uint8_t namelen=f->read8();
      if (!namelen) break;
      if (cellid>=256) sitter_throw(FormatError,"grid: too many entries in Cell Property Table");
      if (f->read(resname,namelen)!=namelen) throw ShortIOError();
      resname[namelen]=0;
      if ((namelen==1)&&(resname[0]==0x20)) {
        f->read32be();
      } else {
        unlock();
        grid->tidv[cellid]=game->video->loadTexture(resname,true,0);
        lock();
        grid->cellpropv[cellid]=f->read32be();
      }
      cellid++;
    }
    /* sprite table */
    bool gotplr[4]={false};
    while (1) {
      uint8_t namelen=f->read8();
      if (!namelen) break;
      if (f->read(resname,namelen)!=namelen) throw ShortIOError();
      resname[namelen]=0;
      int plrid=f->read8();
      int col=f->read16be();
      int row=f->read16be();
      if (plrid>>4==playerc) {
        int p=(plrid&0xf)-1;
        if ((p>=0)&&(p<4)) gotplr[p]=true;
      }
      grid->addSprite(resname,col,row,plrid);
    }
    for (int i=0;i<playerc;i++) if (!gotplr[i]) throw DontUseThisGrid();
    /* cells */
    int cellc=grid->colc*grid->rowc;
    if (!(grid->cellv=(uint8_t*)malloc(cellc))) sitter_throw(AllocationError,"");
    if (f->read(grid->cellv,cellc)!=cellc) throw ShortIOError();
  } catch (ShortIOError) { sitter_throw(FormatError,"grid: unexpected end of file"); }
}

void ResourceManager::saveGrid(const Grid *grid,const char *gridset,const char *resname) {
  /* make qualified resource name and path */
  int setlen=0; while (gridset[setlen]) setlen++;
  int reslen=0; while (resname[reslen]) reslen++;
  if (setlen+reslen+2>256) sitter_throw(ArgumentError,"grid name too long");
  char qresname[256];
  memcpy(qresname,gridset,setlen);
  memcpy(qresname+setlen+1,resname,reslen);
  qresname[setlen]='/';
  qresname[setlen+1+reslen]=0;
  const char *path=resnameToPath(qresname,&gridpfx,&gridpfxlen);
  if (File *f=sitter_open(path,"wb")) {
    try {
      /* header */
      if (f->write("BIN_GRID",8)!=8) sitter_throw(IOError,"grid %s: short write",path);
      f->write16be(grid->colc-1);
      f->write16be(grid->rowc-1);
      f->write8(grid->colw-1);
      f->write8(grid->rowh-1);
      int bgnamelen=0; if (grid->bgname) while (grid->bgname[bgnamelen]) bgnamelen++;
      int songnamelen=0; if (grid->songname) while (grid->songname[songnamelen]) songnamelen++;
      f->write32be(7+bgnamelen+songnamelen); // add'l header
      f->write8(grid->play_mode);
      f->write16be(grid->murderlimit);
      f->write16be(grid->timelimit);
      f->write8(bgnamelen);
      if (grid->bgname) { if (f->write(grid->bgname,bgnamelen)!=bgnamelen) sitter_throw(IOError,"grid %s: short write",path); }
      f->write8(songnamelen);
      if (grid->songname) { if (f->write(grid->songname,songnamelen)!=songnamelen) sitter_throw(IOError,"grid %s: short write",path); }
      /* cell property table */
      int cdc=256;
      while (cdc&&(grid->tidv[cdc-1]<0)&&!grid->cellpropv[cdc-1]) cdc--;
      for (int i=0;i<cdc;i++) {
        const char *texresname=game->video->texidToResname(grid->tidv[i]);
        if (!texresname||!texresname[0]) texresname=" ";
        int namelen=0; while (texresname[namelen]) namelen++;
        f->write8(namelen);
        if (f->write(texresname,namelen)!=namelen) sitter_throw(IOError,"grid %s: short write",path);
        f->write32be(grid->cellpropv[i]);
      }
      f->write8(0);
      /* sprite table */
      for (int i=0;i<grid->spawnc;i++) {
        int namelen=0; while (grid->spawnv[i].sprresname[namelen]) namelen++;
        f->write8(namelen);
        if (f->write(grid->spawnv[i].sprresname,namelen)!=namelen) sitter_throw(IOError,"grid %s: short write",path);
        f->write8(grid->spawnv[i].plrid);
        f->write16be(grid->spawnv[i].col);
        f->write16be(grid->spawnv[i].row);
      }
      f->write8(0);
      /* cell data */
      if (f->write(grid->cellv,grid->colc*grid->rowc)!=grid->colc*grid->rowc) sitter_throw(IOError,"grid %s: short write",path);
    } catch (...) { delete f; throw; }
    delete f;
    sitter_log("saved grid to %s",path);
  } else sitter_throw(IOError,"sitter_open '%s' failed",path);
}

char **ResourceManager::listGrids(const char *gridset) {
  char **rtn=NULL;
  lock();
  try {
  if (!gridpfx) refreshPrefixes();
  if (!gridpfx) { 
    rtn=(char**)malloc(sizeof(void*));
    if (!rtn) sitter_throw(AllocationError,"");
    rtn[0]=NULL;
    unlock();
    return rtn;
  }
  const char *path=resnameToPath(gridset,&gridpfx,&gridpfxlen);
  rtn=sitter_listDir(path,SITTER_LISTDIR_SORT|SITTER_LISTDIR_SHOWFILE);
  } catch (...) { unlock(); throw; }
  unlock();
  return rtn;
}

char **ResourceManager::listGridSets() {
  char **rtn=NULL;
  lock();
  try {
  if (!gridpfx) refreshPrefixes();
  if (!gridpfx) { 
    rtn=(char**)malloc(sizeof(void*));
    if (!rtn) sitter_throw(AllocationError,"");
    rtn[0]=NULL;
    unlock();
    return rtn;
  }
  rtn=sitter_listDir(gridpfx,SITTER_LISTDIR_SORT|SITTER_LISTDIR_SHOWDIR);
  } catch (...) { unlock(); throw; }
  unlock();
  return rtn;
}
