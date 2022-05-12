#include <malloc.h>
#include <string.h>
#include <zlib.h>
#include "sitter_byteorder.h"
#include "sitter_Error.h"
#include "sitter_File.h"
#include "sitter_surfacecvt.h"
#include "sitter_ResourceManager.h"

#ifdef SITTER_WIN32
  #define memalign(algn,len) malloc(len)
#endif

/******************************************************************************
 * image load
 *****************************************************************************/
 
bool ResourceManager::loadImage(const char *resname,int *fmt,int *w,int *h,void **pixels) {
  //sitter_log("image %s...",resname);
  lock();
  try {
  const char *path=resnameToPath(resname,&gfxpfx,&gfxpfxlen);
  if (File *f=sitter_open(path,"rb")) {
    try {
      uint8_t signature[8];
      if (f->read(signature,8)!=8) sitter_throw(FormatError,"%s: too small for image",path);
      if (!memcmp(signature,"\x89PNG\r\n\x1a\n",8)) loadImage_PNG(f,fmt,w,h,pixels);
      else if (!memcmp(signature,"STR\0TEX\0",8)) loadImage_STR0TEX0(f,fmt,w,h,pixels);
      else sitter_throw(FormatError,"%s: not an image file",path);
    } catch (...) { delete f; throw; }
    delete f;
    //sitter_log("...image");
    unlock();
    return true;
  } else sitter_throw(IdiotProgrammerError,"");
  } catch (...) { unlock(); throw; }
}

/******************************************************************************
 * PNG
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

void ResourceManager::loadImage_PNG(File *f,int *fmt,int *w,int *h,void **pixels) {

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

/******************************************************************************
 * STR0TEX0
 * A private image format used by the wii port. Images are stored in a format
 * suitable for GX.
 * 0000   8 signature "STR\0TEX\0"
 * 0008   1 format: (SITTER_TEXFMT_*)
 *            01 rgba8888
 *            02 rgb888
 *            03 n1
 *            04 GX_TF_I4
 *            05 GX_TF_RGB565
 *            06 GX_TF_RGBA8
 * 0009   1 compression:
 *            00 uncompressed
 *            01 zlib
 * 000a   4 width, big-endian
 * 000e   4 height, big-endian
 * 0012 ... additional header:
 *            0000   4 block size (not counting size+ID, ie can be zero)
 *            0004   4 block ID
 *            0008 ... data
 *          terminate with a zero-length block, ID zero.
 * .... ... pixels, maybe compressed
 *****************************************************************************/
 
#define STR0TEX0_WIDTH_LIMIT  1024 // arbitrary, inclusive
#define STR0TEX0_HEIGHT_LIMIT 1024 // arbitrary, inclusive
 
void ResourceManager::loadImage_STR0TEX0(File *f,int *fmt,int *w,int *h,void **pixels) {
  *pixels=NULL;
  try {
  
    /* read fixed header: */
    uint8_t compression;
    { uint8_t ffmt=f->read8();
      if ((ffmt<SITTER_TEXFMT_RGBA8888)||(ffmt>SITTER_TEXFMT_GXRGBA8888)) sitter_throw(FormatError,"STR0TEX0: bad format 0x%02x",ffmt);
      compression=f->read8();
      if (compression>1) sitter_throw(FormatError,"STR0TEX0: bad compression 0x%02x",compression);
      uint32_t fw=f->read32be();
      if (!fw||(fw>STR0TEX0_WIDTH_LIMIT)) sitter_throw(FormatError,"STR0TEX0: invalid width %d",fw);
      uint32_t fh=f->read32be();
      if (!fh||(fh>STR0TEX0_HEIGHT_LIMIT)) sitter_throw(FormatError,"STR0TEX0: invalid height %d",fh);
      *fmt=ffmt;
      *w=fw;
      *h=fh;
    }
    
    /* "read" additional header: */
    while (1) {
      uint32_t blocksize=f->read32be();
      uint32_t blockid=f->read32be();
      if (!blocksize&&!blockid) break;
      f->skip(blocksize);
    }
    
    /* prepare buffer */
    int buflen;
    { switch (*fmt) {
        case SITTER_TEXFMT_RGBA8888: buflen=((*w)*(*h))<<2; break;
        case SITTER_TEXFMT_RGB888: buflen=(*w)*(*h)*3; break;
        case SITTER_TEXFMT_N1: buflen=(((*w)+7)>>3)*(*h); break;
        case SITTER_TEXFMT_GXN4: {
            if (((*w)&7)||((*h)&7)) sitter_throw(FormatError,"STR0TEX0: invalid dimensions %d x %d for GX_TF_I4",*w,*h);
            buflen=((*w)*(*h))>>1;
          } break;
        case SITTER_TEXFMT_GXRGB565: {
            if (((*w)&3)||((*h)&3)) sitter_throw(FormatError,"STR0TEX0: invalid dimensions %d x %d for GX_TF_RGB565",*w,*h);
            buflen=((*w)*(*h))<<1;
          } break;
        case SITTER_TEXFMT_GXRGBA8888: {
            if (((*w)&3)||((*h)&3)) sitter_throw(FormatError,"STR0TEX0: invalid dimensions %d x %d for GX_TF_RGBA8",*w,*h);
            buflen=((*w)*(*h))<<2;
          } break;
        default: sitter_throw(IdiotProgrammerError,"STR0TEX0: fmt=%d",*fmt);
      }
      if (!(*pixels=memalign(32,buflen))) sitter_throw(AllocationError,"");
    }
    
    /* read pixels (with zlib) */
    if (compression) {
      #define ZBUF_LEN 1024
      void *zbuf=malloc(ZBUF_LEN);
      if (!zbuf) sitter_throw(AllocationError,"");
      try {
        z_stream zstrm={0};
        zstrm.next_out=(Bytef*)*pixels;
        zstrm.avail_out=buflen;
        if (inflateInit(&zstrm)) sitter_throw(ZError,"inflateInit");
        while (1) {
          int clen=f->read(zbuf,ZBUF_LEN);
          if (!clen) break;
          zstrm.next_in=(Bytef*)zbuf;
          zstrm.avail_in=clen;
          switch (int err=inflate(&zstrm,Z_NO_FLUSH)) {
            case Z_OK:
            case Z_STREAM_END: break;
            default: sitter_throw(ZError,"inflate: %d",err);
          }
        }
        switch (int err=inflate(&zstrm,Z_FINISH)) {
          case Z_OK:
          case Z_STREAM_END: break;
          default: sitter_throw(ZError,"inflate(Z_FINISH): %d",err);
        }
        if (zstrm.avail_out) sitter_throw(FormatError,"STR0TEX0: pixels incomplete");
        inflateEnd(&zstrm);
      } catch (...) { free(zbuf); throw; }
      free(zbuf);
      #undef ZBUF_LEN
    
    /* read pixels (uncompressed) */
    } else {
      if (f->read(*pixels,buflen)!=buflen) throw ShortIOError();
    }
    
  } catch (ShortIOError) {
    if (*pixels) { free(*pixels); *pixels=NULL; }
    sitter_throw(FormatError,"STR0TEX0: unexpected end of file");
  } catch (...) {
    if (*pixels) { free(*pixels); *pixels=NULL; }
    throw;
  }
}
