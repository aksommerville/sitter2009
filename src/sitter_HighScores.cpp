#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_Configuration.h"
#include "sitter_File.h"
#include "sitter_HighScores.h"

/* Format of high-score file. *************************************************
 * Text, UNIX or DOS newlines. Leading and trailing space trimmed, blank lines ignored.
 * No comments or line-continuation.
 * SET HEADER:
 *   sSETNAME
 * GRID HEADER:
 *   gGRIDINDEX,PLAYERC
 *   Only after SET HEADER.
 *   GRIDINDEX is decimal, >=1.
 *   PLAYERC is 1,2,3,4.
 * ENTRY
 *   =PLAYTIME;MURDERC;REALTIME;NAME
 *   PLAYTIME is decimal >=0, Game::play_clock.
 *   MURDERC is decimal >=0, Game::murderc.
 *   REALTIME is decimal >=0, system time when score registered.
 *   NAME is any ASCII string, newline and trailing space forbidden. Human consumption only.
 *   Entries must be in order, ie the first is "first place", ...
 *****************************************************************************/
 
#define SETV_INCREMENT 2
#define GRIDV_INCREMENT 16
#define SCOREV_INCREMENT 10

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
HighScores::HighScores(Game *game):game(game) {
  setv=NULL; setc=seta=0;
  scorelimit=game->cfg->getOption_int("highscore-limit");
  if (scorelimit<1) scorelimit=1;
}

HighScores::~HighScores() {
  flush();
  if (setv) free(setv);
}

/******************************************************************************
 * list access, primitive
 *****************************************************************************/

void HighScores::flush() {
  for (int seti=0;seti<setc;seti++) {
    for (int gridi=0;gridi<setv[seti].entc;gridi++) {
      for (int scorei=0;scorei<setv[seti].entv[gridi].entc;scorei++) {
        if (setv[seti].entv[gridi].entv[scorei].name) free(setv[seti].entv[gridi].entv[scorei].name);
      }
      if (setv[seti].entv[gridi].entv) free(setv[seti].entv[gridi].entv);
    }
    if (setv[seti].entv) free(setv[seti].entv);
  }
  setc=0;
}
  
HighScores::SetListEntry *HighScores::findSet(const char *setname) const {
  for (int i=0;i<setc;i++) if (!strcasecmp(setname,setv[i].setname)) return setv+i;
  return NULL;
}

HighScores::SetListEntry *HighScores::addSet(const char *setname) {
  if (setc>=seta) {
    seta+=SETV_INCREMENT;
    if (!(setv=(SetListEntry*)realloc(setv,sizeof(SetListEntry)*seta))) sitter_throw(AllocationError,"");
  }
  memset(setv+setc,0,sizeof(SetListEntry));
  if (!(setv[setc].setname=strdup(setname))) sitter_throw(AllocationError,"");
  SetListEntry *rtn=setv+setc;
  setc++;
  return rtn;
}

HighScores::GridListEntry *HighScores::findGrid(HighScores::SetListEntry *set,int gridindex,int playerc) const {
  for (int i=0;i<set->entc;i++) 
    if ((set->entv[i].gridindex==gridindex)&&(set->entv[i].playerc==playerc))
      return set->entv+i;
  return NULL;
}

HighScores::GridListEntry *HighScores::addGrid(HighScores::SetListEntry *set,int gridindex,int playerc) {
  if (set->entc>=set->enta) {
    set->enta+=GRIDV_INCREMENT;
    if (!(set->entv=(GridListEntry*)realloc(set->entv,sizeof(GridListEntry)*set->enta))) sitter_throw(AllocationError,"");
  }
  memset(set->entv+set->entc,0,sizeof(GridListEntry));
  set->entv[set->entc].gridindex=gridindex;
  set->entv[set->entc].playerc=playerc;
  GridListEntry *rtn=set->entv+set->entc;
  set->entc++;
  return rtn;
}

int HighScores::addScore(HighScores::GridListEntry *grid,int play_clock,int murderc,int64_t realtime,const char *name) {
  if (grid->entc>=grid->enta) {
    grid->enta+=SCOREV_INCREMENT;
    if (!(grid->entv=(ScoreEntry*)realloc(grid->entv,sizeof(ScoreEntry)*grid->enta))) sitter_throw(AllocationError,"");
  }
  int before=grid->entc;
  for (int i=0;i<grid->entc;i++) {
    if (murderc<grid->entv[i].murderc) { before=i; break; }
    if ((murderc==grid->entv[i].murderc)&&(play_clock<grid->entv[i].play_clock)) { before=i; break; }
  }
  if (before>=scorelimit) return -1;
  if (before<grid->entc) memmove(grid->entv+before+1,grid->entv+before,sizeof(ScoreEntry)*(grid->entc-before));
  grid->entc++;
  while (grid->entc>scorelimit) {
    grid->entc--;
    if (grid->entv[grid->entc].name) free(grid->entv[grid->entc].name);
  }
  grid->entv[before].play_clock=play_clock;
  grid->entv[before].murderc=murderc;
  grid->entv[before].realtime=realtime;
  if (!(grid->entv[before].name=strdup(name))) sitter_throw(AllocationError,"");
  return before;
}

/******************************************************************************
 * read
 *****************************************************************************/
 
void HighScores::readStoredScores() {
  flush();
  const char *hspath=game->cfg->getOption_str("highscore-file");
  File *f=NULL;
  try { f=sitter_open(hspath,"r");
  } catch (FileNotFoundError) { return; }
  if (!f) return;
  int lineno=0;
  SetListEntry *set=NULL;
  GridListEntry *grid=NULL;
  try {
    while (char *line=f->readLine(true,true,false,false,&lineno)) {
      try {
        switch (line[0]) {
          case 's': { // begin set
              if (findSet(line+1)) throw FormatError();
              set=addSet(line+1);
            } break;
          case 'g': { // begin grid
              if (!set) throw FormatError();
              char *tail=NULL; int gridindex=strtol(line+1,&tail,10);
              if (!tail||(tail[0]!=',')) throw FormatError();
              if (gridindex<1) throw FormatError();
              tail++; char *tail2=tail;
              int playerc=strtol(tail,&tail2,10);
              if ((tail2==tail)||tail2[0]) throw FormatError();
              if ((playerc<1)||(playerc>4)) throw FormatError();
              if (findGrid(set,gridindex,playerc)) throw FormatError();
              grid=addGrid(set,gridindex,playerc);
            } break;
          case '=': { // entry
              if (!set||!grid) throw FormatError();
              char *tail=line+1,*tail2;
              int play_clock=strtol(tail,&tail,10);
              if ((tail==line+1)||(tail[0]!=';')) throw FormatError();
              tail++; tail2=tail;
              int murderc=strtol(tail,&tail2,10);
              if ((tail2==tail)||(tail2[0]!=';')) throw FormatError();
              tail=tail2+1; tail2=tail;
              int64_t realtime=strtol(tail,&tail2,10);
              if ((tail2==tail)||(tail2[0]!=';')) throw FormatError();
              const char *name=tail2+1;
              addScore(grid,play_clock,murderc,realtime,name);
            } break;
          default: throw FormatError();
        }
      } catch (FormatError) {
        sitter_log("Error in high scores file '%s', line %d. Flushing scores. Don't tamper with this file, jerktard.",hspath,lineno);
        free(line);
        delete f;
        return;
      } catch (...) { free(line); throw; }
      free(line);
    }
  } catch (...) { delete f; throw; }
  delete f;
}

/******************************************************************************
 * write
 *****************************************************************************/
 
void HighScores::writeScores() {
  const char *hspath=game->cfg->getOption_str("highscore-file");
  if (!hspath||!hspath[0]) { return; }
  File *f=NULL;
  try { f=sitter_open(hspath,"w");
  } catch (FileNotFoundError) { return; }
  if (!f) { return; }
  try {
    for (int seti=0;seti<setc;seti++) {
      f->writef("s%s\n",setv[seti].setname);
      for (int gridi=0;gridi<setv[seti].entc;gridi++) {
        f->writef("g%d,%d\n",setv[seti].entv[gridi].gridindex,setv[seti].entv[gridi].playerc);
        for (int scorei=0;scorei<setv[seti].entv[gridi].entc;scorei++) {
          ScoreEntry *e=setv[seti].entv[gridi].entv+scorei;
          // watch out! if "%lld" does not consume 64 bits, this will get ugly.
          f->writef("=%d;%d;%lld;%s\n",e->play_clock,e->murderc,e->realtime,e->name?e->name:"");
        }
      }
    }
  } catch (...) { delete f; throw; }
  delete f;
}
 
void HighScores::format(char *dst,int dstlen,const char *setname,int gridindex,int playerc,int highlight) {
  if (dstlen<1) sitter_throw(IdiotProgrammerError,"HighScores::format, dstlen=%d",dstlen);
  SetListEntry *set=findSet(setname);
  if (!set) { dst[0]=0; return; }
  GridListEntry *grid=findGrid(set,gridindex,playerc);
  if (!grid) { dst[0]=0; return; }
  int offset=snprintf(dst,dstlen,"DD/MM/YY  MM:SS.00 KILL NAME\n");
  for (int i=0;i<grid->entc;i++) {
    time_t not_always_int_64_t=grid->entv[i].realtime;
    struct tm *btime=localtime(&not_always_int_64_t);
    int hundredths=((grid->entv[i].play_clock%60)*10)/6;
    int seconds=grid->entv[i].play_clock/60;
    int minutes=seconds/60; seconds%=60;
    if (i==highlight) {
      if (offset<dstlen) dst[offset]=0x01;
      offset++;
    }
    offset+=snprintf(dst+offset,dstlen-offset,"%02d/%02d/%02d ",btime->tm_mday,btime->tm_mon+1,btime->tm_year%100);
    offset+=snprintf(dst+offset,dstlen-offset,"%3d:%02d.%02d %4d %s\n",minutes,seconds,hundredths,grid->entv[i].murderc,grid->entv[i].name);
    if (i==highlight) {
      if (offset<dstlen) dst[offset]=0x02;
      offset++;
    }
  }
  if (offset>=dstlen) dst[dstlen-1]=0;
  else dst[offset]=0;
}

/******************************************************************************
 * query
 *****************************************************************************/
 
int HighScores::registerScore(const char *setname,int gridindex,int playerc,int time,int murderc) {
  SetListEntry *set=findSet(setname);
  if (!set) set=addSet(setname);
  GridListEntry *grid=findGrid(set,gridindex,playerc);
  if (!grid) grid=addGrid(set,gridindex,playerc);
  int rtn=addScore(grid,time,murderc,::time(0),"");
  return rtn+1;
}

void HighScores::setScoreName(const char *setname,int gridindex,int playerc,int scoreid,const char *name) {
  SetListEntry *set=findSet(setname);
  if (!set) return;
  GridListEntry *grid=findGrid(set,gridindex,playerc);
  if (!grid) return;
  scoreid--;
  if ((scoreid<0)||(scoreid>=grid->entc)) return;
  if (grid->entv[scoreid].name) free(grid->entv[scoreid].name);
  if (!(grid->entv[scoreid].name=strdup(name))) sitter_throw(AllocationError,"");
}
