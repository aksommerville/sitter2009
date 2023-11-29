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

/******************************************************************************
 * PNG -- yoinked from sitter_resload_image.cpp
 *****************************************************************************/
 
static void png_row(uint8_t *linebuf,int stride,int colortype,int depth,int peny,uint8_t *rawbuf) {
  uint8_t filter=*linebuf++;
  uint8_t *currrow=rawbuf+peny*stride;
  uint8_t *prevrow;
  if (peny) prevrow=currrow-stride;
  else { prevrow=currrow; memset(prevrow,0,stride); } // a little trickery so we don't need to check peny again
  int xskip;
  switch (colortype) {
    case 0: xskip=(depth==16)?2:1; break;
    case 2: xskip=(depth==16)?6:3; break;
    case 3: xskip=1; break;
    case 4: xskip=(depth==16)?4:2; break;
    case 6: xskip=(depth==16)?8:4; break;
  }
  switch (filter) {
    case 0: { // NONE
        memcpy(currrow,linebuf,stride);
      } break;
    case 1: { // SUB
        memcpy(currrow,linebuf,xskip);
        for (int x=xskip;x<stride;x++) {
          currrow[x]=((int)(linebuf[x])+(int)(currrow[x-xskip]));
        }
      } break;
    case 2: { // UP
        for (int x=0;x<stride;x++) {
          currrow[x]=((int)(linebuf[x])+(int)(prevrow[x]));
        }
      } break;
    case 3: { // AVG
        for (int x=0;x<stride;x++) {
          int avg=(x>=xskip)?currrow[x-xskip]:0;
          avg+=(int)(prevrow[x]);
          avg>>=1;
          currrow[x]=((int)(linebuf[x])+avg);
        }
      } break;
    case 4: { // PAETH
        for (int x=0;x<stride;x++) {
          int a=(x>=xskip)?currrow[x-xskip]:0;
          int b=prevrow[x];
          int c=(x>=xskip)?prevrow[x-xskip]:0;
          int p=a+b-c;
          int pa=p-a; if (pa<0) pa=-pa;
          int pb=p-b; if (pb<0) pb=-pb;
          int pc=p-c; if (pc<0) pc=-pc;
          if ((pa<=pb)&&(pa<=pc)) p=a;
          else if (pb<=pc) p=b;
          else p=c;
          currrow[x]=((int)(linebuf[x])+p);
        }
      } break;
    default: sitter_throw(FormatError,"PNG: illegal filter 0x%02x, row %d",filter,peny);
  }
  //printf("\e[2m"); for (int i=0;i<stride;i++) printf("%02x ",linebuf[i]); printf("\e[0m\n");
  //for (int i=0;i<stride;i++) printf("%02x ",currrow[i]); printf("\n");
}

static void loadImage_PNG(File *f,int *fmt,int *w,int *h,void **pixels) {

  /* IHDR */
  f->seek(8);
  uint8_t ihdr[25];
  if (f->read(ihdr,25)!=25) sitter_throw(FormatError,"short read in IHDR");
  if (memcmp(ihdr,"\0\0\0\x0dIHDR",8)) sitter_throw(FormatError,"expected 13-byte IHDR");
  if (!(*w=sitter_get32be(ihdr,8))) sitter_throw(FormatError,"PNG: w==0");
  if (!(*h=sitter_get32be(ihdr,12))) sitter_throw(FormatError,"PNG: h==0");
  int depth=ihdr[16];
  int colortype=ihdr[17];
  if (ihdr[18]) sitter_throw(FormatError,"PNG: unsupported compression 0x%02x",ihdr[18]);
  if (ihdr[19]) sitter_throw(FormatError,"PNG: unsupported filter 0x%02x",ihdr[19]);
  if (ihdr[20]==1) sitter_throw(FormatError,"PNG: Adam7 interlacing not supported"); // our only standards violation, i believe
  if (ihdr[20]) sitter_throw(FormatError,"PNG: unsupported interlacing 0x%02x",ihdr[20]);
  int chanc;
  switch (colortype) {
    case 0: chanc=1; switch (depth) {
        case 1: case 2: case 4: case 8: case 16: break;
        default: sitter_throw(FormatError,"PNG: bad depth %d for gray",depth);
      } break;
    case 2: chanc=3; switch (depth) {
        case 8: case 16: break;
        default: sitter_throw(FormatError,"PNG: bad depth %d for RGB",depth);
      } break;
    case 3: chanc=1; switch (depth) {
        case 1: case 2: case 4: case 8: break;
        default: sitter_throw(FormatError,"PNG: bad depth %d for index",depth);
      } break;
    case 4: chanc=2; switch (depth) {
        case 8: case 16: break;
        default: sitter_throw(FormatError,"PNG: bad depth %d for gray+alpha",depth);
      } break;
    case 6: chanc=4; switch (depth) {
        case 8: case 16: break;
        default: sitter_throw(FormatError,"PNG: bad depth %d for RGBA",depth);
      } break;
    default: sitter_throw(FormatError,"PNG: bad color type %d",colortype);
  }
  int pixelsize=depth*chanc;
  //int byteperpix=pixelsize>>3; if (!byteperpix) byteperpix=1;
  //int pixperbyte=8/pixelsize; if (!pixperbyte) pixperbyte=1;
  int stride=((*w)*pixelsize+7)>>3;
  void *rawbuf=malloc(stride*(*h)); // will hold decompressed, unfiltered pixels in png's format
  if (!rawbuf) sitter_throw(AllocationError,"PNG %dx%d",*w,*h);
  void *plte=NULL; int pltelen=0; // pltelen=RGBA entry count
  bool gottrns=false; // easy way to check if plte has (may have) any not-0xff alphas
  try {
    try {
      void *linebuf=malloc(stride+1); // will hold one filtered scan line straight off decompressor
      if (!linebuf) sitter_throw(AllocationError,"");
      void *zbuf=NULL;
      int zbufa=0;
      int peny=0;
      try {
      
        z_stream zs={0};
        if (inflateInit(&zs)) sitter_throw(ZError,"inflateInit");
        while (1) {
          uint32_t chklen=f->read32be();
          char chkid[5];
          if (f->read(chkid,4)!=4) throw ShortIOError();
          chkid[4]=0;
          
          if (!strcmp(chkid,"IDAT")) { //------- IDAT ---------------------------
            if (peny>=*h) sitter_throw(FormatError,"PNG: too much pixel data");
            if (zbufa<chklen) {
              if (zbuf) free(zbuf);
              if (!(zbuf=malloc(zbufa=chklen))) sitter_throw(AllocationError,"");
            }
            if (f->read(zbuf,chklen)!=chklen) throw ShortIOError();
            
            /* decompression loop -- 1 row at a time */
            zs.next_in=(Bytef*)zbuf;
            zs.avail_in=chklen;
            zs.total_in=0;
            if (!zs.avail_out) {
              zs.next_out=(Bytef*)linebuf;
              zs.avail_out=stride+1;
              zs.total_out=0;
            }
            bool donechk=false;
            while (!donechk) {
              switch (int err=inflate(&zs,Z_NO_FLUSH)) {
                case Z_STREAM_END: donechk=true;
                case Z_OK: {
                    if (!zs.avail_out) {
                      png_row((uint8_t*)linebuf,stride,colortype,depth,peny,(uint8_t*)rawbuf);
                      peny++;
                      zs.next_out=(Bytef*)linebuf;
                      zs.avail_out=stride+1;
                      zs.total_out=0;
                    }
                    if (!zs.avail_in) donechk=true;
                  } break;
                case Z_NEED_DICT: sitter_throw(ZError,"PNG: error in zlib stream (%s)","Z_NEED_DICT");
                case Z_DATA_ERROR: sitter_throw(ZError,"PNG: error in zlib stream (%s)","Z_DATA_ERROR");
                case Z_MEM_ERROR: sitter_throw(AllocationError,"(zlib)");
                case Z_BUF_ERROR: sitter_throw(ZError,"PNG: Z_BUF_ERROR"); // this should not happen, but is not fatal
                default: sitter_throw(ZError,"PNG: error in zlib stream (%d)",err);
              }
              if (peny>=*h) donechk=true;
            }
            
            f->skip(4); // don't bother with CRC
            
          } else if (!strcmp(chkid,"PLTE")) { // PLTE ---------------------------
            if (colortype==3) { // don't care about suggested quantisation
              if ((chklen<3)||(chklen>768)||(chklen%3)) sitter_throw(FormatError,"PNG: bad length %d for PLTE",chklen);
              if (plte) sitter_throw(FormatError,"PNG: multiple PLTE");
              pltelen=chklen/3;
              if (!(plte=malloc(pltelen<<2))) sitter_throw(AllocationError,"");
              if (f->read(plte,chklen)!=chklen) throw ShortIOError();
              f->skip(4);
              // convert RGB to RGBA, A=0xff
              int op=pltelen<<2,ip=chklen;
              for (int i=0;i<pltelen;i++) {
                ((uint8_t*)plte)[--op]=0xff; // a
                ((uint8_t*)plte)[--op]=((uint8_t*)plte)[--ip]; // b
                ((uint8_t*)plte)[--op]=((uint8_t*)plte)[--ip]; // g
                ((uint8_t*)plte)[--op]=((uint8_t*)plte)[--ip]; // r
              }
            }
            
          } else if (!strcmp(chkid,"tRNS")) { // tRNS ---------------------------
            if (colortype==3) { // don't care about colorkeys
              if (chklen>pltelen) sitter_throw(FormatError,"PNG: tRNS longer than PLTE (%d>%d)",chklen,pltelen);
              uint8_t trns[256];
              if (f->read(trns,chklen)!=chklen) throw ShortIOError();
              f->skip(4);
              for (int i=0;i<chklen;i++) sitter_set8(plte,(i<<2)+3,trns[i]);
            }
            gottrns=true;
          
          } else if (!strcmp(chkid,"IEND")) { // IEND ---------------------------
            break;
          } else { // -------------------------- unknown chunk ------------------
            if (!(chkid[0]&0x20)) sitter_throw(FormatError,"PNG: unrecognised critical chunk '%s'",chkid);
            f->skip(chklen+4);
          }
        }
        inflateEnd(&zs);
        if (peny!=*h) sitter_throw(FormatError,"PNG: incomplete pixels (awaiting row %d of %d)",peny+1,*h);
      
      } catch (...) { free(linebuf); if (zbuf) free(zbuf); throw; }
      free(linebuf);
    } catch (...) { free(rawbuf); if (plte) free(plte); throw; }
  } catch (ShortIOError) { sitter_throw(FormatError,"PNG: unexpected end of file"); }
  
  /* check format, can we return it as-is? */
  *fmt=-12345;
  switch (colortype) {
    case 0: if (depth==1) { // with no alpha, treat luma as intensity
        *pixels=rawbuf;
        *fmt=SITTER_TEXFMT_N1;
        return;
      } *fmt=SITTER_TEXFMT_RGBA8888; break;
    case 2: if (depth==8) {
        *pixels=rawbuf;
        *fmt=SITTER_TEXFMT_RGB888;
        return;
      } *fmt=SITTER_TEXFMT_RGB888; break;
    case 3: if (depth==1) { // i1 => n1, if the color table matches
        if (pltelen<2) { // PNG violation, but we can play with it. assume they meant n1
          *pixels=rawbuf;
          *fmt=SITTER_TEXFMT_N1;
          if (plte) free(plte);
          return;
        }
        if (!memcmp(((char*)plte)+4,"\xff\xff\xff\xff",4)) { // entry 1: opaque white
          if ((sitter_get8(plte,3)==0x00)||!memcmp(plte,"\0\0\0",3)) { // entry 2: black-any-alpha or anything-transparent
            *pixels=rawbuf;
            *fmt=SITTER_TEXFMT_N1;
            free(plte);
            return;
          }
        }
      } else if (gottrns) *fmt=SITTER_TEXFMT_RGBA8888;
      else *fmt=SITTER_TEXFMT_RGB888;
      break;
    case 4: *fmt=SITTER_TEXFMT_RGBA8888; break;
    case 6: if (depth==8) {
        *pixels=rawbuf;
        *fmt=SITTER_TEXFMT_RGBA8888;
        return;
      } *fmt=SITTER_TEXFMT_RGBA8888; break;
  }
  
  /* convert to one of our formats */
  int buflen;
  const char *dstdesc;
  switch (*fmt) {
    case SITTER_TEXFMT_N1: dstdesc=">n1"; buflen=(((*w)+7)>>3)*(*h); break;
    case SITTER_TEXFMT_RGB888: dstdesc=">rgb888"; buflen=(*w)*(*h)*3; break;
    case SITTER_TEXFMT_RGBA8888: dstdesc=">rgba8888"; buflen=(*w)*(*h)*4; break;
    default: sitter_throw(IdiotProgrammerError,"PNG: convert-to format undetermined");
  }
  void *cvtbuf=malloc(buflen);
  if (!cvtbuf) sitter_throw(AllocationError,"");
  switch (colortype) {
    case 0: switch (depth) {
        case 2: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">n2"); break;
        case 4: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">n4"); break;
        case 8: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">n8"); break;
        case 16: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">nG"); break;
      } break;
    case 2: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">rgbGGG"); break;
    case 3: switch (depth) {
        case 1: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">i1",plte,pltelen); break;
        case 2: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">i2",plte,pltelen); break;
        case 4: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">i4",plte,pltelen); break;
        case 8: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">i8",plte,pltelen); break;
      } break;
    case 4: switch (depth) {
        case 8: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">ya88"); break;
        case 16: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">yaGG"); break;
      } break;
    case 6: sitter_convertSurface(cvtbuf,rawbuf,*w,*h,dstdesc,">rgbaGGGG"); break;
  }
  
  free(rawbuf);
  if (plte) free(plte);
  *pixels=cvtbuf;
}

/******************* END OF PNG DECODER *****************************************************/

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
        int w=0,h=0,fmt=0; void *pixels=0;
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
            if (!memcmp(signature,"\x89PNG\r\n\x1a\n",8)) loadImage_PNG(f,&fmt,&w,&h,&pixels);
            //else if (!memcmp(signature,"STR\0TEX\0",8)) ResourceManager::loadImage_STR0TEX0(f,&fmt,&w,&h,&pixels);
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
