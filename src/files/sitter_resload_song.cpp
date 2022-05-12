#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_Game.h"
#include "sitter_string.h"
#include "sitter_File.h"
#include "sitter_Song.h"
#include "sitter_ResourceManager.h"

/******************************************************************************
 * Song
 *****************************************************************************/
 
Song *ResourceManager::loadSong(const char *resname) {
  //sitter_log("loadSong '%s'",resname);
  lock();
  Song *song=new Song();
  song->mgr=game->audio;
  try {
    const char *path=resnameToPath(resname,&bgmpfx,&bgmpfxlen);
    if (File *f=sitter_open(path,"rb")) {
      try {
        uint8_t signature[8];
        if (f->read(signature,8)!=8) throw ShortIOError();
        f->seek(0);
        if (!memcmp(signature,"#TABLATU",8)) loadSong_tab(song,f,resname);
        else if (!memcmp(signature,"STR_SONG",8)) loadSong_asm(song,f,resname);
        else sitter_throw(FormatError,"song %s: unknown format",path);
      } catch (ShortIOError) { delete f; sitter_throw(FormatError,"song %s: unexpected end of file",path);
      } catch (...) { delete f; throw; }
      delete f;
    } else sitter_throw(FileNotFoundError,"song %s",resname);
  } catch (...) { unlock(); delete song; throw; }
  unlock();
  return song;
}

/******************************************************************************
 * temp
 *****************************************************************************/

void ResourceManager::loadSong_temp(Song *song,File *f,const char *resname) {
  
  #define F55(f) (f*8.0)
  #define A4  F55(440.000)
  #define AS4 F55(471.429)
  #define B4  F55(495.000)
  #define C4  F55(513.333)
  #define CS4 F55(550.000)
  #define D4  F55(586.667)
  #define DS4 F55(616.000)
  #define E4  F55(660.000)
  #define F4  F55(704.000)
  #define FS4 F55(733.333)
  #define G4  F55(792.000)
  #define GS4 F55(825.000)
  #define A5  F55(880.000)
  
  int sineid=song->addSample("sine55");
  song->chanc=2;
  song->samplesperbeat=10000;
  
  song->addop_SAMPLE(0,0,sineid);
  song->addop_SAMPLE(0,1,sineid);
  song->addop_START(0,0);
  song->addop_START(0,1);
  int headpos=song->addop_NOOP(0); // song begins at beat 0, but not command 0
  
  song->addop_FREQ (  0,0, A4,0);
  song->addop_LEVEL(  0,1,0xff,100);
  song->addop_FREQ (  0,1, A5,0);
  song->addop_LEVEL(  1,1,0,5000);
  
  song->addop_FREQ (  2,0, B4,1000);
  song->addop_LEVEL(  2,1,0xff,100);
  song->addop_FREQ (  2,1, G4,1000);
  song->addop_LEVEL(  3,1,0,5000);
  
  song->addop_FREQ (  4,0, C4,1000);
  song->addop_LEVEL(  4,1,0xff,100);
  song->addop_FREQ (  4,1, F4,1000);
  song->addop_LEVEL(  5,1,0,5000);
  
  song->addop_FREQ (  6,0, B4,1000);
  song->addop_LEVEL(  6,1,0xff,100);
  song->addop_FREQ (  6,1, E4,1000);
  song->addop_LEVEL(  7,1,0,5000);
  
  song->addop_GOTO(8,headpos,-1);
  
  #undef F55
  #undef A4
  #undef AS4
  #undef B4
  #undef C4
  #undef CS4
  #undef D4
  #undef DS4
  #undef E4
  #undef F4
  #undef FS4
  #undef G4
  #undef GS4
  #undef A5
}

/******************************************************************************
 * tablature
 *****************************************************************************/
 
const static int stringstarts[6]={ // from high to low
  24,
  19,
  15,
  10,
  5,
  0,
};
const static struct { int n,d; } intervals[12]={ // from low to high
  { 1, 1},
  {15,14},
  { 9, 8},
  { 6, 5},
  { 5, 4},
  { 4, 3},
  { 7, 5},
  { 3, 2},
  { 8, 5},
  { 5, 3},
  { 9, 5},
  {15, 8},
};
//#define FREQ_NOTE_0 330.0 // E3, assuming base frequency of 440
#define FREQ_NOTE_0 330.0 // E3, assuming base frequency of 110

class TabLabelTable {
public:

  typedef struct {
    char *lbl;
    int cmdp;
  } Entry;
  Entry *entv; int entc,enta;
  int drumnames[128]; // index=symbol, value=sample ID
  
  TabLabelTable() {
    entv=NULL; entc=enta=0;
    memset(drumnames,0xff,sizeof(int)*128);
  }
  
  ~TabLabelTable() {
    for (int i=0;i<entc;i++) free(entv[i].lbl);
    if (entv) free(entv);
  }
  
  int lookup(const char *lbl) const {
    for (int i=0;i<entc;i++) if (!strcmp(lbl,entv[i].lbl)) return entv[i].cmdp;
    sitter_throw(FormatError,"label '%s' not defined",lbl);
  }
  
  /* I take ownership of lbl.
   */
  void define(char *lbl,int cmdp) {
    for (int i=0;i<entc;i++) if (!strcmp(lbl,entv[i].lbl)) sitter_throw(FormatError,"redefinition of label '%s'",lbl);
    if (entc>=enta) {
      enta+=16;
      if (!(entv=(Entry*)realloc(entv,sizeof(Entry)*enta))) sitter_throw(AllocationError,"");
    }
    entv[entc].lbl=lbl;
    entv[entc].cmdp=cmdp;
    entc++;
  }
  
  void setDrum(char sym,int smplid) {
    if (sym&0x80) sitter_throw(ArgumentError,"invalid character 0x%02x for drum name",(uint8_t)sym);
    drumnames[sym]=smplid;
  }
  
};
 
void ResourceManager::loadSong_tab(Song *song,File *f,const char *resname) {
  #define READPHASE_HEAD 1
  #define READPHASE_BODY 2
  #define TRACK_LIMIT 7
  TabLabelTable lbltab;
  song->chanc=6;
  
  song->samplesperbeat=5000;
  unlock();
  int smplid=song->addSample("cello");
  lock();
  for (int i=0;i<6;i++) {
    song->addop_SAMPLE(0,i,smplid);
    song->addop_LEVEL(0,i,0,0);
  }
  int headcmdid=0;
  int drumtrackc=0;
  
  int lineno=0;
  int beatno=1;
  int readphase=READPHASE_HEAD;
  char *notesbuf[TRACK_LIMIT]={0};
  int nextnotesline=0;
  while (char *line=f->readLine(true,true,true,true,&lineno)) {
    try {
      
      if (!strcasecmp(line,"fin")) { // end of file, ignore the rest
        free(line);
        break;
      } else if (!strcasecmp(line,"capa")) { // end of header, start of notes
        if (readphase!=READPHASE_HEAD) sitter_throw(FormatError,"unexpected 'capa' in song body");
        readphase=READPHASE_BODY;
        headcmdid=song->addop_NOOP(1);
      } else switch (readphase) {
      
        case READPHASE_HEAD: {
            char **wordv=sitter_strsplit_white(line);
            int wordc=0; while (wordv[wordc]) wordc++;
            bool ok=false;
            if (wordc>=1) {
            
              if (!strcasecmp(wordv[0],"samplesperbeat")) {
                if (wordc!=2) sitter_throw(FormatError,"samplesperbeat takes one argument");
                char *tail=0; song->samplesperbeat=strtol(wordv[1],&tail,0);
                if (!tail||tail[0]||(song->samplesperbeat<1)) sitter_throw(FormatError,"failed to parse integer");
                ok=true;
                
              } else if (!strcasecmp(wordv[0],"sample")) {
                if (wordc!=2) sitter_throw(FormatError,"'sample' takes one argument");
                unlock();
                song->replaceSample(smplid,wordv[1]);
                lock();
                ok=true;
                
              } else if (!strcasecmp(wordv[0],"sampleloop")) {
                if (wordc!=2) sitter_throw(FormatError,"'sampleloop' takes one argument");
                char *tail=0; song->sampleloop=strtol(wordv[1],&tail,0);
                if (!tail||tail[0]) sitter_throw(FormatError,"failed to parse integer");
                ok=true;
                
              } else if (!strcasecmp(wordv[0],"samplefreq")) {
                if (wordc!=2) sitter_throw(FormatError,"'samplefreq' takes one argument");
                char *tail=0; song->samplefreq=strtol(wordv[1],&tail,0);
                if (!tail||tail[0]||(song->samplefreq<1.0)) sitter_throw(FormatError,"failed to parse integer");
                ok=true;
                
              } else if (!strcasecmp(wordv[0],"drum")) {
                if (wordc!=3) sitter_throw(FormatError,"'drum' takes two arguments (drum <name> <symbol>)");
                unlock();
                lbltab.setDrum(wordv[2][0],song->addSample(wordv[1]));
                lock();
                if (!drumtrackc) {
                  drumtrackc=1;
                  song->chanc=7;
                  song->addop_LEVEL(0,6,0,0);
                }
                ok=true;
            
              }
            }
            for (int i=0;i<wordc;i++) free(wordv[i]);
            free(wordv);
            if (!ok) sitter_throw(FormatError,"bad header command '%s'",line);
          } break;
          
        case READPHASE_BODY: {
            if (nextnotesline==6+drumtrackc) {
              loadSong_tab_line(song,notesbuf,&beatno,drumtrackc,lbltab);
              for (int i=0;i<TRACK_LIMIT;i++) { if (notesbuf[i]) free(notesbuf[i]); notesbuf[i]=NULL; }
              nextnotesline=0;
            }
            if (!strncasecmp(line,"label ",6)) { // define label
              char **wordv=sitter_strsplit_white(line);
              if (!wordv[0]||!wordv[1]||wordv[2]) sitter_throw(FormatError,"'label' takes exactly one argument");
              lbltab.define(wordv[1],song->cmdc);
              free(wordv[0]);
              // don't free wordv[1]; lbltab takes care of it
              free(wordv);
            } else if (!strncasecmp(line,"goto ",5)) { // insert goto
              char **wordv=sitter_strsplit_white(line);
              if (!wordv[0]||!wordv[1]||!wordv[2]||wordv[3]) sitter_throw(FormatError,"'goto' takes exactly two arguments");
              int cmdp=lbltab.lookup(wordv[1]);
              char *tail=0;
              int goct=strtol(wordv[2],&tail,0);
              if (!tail||tail[0]) sitter_throw(FormatError,"failed to parse 'goto' count");
              song->addop_GOTO(beatno,cmdp,goct);
              free(wordv[0]);
              free(wordv[1]);
              free(wordv[2]);
              free(wordv);
            } else { // normal body line
              notesbuf[nextnotesline++]=line;
              line=NULL;
            }
          } break;
          
      }
    } catch (FormatError) {
      sitter_prependErrorMessage("song %s:%d: ",resname,lineno);
      throw;
    }
    if (line) free(line);
  }
  if (notesbuf[0]) loadSong_tab_line(song,notesbuf,&beatno,drumtrackc,lbltab);
  for (int i=0;i<TRACK_LIMIT;i++) if (notesbuf[i]) free(notesbuf[i]);
  song->addop_GOTO(beatno,headcmdid,-1);
  #undef READPHASE_HEAD
  #undef READPHASE_BODY
}

void ResourceManager::loadSong_tab_line(Song *song,char **notesbuf,int *beatno,int drumtrackc,TabLabelTable &lbltab) {
  #define SHORTLEN_LVL 2000
  #define SHORTLEN_FRQ 1000
  #define TOPLEVEL 0xc0
  int trackc=6+drumtrackc;
  for (int i=0;i<trackc;i++) if (!notesbuf[i]) sitter_throw(IdiotProgrammerError,"loadSong_tab_line: line %d == NULL",i);
  int p[TRACK_LIMIT]={0};
  int mute[TRACK_LIMIT]={0};
  int beatc=0;
  while (1) {
    bool onenull=false,allnull=true;
    for (int i=0;i<trackc;i++) {
      while (notesbuf[i][p[i]]==' ') p[i]++;
      if (notesbuf[i][p[i]]) allnull=false; else onenull=true;
    }
    if (allnull) break;
    if (onenull) sitter_throw(FormatError,"lines not equal length");
    for (int chi=0;chi<6;chi++) { // guitar...
      if (notesbuf[chi][p[chi]]=='-') ; // '-' == do nothing
      
      else if (notesbuf[chi][p[chi]]=='!') { // '!' == silence
        if (mute[chi]!=1) song->addop_LEVEL((*beatno)+beatc,chi,0,SHORTLEN_LVL);
        mute[chi]=1;
      
      } else if ((notesbuf[chi][p[chi]]>='0')&&(notesbuf[chi][p[chi]]<='9')) { // '0'..'9' == frets 0..9
        if (mute[chi]!=2) song->addop_LEVEL((*beatno)+beatc,chi,TOPLEVEL,SHORTLEN_LVL);
        mute[chi]=2;
        int note=(notesbuf[chi][p[chi]]-'0')+stringstarts[chi];
        double freq=FREQ_NOTE_0;
        while (note>=12) { freq*=2.0; note-=12; }
        if (note) freq=(freq*intervals[note].n)/intervals[note].d;
        song->addop_FREQ((*beatno)+beatc,chi,freq,SHORTLEN_FRQ);
        song->addop_REWIND((*beatno)+beatc,chi);
      
      } else if ((notesbuf[chi][p[chi]]>='a')&&(notesbuf[chi][p[chi]]<='z')) { // 'a'..'z' == frets 10..35 (35!)
        if (mute[chi]!=2) song->addop_LEVEL((*beatno)+beatc,chi,TOPLEVEL,SHORTLEN_LVL);
        mute[chi]=2;
        int note=(notesbuf[chi][p[chi]]-'a')+10+stringstarts[chi];
        double freq=FREQ_NOTE_0;
        while (note>=12) { freq*=2.0; note-=12; }
        if (note) freq=(freq*intervals[note].n)/intervals[note].d;
        song->addop_FREQ((*beatno)+beatc,chi,freq,SHORTLEN_FRQ);
        song->addop_REWIND((*beatno)+beatc,chi);
      
      } else { // anything else == error
        sitter_throw(FormatError,"unexpected character '%c'",notesbuf[chi][p[chi]]);
      }
      p[chi]++;
    }
    for (int chi=6;chi<trackc;chi++) { // drum...
      if (notesbuf[chi][p[chi]]=='-') ;
      else if (notesbuf[chi][p[chi]]&0x80) sitter_throw(FormatError,"unexpected character 0x%02x",(uint8_t)(notesbuf[chi][p[chi]]));
      else {
        if (lbltab.drumnames[notesbuf[chi][p[chi]]]<0) sitter_throw(FormatError,"unexpected character 0x%02x",(uint8_t)(notesbuf[chi][p[chi]]));
        int smplid=lbltab.drumnames[notesbuf[chi][p[chi]]];
        //sitter_log("drum '%c' (%d) @ %d",notesbuf[chi][p[chi]],smplid,(*beatno)+beatc);
        song->addop_SAMPLE((*beatno)+beatc,chi,smplid);
        song->addop_REWIND((*beatno)+beatc,chi);
        song->addop_LEVEL((*beatno)+beatc,chi,TOPLEVEL,0);
      }
      p[chi]++;
    }
    beatc++;
  }
  (*beatno)+=beatc;
  #undef SHORTLEN_LVL
  #undef SHORTLEN_FRQ
  #undef TOPLEVEL
}

/******************************************************************************
 * internal format as text commands ("assembly")
 *****************************************************************************/

void ResourceManager::loadSong_asm(Song *song,File *f,const char *resname) {
  sitter_log("TODO loadSong_asm");
}
