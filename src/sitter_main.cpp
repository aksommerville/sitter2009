#include <stdlib.h>
#include <time.h>
#ifdef SITTER_WII
  #include <gccore.h>
  #include <fat.h>
#endif
#include "sitter_Error.h"
#include "sitter_Configuration.h"
#include "sitter_Game.h"

int main(int argc,char **argv) {
  try {
    #ifdef SITTER_WII
      VIDEO_Init();
      fatInitDefault();
      sitter_log("----- begin logging -----");
    #endif
    srand(time(0));
    Configuration *cfg=new Configuration();
    #ifdef SITTER_CFG_PREARGV
      cfg->readFiles(SITTER_CFG_PREARGV);
    #endif
    cfg->readArgv(argc,argv);
    #ifdef SITTER_CFG_POSTARGV
      cfg->readFiles(SITTER_CFG_POSTARGV);
    #endif
    Game *game=new Game(cfg);
    game->mmain();
    delete game;
    if (cfg->getOption_bool("autosave")) cfg->save(cfg->getOption_str("autosave-file"));
    delete cfg;
    #ifdef SITTER_WII
      sitter_log("------ end logging ------");
    #endif
  } catch (Error) {
    if (!sitter_err_msg) throw;
    sitter_printError();
    return 1;
  }
  return 0;
}

// ridiculous windows stuff follows:

#ifdef QQ_WIN32
  #include <windows.h>
  int console_main(int argc,char **argv) {
    return main(argc,argv);
  }
  int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
    int argc=1,arga=8;
    char **argv=(char**)malloc(sizeof(void*)*(arga+1));
    if (!argv) qq_throw(AllocError,"");
    argv[0]=(char*)"sitter";
    char quote=0;
    bool white=true;
    int argi=0;
    for (int i=0;((char*)szCmdLine)[i];i++) {
      char ch=((char*)szCmdLine)[i];
      if (ch==quote) quote=0;
      else if (ch=='\\') {
        if (!((char*)szCmdLine)[i+1]) break;
        i++;
      } else if (quote==0) {
        if ((ch=='"')||(ch=='\'')) { white=false; quote=ch; }
        else if (ch==' ') {
          if (!white) {
            white=true;
            ((char*)szCmdLine)[i]=0;
            if (argc>=arga) {
              arga+=8;
              if (!(argv=(char**)realloc(argv,sizeof(void*)*(arga+1)))) qq_throw(AllocError,"");
            }
            argv[argc++]=((char*)szCmdLine)+argi;
          }
          argi=i+1;
        } else white=false;
      }
    }
    if (!white) {
      if (argc>=arga) if (!(argv=(char**)realloc(argv,sizeof(void*)*(arga+2)))) qq_throw(AllocError,"");
      argv[argc++]=((char*)szCmdLine)+argi;
    }
    argv[argc]=0;     
    return main(argc,argv);
  }
#endif
