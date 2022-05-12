#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_Configuration.h"
#include "sitter_Game.h"
#include "sitter_VideoManager.h"
#include "sitter_ResourceManager.h"
#include "sitter_Sprite.h"
#include "sitter_Player.h"
#include "sitter_PlayerSprite.h"
#include "sitter_Grid.h"

#define SPAWNV_INCREMENT 16

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Grid::Grid(Game *game,int colc,int rowc,int colw,int rowh):game(game) {
  if ((colw<0)||(rowh<0)) sitter_throw(ArgumentError,"Grid cell dimensions %dx%d",colw,rowh);
  if ((colc>0)&&(rowc>0)) {
    this->colc=colc;
    this->rowc=rowc;
    this->colw=colw;
    this->rowh=rowh;
    if (!(cellv=(uint8_t*)malloc(colc*rowc))) sitter_throw(AllocationError,"");
    memset(cellv,0,colc*rowc);
  } else {
    this->colc=this->rowc=0;
    this->colw=colw;
    this->rowh=rowh;
    cellv=NULL;
  }
  memset(tidv,-1,sizeof(int)*256);
  scrollx=scrolly=0;
  memset(cellpropv,0,1024);
  spawnv=NULL; spawnc=spawna=0;
  play_mode=SITTER_GRIDMODE_COOP;
  murderlimit=0;
  timelimit=0;
  bgname=NULL;
  bgtexid=-1;
  bgcolor=0x808080ff;
  songname=NULL;
}

Grid::~Grid() {
  flush();
  for (int i=0;i<spawnc;i++) if (spawnv[i].sprresname) free(spawnv[i].sprresname);
  if (spawnv) free(spawnv);
  if (bgname) free(bgname);
  if (bgtexid>=0) game->video->unloadTexture(bgtexid);
  if (songname) free(songname);
}

/******************************************************************************
 * load
 *****************************************************************************/

void Grid::flush() {
  colw=rowh=32;
  colc=rowc=0;
  if (cellv) free(cellv);
  cellv=NULL;
  for (int i=0;i<256;i++) {
    if (tidv[i]>=0) game->video->unloadTexture(tidv[i]);
    tidv[i]=-1;
  }
  scrollx=scrolly=0;
  memset(cellpropv,0,1024);
}

/******************************************************************************
 * spawn
 *****************************************************************************/
 
int Grid::addSprite(const char *name,int col,int row,int plrid) {
  if (spawnc>=spawna) {
    spawna+=SPAWNV_INCREMENT;
    if (!(spawnv=(SpriteSpawn*)realloc(spawnv,sizeof(SpriteSpawn)*spawna))) sitter_throw(AllocationError,"");
  }
  if (!(spawnv[spawnc].sprresname=strdup(name))) sitter_throw(AllocationError,"");
  spawnv[spawnc].plrid=plrid;
  spawnv[spawnc].col=col;
  spawnv[spawnc].row=row;
  return spawnc++;
}

void Grid::removeSpawn(int index) {
  if ((index<0)||(index>=spawnc)) return;
  if (spawnv[index].sprresname) free(spawnv[index].sprresname);
  spawnc--;
  if (index<spawnc) memmove(spawnv+index,spawnv+index+1,sizeof(SpriteSpawn)*(spawnc-index));
}

void Grid::makeSprites(int playerc) {
  bool gotplayer[4]={0};
  for (int i=0;i<spawnc;i++) {
    if (playerc&&spawnv[i].plrid&&((spawnv[i].plrid>>4)!=playerc)) continue;
    int xoffset,yoffset;
    Sprite *spr=game->res->loadSprite(spawnv[i].sprresname,&xoffset,&yoffset);
    //sitter_log("  @ %d,%d",spawnv[i].col,spawnv[i].row);
    spr->r.centerx(spawnv[i].col*colw+(colw>>1));
    spr->r.bottom((spawnv[i].row+1)*rowh);
    spr->r.x+=xoffset;
    spr->r.y+=yoffset;
    if (spawnv[i].plrid&&playerc) {
      if (gotplayer[(spawnv[i].plrid&0xf)-1]) sitter_throw(IdiotProgrammerError,"Grid: multiple player %d",spawnv[i].plrid&0xf);
      if (Player *plr=game->getPlayer(spawnv[i].plrid&0xf)) {
        gotplayer[(spawnv[i].plrid&0xf)-1]=true;
        plr->spr=(PlayerSprite*)spr;
        ((PlayerSprite*)spr)->plr=plr;
        plr->reset();
        char tintname[7]={'p','t','i','n','t',0,0};
        tintname[5]=(spawnv[i].plrid&0xf)+0x30;
        spr->tint=game->cfg->getOption_int(tintname);
      } else sitter_throw(IdiotProgrammerError,"Grid: player %d not found",spawnv[i].plrid&0xf);
    }
  }
  for (int i=0;i<playerc;i++) if (!gotplayer[i]) sitter_throw(IdiotProgrammerError,"player %d not found",i+1);
}

/******************************************************************************
 * resize
 *****************************************************************************/
 
void Grid::safeResize(int colc,int rowc) {
  if ((colc<1)||(rowc<1)) sitter_throw(ArgumentError,"Grid::safeResize(%d,%d)",colc,rowc);
  uint8_t *ncellv=(uint8_t*)malloc(colc*rowc);
  if (!ncellv) sitter_throw(AllocationError,"");
  memset(ncellv,0,colc*rowc);
  int cpcolc=colc; if (cpcolc>this->colc) cpcolc=this->colc;
  int cprowc=rowc; if (cprowc>this->rowc) cprowc=this->rowc;
  if (cpcolc) for (int row=0;row<cprowc;row++) {
    memcpy(ncellv+row*colc,cellv+row*this->colc,cpcolc);
  }
  if (cellv) free(cellv);
  cellv=ncellv;
  this->colc=colc;
  this->rowc=rowc;
  bool rmsome=false;
  int i=0; while (i<spawnc) {
    if ((spawnv[i].col>=colc)||(spawnv[i].row>=rowc)) {
      rmsome=true;
      sitter_log("WARNING: removing out-of-range spawn point '%s' @ %d,%d",spawnv[i].sprresname,spawnv[i].col,spawnv[i].row);
      free(spawnv[i].sprresname);
      spawnc--;
      if (i<spawnc) memmove(spawnv+i,spawnv+i+1,sizeof(SpriteSpawn)*(spawnc-i));
    } else i++;
  }
  if (rmsome) {
    game->grp_all->killAllSprites();
    makeSprites(0);
  }
}

/******************************************************************************
 * background
 *****************************************************************************/
 
void Grid::setBackground(const char *bg) {
  if (bgtexid>=0) { game->video->unloadTexture(bgtexid); bgtexid=-1; }
  if (bgname) free(bgname);
  if (!bg||!bg[0]) { bgname=NULL; bgtexid=-1; bgcolor=0x808080ff; return; }
  if (!(bgname=strdup(bg))) sitter_throw(AllocationError,"");
  bool ishex=true;
  uint32_t hex=0;
  int bglen=0; 
  while (bg[bglen]) {
    if ((bg[bglen]>=0x30)&&(bg[bglen]<=0x39)) { hex<<=4; hex|=(bg[bglen]-0x30); }
    else if ((bg[bglen]>='a')&&(bg[bglen]<='f')) { hex<<=4; hex|=(bg[bglen]-'a')+10; }
    else if ((bg[bglen]>='A')&&(bg[bglen]<='F')) { hex<<=4; hex|=(bg[bglen]-'A')+10; }
    else ishex=false;
    bglen++;
  }
  guess_again_wise_guy:
  if (ishex) {
    if (bglen==6) { hex<<=8; hex|=0xff; }
    else if (bglen==8) ; // ok
    else { ishex=false; goto guess_again_wise_guy; } // do attempt to load eg "ff000" as a texture, even though it's going to fail
    bgcolor=hex;
  } else {
    bgtexid=game->video->loadTexture(bgname,false);
  }
}

/******************************************************************************
 * segmented texture loader
 *****************************************************************************/
 
void Grid::loadMultiTexture(const char *texname,uint8_t cellnw) {
  int texnamelen=0; while (texname[texnamelen]) texnamelen++;
  int fullnamelen=texnamelen+7; // "-00.png"
  char *fullname=(char*)malloc(fullnamelen+1);
  if (!fullname) sitter_throw(AllocationError,"");
  try {
    memcpy(fullname,texname,texnamelen);
    char *sfx=fullname+texnamelen;
    int colc=16-(cellnw&0xf),rowc=16-(cellnw>>4);
    for (int row=0;row<rowc;row++) {
      for (int col=0;col<colc;col++) {
        int tid=-1;
        sprintf(sfx,"-%x%x.png",row,col);
        try { 
          tid=game->video->loadTexture(fullname,false,0);
          int cellid=((cellnw>>4)+row)*16+(cellnw&0xf)+col;
          if (tidv[cellid]>=0) game->video->unloadTexture(tidv[cellid]);
          tidv[cellid]=tid;
          cellpropv[cellid]=cellpropv[cellnw];
        } catch (...) {
          if (!col&&!row) throw;
          if (!col) { rowc=row; continue; }
          colc=col;
          continue;
        }
      }
    }
  } catch (...) { free(fullname); throw; }
  free(fullname);
}

/******************************************************************************
 * rotate
 *****************************************************************************/
 
void Grid::rotate(int dx,int dy) {
  if (!colc||!rowc) return;
  while (dx>=colc) dx-=colc; while (dx<=-colc) dx+=colc;
  while (dy>=rowc) dy-=rowc; while (dy<=-rowc) dy+=rowc;
  if (!dx&&!dy) return;
  uint8_t *ncellv=(uint8_t*)malloc(colc*rowc);
  if (!ncellv) sitter_throw(AllocationError,"");
  uint8_t *ncellv_row=ncellv;
  for (int dsty=0;dsty<rowc;dsty++) {
    int srcy=dsty-dy;
    if (srcy<0) srcy+=rowc; else if (srcy>=rowc) srcy-=rowc;
    uint8_t *cellv_row=cellv+srcy*colc;
    for (int dstx=0;dstx<colc;dstx++) {
      int srcx=dstx-dx;
      if (srcx<0) srcx+=colc; else if (srcx>=colc) srcx-=colc;
      ncellv_row[dstx]=cellv_row[srcx];
    }
    ncellv_row+=colc;
  }
  free(cellv);
  cellv=ncellv;
  for (int i=0;i<spawnc;i++) {
    spawnv[i].col+=dx; if (spawnv[i].col<0) spawnv[i].col+=colc; else if (spawnv[i].col>=colc) spawnv[i].col-=colc;
    spawnv[i].row+=dy; if (spawnv[i].row<0) spawnv[i].row+=rowc; else if (spawnv[i].row>=rowc) spawnv[i].row-=rowc;
  }
}
