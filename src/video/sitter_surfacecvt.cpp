#include "sitter_Error.h"
#include "sitter_byteorder.h"
#include "sitter_surfacecvt.h"

/******************************************************************************
 * PixelFormat
 *****************************************************************************/
 
PixelFormat::PixelFormat(const char *desc,const void *ctab,int ctlen):ctab(ctab),ctlen(ctlen) {
  const char *desc_chid,*desc_chsz;
  has_r=has_g=has_b=has_a=has_i=has_n=has_y=false;
  roff=goff=boff=aoff=-1;
  pixelsize=0;
  
  /* read byte order, default to big-endian */
       if (desc[0]=='>') { order='>'; desc_chid=desc+1; }
  else if (desc[0]=='<') { order='<'; desc_chid=desc+1; }
  else                   { order='>'; desc_chid=desc;   }
  
  /* read channel ids, assert sane set of channels */
  bool donechid=false;
  desc_chsz=desc_chid;
  int chanc=0;
  while (!donechid) {
    switch (desc_chid[chanc]) {
      case 0: sitter_throw(ArgumentError,"PixelFormat '%s': expected channel size",desc);
      case 'r': {
          if (has_r) sitter_throw(ArgumentError,"PixelFormat '%s': multiple '%c'",desc,'r');
          if (has_y||has_n||has_i) 
            sitter_throw(ArgumentError,"PixelFormat '%s': '%c' not allowed with '%c','%c','%c'",desc,'r','y','n','i');
          has_r=true;
        } break;
      case 'g': {
          if (has_g) sitter_throw(ArgumentError,"PixelFormat '%s': multiple '%c'",desc,'g');
          if (has_y||has_n||has_i) 
            sitter_throw(ArgumentError,"PixelFormat '%s': '%c' not allowed with '%c','%c','%c'",desc,'g','y','n','i');
          has_g=true;
        } break;
      case 'b': {
          if (has_b) sitter_throw(ArgumentError,"PixelFormat '%s': multiple '%c'",desc,'b');
          if (has_y||has_n||has_i) 
            sitter_throw(ArgumentError,"PixelFormat '%s': '%c' not allowed with '%c','%c','%c'",desc,'b','y','n','i');
          has_b=true;
        } break;
      case 'a': {
          if (has_a) sitter_throw(ArgumentError,"Pixelformat '%s': multiple '%c'",desc,'a');
          if (has_n||has_i) sitter_throw(ArgumentError,"PixelFormat '%s': '%c' not allowed with '%c','%c'",desc,'a','n','i');
          has_a=true;
        } break;
      case 'y': {
          if (has_y) sitter_throw(ArgumentError,"Pixelformat '%s': multiple '%c'",desc,'y');
          if (has_r||has_g||has_b||has_i) 
            sitter_throw(ArgumentError,"PixelFormat '%s': '%c' not allowed with '%c','%c','%c','%c'",desc,'y','r','g','b','i');
          has_y=true;
        } break;
      case 'n': {
          if (has_n) sitter_throw(ArgumentError,"Pixelformat '%s': multiple '%c'",desc,'n');
          if (has_r||has_g||has_b||has_y||has_a||has_i) 
            sitter_throw(ArgumentError,"PixelFormat '%s': '%c' not allowed with '%c','%c','%c','%c','%c','%c'",desc,'n','r','g','b','a','y','i');
          has_n=true;
        } break;
      case 'i': {
          if (has_i) sitter_throw(ArgumentError,"Pixelformat '%s': multiple '%c'",desc,'i');
          if (has_r||has_g||has_b||has_y||has_a||has_n) 
            sitter_throw(ArgumentError,"PixelFormat '%s': '%c' not allowed with '%c','%c','%c','%c','%c','%c'",desc,'n','r','g','b','a','y','n');
          has_i=true;
        } break;
      case 'x': break;
      default: donechid=true;
    }
    if (donechid) break;
    desc_chsz++; chanc++;
  }
  if (!has_r&&!has_g&&!has_b&&!has_y&&!has_a&&!has_n&&!has_i) sitter_throw(ArgumentError,"PixelFormat '%s': no channels",desc);
  if (has_r||has_g||has_b) { // rgb, must have all three
    if (!has_r||!has_g||!has_b) sitter_throw(ArgumentError,"PixelFormat '%s': incomplete RGB",desc);
  }
  
  /* read channel sizes, determine offsets */
  bool mustalign=false;
  if (desc_chsz[chanc]) sitter_throw(ArgumentError,"PixelFormat '%s': extra tokens after sizes",desc);
  for (int i=0;i<chanc;i++) {
    int chsz=desc_chsz[i];
    if ((chsz>='1')&&(chsz<='9')) chsz-='0';
    else if ((chsz>='A')&&(chsz<='G')) chsz=(chsz-'A')+10;
    else if (chsz=='W') { 
      if (!(pixelsize&7)) sitter_throw(ArgumentError,"PixelFormat '%s': misaligned 32-bit channel",desc); 
      chsz=32; mustalign=true;
    } else sitter_throw(ArgumentError,"PixelFormat '%s': expected channel size for '%c'",desc,desc_chid[i]);
    switch (desc_chid[i]) {
      case 'r': roff=pixelsize; rsz=chsz; break;
      case 'g': goff=pixelsize; gsz=chsz; break;
      case 'b': boff=pixelsize; bsz=chsz; break;
      case 'a': { // alpha alone is treated as intensity
          if (!has_r&&!has_y) { roff=goff=boff=aoff=pixelsize; rsz=bsz=gsz=asz=chsz; }
          else aoff=pixelsize; asz=chsz; 
        } break;
      case 'y': roff=goff=boff=pixelsize; rsz=bsz=gsz=chsz; break;
      case 'i': if (chsz>8) sitter_throw(ArgumentError,"PixelFormat '%s': 'i' too large",desc); // pass to 'n'
      case 'n': roff=goff=boff=aoff=pixelsize; rsz=bsz=gsz=asz=chsz; break;
    }
    pixelsize+=chsz;
  }
  if (mustalign&&(pixelsize&7)) sitter_throw(ArgumentError,"PixelFormat '%s': pixel must align to 1 byte when 32-bit channels present",desc);
  
}

uint32_t PixelFormat::read(const void *src,int offset) const {
  uint8_t r,g,b,a=0xff;
  if (has_i&&ctab) { // read index (no scaling)
    offset+=roff;
    int byteoffset=offset>>3;
    if ((rsz==8)&&!(offset&7)) return sitter_get8(src,byteoffset);
    int i;
    if (order=='>') {
      i=sitter_get16be(src,byteoffset);
      int shift=16-((offset&7)+rsz);
      i>>=shift;
    } else {
      i=sitter_get16le(src,byteoffset);
      i>>=(offset&7);
    }
    switch (rsz) { case 1:i&=1; case 2:i&=3; case 3:i&=7; case 4:i&=15; case 5:i&=31; case 6:i&=63; case 7:i&=127; case 8:i&=255; }
    if (i>=ctlen) return 0x00000000;
    i<<=2;
    return sitter_get32be(ctab,i);
  }
  /* not index (or index treated as intensity) */
  r=(order=='>')?readChannelbe(src,offset+roff,rsz):readChannelle(src,offset+roff,rsz);
  if (goff==roff) g=b=r;
  else {
    g=(order=='>')?readChannelbe(src,offset+goff,gsz):readChannelle(src,offset+goff,gsz);
    b=(order=='>')?readChannelbe(src,offset+boff,bsz):readChannelle(src,offset+boff,bsz);
  }
  if (aoff==roff) a=r;
  else if (aoff<0) ;
  else a=(order=='>')?readChannelbe(src,offset+aoff,asz):readChannelle(src,offset+aoff,asz);
  return (r<<24)|(g<<16)|(b<<8)|a;
}

void PixelFormat::write(void *dst,int offset,uint32_t rgba) const {
  if (aoff>=0) {
    if (aoff==roff) { // intensity
      int n=((rgba>>24)+((rgba>>16)&0xff)+((rgba>>8)&0xff)+(rgba&0xff))>>2;
      if (order=='>') writeChannelbe(dst,offset+aoff,asz,n); else writeChannelle(dst,offset+aoff,asz,n);
      return;
    } else { // independant alpha
      if (order=='>') writeChannelbe(dst,offset+aoff,asz,rgba); else writeChannelle(dst,offset+aoff,asz,rgba);
    }
  }
  if (roff==goff) { // gray
    int y=((rgba>>24)+((rgba>>16)&0xff)+((rgba>>8)&0xff))/3;
    if (order=='>') writeChannelbe(dst,offset+roff,rsz,y); else writeChannelle(dst,offset+roff,rsz,y);
  } else { // rgb
    if (order=='>') {
      writeChannelbe(dst,offset+roff,rsz,rgba>>24);
      writeChannelbe(dst,offset+goff,gsz,rgba>>16);
      writeChannelbe(dst,offset+boff,bsz,rgba>>8);
    } else {
      writeChannelle(dst,offset+roff,rsz,rgba>>24);
      writeChannelle(dst,offset+goff,gsz,rgba>>16);
      writeChannelle(dst,offset+boff,bsz,rgba>>8);
    }
  }
}

/******************************************************************************
 * pixel format: primitive accessors
 *****************************************************************************/
 
#define SCALEUPTO8(n,sz) ( \
    (sz==1)?(((n)&1)?0xff:0x00): \
    (sz==2)?((((n)&3)<<6)|(((n)&3)<<4)|(((n)&3)<<2)|((n)&3)): \
    (sz==3)?((((n)&7)<<5)|(((n)&7)<<2)|(((n)&7)>>1)): \
    (sz==4)?((((n)&15)<<4)|((n)&15)): \
    (sz==5)?((((n)&31)<<3)|(((n)&31)>>2)): \
    (sz==6)?((((n)&63)<<2)|(((n)&63)>>4)): \
    (sz==7)?((((n)&127)<<1)|(((n)&127)>>1)): \
    ((n)&255))

uint8_t PixelFormat::readChannelle(const void *src,int off,int sz) const {
  src=((char*)src)+(off>>3); off&=7; // normalise src,off such that off<8
  if (!off) switch (sz) {
    case 8: return sitter_get8(src,0);
    case 16: return sitter_get16le(src,0);
    case 32: return sitter_get32le(src,0);
  }
  uint32_t ch;
  if (off+sz>16) ch=(sitter_get8(src,0)|(sitter_get8(src,1)<<8)|(sitter_get8(src,2)<<16));
  else if (off+sz>8) ch=(sitter_get8(src,0)|(sitter_get8(src,1)<<8));
  else ch=sitter_get8(src,0);
  ch>>=off;
  if (sz==8) return ch&0xff;
  if (sz>8) return (ch>>(sz-8))&0xff;
  return SCALEUPTO8(ch,sz);
}

uint8_t PixelFormat::readChannelbe(const void *src,int off,int sz) const {
  src=((char*)src)+(off>>3); off&=7;
  if (!off) switch (sz) {
    case 8: return sitter_get8(src,0);
    case 16: return sitter_get16be(src,0);
    case 32: return sitter_get32le(src,0);
  }
  uint32_t ch;
  if (off+sz>16) ch=((sitter_get8(src,0)<<16)|(sitter_get8(src,1)<<8)|sitter_get8(src,2))>>(24-(off+sz));
  else if (off+sz>8) ch=((sitter_get8(src,0)<<8)|sitter_get8(src,1))>>(16-(off+sz));
  else ch=sitter_get8(src,0)>>(8-(off+sz));
  if (sz==8) return ch&0xff;
  if (sz>8) return (ch>>(sz-8))&0xff;
  return SCALEUPTO8(ch,sz);
}

void PixelFormat::writeChannelle(void *dst,int off,int sz,uint8_t ch) const {
  dst=((char*)dst)+(off>>3); off&=7;
  if (!off) switch (sz) {
    case 8: sitter_set8(dst,0,ch); return;
    case 16: sitter_set16le(dst,0,ch); return;
    case 32: sitter_set32le(dst,0,ch); return;
  }
  /* scale channel to storage size, make a mask */
  uint32_t chout=ch;
  if (sz<8) chout>>=(8-sz);
  else if (sz>8) {
    chout<<=(sz-8);
    int havesz=8;
    while (havesz<sz) {
      chout|=(chout>>havesz);
      havesz<<=1;
    }
  }
  chout<<=off;
  uint32_t chmask=((1<<sz)-1)<<off;
  /* dump it to buffer */
  if (off+sz>16) { // 3 bytes
    uint8_t prv=sitter_get8(dst,0);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,0,prv);
    chout>>=8;
    sitter_set8(dst,1,chout);
    chout>>=8; chmask>>=16;
    prv=sitter_get8(dst,2);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,2,prv);
  } else if (off+sz>8) { // 2 bytes
    uint8_t prv=sitter_get8(dst,0);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,0,prv);
    chout>>=8; chmask>>=8;
    prv=sitter_get8(dst,1);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,1,prv);
  } else { // 1 byte
    uint8_t prv=sitter_get8(dst,0);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,0,prv);
  }
}

void PixelFormat::writeChannelbe(void *dst,int off,int sz,uint8_t ch) const {
  dst=((char*)dst)+(off>>3); off&=7;
  if (!off) switch (sz) {
    case 8: sitter_set8(dst,0,ch); return;
    case 16: sitter_set16be(dst,0,ch); return;
    case 32: sitter_set32be(dst,0,ch); return;
  }
  /* scale channel to storage size, make a mask */
  uint32_t chout=ch;
  if (sz<8) chout>>=(8-sz);
  else if (sz>8) {
    chout<<=(sz-8);
    int havesz=8;
    while (havesz<sz) {
      chout|=(chout>>havesz);
      havesz<<=1;
    }
  }
  chout<<=(7-off);
  uint32_t chmask=((1<<sz)-1)<<(7-off);
  /* dump it to buffer */
  if (off+sz>16) { // 3 bytes
    uint8_t prv=sitter_get8(dst,2);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,2,prv);
    chout>>=8;
    sitter_set8(dst,1,chout);
    chout>>=8; chmask>>=16;
    prv=sitter_get8(dst,0);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,0,prv);
  } else if (off+sz>8) { // 2 bytes
    uint8_t prv=sitter_get8(dst,1);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,1,prv);
    chout>>=8; chmask>>=8;
    prv=sitter_get8(dst,0);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,0,prv);
  } else { // 1 byte
    uint8_t prv=sitter_get8(dst,0);
    prv=(prv&~chmask)|chout;
    sitter_set8(dst,0,prv);
  }
}

/******************************************************************************
 * sitter_convertSurface
 *****************************************************************************/

void sitter_convertSurface(void *dst,const void *src,int w,int h,const char *_dstfmt,
const char *_srcfmt,const void *ctab,int ctlen) {
  PixelFormat srcfmt(_srcfmt,ctab,ctlen);
  PixelFormat dstfmt(_dstfmt);
  int srcstride=(srcfmt.pixelsize*w+7)>>3;
  int dststride=(dstfmt.pixelsize*w+7)>>3;
  for (int y=0;y<h;y++) {
    int srcoff=0,dstoff=0;
    for (int x=0;x<w;x++) {
      dstfmt.write(dst,dstoff,srcfmt.read(src,srcoff));
      srcoff+=srcfmt.pixelsize;
      dstoff+=dstfmt.pixelsize;
    }
    src=((char*)src)+srcstride;
    dst=((char*)dst)+dststride;
  }
}

/******************************************************************************
 * GX interlacing
 *****************************************************************************/
 
#define SRC(off) ((uint8_t*)src)[off]
#define DST(off) ((uint8_t*)dst)[off]
 
void sitter_gxinterlace_n1_n4(void *dst,const void *src,int w,int h) {
  int colc=w>>3,rowc=h>>3; // 8x8 cells
  int srcstride=(w+7)>>3;
  int dstoffset=0;
  for (int row=0;row<rowc;row++) {
    int srcoffset=(row<<3)*srcstride;
    for (int col=0;col<colc;col++) {
      for (int subrow=0;subrow<8;subrow++) {
        uint8_t srcpx=SRC(srcoffset+(subrow*srcstride));
        switch (srcpx&0xc0) {
          case 0x00: DST(dstoffset++)=0x00; break;
          case 0x40: DST(dstoffset++)=0x0f; break;
          case 0x80: DST(dstoffset++)=0xf0; break;
          case 0xc0: DST(dstoffset++)=0xff; break;
        }
        switch (srcpx&0x30) {
          case 0x00: DST(dstoffset++)=0x00; break;
          case 0x10: DST(dstoffset++)=0x0f; break;
          case 0x20: DST(dstoffset++)=0xf0; break;
          case 0x30: DST(dstoffset++)=0xff; break;
        }
        switch (srcpx&0x0c) {
          case 0x00: DST(dstoffset++)=0x00; break;
          case 0x04: DST(dstoffset++)=0x0f; break;
          case 0x08: DST(dstoffset++)=0xf0; break;
          case 0x0c: DST(dstoffset++)=0xff; break;
        }
        switch (srcpx&0x03) {
          case 0x00: DST(dstoffset++)=0x00; break;
          case 0x01: DST(dstoffset++)=0x0f; break;
          case 0x02: DST(dstoffset++)=0xf0; break;
          case 0x03: DST(dstoffset++)=0xff; break;
        }
      }
    }
  }
}
 
void sitter_gxdeinterlace_n4_n1(void *dst,const void *src,int w,int h) {
  int colc=w>>3,rowc=h>>3; // 8x8 cells
  int dststride=(w+7)>>3;
  int srcoffset=0;
  for (int row=0;row<rowc;row++) {
    int dstoffset=(row<<3)*dststride;
    for (int col=0;col<colc;col++) {
      for (int subrow=0;subrow<8;subrow++) {
        uint8_t dstpx=0;
        if (SRC(srcoffset)&0x80) dstpx|=0x80;
        if (SRC(srcoffset)&0x08) dstpx|=0x40; srcoffset++;
        if (SRC(srcoffset)&0x80) dstpx|=0x20;
        if (SRC(srcoffset)&0x08) dstpx|=0x10; srcoffset++;
        if (SRC(srcoffset)&0x80) dstpx|=0x08;
        if (SRC(srcoffset)&0x08) dstpx|=0x04; srcoffset++;
        if (SRC(srcoffset)&0x80) dstpx|=0x02;
        if (SRC(srcoffset)&0x08) dstpx|=0x01; srcoffset++;
        DST(dstoffset+(subrow*dststride))=dstpx;
      }
    }
  }
}

void sitter_gxinterlace_rgb888_rgb565(void *dst,const void *src,int w,int h) {
  int colc=w>>2,rowc=h>>2; // 4x4 cells
  int srcstride=w*3;
  int dstoffset=0;
  for (int row=0;row<rowc;row++) {
    for (int col=0;col<colc;col++) {
      int srccelloffset=(row<<2)*srcstride+col*12;
      for (int subrow=0;subrow<4;subrow++) {
        int srcoff=srccelloffset;
        for (int subcol=0;subcol<4;subcol++) {
          register uint8_t r=SRC(srcoff++);
          register uint8_t g=SRC(srcoff++);
          register uint8_t b=SRC(srcoff++);
          DST(dstoffset++)=(r&0xf8)|(g>>5);
          DST(dstoffset++)=(g<<5)|(b>>3);
        }
        srccelloffset+=srcstride;
      }
    }
  }
}

void sitter_gxdeinterlace_rgb565_rgb888(void *dst,const void *src,int w,int h) {
  int colc=w>>2,rowc=h>>2;
  int dststride=w*3;
  int srcoffset=0;
  for (int row=0;row<rowc;row++) {
    for (int col=0;col<colc;col++) {
      int dstcelloffset=(row<<2)*dststride+col*12;
      for (int subrow=0;subrow<4;subrow++) {
        int dstoff=dstcelloffset;
        for (int subcol=0;subcol<4;subcol++) {
          uint16_t rgb565=(SRC(srcoffset)<<8)|SRC(srcoffset+1); srcoffset+=2;
          DST(dstoff++)=((rgb565&0xf800)>>8)|(rgb565>>13);
          DST(dstoff++)=((rgb565&0x07e0)>>3)|((rgb565&0x0600)>>9);
          DST(dstoff++)=(rgb565<<3)|((rgb565&0x001c)>>2);
        }
      }
      dstcelloffset+=dststride;
    }
  }
}

void sitter_gxinterlace_rgba8888_rgba8888(void *dst,const void *src,int w,int h) {
  int colc=w>>2,rowc=h>>2; // 4x4 cells (2 planes each)
  int srcstride=w<<2;
  int dstoffset=0;
  for (int row=0;row<rowc;row++) {
    for (int col=0;col<colc;col++) {
      int srccelloffset=(row<<2)*srcstride+(col<<4);
      for (int subrow=0;subrow<4;subrow++) {
        int srcoff=srccelloffset;
        for (int subcol=0;subcol<4;subcol++) {
          DST(dstoffset   )=SRC(srcoff+3);
          DST(dstoffset+32)=SRC(srcoff+1);
          dstoffset++;
          DST(dstoffset   )=SRC(srcoff+0);
          DST(dstoffset+32)=SRC(srcoff+2);
          dstoffset++;
          srcoff+=4;
        }
        srccelloffset+=srcstride;
      }
      dstoffset+=32;
    }
  }
}

void sitter_gxdeinterlace_rgba8888_rgba8888(void *dst,const void *src,int w,int h) {
  int colc=w>>2,rowc=h>>2;
  int dststride=w<<2;
  int srcoffset=0;
  for (int row=0;row<rowc;row++) {
    for (int col=0;col<colc;col++) {
      int dstcelloffset=(row<<2)*dststride+(col<<4);
      for (int subrow=0;subrow<4;subrow++) {
        int dstoff=dstcelloffset;
        for (int subcol=0;subcol<4;subcol++) {
          DST(dstoff  )=SRC(srcoffset+ 1);
          DST(dstoff+1)=SRC(srcoffset+32);
          DST(dstoff+2)=SRC(srcoffset+33);
          DST(dstoff+3)=SRC(srcoffset   );
          dstoff+=4;
          srcoffset+=2;
        }
        dstcelloffset+=dststride;
      }
      srcoffset+=32;
    }
  }
}

#undef SRC
#undef DST
