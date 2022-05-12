#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

char *sitter_err_msg=NULL;
int sitter_err_no=0;
const char *sitter_err_file=NULL;
const char *sitter_err_func=NULL;
int sitter_err_line=0;

#ifdef SITTER_WII // try really hard to save the error message to SD card

#include "sitter_File.h"

void sitter_printError() {
  File *f=NULL;
  f=sitter_open("sd0:/apps/sitter/sitter.log","ab");
  if (!f) return;
  f->writef("Sitter error: %s\n",sitter_err_msg?sitter_err_msg:"<no message>");
  f->writef("              at %s:%d (%s)\n",sitter_err_file,sitter_err_line,sitter_err_func);
  delete f;
}

void sitter_log(const char *fmt,...) {
  try {
    File *f=sitter_open("sd0:/apps/sitter/sitter.log","ab");
    if (!f) return;
    va_list vargs;
    va_start(vargs,fmt);
    int len=vsnprintf(NULL,0,fmt,vargs);
    char *buf=(char*)malloc(len+2);
    if (!buf) { f->write("sitter_log: OUT OF MEMORY\n",26); delete f; return; }
    va_start(vargs,fmt);
    vsprintf(buf,fmt,vargs);
    buf[len]='\n'; buf[len+1]=0;
    f->write(buf,len+1);
    free(buf);
    delete f;
  } catch (...) {
    return;
  }
}

#else

void sitter_printError() {
  #define OUTFILE stdout
  if (!sitter_err_msg&&!sitter_err_no) { 
    fprintf(OUTFILE,"sitter_printError: no error logged\n");
    return;
  }
  fprintf(OUTFILE,"Sitter error: %s\n",sitter_err_msg?sitter_err_msg:"<no message>");
  fprintf(OUTFILE,"              errno=%d (%s)\n",sitter_err_no,strerror(sitter_err_no));
  if (sitter_err_file||sitter_err_line||sitter_err_func)
    fprintf(OUTFILE,"              at %s:%d (%s)\n",sitter_err_file,sitter_err_line,sitter_err_func);
  #undef OUTFILE
}

#endif

void sitter_clearError() {
  if (sitter_err_msg) free(sitter_err_msg);
  sitter_err_msg=NULL;
  sitter_err_no=0;
  sitter_err_file=NULL;
  sitter_err_func=NULL;
  sitter_err_line=0;
}

void _sitter_setError(const char *srcfile,const char *srcfunc,int srcline,const char *fmt,...) {
  sitter_err_no=errno;
  sitter_err_file=srcfile;
  sitter_err_func=srcfunc;
  sitter_err_line=srcline;
  if (sitter_err_msg) { free(sitter_err_msg); sitter_err_msg=NULL; }
  if (!fmt||!fmt[0]) return;
  va_list vargs;
  va_start(vargs,fmt);
  int len=vsnprintf(NULL,0,fmt,vargs);
  if (!(sitter_err_msg=(char*)malloc(len+1))) { if (!sitter_err_no) sitter_err_no=ENOMEM; return; }
  va_start(vargs,fmt);
  vsprintf(sitter_err_msg,fmt,vargs);
}

void sitter_prependErrorMessage(const char *fmt,...) {
  if (!fmt||!fmt[0]) return;
  va_list vargs;
  va_start(vargs,fmt);
  int len=vsnprintf(NULL,0,fmt,vargs);
  if (!len) return;
  int prvlen=0; if (sitter_err_msg) while (sitter_err_msg[prvlen]) prvlen++;
  char *nmsg=(char*)malloc(len+prvlen+1);
  if (!nmsg) { if (!sitter_err_no) sitter_err_no=ENOMEM; return; }
  va_start(vargs,fmt);
  vsprintf(nmsg,fmt,vargs);
  if (prvlen) memcpy(nmsg+len,sitter_err_msg,prvlen+1);
  if (sitter_err_msg) free(sitter_err_msg);
  sitter_err_msg=nmsg;
}
