#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <zlib.h>
#include "sitter_Error.h"
#include "sitter_byteorder.h"
#include "sitter_File.h"
#include "sitter_ResourceManager.h"
#include "sitter_surfacecvt.h"

static uint32_t getPixel(const void *src,int x,int y,int w,int fmt) {
  switch (fmt) {
    case SITTER_TEXFMT_RGBA8888: {
        int offset=(y*w+x)*4;
        return sitter_get32be(src,offset);
      }
    case SITTER_TEXFMT_RGB888: {
        int offset=(y*w+x)*3;
        return ((sitter_get8(src,offset)<<24)|(sitter_get8(src,offset+1)<<16)|(sitter_get8(src,offset+2)<<8)|0xff);
      }
    case SITTER_TEXFMT_N1: {
        int stride=(w+7)>>3;
        int offset=y*stride+(x>>3);
        int mask=0x80>>(x&7);
        return (sitter_get8(src,offset)&mask)?0xffffffff:0x00000000;
      }
    case SITTER_TEXFMT_GXRGBA8888: {
        int col=x>>2,row=y>>2,colc=w>>2;
        int celloffset=(row*colc+col)*64;
        int subx=x&3,suby=y&3;
        int offset=celloffset+(suby<<3)+(subx<<1);
        int r=sitter_get8(src,offset+1);
        int g=sitter_get8(src,offset+32);
        int b=sitter_get8(src,offset+33);
        int a=sitter_get8(src,offset);
        return (r<<24)|(g<<16)|(b<<8)|a;
      }
    case SITTER_TEXFMT_GXRGB565: {
        int col=x>>2,row=y>>2,colc=w>>2;
        int celloffset=(row*colc+col)*32;
        int subx=x&3,suby=y&3;
        int offset=celloffset+(suby<<3)+(subx<<1);
        uint32_t px=sitter_get16be(src,offset);
        return ((px&0xf800)<<16)|((px&0xe000)<<11)|((px&0x07e0)<<13)|((px&0x0600)<<7)|((px&0x001f)<<3)|((px&0x001c)>>2);
      }
    case SITTER_TEXFMT_GXN4: {
        int col=x>>3,row=y>>3,colc=w>>3;
        int celloffset=(row*colc+col)*32;
        int subx=x&7,suby=y&7;
        int offset=celloffset+(suby<<2)+(subx>>1);
        uint32_t px=sitter_get8(src,offset);
        if (subx&1) px=((px&0x0f)<<4)|(px&0x0f);
        else px=((px&0xf0)>>4)|(px&0xf0);
        return (px<<24)|(px<<16)|(px<<8)|px;
      }
  }
  return 0;
}

static void setPixel(void *dst,int x,int y,int w,uint32_t px,int fmt) {
  switch (fmt) {
    case SITTER_TEXFMT_RGBA8888: {
        int offset=(y*w+x)*4;
        sitter_set32be(dst,offset,px);
      } return;
    case SITTER_TEXFMT_RGB888: {
        int offset=(y*w+x)*3;
        sitter_set8(dst,offset,px>>24); offset++;
        sitter_set8(dst,offset,px>>16); offset++;
        sitter_set8(dst,offset,px>>8);
      } return;
    case SITTER_TEXFMT_N1: {
        int stride=(w+7)>>3;
        int offset=y*stride+(x>>3);
        int mask=0x80>>(x&7);
        if (px) sitter_set8(dst,offset,sitter_get8(dst,offset)|mask);
        else sitter_set8(dst,offset,sitter_get8(dst,offset)&~mask);
      } return;
    case SITTER_TEXFMT_GXRGBA8888: {
        int col=x>>2,row=y>>2,colc=w>>2;
        int celloffset=(row*colc+col)*64;
        int subx=x&3,suby=y&3;
        int offset=celloffset+(suby<<3)+(subx<<1);
        sitter_set8(dst,offset+1,px>>24);
        sitter_set8(dst,offset+32,px>>16);
        sitter_set8(dst,offset+33,px>>8);
        sitter_set8(dst,offset,px);
      } return;
    case SITTER_TEXFMT_GXRGB565: {
        int col=x>>2,row=y>>2,colc=w>>2;
        int celloffset=(row*colc+col)*32;
        int subx=x&3,suby=y&3;
        int offset=celloffset+(suby<<3)+(subx<<1);
        px=((px>>16)&0xf800)|((px>>13)&0x07e0)|((px>>11)&0x001f);
        sitter_set16be(dst,offset,px);
      } return;
    case SITTER_TEXFMT_GXN4: {
        int col=x>>3,row=y>>3,colc=w>>3;
        int celloffset=(row*colc+col)*32;
        int subx=x&7,suby=y&7;
        int offset=celloffset+(suby<<2)+(subx>>1);
        if (subx&1) sitter_set8(dst,offset,(sitter_get8(dst,offset)&0xf0)|((px>>4)&0x0f));
        else sitter_set8(dst,offset,(sitter_get8(dst,offset)&0x0f)|(px&0xf0));
      } return;
  }
}

static void extractImageSegment(void *dst,const void *src,int col,int row,int colw,int rowh,int colc,int fmt) {
  int srcyoff=row*rowh;
  int srcxoff=col*colw;
  int srcw=colw*colc;
  for (int y=0;y<rowh;y++) {
    for (int x=0;x<colw;x++) {
      uint32_t px=getPixel(src,x+srcxoff,y+srcyoff,srcw,fmt);
      setPixel(dst,x,y,colw,px,fmt);
    }
  }
}

int main(int argc,char **argv) {
  try {
    int koutfmt=-1;
    bool enumerate_only=true;
    bool do_segments=false;
    int colw=32,rowh=32;
    bool do_compress=true;
    bool togx=true;
    const char *outpfx="";
    bool dont_convert=false;
    
    for (int i=1;i<argc;i++) {
      if (argv[i][0]=='-') {
      
        #define NUMBER(v,s,pos) { \
            char *tail=0; \
            int n=strtol(s,&tail,0); \
            if (!tail||tail[0]) sitter_throw(ArgumentError,"expected number, found '%s'",s); \
            if (pos&&(n<1)) sitter_throw(ArgumentError,"expected positive integer, found %d",n); \
            v=n; \
          }
        if (!strcmp(argv[i]+1,"enum")) enumerate_only=true;
        else if (!strcmp(argv[i]+1,"cvt")) enumerate_only=false;
        else if (!strncmp(argv[i]+1,"colw=",5)) NUMBER(colw,argv[i]+6,true)
        else if (!strncmp(argv[i]+1,"rowh=",5)) NUMBER(rowh,argv[i]+6,true)
        else if (!strcmp(argv[i]+1,"togx")) togx=true;
        else if (!strcmp(argv[i]+1,"fromgx")) togx=false;
        else if (!strcmp(argv[i]+1,"compress")) do_compress=true;
        else if (!strcmp(argv[i]+1,"no-compress")) do_compress=false;
        else if (!strncmp(argv[i]+1,"fmt=",4)) { NUMBER(koutfmt,argv[i]+5,false) dont_convert=false; }
        else if (!strcmp(argv[i]+1,"seg")) { do_segments=true; enumerate_only=false; }
        else if (!strcmp(argv[i]+1,"single")) do_segments=false;
        else if (!strncmp(argv[i]+1,"out=",4)) outpfx=argv[i]+5;
        else if (!strcmp(argv[i]+1,"samefmt")) dont_convert=true;
        else sitter_throw(ArgumentError,"unexpected option '%s'",argv[i]+1);
        #undef NUMBER
      
      } else try {
        int w,h,fmt; void *pixels;
        File *f=sitter_open(argv[i],"rb");
        try {
          uint8_t signature[8];
          if (f->read(signature,8)!=8) throw ShortIOError();
          if (enumerate_only) {
            if (!memcmp(signature,"\x89PNG\r\n\x1a\n",8)) {
              if (f->read(signature,8)!=8) throw ShortIOError();
              uint32_t w=f->read32be();
              uint32_t h=f->read32be();
              if ((w%colw)||(h%rowh)) sitter_throw(FormatError,"image %dx%d, not aligned to cells of %dx%d",w,h,colw,rowh);
              printf("%d %d\n",w/colw,h/rowh);
            } else if (!memcmp(signature,"STR\0TEX\0",8)) {
              f->skip(2);
              uint32_t w=f->read32be();
              uint32_t h=f->read32be();
              if ((w%colw)||(h%rowh)) sitter_throw(FormatError,"image %dx%d, not aligned to cells of %dx%d",w,h,colw,rowh);
              printf("%d %d\n",w/colw,h/rowh);
            } else sitter_throw(FormatError,"unknown format");
            delete f;
            continue;
          } else {
            if (!memcmp(signature,"\x89PNG\r\n\x1a\n",8)) ResourceManager::loadImage_PNG(f,&fmt,&w,&h,&pixels);
            else if (!memcmp(signature,"STR\0TEX\0",8)) ResourceManager::loadImage_STR0TEX0(f,&fmt,&w,&h,&pixels);
            else sitter_throw(FormatError,"unknown format");
          }
        } catch (ShortIOError) {
          delete f;
          sitter_throw(FormatError,"unexpected end of file");
        } catch (...) { delete f; throw; }
        delete f;
        
        int outfmt=koutfmt;
        if (dont_convert) outfmt=fmt;
        if (outfmt<0) if (togx) switch (fmt) {
          case SITTER_TEXFMT_RGBA8888: outfmt=SITTER_TEXFMT_GXRGBA8888; break;
          case SITTER_TEXFMT_RGB888: outfmt=SITTER_TEXFMT_GXRGB565; break;
          case SITTER_TEXFMT_N1: outfmt=SITTER_TEXFMT_GXN4; break;
        } else switch (fmt) {
          case SITTER_TEXFMT_GXRGBA8888: outfmt=SITTER_TEXFMT_RGBA8888; break;
          case SITTER_TEXFMT_GXRGB565: outfmt=SITTER_TEXFMT_RGB888; break;
          case SITTER_TEXFMT_GXN4: outfmt=SITTER_TEXFMT_N1; break;
        }
        void *outpixels=pixels;
        int outlen;
        if (fmt!=outfmt) {
          void (*cvtfn)(void *,const void *,int,int);
          switch (outfmt) {
            case SITTER_TEXFMT_RGBA8888: {
                if (fmt!=SITTER_TEXFMT_GXRGBA8888) sitter_throw(IdiotProgrammerError,"%d=>%d, conversion only to like formats",fmt,outfmt);
                cvtfn=sitter_gxdeinterlace_rgba8888_rgba8888;
                outlen=w*h*4; 
              } break;
            case SITTER_TEXFMT_RGB888: {
                if (fmt!=SITTER_TEXFMT_GXRGB565) sitter_throw(IdiotProgrammerError,"%d=>%d, conversion only to like formats",fmt,outfmt);
                cvtfn=sitter_gxdeinterlace_rgb565_rgb888;
                outlen=w*h*3; 
              } break;
            case SITTER_TEXFMT_N1: {
                if (fmt!=SITTER_TEXFMT_GXN4) sitter_throw(IdiotProgrammerError,"%d=>%d, conversion only to like formats",fmt,outfmt);
                cvtfn=sitter_gxdeinterlace_n4_n1;
                outlen=((w+7)>>3)*h; 
              } break;
            case SITTER_TEXFMT_GXRGBA8888: {
                if (fmt!=SITTER_TEXFMT_RGBA8888) sitter_throw(IdiotProgrammerError,"%d=>%d, conversion only to like formats",fmt,outfmt);
                cvtfn=sitter_gxinterlace_rgba8888_rgba8888;
                outlen=w*h*4;
              } break;
            case SITTER_TEXFMT_GXRGB565: {
                if (fmt!=SITTER_TEXFMT_RGB888) sitter_throw(IdiotProgrammerError,"%d=>%d, conversion only to like formats",fmt,outfmt);
                cvtfn=sitter_gxinterlace_rgb888_rgb565;
                outlen=w*h*3;
              } break;
            case SITTER_TEXFMT_GXN4: {
                if (fmt!=SITTER_TEXFMT_N1) sitter_throw(IdiotProgrammerError,"%d=>%d, conversion only to like formats",fmt,outfmt);
                cvtfn=sitter_gxinterlace_n1_n4;
                outlen=w*h/2;
              } break;
            default: sitter_throw(IdiotProgrammerError,"outfmt=%d fmt=%d",outfmt,fmt);
          }
          outpixels=malloc(outlen);
          if (!outpixels) sitter_throw(AllocationError,"");
          cvtfn(outpixels,pixels,w,h);
          free(pixels);
        } else switch (fmt) {
          case SITTER_TEXFMT_RGBA8888: outlen=w*h*4; break;
          case SITTER_TEXFMT_RGB888: outlen=w*h*3; break;
          case SITTER_TEXFMT_N1: outlen=((w+7)/8)*h; break;
          case SITTER_TEXFMT_GXRGBA8888: outlen=w*h*4; break;
          case SITTER_TEXFMT_GXRGB565: outlen=w*h*2; break;
          case SITTER_TEXFMT_GXN4: outlen=w*h/2; break;
        }
        
        if (do_segments) { // ---------- output tiles -------------------------
          if (!outpfx||!outpfx[0]) sitter_throw(ArgumentError,"Please indicate output prefix with \"-out=PREFIX\"");
          int colc=w/colw;
          int rowc=h/rowh;
          if ((colc>10)||(rowc>10)) sitter_throw(ArgumentError,"Too many cells (%d x %d). Limit 10 per axis, "
            "as some of my code thinks it's decimal, and some thinks it's hexadecimal. Wow, that was sloppy.",colc,rowc);
          int tilebuflen=colw*rowh;
          switch (outfmt) {
            case SITTER_TEXFMT_RGBA8888: tilebuflen*=4; break;
            case SITTER_TEXFMT_RGB888: tilebuflen*=3; break;
            case SITTER_TEXFMT_N1: tilebuflen=((colw+7)/8)*rowh; break;
            case SITTER_TEXFMT_GXRGBA8888: {
                if ((colw%4)||(rowh%4)) sitter_throw(ArgumentError,"bad cell size %d x %d for GX_TF_RGBA8",colw,rowh);
                tilebuflen*=4;
              } break;
            case SITTER_TEXFMT_GXRGB565: {
                if ((colw%4)||(rowh%4)) sitter_throw(ArgumentError,"bad cell size %d x %d for GX_TF_RGB565",colw,rowh);
                tilebuflen*=2;
              } break;
            case SITTER_TEXFMT_GXN4: {
                if ((colw%8)||(rowh%8)) sitter_throw(ArgumentError,"bad cell size %d x %d for GX_TF_I4",colw,rowh);
                tilebuflen/=2;
              } break;
          }
          void *tilebuf=malloc(tilebuflen);
          if (!tilebuf) sitter_throw(AllocationError,"");
          try {
            int outpfxlen=0; while (outpfx[outpfxlen]) outpfxlen++;
            char *outpath=(char*)malloc(outpfxlen+9); // "-RC.png\0"
            if (!outpath) sitter_throw(AllocationError,"");
            try {
              uLongf clen=compressBound(tilebuflen);
              void *cdata=NULL;
              if (do_compress) {
                cdata=malloc(clen);
                if (!cdata) sitter_throw(AllocationError,"");
              }
              for (int row=0;row<rowc;row++) {
                for (int col=0;col<colc;col++) {
                  sprintf(outpath,"%s-%d%d.png",outpfx,row,col);
                  extractImageSegment(tilebuf,outpixels,col,row,colw,rowh,colc,outfmt);
                  f=sitter_open(outpath,"wb");
                  try {
                    if (f->write("STR\0TEX\0",8)!=8) throw ShortIOError();
                    f->write8(outfmt);
                    f->write8(do_compress?1:0);
                    f->write32be(colw);
                    f->write32be(rowh);
                    if (f->write("\0\0\0\0\0\0\0\0",8)!=8) throw ShortIOError();
                    if (do_compress) {
                      clen=compressBound(tilebuflen);
                      if (int err=compress((Bytef*)cdata,&clen,(Bytef*)tilebuf,tilebuflen)) sitter_throw(ZError,"compress: %d",err);
                      if (f->write(cdata,clen)!=clen) throw ShortIOError();
                    } else {
                      if (f->write(outpixels,outlen)!=outlen) throw ShortIOError();
                    }
                  } catch (...) { delete f; throw; }
                  delete f;
                }
              }
              if (cdata) free(cdata);
            } catch (...) { free(outpath); throw; }
            free(outpath);
          } catch (...) { free(tilebuf); throw; }
          free(tilebuf);
        
        } else { // -------------------- output converted image in place ------
          f=sitter_open((outpfx&&outpfx[0])?outpfx:argv[i],"wb");
          try {
            if (f->write("STR\0TEX\0",8)!=8) throw ShortIOError();
            f->write8(outfmt);
            f->write8(do_compress?1:0);
            f->write32be(w);
            f->write32be(h);
            if (f->write("\0\0\0\0\0\0\0\0",8)!=8) throw ShortIOError();
            if (do_compress) {
              uLongf clen=compressBound(outlen);
              void *cdata=malloc(clen);
              if (!cdata) sitter_throw(AllocationError,"");
              if (compress((Bytef*)cdata,&clen,(Bytef*)outpixels,outlen)) sitter_throw(ZError,"compress");
              if (f->write(cdata,clen)!=clen) throw ShortIOError();
              free(cdata);
            } else {
              if (f->write(outpixels,outlen)!=outlen) throw ShortIOError();
            }
          } catch (...) { delete f; throw; }
          delete f;
        }
        free(outpixels);
        
      } catch (...) {
        sitter_log("'%s' failed",argv[i]);
        sitter_printError();
      }
    }
    
  } catch (Error) {
    sitter_printError();
    return 1;
  }
  return 0;
}
