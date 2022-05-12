#ifndef SITTER_BYTEORDER_H
#define SITTER_BYTEORDER_H

#include <stdint.h>

#if defined(SITTER_LITTLE_ENDIAN) && defined(SITTER_BIG_ENDIAN)
  #error "DOES NOT COMPUTE: SITTER_LITTLE_ENDIAN and SITTER_BIG_ENDIAN are both defined."
#endif

#define sitter_swap16(n) ((uint16_t)((((uint16_t)(n))>>8)|((n)<<8)))
#define sitter_swap32(n) ((uint32_t)((((uint32_t)(n))>>24)|(((n)&0xff0000)>>8)|(((n)&0xff00)<<8)|((n)<<24)))

#define rd8(buf,off) (*(((uint8_t*)(buf))+(off)))
#define wr8(buf,off,val) ((*(((uint8_t*)(buf))+(off)))=(uint8_t)(val))

#define rdn16(buf,off) (*((uint16_t*)(((uint8_t*)(buf))+(off))))
#define wrn16(buf,off,val) (*((uint16_t*)(((uint8_t*)(buf))+(off)))=(uint16_t)(val))
#define rdn32(buf,off) (*((uint32_t*)(((uint8_t*)(buf))+(off))))
#define wrn32(buf,off,val) (*((uint32_t*)(((uint8_t*)(buf))+(off)))=(uint32_t)(val))

#define rdb16(buf,off) ((rd8(buf,off)<<8)|rd8(buf,(off)+1))
#define rdb32(buf,off) ((rd8(buf,off)<<24)|(rd8(buf,(off)+1)<<16)|(rd8(buf,(off)+2)<<8)|rd8(buf,(off)+3))
#define rdl16(buf,off) (rd8(buf,off)|(rd8(buf,(off)+1)<<8))
#define rdl32(buf,off) (rd8(buf,off)|(rd8(buf,(off)+1)<<8)|(rd8(buf,(off)+2)<<16)|(rd8(buf,(off)+3)<<24))

#define wrb16(buf,off,val) do { wr8(buf,off,(val)>>8); wr8(buf,(off)+1,val); } while (0)
#define wrb32(buf,off,val) do { wr8(buf,off,(val)>>24); wr8(buf,(off)+1,(val)>>16); \
  wr8(buf,(off)+2,(val)>>8); wr8(buf,(off)+3,val); } while (0)
#define wrl16(buf,off,val) do { wr8(buf,off,val); wr8(buf,(off)+1,(val)>>8); } while (0)
#define wrl32(buf,off,val) do { wr8(buf,off,val); wr8(buf,(off)+1,(val)>>8); \
  wr8(buf,(off)+2,(val)>>16); wr8(buf,(off)+3,(val)>>24); } while (0)

#define sitter_get8 rd8
#define sitter_set8 wr8
#if defined(SITTER_BIG_ENDIAN)
  #define sitter_get16le(buf,off) rdl16(buf,off)
  #define sitter_get32le(buf,off) rdl32(buf,off)
  #define sitter_get16be(buf,off) rdn16(buf,off)
  #define sitter_get32be(buf,off) rdn32(buf,off)
  #define sitter_set16le(buf,off,val) wrl16(buf,off,val)
  #define sitter_set32le(buf,off,val) wrl32(buf,off,val)
  #define sitter_set16be(buf,off,val) wrn16(buf,off,val)
  #define sitter_set32be(buf,off,val) wrn32(buf,off,val)
#elif defined(SITTER_LITTLE_ENDIAN)
  #define sitter_get16le(buf,off) rdn16(buf,off)
  #define sitter_get32le(buf,off) rdn32(buf,off)
  #define sitter_get16be(buf,off) rdb16(buf,off)
  #define sitter_get32be(buf,off) rdb32(buf,off)
  #define sitter_set16le(buf,off,val) wrn16(buf,off,val)
  #define sitter_set32le(buf,off,val) wrn32(buf,off,val)
  #define sitter_set16be(buf,off,val) wrb16(buf,off,val)
  #define sitter_set32be(buf,off,val) wrb32(buf,off,val)
#else
  #warning "This will run faster if you define SITTER_BIG_ENDIAN or SITTER_LITTLE_ENDIAN"
  #define sitter_get16le(buf,off) rdl16(buf,off)
  #define sitter_get32le(buf,off) rdl32(buf,off)
  #define sitter_get16be(buf,off) rdb16(buf,off)
  #define sitter_get32be(buf,off) rdb32(buf,off)
  #define sitter_set16le(buf,off,val) wrl16(buf,off,val)
  #define sitter_set32le(buf,off,val) wrl32(buf,off,val)
  #define sitter_set16be(buf,off,val) wrb16(buf,off,val)
  #define sitter_set32be(buf,off,val) wrb32(buf,off,val)
#endif

#endif
