#ifndef SITTER_SURFACECVT_H
#define SITTER_SURFACECVT_H

/* Surface format is a string: [byteorder] channels sizes
 *   byteorder is '<'=little-endian or '>'=big-endian. default=big-endian
 *   channels is some combination of:
 *       'r'=red
 *       'g'=green
 *       'b'=blue
 *       'a'=alpha
 *       'y'=luma
 *       'n'=intensity
 *       'i'=index
 *       'x'=padding
 *     If 'r', 'g',or 'b' is present all three must be, and 'y', 'n', and 'i' are forbidden.
 *     Only one of 'y','n','i' may be present.
 *     'a' may not appear with 'n' or 'i'.
 *     Only 'x' may be repeated.
 *     'i' may not be larger than 8 bits.
 *
 * Channel size may be from 1 through 16.
 * As a special case, 32-bit channels are permitted if the pixel's size is a multiple of 8, and
 * the channel's offset is a multiple of 8. (eg, if it's the only channel or if there are 2 32-bit channels and nothing else).
 * Pixels within a row are presumed to be packed as tightly as possible.
 * Rows are assumed to be aligned to 1 byte.
 * When copying into Intensity, only the alpha channel is copied (even if alpha is implicitly 1.0).
 */
 
#include <stdint.h>

/* texture format (not surface format) */
#define SITTER_TEXFMT_RGBA8888    1
#define SITTER_TEXFMT_RGB888      2
#define SITTER_TEXFMT_N1          3
#define SITTER_TEXFMT_GXN4        4
#define SITTER_TEXFMT_GXRGB565    5
#define SITTER_TEXFMT_GXRGBA8888  6

class PixelFormat {

  uint8_t readChannelle(const void *src,int off,int sz) const;
  uint8_t readChannelbe(const void *src,int off,int sz) const;
  void writeChannelle(void *dst,int off,int sz,uint8_t ch) const;
  void writeChannelbe(void *dst,int off,int sz,uint8_t ch) const;

public:

  const void *ctab; int ctlen;
  int pixelsize;
  char order;
  bool has_r,has_g,has_b,has_a,has_y,has_n,has_i;
  int roff,rsz;
  int goff,gsz;
  int boff,bsz;
  int aoff,asz;
  
  PixelFormat(const char *desc,const void *ctab=0,int ctlen=0);
  
  /* offset is in bits.
   * Exchange format is rgba8888, native byte order.
   */
  uint32_t read(const void *src,int offset) const;
  void write(void *dst,int offset,uint32_t rgba) const;
  
};

void sitter_convertSurface(
  void *dst,const void *src,int w,int h, // dst and src share same dimensions. you are responsible for making correct sizes
  const char *dstfmt,const char *srcfmt, // see comment above
  const void *ctab=0,int ctlen=0         // color table for src surface, >rgba8888, ctlen is entries not bytes
);

/* Turn a sequential surface into a GX-ready texture.
 */
void sitter_gxinterlace_n1_n4(void *dst,const void *src,int w,int h);
void sitter_gxinterlace_rgb888_rgb565(void *dst,const void *src,int w,int h);
void sitter_gxinterlace_rgba8888_rgba8888(void *dst,const void *src,int w,int h);

/* Turn a GX-ready texture into a sequential surface.
 */
void sitter_gxdeinterlace_n4_n1(void *dst,const void *src,int w,int h);
void sitter_gxdeinterlace_rgb565_rgb888(void *dst,const void *src,int w,int h);
void sitter_gxdeinterlace_rgba8888_rgba8888(void *dst,const void *src,int w,int h);

#endif
