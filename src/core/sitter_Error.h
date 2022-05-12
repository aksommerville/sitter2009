#ifndef SITTER_ERROR_H
#define SITTER_ERROR_H

class Error {};
class AllocationError : public Error {};
class ArgumentError : public Error {};
  class KeyError : public ArgumentError {};
  class ValueError : public ArgumentError {};
class IdiotProgrammerError : public Error {};
class TODOError : public Error {};
class SDLError : public Error {};
class OpenGLError : public Error {};
class ZError : public Error {};
class IOError : public Error {};
  class ShortIOError : public IOError {};
  class FileNotFoundError : public IOError {};
  class FileAccessError : public IOError {};
class FormatError : public Error {};

extern char *sitter_err_msg;
extern int sitter_err_no;
extern const char *sitter_err_file;
extern const char *sitter_err_func;
extern int sitter_err_line;

void sitter_printError();
void sitter_clearError();
void _sitter_setError(const char *srcfile,const char *srcfunc,int srcline,const char *fmt,...);
void sitter_prependErrorMessage(const char *fmt,...);

#define SITTER_ERROR_VERBOSE    100
#define SITTER_ERROR_SILENT       0
#ifndef SITTER_ERROR_VERBOSITY
  #define SITTER_ERROR_VERBOSITY SITTER_ERROR_VERBOSE
#endif

#if SITTER_ERROR_VERBOSITY==SITTER_ERROR_VERBOSE
  #include <stdio.h>
  #define sitter_setError(fmt,...) _sitter_setError(__FILE__,__func__,__LINE__,fmt,##__VA_ARGS__)
  #ifdef SITTER_WII
    void sitter_log(const char *fmt,...);
  #else
    #define sitter_log(fmt,...) fprintf(stdout,fmt "\n",##__VA_ARGS__)
  #endif
  #define sitter_throw(C,fmt,...) do { \
      sitter_log("throw %s: " fmt,#C,##__VA_ARGS__); \
      sitter_setError(fmt,##__VA_ARGS__); \
      throw C(); \
    } while (0)
#elif SITTER_ERROR_VERBOSITY==SITTER_ERROR_SILENT
  #define sitter_setError(fmt,...) _sitter_setError(0,0,0,"")
  #define sitter_log(fmt,...)
  #define sitter_throw(C,fmt,...) throw C()
#else
  #error "Bad value for SITTER_ERROR_VERBOSITY (SITTER_ERROR_SILENT,SITTER_ERROR_VERBOSE)"
#endif
  
#endif
