#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#ifndef SITTER_WIN32
  #include <wordexp.h>
#endif
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include "sitter_File.h"

/******************************************************************************
 * stream (fopen,etc) file
 *****************************************************************************/
 
class StreamFile : public File {
  FILE *f;
  char *path;
public:
  
  /* I take ownership of <path> (that happens to be convenient wrt sitter_open).
   */
  StreamFile(char *path,const char *mode) {
    if (!path) sitter_throw(IdiotProgrammerError,"StreamFile path==NULL");
    if (!(f=fopen(path,mode))) switch (errno) {
      case ENOENT: sitter_throw(FileNotFoundError,"%s",path);
      case EACCES: sitter_throw(FileAccessError,"%s",path);
      default: sitter_throw(IOError,"open %s",path);
    }
    this->path=path;
  }
  
  ~StreamFile() {
    fclose(f);
    if (path) free(path);
  }
  
  int read(void *dst,int len) {
    if (len<=0) return 0;
    int rtn=fread(dst,1,len,f);
    if (rtn<0) sitter_throw(IOError,"read %s (%d)",path,len);
    return rtn;
  }
  
  int write(const void *src,int len) {
    if (len<=0) return 0;
    int rtn=fwrite(src,1,len,f);
    if (rtn<0) sitter_throw(IOError,"write %s (%d)",path,len);
    return rtn;
  }
  
  int64_t seek(int64_t where,int whence) {
    #ifdef SITTER_WIN32
      if (fseek(f,where,whence)<0) sitter_throw(IOError,"seek %s (%d,%d)",path,(int)where,whence);
      return ftell(f);
    #else
      if (fseeko(f,where,whence)<0) sitter_throw(IOError,"seek %s (%d,%d)",path,(int)where,whence);
      return ftello(f);
    #endif
  }
  
};

/******************************************************************************
 * line reader
 *****************************************************************************/
 
char *File::readLine(bool strip,bool skip,bool cont,bool comment,int *lineno) {
  int c=0,a=255;
  char *s=(char*)malloc(a+1);
  if (!s) sitter_throw(AllocationError,"");
  bool eof=false;
  #define APPEND(ch) { \
      if (c>=a) { \
        a+=256; \
        if (!(s=(char*)realloc(s,a+1))) sitter_throw(AllocationError,""); \
      } \
      s[c++]=ch; \
    }
  do {
    try {
      bool lead=true,esc=false,cmt=false;
      while (1) {
        uint8_t inch=read8();
        if (!inch) continue;
        if (inch=='\n') { if (lineno) (*lineno)++; if (esc&&cont) { esc=false; continue; } break; }
        if (inch=='\\') {
          if (esc) { APPEND('\\') APPEND('\\') esc=false; continue; }
          esc=true; continue;
        }
        if (esc) { APPEND('\\') APPEND(inch) esc=false; continue; }
        esc=false;
        if ((inch=='#')&&comment) { cmt=true; continue; }
        if (cmt) continue;
        if (lead&&strip) {
          if (isspace(inch)) continue;
          lead=false;
        }
        APPEND(inch)
      }
    } catch (ShortIOError) { eof=true; }
    if (strip) while (c&&isspace(s[c-1])) c--;
  } while (!eof&&skip&&!c);
  if (eof&&!c) { free(s); return NULL; }
  s[c]=0;
  return s;
  #undef APPEND
}

/******************************************************************************
 * formatter
 *****************************************************************************/
#if 1 // QQ
int File::writef(const char *fmt,...) {
  if (!fmt||!fmt[0]) return 0;
  va_list vargs;
  va_start(vargs,fmt);
  int len=vsnprintf(NULL,0,fmt,vargs);
  if (!len) return 0;
  char *buf=(char*)malloc(len+1);
  if (!buf) sitter_throw(AllocationError,"");
  va_start(vargs,fmt);
  vsprintf(buf,fmt,vargs);
  int rtn;
  try { rtn=write(buf,len); }
  catch (...) { free(buf); throw; }
  free(buf);
  if (rtn!=len) sitter_throw(ShortIOError,"");
  return rtn;
}
#endif

/******************************************************************************
 * path parser
 *****************************************************************************/
 
char *sitter_resolvePath(const char *path) {
  #if defined(SITTER_LINUX_SDL) || defined(SITTER_LINUX_DRM) // wordexp is available, so use it.
    wordexp_t wet={0};
    switch (int err=wordexp(path,&wet,WRDE_NOCMD)) {
      case 0: break;
      case WRDE_SYNTAX:
      case WRDE_CMDSUB:
      case WRDE_BADCHAR: { // maybe an unusual file name (with | or $ in it?), send it back verbatim
          wordfree(&wet);
          char *rtn=strdup(path);
          if (!rtn) sitter_throw(AllocationError,"");
          return rtn;
        }
      case WRDE_NOSPACE: {
          wordfree(&wet);
          sitter_throw(AllocationError,"");
        }
      default: {
          wordfree(&wet);
          sitter_throw(Error,"wordexp failed with unknown code %d",err);
        }
    }
    if (wet.we_wordc!=1) { // we can't consistently use 0 or 2+ paths; send it back verbatim
      wordfree(&wet);
      char *rtn=strdup(path);
      if (!rtn) sitter_throw(AllocationError,"");
      return rtn;
    }
    char *rtn=wet.we_wordv[0]; // this is safe, right?
    wet.we_wordv[0]=NULL;
    wordfree(&wet);
    return rtn;
  #else // wii and windows. make sure to trim spaces from the path; some grids mistakenly have trailing spaces in the file names
    while (path[0]&&isspace(path[0])) path++;
    int pathlen=0; while (path[pathlen]) pathlen++;
    while (pathlen&&isspace(path[pathlen-1])) pathlen--;
    char *outpath=(char*)malloc(pathlen+1);
    if (!outpath) sitter_throw(AllocationError,"");
    if (pathlen) memcpy(outpath,path,pathlen);
    outpath[pathlen]=0;
    return outpath;
  #endif
}

/******************************************************************************
 * open dispatcher
 *****************************************************************************/

File *sitter_open(const char *path,const char *mode) {
  char *rpath=sitter_resolvePath(path);
  File *rtn;
  try { rtn=new StreamFile(rpath,mode); }
  catch (...) { free(rpath); throw; }
  return rtn;
}

/******************************************************************************
 * recursive mkdir (think "mkdir -p")
 *****************************************************************************/
 
void sitter_mkdirp(const char *path) {
  if (!path||!path[0]) return;
  char *vpath=sitter_resolvePath(path);
  try {
    int slash=-1; for (int i=0;vpath[i];i++) if (vpath[i]=='/') slash=i;
    if (slash<=0) { free(vpath); return; } // yes, "<=" -- don't work with empty paths
    vpath[slash]=0;
    #ifdef SITTER_WIN32
      if (mkdir(vpath)) {
    #else
      if (mkdir(vpath,0777)) {
    #endif
      if (errno==ENOENT) {
        sitter_mkdirp(vpath);
        #ifdef SITTER_WIN32
          if (mkdir(vpath)) sitter_throw(IOError,"mkdir failed");
        #else
          if (mkdir(vpath,0777)) sitter_throw(IOError,"mkdir failed");
        #endif
      } else if (errno==EEXIST) { // my work on this planet is done
        free(vpath);
        return;
      } else sitter_throw(IOError,"mkdir failed");
    }
  } catch (...) { free(vpath); throw; }
  free(vpath);
}

/******************************************************************************
 * sitter_listDir
 *****************************************************************************/
 
char **sitter_listDir(const char *path,uint32_t flags) {
  char *rpath=sitter_resolvePath(path);
  try {
    DIR *dir=opendir(rpath);
    if (!dir) switch (errno) {
      case ENOENT: sitter_throw(FileNotFoundError,"%s",rpath);
      case EACCES: sitter_throw(FileAccessError,"%s",rpath);
      default: sitter_throw(IOError,"opendir '%s'",rpath);
    }
    try {
    
      int c=0,a=63;
      char **v=(char**)malloc(sizeof(void*)*(a+1));
      if (!v) sitter_throw(AllocationError,"");
      
      while (struct dirent *de=readdir(dir)) {
        if (de->d_name[0]=='.') continue;
        bool slash=false;
        #ifdef _DIRENT_HAVE_D_TYPE
          switch (de->d_type) {
            case DT_DIR: {
                if (!(flags&SITTER_LISTDIR_SHOWDIR)) continue;
                if (flags&SITTER_LISTDIR_DIRSLASH) slash=true;
              } break;
            case DT_REG: if (!(flags&SITTER_LISTDIR_SHOWFILE)) continue; break;
            default: if (!(flags&SITTER_LISTDIR_SHOWOTHER)) continue; break;
          }
        #endif
        int namelen=0; while (de->d_name[namelen]) namelen++;
        char *fname;
        if (slash) { 
          if (!(fname=(char*)malloc(namelen+2))) sitter_throw(AllocationError,"");
          memcpy(fname,de->d_name,namelen);
          fname[namelen]='/';
          fname[namelen+1]=0;
        } else {
          if (!(fname=(char*)malloc(namelen+1))) sitter_throw(AllocationError,"");
          memcpy(fname,de->d_name,namelen);
          fname[namelen]=0;
        }
        int before=c;
        if (c&&(flags&SITTER_LISTDIR_SORT)) {
          int lo=0,hi=c,ck=c>>1;
          while (lo<hi) {
            bool match=false;
            int cmp=strcasecmp(fname,v[ck]);
            if (cmp<0) hi=ck;
            else if (cmp>0) lo=ck+1;
            else match=true;
            if (match) { lo=ck; break; }
            ck=(lo+hi)>>1;
          }
          before=lo;
        }
        if (c>=a) {
          a+=64;
          if (!(v=(char**)realloc(v,sizeof(void*)*(a+1)))) sitter_throw(AllocationError,"");
        }
        if (before<c) memmove(v+before+1,v+before,sizeof(void*)*(c-before));
        v[before]=fname;
        c++;
      }
       
      free(rpath);
      closedir(dir);
      v[c]=NULL;
      return v;
      
    } catch (...) { closedir(dir); throw; }
  } catch (...) { free(rpath); throw; }
}
