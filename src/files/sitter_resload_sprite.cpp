#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "sitter_Error.h"
#include "sitter_string.h"
#include "sitter_File.h"
#include "sitter_Game.h"
#include "sitter_VideoManager.h"
#include "sitter_Sprite.h"
#include "sitter_SpriteFace.h"
#include "sitter_ChildSprite.h"
#include "sitter_PlayerSprite.h"
#include "sitter_WalkingChildSprite.h"
#include "sitter_AcrobatChildSprite.h"
#include "sitter_HelicopterSprite.h"
#include "sitter_CrusherSprite.h"
#include "sitter_LumberjackSprite.h"
#include "sitter_SeesawSprite.h"
#include "sitter_BeltSprite.h"
#include "sitter_ClimbingChildSprite.h"
#include "sitter_BirdSprite.h"
#include "sitter_CannonSprite.h"
#include "sitter_HeightTestSprite.h"
#include "sitter_TeleporterSprite.h"
#include "sitter_ResourceManager.h"

/******************************************************************************
 * sprite loader
 *****************************************************************************/
 
Sprite *ResourceManager::loadSprite(const char *resname,int *xoffset,int *yoffset) {
  Sprite *spr=NULL;
  //sitter_log("sprite %s...",resname);
  lock();
  int _xoffset,_yoffset;
  if (!xoffset) xoffset=&_xoffset;
  if (!yoffset) yoffset=&_yoffset;
  *xoffset=*yoffset=0;
  try {
  const char *path=resnameToPath(resname,&sprpfx,&sprpfxlen);
  char *firstface=NULL;
  if (File *f=sitter_open(path,"rb")) {
    int lineno=0;
    try {
      while (char *line=f->readLine(true,true,true,true,&lineno)) {
        char **wordv=sitter_strsplit_white(line);
        int wordc=0; while (wordv[wordc]) wordc++;
        
        #define NUMBER(var,key) { \
            if (wordc!=2) sitter_throw(FormatError,"'%s' takes exactly one argument",key); \
            char *tail=0; var=strtol(wordv[1],&tail,0); \
            if (!tail||tail[0]) sitter_throw(FormatError,"expected number after '%s'",key); \
          }
        if (wordc) {
        
          if (!spr) {
            if ((wordc!=2)||strcasecmp(wordv[0],"class")) sitter_throw(FormatError,"sprite definition must begin with 'class CLASSNAME'");
            /* SPRITE CLASS LIST */
            unlock(); // sprites are allowed to access resources during load
            if (!strcasecmp(wordv[1],"Sprite")) spr=new Sprite(game);
            else if (!strcasecmp(wordv[1],"ChildSprite")) spr=new ChildSprite(game);
            else if (!strcasecmp(wordv[1],"PlayerSprite")) spr=new PlayerSprite(game);
            else if (!strcasecmp(wordv[1],"WalkingChildSprite")) spr=new WalkingChildSprite(game);
            else if (!strcasecmp(wordv[1],"AcrobatChildSprite")) spr=new AcrobatChildSprite(game);
            else if (!strcasecmp(wordv[1],"HelicopterSprite")) spr=new HelicopterSprite(game);
            else if (!strcasecmp(wordv[1],"HHelicopterSprite")) spr=new HHelicopterSprite(game);
            else if (!strcasecmp(wordv[1],"CrusherSprite")) spr=new CrusherSprite(game);
            else if (!strcasecmp(wordv[1],"LumberjackSprite")) spr=new LumberjackSprite(game);
            else if (!strcasecmp(wordv[1],"SeesawSprite")) spr=new SeesawSprite(game);
            else if (!strcasecmp(wordv[1],"BeltSprite")) spr=new BeltSprite(game);
            else if (!strcasecmp(wordv[1],"ClimbingChildSprite")) spr=new ClimbingChildSprite(game);
            else if (!strcasecmp(wordv[1],"BirdSprite")) spr=new BirdSprite(game);
            else if (!strcasecmp(wordv[1],"CannonSprite")) spr=new CannonSprite(game);
            else if (!strcasecmp(wordv[1],"CannonButtonSprite")) spr=new CannonButtonSprite(game);
            else if (!strcasecmp(wordv[1],"HeightTestSprite")) spr=new HeightTestSprite(game);
            else if (!strcasecmp(wordv[1],"TeleporterSprite")) spr=new TeleporterSprite(game);
            else if (!strcasecmp(wordv[1],"TeleporterRecSprite")) spr=new TeleporterRecSprite(game);
            else sitter_throw(FormatError,"unknown sprite class '%s'",wordv[1]);
            //sitter_log("%p %s",spr,wordv[1]);
            lock();
            
          } else {
            if (!strcasecmp(wordv[0],"width")) { NUMBER(spr->r.w,"width") if (spr->r.w<1) sitter_throw(FormatError,"invalid width %d",spr->r.w); }
            else if (!strcasecmp(wordv[0],"height")) { NUMBER(spr->r.h,"height") if (spr->r.h<1) sitter_throw(FormatError,"invalid height %d",spr->r.h); }
            
            else if (!strcasecmp(wordv[0],"xoffset")) { NUMBER(*xoffset,"xoffset") }
            else if (!strcasecmp(wordv[0],"yoffset")) { NUMBER(*yoffset,"yoffset") }
            
            else if (!strcasecmp(wordv[0],"groups")) {
              for (int i=1;i<wordc;i++) {
                if (!strcasecmp(wordv[i],"all")) spr->addGroup(game->grp_all);
                else if (!strcasecmp(wordv[i],"vis")) spr->addGroup(game->grp_vis);
                else if (!strcasecmp(wordv[i],"upd")) spr->addGroup(game->grp_upd);
                else if (!strcasecmp(wordv[i],"solid")) spr->addGroup(game->grp_solid);
                else if (!strcasecmp(wordv[i],"carry")) spr->addGroup(game->grp_carry);
                else if (!strcasecmp(wordv[i],"quarry")) spr->addGroup(game->grp_quarry);
                else if (!strcasecmp(wordv[i],"crushable")) spr->addGroup(game->grp_crushable);
                else if (!strcasecmp(wordv[i],"slicable")) spr->addGroup(game->grp_slicable);
                else sitter_throw(FormatError,"expected sprite group name, found '%s'",wordv[i]);
              }
              
            } else if (!strcasecmp(wordv[0],"face")) {
              if (wordc!=2) sitter_throw(FormatError,"'face' takes exactly one argument");
              if (!firstface) firstface=strdup(wordv[1]);
              SpriteFace *face=new SpriteFace(game,wordv[1]);
              spr->addFace(face);
              bool goneto=false;
              while (char *sline=f->readLine(true,true,true,true,&lineno)) {
                if (!strcasecmp(sline,"endface")) { free(sline); break; }
                else if (goneto) sitter_throw(FormatError,"'goto' must be final item in face");
                char **fdwordv=sitter_strsplit_white(sline);
                int fdwordc=0; while (fdwordv[fdwordc]) fdwordc++;
                if ((fdwordc<2)||(fdwordc>4)) sitter_throw(FormatError,"face line must be 2,3,4 words");
                if ((fdwordc==2)&&!strcasecmp(fdwordv[0],"goto")) {
                  goneto=true;
                  if (face->donefacename) free(face->donefacename);
                  if (!(face->donefacename=strdup(fdwordv[1]))) sitter_throw(AllocationError,"");
                  face->loopc=1;
                } else {
                  char *tail=0;
                  int delaymin=strtol(fdwordv[1],&tail,0);
                  if (!tail||tail[0]) sitter_throw(FormatError,"error parsing delay minimum");
                  int delaymax=-1;
                  uint32_t flags=SITTER_TEX_FILTER;
                  if (fdwordc>=3) {
                    delaymax=strtol(fdwordv[2],&tail,0);
                    if (!tail||tail[0]) sitter_throw(FormatError,"error parsing delay maximum");
                  }
                  if (fdwordc>=4) {
                    flags=strtol(fdwordv[3],&tail,0);
                    if (!tail||tail[0]) sitter_throw(FormatError,"error parsing flags");
                  }
                  unlock();
                  face->addFrame(fdwordv[0],delaymin,delaymax,flags);
                  lock();
                }
                for (char **fdwordi=fdwordv;*fdwordi;fdwordi++) free(*fdwordi);
                free(fdwordv);
                free(sline);
              }
              
            // i keep forgetting whether these were plural. so now they're both.
            } else if (!strcasecmp(wordv[0],"collision")||!strcasecmp(wordv[0],"collisions")) {
              for (int i=1;i<wordc;i++) {
                if (!strcasecmp(wordv[i],"edge")||!strcasecmp(wordv[i],"edges")) spr->edge_collisions=true;
                else if (!strcasecmp(wordv[i],"grid")||!strcasecmp(wordv[i],"grids")) spr->grid_collisions=true;
                else if (!strcasecmp(wordv[i],"sprite")||!strcasecmp(wordv[i],"sprites")) spr->sprite_collisions=true;
                else sitter_throw(FormatError,"expected collision class, found '%s'",wordv[i]);
              }
              
            } else if (!strcasecmp(wordv[0],"sprid")) {
              if (wordc!=2) sitter_throw(FormatError,"'sprid' takes one argument");
              if (!strcasecmp(wordv[1],"belt")) spr->identity=SITTER_SPRIDENT_BELT;
              else if (!strcasecmp(wordv[1],"chainsaw")) spr->identity=SITTER_SPRIDENT_BELT;
              else if (!strcasecmp(wordv[1],"tuxdoll")) spr->identity=SITTER_SPRIDENT_TUXDOLL;
              else if (!strcasecmp(wordv[1],"cannon")) spr->identity=SITTER_SPRIDENT_CANNON;
              else if (!strcasecmp(wordv[1],"cbtn-fire")) spr->identity=SITTER_SPRIDENT_CBTN_FIRE;
              else if (!strcasecmp(wordv[1],"cbtn-clk")) spr->identity=SITTER_SPRIDENT_CBTN_CLK;
              else if (!strcasecmp(wordv[1],"cbtn-ctr")) spr->identity=SITTER_SPRIDENT_CBTN_CTR;
              else if (!strcasecmp(wordv[1],"teleporter")) spr->identity=SITTER_SPRIDENT_TELEPORTER;
              else if (!strcasecmp(wordv[1],"teleporterrec")) spr->identity=SITTER_SPRIDENT_TELEPORTERREC;
              else if (!strcasecmp(wordv[1],"player")) spr->identity=SITTER_SPRIDENT_PLAYER;
              else if (!strcasecmp(wordv[1],"helicopter")) spr->identity=SITTER_SPRIDENT_HELICOPTER;
              else sitter_throw(FormatError,"expected sprite identity, found '%s'",wordv[1]);
              
            } else if (!strcasecmp(wordv[0],"gravity")) {
              if (wordc!=1) sitter_throw(FormatError,"'gravity' takes no arguments");
              spr->turnOnGravity();
              
            } else if (!strcasecmp(wordv[0],"neverflop")) {
              if (wordc!=1) sitter_throw(FormatError,"'neverflop' takes no arguments");
              spr->neverflop=true;
              
            } else if (!strcasecmp(wordv[0],"pushable")) {
              if (wordc!=1) sitter_throw(FormatError,"'pushable' takes no arguments");
              spr->pushable=true;
              
            } else sitter_throw(FormatError,"unexpected command '%s'",wordv[0]);
          }
        }
        #undef NUMBER
        
        for (char **wordi=wordv;*wordi;wordi++) free(*wordi);
        free(wordv);
        free(line);
      }
    } catch (...) {
      sitter_prependErrorMessage("%s:%d: ",path,lineno);
      delete f;
      throw;
    }
    delete f;
  } else sitter_throw(FileNotFoundError,"sprite '%s'",resname);
  if (!spr) sitter_throw(FormatError,"sprite '%s': init failed (incomplete file?)",resname);
  if (firstface) { 
    spr->setFace(firstface); 
    free(firstface); 
    spr->update_animation();
  }
  } catch (...) { unlock(); throw; }
  unlock();
  //sitter_log("...sprite");
  return spr;
}
