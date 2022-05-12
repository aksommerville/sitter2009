#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "sitter_Error.h"
#include "sitter_string.h"

/******************************************************************************
 * comparison
 *****************************************************************************/
 
int sitter_strlenicmp(const char *a,const char *b) {
  int rtn=0;
  while (*a&&*b) {
    uint8_t cha=*a++; if ((cha>=0x41)&&(cha<=0x5a)) cha+=0x20;
    uint8_t chb=*b++; if ((chb>=0x41)&&(chb<=0x5a)) chb+=0x20;
    if (cha!=chb) return rtn;
    rtn++;
  }
  return rtn;
}

/******************************************************************************
 * split lists
 *****************************************************************************/
 
char **sitter_strsplit_selfDescribing(const char *s) {
  if (!s||!s[0]) {
    char **rtn=(char**)malloc(sizeof(void*));
    if (!rtn) sitter_throw(AllocationError,"");
    *rtn=NULL;
    return rtn;
  }
  int sc=0,sa=15;
  char **sv=(char**)malloc(sizeof(void*)*(sa+1));
  if (!sv) sitter_throw(AllocationError,"");
  char delim=*s++;
  int start=0,len=0,ip=0;
  while (1) {
    if (!s[ip]||(s[ip]==delim)) {
      if (sc>=sa) {
        sa+=16;
        if (!(sv=(char**)realloc(sv,sizeof(void*)*(sa+1)))) sitter_throw(AllocationError,"");
      }
      if (!(sv[sc]=(char*)malloc(len+1))) sitter_throw(AllocationError,"");
      if (len) memcpy(sv[sc],s+start,len);
      sv[sc][len]=0;
      sc++;
      start=ip+1;
      len=0;
      if (!s[ip]) break;
      ip++;
      continue;
    }
    len++;
    ip++;
  }
  sv[sc]=NULL;
  return sv;
}

char **sitter_strsplit_white(const char *s) {
  if (!s||!s[0]) {
    char **rtn=(char**)malloc(sizeof(void*));
    if (!rtn) sitter_throw(AllocationError,"");
    *rtn=NULL;
    return rtn;
  }
  int sc=0,sa=15;
  char **sv=(char**)malloc(sizeof(void*)*(sa+1));
  if (!sv) sitter_throw(AllocationError,"");
  int start=0,len=0,ip=0;
  while (1) {
    if (!s[ip]||isspace(s[ip])) {
      if (len) {
        if (sc>=sa) {
          sa+=16;
          if (!(sv=(char**)realloc(sv,sizeof(void*)*(sa+1)))) sitter_throw(AllocationError,"");
        }
        if (!(sv[sc]=(char*)malloc(len+1))) sitter_throw(AllocationError,"");
        memcpy(sv[sc],s+start,len);
        sv[sc][len]=0;
        sc++;
        start=ip+1;
        len=0;
      } else start=ip+1;
      if (!s[ip]) break;
      ip++;
      continue;
    }
    len++;
    ip++;
  }
  sv[sc]=NULL;
  return sv;
}

/******************************************************************************
 * hash
 *****************************************************************************/
 
uint32_t sitter_strhash(const char *s,int slen) {
  if (slen<0) { slen=0; while (s[slen]) slen++; }
  uint32_t  hash=0;
  while (slen>4) {
    hash^=*(int32_t*)s;
    hash=((hash&1)?0x80000000u:0)|(hash>>1);
    s+=4; slen-=4;
  }
  while (slen) {
    hash^=*(int8_t*)s;
    hash=((hash&1)?0x80000000u:0)|(hash>>1);
    s++; slen--;
  }
  return hash;
}
