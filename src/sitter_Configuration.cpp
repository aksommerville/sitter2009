#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include "sitter_string.h"
#include "sitter_Error.h"
#include "sitter_File.h"
#include "sitter_Configuration.h"

#define OPTV_INCREMENT 32
#define KV_SEPARATOR '='
#define NEGATIVE_SHORT_OPTION '^' // in a short option string, negates the next option
#define CVTBUF_LEN 256
static char _Configuration_cvtbuf[CVTBUF_LEN];
#define KEY_LEN_LIMIT 64
#define DEFAULT_VALUE "1" // value when key is given alone
#define DEFAULT_NO_VALUE "0" // value when given as "no-KEY"

//#define IS_INPUT_OPTION(k) ((((k)[0]=='i')||((k)[0]=='I'))&&(((k)[1]=='n')||((k)[1]=='N'))&&((k)[2]==':'))

/******************************************************************************
 * string conversion
 *****************************************************************************/
 
/* i18n: these strings evaluate to true and false. The first element of each
 * array is used when converting to string.
 */
static const char *_true_strings[]={ "true","on","1","yes",NULL };
static const char *_false_strings[]={ "false","off","0","no",NULL };
 
const char *Configuration::intToString(int n) {
  snprintf(_Configuration_cvtbuf,CVTBUF_LEN,"%d",n);
  _Configuration_cvtbuf[CVTBUF_LEN-1]=0;
  return _Configuration_cvtbuf;
}

const char *Configuration::floatToString(double n) {
  snprintf(_Configuration_cvtbuf,CVTBUF_LEN,"%1.5f",n);
  _Configuration_cvtbuf[CVTBUF_LEN-1]=0;
  return _Configuration_cvtbuf;
}

const char *Configuration::boolToString(bool n) {
  if (n) return _true_strings[0];
  return _false_strings[0];
}

bool Configuration::stringToInt(const char *s,int *n) {
  char *tail=NULL;
  *n=strtol(s,&tail,0);
  return (tail&&!tail[0]);
}

bool Configuration::stringToFloat(const char *s,double *n) {
  *n=0;
  double denom=0.0;
  bool positive=true;
  if (*s=='-') { positive=false; s++; }
  while (*s) {
    if (*s=='.') { if (denom!=0.0) return false; denom=0.1; }
    else if (denom==0.0) {
      if (((*s)<0x30)||((*s)>0x39)) return false;
      (*n)*=10.0;
      (*n)+=((*s)-0x30);
    } else {
      if (((*s)<0x30)||((*s)>0x39)) return false;
      (*n)+=((*s)-0x30)*denom;
      denom/=10.0;
    }
    s++;
  }
  if (!positive) (*n)=-(*n);
  return true;
}

bool Configuration::stringToBool(const char *s,bool *n) {
  for (const char **si=_true_strings;*si;si++) if (!strcasecmp(s,*si)) { *n=true; return true; }
  for (const char **si=_false_strings;*si;si++) if (!strcasecmp(s,*si)) { *n=false; return true; }
  int i; double f;
  if (stringToInt(s,&i)) { *n=(i!=0); return true; }
  if (stringToFloat(s,&f)) { *n=(f!=0.0); return true; }
  return false;
}

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
Configuration::Configuration() {
  optv=NULL; optc=opta=0;
  setDefaults();
}

Configuration::~Configuration() {
  for (int i=0;i<optc;i++) {
    if (optv[i].k) free(optv[i].k);
    if (optv[i].shortk) free(optv[i].shortk);
    if ((optv[i].type==SITTER_CFGTYPE_STR)&&optv[i]._str.v) free(optv[i]._str.v);
  }
  if (optv) free(optv);
}

void Configuration::setDefaults() {

  // video:
  #ifdef SITTER_WII
    addOption("fswidth" ,"",SITTER_CFGTYPE_INT,0,0,99999); // width of projection (0=screen size)
    addOption("fsheight","",SITTER_CFGTYPE_INT,0,0,99999); // height of projection (0=screen size)
    addOption("wii-overscan-x","",SITTER_CFGTYPE_INT, 0,-99999,99999); // remove so many pixels horizontally
    addOption("wii-overscan-y","",SITTER_CFGTYPE_INT,30,-99999,99999); // remove so many pixels vertically
  #else
    addOption("fullscreen","f",SITTER_CFGTYPE_BOOL,  0); // start up in fullscreen (i recommend false, it doesn't work on every machine)
    addOption("fswidth",   "", SITTER_CFGTYPE_INT, 800,1,99999); // screen width, fullscreen
    addOption("fsheight",  "", SITTER_CFGTYPE_INT, 600,1,99999); // screen height, fullscreen
    addOption("fssmart",   "", SITTER_CFGTYPE_BOOL,  1); // check available fullscreen dimensions first (i strongly recommend true)
    addOption("winwidth",  "", SITTER_CFGTYPE_INT, 800,1,99999); // screen width, windowed
    addOption("winheight", "", SITTER_CFGTYPE_INT, 576,1,99999); // screen height, windowed
  #endif
  addOption("tex-filter","",SITTER_CFGTYPE_INT,0,0,2); // linear texture filter, 0=auto 1=never 2=always
  
  // resource search:
  addOption("resprefix", "", SITTER_CFGTYPE_STR,SITTER_DATA_DIR); // resource root
  addOption("dataset",   "", SITTER_CFGTYPE_STR,"default/");       // immediately under resource root
  addOption("gfxpfx",    "", SITTER_CFGTYPE_STR,"graphics/");      // graphics, under 'dataset'
  addOption("gridpfx",   "", SITTER_CFGTYPE_STR,"grids/");         // grids "
  addOption("sfxpfx",    "", SITTER_CFGTYPE_STR,"sounds/");        // sound effects "
  addOption("bgmpfx",    "", SITTER_CFGTYPE_STR,"music/");         // background music "
  addOption("sprpfx",    "", SITTER_CFGTYPE_STR,"sprites/");       // sprite definitions (not graphics) "
  
  // audio:
  addOption("music",    "m",SITTER_CFGTYPE_BOOL,1); // enable background music
  addOption("sound",    "s",SITTER_CFGTYPE_BOOL,1); // enable sound effects (does not affect music)
  addOption("audio",    "A",SITTER_CFGTYPE_BOOL,1); // enable audio ('music' and 'sound' are ignored; AudioManager never instantiated)
  
  // auto-save
  addOption("autosave","a",SITTER_CFGTYPE_BOOL,1); // write full config file every time the program exits (barring exceptions)
  #ifdef SITTER_CFG_DEST
    addOption("autosave-file","",SITTER_CFGTYPE_STR,SITTER_CFG_DEST);
  #else
    addOption("autosave-file","",SITTER_CFGTYPE_STR,"");
  #endif
  
  #ifdef SITTER_HIGHSCORE_FILE
    addOption("highscore-file","",SITTER_CFGTYPE_STR,SITTER_HIGHSCORE_FILE);
  #else
    addOption("highscore-file","",SITTER_CFGTYPE_STR,"sitter.highscore");
  #endif
  addOption("highscore-limit","",SITTER_CFGTYPE_INT,10,1,99999); // how many scores stored for each level (read at init only)
  
  // multiplayer tint
  addOption("ptint1","",SITTER_CFGTYPE_INT,0x00000000,INT_MIN,INT_MAX);
  addOption("ptint2","",SITTER_CFGTYPE_INT,0xff000040,INT_MIN,INT_MAX);
  addOption("ptint3","",SITTER_CFGTYPE_INT,0x00ff0040,INT_MIN,INT_MAX);
  addOption("ptint4","",SITTER_CFGTYPE_INT,0x0000ff40,INT_MIN,INT_MAX);
  
  // debuggery
  addOption("debug-goals","G",SITTER_CFGTYPE_BOOL,0); // draw sprites as a 1-color silhouette when on goal (color depends on which goal). read once at init.
  addOption("rabbit_mode","R",SITTER_CFGTYPE_BOOL,0); // all players jump nonstop. i needed it briefly for debugging, and it's so funny i kept it
  
  /* Input configuration.
   * The value is a self-describing list; its elements are whitespace lists "map desc".
   * map desc:
   *    SRC DEST   # direct mapping
   *    SRC ! DEST # reverse mapping
   *    SRC THRESH_LO THRESH_HI BTN_LO BTN_MID BTN_HI # axis mapping
   *    SRC > THRESH DEST # direct with threshhold
   *    SRC < THRESH DEST # reverse with threshhold
   * keys: "keymap" "joymap"(default) "joymap0"(device 0).."joymapN"
   * "joymap" may use PX* button ids for "next player with no joystick maps yet"
   * or P1*,P2*,etc.
   */
  #ifdef SITTER_WII
    addOption("gcpadmap","",SITTER_CFGTYPE_STR,",1,2,3,4");
    addOption("wiimotemap","",SITTER_CFGTYPE_STR,",1,2,3,4");
  #else
    addOption("keymap","",SITTER_CFGTYPE_STR,",ESC MENU , ESC MNCANCEL , UP MNUP , DOWN MNDOWN , LEFT MNLEFT , RIGHT MNRIGHT , RETURN MNOK"
                                             ",SPACE MNOK , UP P1JUMP , DOWN P1DUCK , LEFT P1LEFT , RIGHT P1RIGHT , SPACE P1PICKUP"
                                             ",SPACE P1TOSS , LSHIFT MNFAST");
    addOption("joymap","",SITTER_CFGTYPE_STR,",a0 -10000 10000 PXLEFT 0 PXRIGHT , a1 -10000 10000 PXJUMP 0 PXDUCK"
                                             ",0 PXPICKUP , 1 PXTOSS");
  #endif
}

void Configuration::removeInputOptions() {
  for (int i=0;i<optc;) {
    if (!strcmp(optv[i].k,"keymap")||!strncmp(optv[i].k,"joymap",6)) {
      free(optv[i].k);
      if (optv[i].shortk) free(optv[i].shortk);
      if ((optv[i].type==SITTER_CFGTYPE_STR)&&optv[i]._str.v) free(optv[i]._str.v);
      optc--;
      if (i<optc) memmove(optv+i,optv+i+1,sizeof(Option)*(optc-i));
    } else i++;
  }
}

/******************************************************************************
 * option list internals
 *****************************************************************************/
 
void Configuration::addOption(const char *k,const char *shortk,int type,...) {
  /* sanity check */
  if (!k) sitter_throw(IdiotProgrammerError,"k==NULL");
  for (int i=0;i<optc;i++) {
    int cmplen=sitter_strlenicmp(k,optv[i].k); // strcasecmp, but make sure it's the same operation findOption uses
    if (!k[cmplen]&&!optv[i].k[cmplen]) sitter_throw(IdiotProgrammerError,"config key '%s' redefined",k);
    if (shortk&&optv[i].shortk) {
      for (const char *mk=shortk;*mk;mk++) for (const char *yk=optv[i].shortk;*yk;yk++) 
        if (*mk==*yk) sitter_throw(IdiotProgrammerError,"config short key '%c' redefined ('%s' and '%s')",*mk,optv[i].k,k);
    }
  }
  /* grow optv if necessary */
  if (optc>=opta) {
    opta+=OPTV_INCREMENT;
    optv=(Option*)realloc(optv,sizeof(Option)*opta);
    if (!optv) sitter_throw(AllocationError,"");
  }
  /* populate new entry */
  if (!(optv[optc].k=strdup(k))) sitter_throw(AllocationError,"");
  if (shortk) { if (!(optv[optc].shortk=strdup(shortk))) sitter_throw(AllocationError,""); }
  else optv[optc].shortk=NULL;
  va_list vargs;
  va_start(vargs,type);
  switch (optv[optc].type=type) {
    case SITTER_CFGTYPE_STR: {
        const char *v=va_arg(vargs,const char*);
        if (v) { if (!(optv[optc]._str.v=strdup(v))) sitter_throw(AllocationError,""); }
        else optv[optc]._str.v=NULL;
      } break;
    case SITTER_CFGTYPE_INT: {
        optv[optc]._int.v=va_arg(vargs,int);
        optv[optc]._int.lo=va_arg(vargs,int);
        optv[optc]._int.hi=va_arg(vargs,int);
      } break;
    case SITTER_CFGTYPE_FLOAT: {
        optv[optc]._float.v=va_arg(vargs,double);
        optv[optc]._float.lo=va_arg(vargs,double);
        optv[optc]._float.hi=va_arg(vargs,double);
      } break;
    case SITTER_CFGTYPE_BOOL: {
        optv[optc]._bool.v=va_arg(vargs,int);
      } break;
    default: sitter_throw(IdiotProgrammerError,"config option type=%d",type);
  }
  optc++;
}

int Configuration::findOption(const char *k) const {
  int rtn=-1,matchlen=0;
  bool ambiguous=false;
  for (int i=0;i<optc;i++) {
    int cmplen=sitter_strlenicmp(k,optv[i].k);
    if (!cmplen) continue;
    if (k[cmplen]) continue; // must match entire query
    if (!optv[i].k[cmplen]) return i; // exact match
    if (cmplen<matchlen) continue;
    if (cmplen>matchlen) { rtn=i; matchlen=cmplen; ambiguous=false; continue; }
    ambiguous=true; // two matches, same length
  }
  if (rtn<0) return -1;
  if (ambiguous) return -2;
  return rtn;
}

int Configuration::findShortOption(char k) const {
  for (int i=0;i<optc;i++) {
    if (!optv[i].shortk) continue;
    for (const char *ci=optv[i].shortk;*ci;ci++) if (*ci==k) return i;
  }
  return -1;
}

/******************************************************************************
 * primitive gets
 *****************************************************************************/
 
const char *Configuration::getOption_str(const char *k) const {
  int oid=findOption(k);
  if (oid<0) return "";
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: return optv[oid]._str.v;
    case SITTER_CFGTYPE_INT: return intToString(optv[oid]._int.v);
    case SITTER_CFGTYPE_FLOAT: return floatToString(optv[oid]._float.v);
    case SITTER_CFGTYPE_BOOL: return boolToString(optv[oid]._bool.v);
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

int Configuration::getOption_int(const char *k) const {
  int oid=findOption(k);
  if (oid<0) return 0;
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: { int v; if (!stringToInt(optv[oid]._str.v,&v)) return 0; return v; }
    case SITTER_CFGTYPE_INT: return optv[oid]._int.v;
    case SITTER_CFGTYPE_FLOAT: return lround(optv[oid]._float.v);
    case SITTER_CFGTYPE_BOOL: return (optv[oid]._bool.v?1:0);
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

double Configuration::getOption_float(const char *k) const {
  int oid=findOption(k);
  if (oid<0) return 0.0;
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: { double v; if (!stringToFloat(optv[oid]._str.v,&v)) return 0.0; return v; }
    case SITTER_CFGTYPE_INT: return optv[oid]._int.v;
    case SITTER_CFGTYPE_FLOAT: return optv[oid]._float.v;
    case SITTER_CFGTYPE_BOOL: return (optv[oid]._bool.v?1.0:0.0);
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

bool Configuration::getOption_bool(const char *k) const {
  int oid=findOption(k);
  if (oid<0) return false;
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: { bool v; if (!stringToBool(optv[oid]._str.v,&v)) return false; return v; }
    case SITTER_CFGTYPE_INT: return optv[oid]._int.v;
    case SITTER_CFGTYPE_FLOAT: return (optv[oid]._float.v!=0.0);
    case SITTER_CFGTYPE_BOOL: return optv[oid]._bool.v;
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

int Configuration::getOptionType(const char *k) const {
  int oid=findOption(k);
  if (oid<0) return SITTER_CFGTYPE_NONE;
  return optv[oid].type;
}

/******************************************************************************
 * primitive sets
 *****************************************************************************/
 
#define CLAMPV(val,opt) (((val)<opt.lo)?opt.lo:((val)>opt.hi)?opt.hi:(val))
 
void Configuration::setOption_str(const char *k,const char *v) {
  int oid=findOption(k);
  if (oid<0) sitter_throw(KeyError,"%s",k);
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: {
        if (optv[oid]._str.v) free(optv[oid]._str.v);
        if (!v||!v[0]) { optv[oid]._str.v=NULL; return; }
        if (!(optv[oid]._str.v=strdup(v))) sitter_throw(AllocationError,"");
      } break;
    case SITTER_CFGTYPE_INT: { 
        int vv; if (!stringToInt(v,&vv)) sitter_throw(ValueError,"%s=%s",k,v); 
        optv[oid]._int.v=CLAMPV(vv,optv[oid]._int); 
      } break;
    case SITTER_CFGTYPE_FLOAT: { 
        double vv; if (!stringToFloat(v,&vv)) sitter_throw(ValueError,"%s=%s",k,v); 
        optv[oid]._float.v=CLAMPV(vv,optv[oid]._float); 
      } break;
    case SITTER_CFGTYPE_BOOL: { 
        bool vv; if (!stringToBool(v,&vv)) sitter_throw(ValueError,"%s=%s",k,v); 
        optv[oid]._bool.v=vv; 
      } break;
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

void Configuration::setOption_int(const char *k,int v) {
  int oid=findOption(k);
  if (oid<0) sitter_throw(KeyError,"%s",k);
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: {
        if (optv[oid]._str.v) free(optv[oid]._str.v);
        if (!(optv[oid]._str.v=strdup(intToString(v)))) sitter_throw(AllocationError,"");
      } break;
    case SITTER_CFGTYPE_INT: optv[oid]._int.v=CLAMPV(v,optv[oid]._int); break;
    case SITTER_CFGTYPE_FLOAT: optv[oid]._float.v=CLAMPV(v,optv[oid]._float); break;
    case SITTER_CFGTYPE_BOOL: optv[oid]._bool.v=v; break;
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

void Configuration::setOption_float(const char *k,double v) {
  int oid=findOption(k);
  if (oid<0) sitter_throw(KeyError,"%s",k);
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: {
        if (optv[oid]._str.v) free(optv[oid]._str.v);
        if (!(optv[oid]._str.v=strdup(floatToString(v)))) sitter_throw(AllocationError,"");
      } break;
    case SITTER_CFGTYPE_INT: optv[oid]._int.v=lround(CLAMPV(v,optv[oid]._int)); break;
    case SITTER_CFGTYPE_FLOAT: optv[oid]._float.v=CLAMPV(v,optv[oid]._float); break;
    case SITTER_CFGTYPE_BOOL: optv[oid]._bool.v=(v!=0.0); break;
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

void Configuration::setOption_bool(const char *k,bool v) {
  int oid=findOption(k);
  if (oid<0) sitter_throw(KeyError,"%s",k);
  switch (optv[oid].type) {
    case SITTER_CFGTYPE_STR: {
        if (optv[oid]._str.v) free(optv[oid]._str.v);
        if (!(optv[oid]._str.v=strdup(boolToString(v)))) sitter_throw(AllocationError,"");
      } break;
    case SITTER_CFGTYPE_INT: optv[oid]._int.v=(v?optv[oid]._int.hi:0); break;
    case SITTER_CFGTYPE_FLOAT: optv[oid]._float.v=(v?optv[oid]._float.hi:0.0); break;
    case SITTER_CFGTYPE_BOOL: optv[oid]._bool.v=v; break;
    default: sitter_throw(IdiotProgrammerError,"config option '%s' type=%d",k,optv[oid].type);
  }
}

#undef CLAMPV

/******************************************************************************
 * set from "k=v" string
 *****************************************************************************/
 
void Configuration::readOption(const char *line) {
  int kstart=0,klen=0,vstart=-1,vlen=0;
  for (int i=0;line[i];i++) {
    if (vstart<0) {
      if (line[i]==KV_SEPARATOR) {
        vstart=i+1;
        while (klen&&isspace(line[klen-1])) klen--;
      } else if ((kstart==i)&&isspace(line[i])) kstart++;
      else klen++;
    } else {
      if ((vstart==i)&&isspace(line[i])) vstart++;
      else vlen++;
    }
  }
  if (klen>=KEY_LEN_LIMIT) {
    //sitter_throw(ArgumentError,"key too long (%d, limit=%d)",klen,KEY_LEN_LIMIT-1);
    return;
  }
  if (!klen) sitter_throw(ArgumentError,"empty key");
  while (vlen&&isspace(line[vstart+vlen-1])) vlen--;
  char k[KEY_LEN_LIMIT];
  char *kk=NULL;
  memcpy(k,line+kstart,klen);
  k[klen]=0;
  char *v;
  if (vstart>=0) {
    if (!(v=(char*)malloc(vlen+1))) sitter_throw(AllocationError,"");
    if (vlen) memcpy(v,line+vstart,vlen);
    v[vlen]=0;
  } else {
    if ((klen>3)&&(k[0]=='n')&&(k[1]=='o')&&(k[2]=='-')) {
      kk=k+3;;
      if (!(v=strdup(DEFAULT_NO_VALUE))) sitter_throw(AllocationError,"");
    } else if (!(v=strdup(DEFAULT_VALUE))) sitter_throw(AllocationError,"");
  }
  try {
    setOption_str(kk?kk:k,v);
  } catch (KeyError) {
    addOption(kk?kk:k,"",SITTER_CFGTYPE_STR,v);
  } catch (...) { free(v); throw; }
  free(v);
}

/******************************************************************************
 * Read a file.
 * Each line is "k","k=v",or blank.
 * Anything after '#' is ignored.
 * '\' at end of line continues line.
 *****************************************************************************/
 
void Configuration::readFile(const char *path) {
  File *f;
  try { f=sitter_open(path,"r"); }
  catch (...) { return; }
  //sitter_log("reading configuration from '%s'...",path);
  int lineno=0;
  try {
    while (char *line=f->readLine(true,true,true,true,&lineno)) {
      readOption(line);
      free(line);
    }
  } catch (ArgumentError) {
    sitter_prependErrorMessage("%s:%d: ",path,lineno);
    delete f;
    throw;
  }
  delete f;
}

/******************************************************************************
 * Read a list of files
 *****************************************************************************/
 
void Configuration::readFiles(const char *pathlist) {
  char **pathv=sitter_strsplit_selfDescribing(pathlist);
  for (char **pathi=pathv;*pathi;pathi++) {
    readFile(*pathi);
    free(*pathi);
  }
  free(pathv);
}

/******************************************************************************
 * Read command line
 *****************************************************************************/
 
void Configuration::readArgv(int argc,char **argv) {
  for (int i=1;i<argc;i++) {
    if ((argv[i][0]=='-')&&(argv[i][1]!='-')) { // short options
      bool v=true;
      for (const char *sopt=argv[i]+1;*sopt;sopt++) {
        if (*sopt==NEGATIVE_SHORT_OPTION) { v=false; continue; }
        int oid=findShortOption(*sopt);
        if (oid<0) sitter_throw(ArgumentError,"unknown short option '%c'",*sopt);
        setOption_bool(optv[oid].k,v); // TODO -- we've looked it up twice
        v=true;
      }
    } else { // long options
      int skip=0; while (argv[i][skip]=='-') skip++;
      try { readOption(argv[i]+skip); }
      catch (KeyError) { sitter_throw(ArgumentError,"unknown long option '%s'",argv[i]+skip); }
      catch (ValueError) { sitter_throw(ArgumentError,"error parsing '%s', inappropriate value",argv[i]+skip); }
    }
  }
}

/******************************************************************************
 * save
 *****************************************************************************/
 
void Configuration::save(const char *path) {
  //sitter_log("saving configuration to '%s'...",path);
  sitter_mkdirp(path);
  if (File *f=sitter_open(path,"w")) {
  
    for (int i=0;i<optc;i++) {
      switch (optv[i].type) {
        case SITTER_CFGTYPE_STR: f->writef("%s=%s\n",optv[i].k,optv[i]._str.v); break;
        case SITTER_CFGTYPE_INT: f->writef("%s=%d\n",optv[i].k,optv[i]._int.v); break;
        case SITTER_CFGTYPE_FLOAT: f->writef("%s=%f\n",optv[i].k,optv[i]._float.v); break;
        case SITTER_CFGTYPE_BOOL: f->writef("%s=%s\n",optv[i].k,optv[i]._bool.v?"true":"false"); break;
      }
    }
  
    delete f;
  } else sitter_throw(IOError,"save config '%s' -- open failed",path);
}
