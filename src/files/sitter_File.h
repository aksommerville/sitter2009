#ifndef SITTER_FILE_H
#define SITTER_FILE_H

#include <stdint.h>
#include "sitter_Error.h"
//#include "qq_files_all.h"

#ifndef SEEK_SET
  #define SEEK_SET 0
  #define SEEK_CUR 1
  #define SEEK_END 2
#endif

/* flags to sitter_listDir */
#define SITTER_LISTDIR_SORT       0x00000001 // sort alphabetically, case-insensitive
#define SITTER_LISTDIR_SHOWDIR    0x00000002 // show directories
#define SITTER_LISTDIR_SHOWFILE   0x00000004 // show regular files
#define SITTER_LISTDIR_SHOWOTHER  0x00000008 // show things not directory or regular file (symlink,device,...)
#define SITTER_LISTDIR_DIRSLASH   0x00000010 // append "/" to directories
#define SITTER_LISTDIR_DEFAULT    0x0000000b // SORT|SHOWDIR|SHOWFILE

#if 1 // File is now in the QQ sources: src/files/wii/files (i'll clean this up later)
class File {
protected:
  File() {}
public:
  virtual ~File() {}
  virtual int read(void *dst,int len) =0;
  virtual int write(const void *src,int len) =0;
  virtual int64_t seek(int64_t where,int whence=SEEK_SET) =0;
  
  uint8_t read8() { uint8_t n; if (read(&n,1)!=1) throw ShortIOError(); return n; }
  void write8(uint8_t n) { if (write(&n,1)!=1) throw ShortIOError(); }
  #define RD_NATIVE(bitc,bytec) { uint##bitc##_t n; if (read(&n,bytec)!=bytec) throw ShortIOError(); return n; }
  #define WR_NATIVE(bitc,bytec) { if (write(&n,bytec)!=bytec) throw ShortIOError(); }
  #define RD_LE(bitc,bytec) { \
      uint8_t buf[bytec]; \
      if (read(buf,bytec)!=bytec) throw ShortIOError(); \
      uint##bitc##_t n=0; for (int i=bytec;i--;) { n<<=8; n|=buf[i]; } \
      return n; }
  #define WR_LE(bitc,bytec) { \
      uint8_t buf[bytec]; for (int i=0;i<bytec;i++) { buf[i]=n; n>>=8; } \
      if (write(buf,bytec)!=bytec) throw ShortIOError(); }
  #define RD_BE(bitc,bytec) { \
      uint8_t buf[bytec]; \
      if (read(buf,bytec)!=bytec) throw ShortIOError(); \
      uint##bitc##_t n=0; for (int i=0;i<bytec;i++) { n<<=8; n|=buf[i]; } \
      return n; }
  #define WR_BE(bitc,bytec) { \
      uint8_t buf[bytec]; for (int i=bytec;i--;i) { buf[i]=n; n>>=8; } \
      if (write(buf,bytec)!=bytec) throw ShortIOError(); }
  #if defined(SITTER_LITTLE_ENDIAN)
    uint16_t read16le() RD_NATIVE(16,2)
    uint32_t read32le() RD_NATIVE(32,4)
    void write16le(uint16_t n) WR_NATIVE(16,2)
    void write32le(uint32_t n) WR_NATIVE(32,4)
    uint16_t read16be() RD_BE(16,2)
    uint32_t read32be() RD_BE(32,4)
    void write16be(uint16_t n) WR_BE(16,2)
    void write32be(uint32_t n) WR_BE(32,4)
  #elif defined(SITTER_BIG_ENDIAN)
    uint16_t read16le() RD_LE(16,2)
    uint32_t read32le() RD_LE(32,4)
    void write16le(uint16_t n) WR_LE(16,2)
    void write32le(uint32_t n) WR_LE(32,4)
    uint16_t read16be() RD_NATIVE(16,2)
    uint32_t read32be() RD_NATIVE(32,4)
    void write16be(uint16_t n) WR_NATIVE(16,2)
    void write32be(uint32_t n) WR_NATIVE(32,4)
  #else
    #warning "File access speed may be improved if you define SITTER_BIG_ENDIAN or SITTER_LITTLE_ENDIAN"
    uint16_t read16le() RD_LE(16,2)
    uint32_t read32le() RD_LE(32,4)
    void write16le(uint16_t n) WR_LE(16,2)
    void write32le(uint32_t n) WR_LE(32,4)
    uint16_t read16be() RD_BE(16,2)
    uint32_t read32be() RD_BE(32,4)
    void write16be(uint16_t n) WR_BE(16,2)
    void write32be(uint32_t n) WR_BE(32,4)
  #endif
  
  /* A batteries-included line reader. 
   * Return new string, caller frees it.
   * The newline is not part of the returned string, you never see it.
   * Return NULL at EOF.
   * strip: remove whitespace from start and end.
   * skip: don't return empty lines.
   * cont: continue lines ending with backslash.
   * comment: remove anything from hash to unescaped newline. (ie, you can continue comments)
   * If <lineno> is not NULL, it's incremented every time a line break is found (initially zero for 1-based numbers).
   * Hash may be escaped by backslash, and will be returned with the backslash (ie, "\#" not "#").
   * Only '\n' is considered a line break.
   * isspace() is used to decide what's whitespace.
   * Input NUL is quietly ignored (as the output string must be NUL-terminated).
   */
  char *readLine(bool strip=false,bool skip=false,bool cont=false,bool comment=false,int *lineno=0);
  
  int writef(const char *fmt,...);
  
  /* Same as seek(ct,SEEK_CUR), but throw ShortIOError if it doesn't work.
   */
  void skip(int ct) { int64_t pv=seek(0,SEEK_CUR); int64_t nw=seek(ct,SEEK_CUR); if (nw!=pv+ct) throw ShortIOError(); }
};
#endif

/* Resolve special symbols in path, eg "~/.config-file".
 * Return a new string, caller frees.
 * Never return NULL.
 */
char *sitter_resolvePath(const char *path);

/* mode is as in fopen.
 * Calls sitter_resolvePath.
 */
File *sitter_open(const char *path,const char *mode);

/* <path> is a regular file; make all of the directories necessary to house it.
 */
void sitter_mkdirp(const char *path);

/* Return a list of items under <path>. Caller frees list and list contents.
 */
char **sitter_listDir(const char *path,uint32_t flags=SITTER_LISTDIR_SORT);

#endif
