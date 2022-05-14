#ifndef SITTER_WII
#include <cstdlib>
#include <cstring>
#include "sitter_Error.h"
#include "sitter_string.h"
#include "sitter_Menu.h"
#include "sitter_Game.h"
#include "sitter_surfacecvt.h"
#include "sitter_ResourceManager.h"
#include "sitter_Grid.h"
#include "sitter_Sprite.h"
#include "sitter_PlayerSprite.h"
#include "sitter_Player.h"
#include "sitter_Configuration.h"
#include "sitter_HighScores.h"
#include "sitter_HAKeyboard.h"
#include "sitter_VideoManager.h"

#define MENUV_INCREMENT 4
#define TEXV_INCREMENT 32
#define EDGEMAPV_INCREMENT 16

#define BLOTTER_COLOR 0x40206080

#ifndef GL_COMBINE // my Windows OpenGL headers don't have the TEV stuff...
#define GL_COMBINE				0x8570
#define GL_COMBINE_RGB				0x8571
#define GL_COMBINE_ALPHA			0x8572
#define GL_SOURCE0_RGB				0x8580
#define GL_SOURCE1_RGB				0x8581
#define GL_SOURCE2_RGB				0x8582
#define GL_SOURCE0_ALPHA			0x8588
#define GL_SOURCE1_ALPHA			0x8589
#define GL_SOURCE2_ALPHA			0x858A
#define GL_OPERAND0_RGB				0x8590
#define GL_OPERAND1_RGB				0x8591
#define GL_OPERAND2_RGB				0x8592
#define GL_OPERAND0_ALPHA			0x8598
#define GL_OPERAND1_ALPHA			0x8599
#define GL_OPERAND2_ALPHA			0x859A
#define GL_INTERPOLATE				0x8575
#define GL_CONSTANT				0x8576
#define GL_PRIMARY_COLOR			0x8577
#endif
#ifndef GL_CLAMP_TO_EDGE
  #define GL_CLAMP_TO_EDGE                      0x812f
#endif

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
VideoManager::VideoManager(Game *game,int w,int h,int flags):game(game) {

  spr_vis=NULL;
  menuv=NULL; menuc=menua=0;
  texv=NULL; texc=texa=0;
  gxtexbuf=NULL; gxtexbuflen=0;
  scalebuf=NULL; scalebuflen=0;
  
  #if defined(SITTER_LINUX_DRM)
    if (!(drm=sitter_drm_init())) sitter_throw(Error,"sitter_drm_init failed");
    w=sitter_drm_get_width(drm);
    h=sitter_drm_get_height(drm);
  #else
    if (SDL_Init(SDL_INIT_VIDEO)) sitter_throw(SDLError,"SDL_Init(SDL_INIT_VIDEO): %s",SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
  
    /* fullscreen snarkery */
    int sdlflags=SDL_OPENGL;
    if (flags&SITTER_VIDEO_FULLSCREEN) {
      sdlflags|=SDL_FULLSCREEN;
      if (flags&SITTER_VIDEO_CHOOSEDIMS) {
        SDL_Rect **modev=SDL_ListModes(NULL,sdlflags);
        if (!modev) sdlflags&=~SDL_FULLSCREEN; // no fullscreen
        else if (modev==(SDL_Rect**)-1) ; // any dimensions ok
        else { // check dimensions based on manhattan distance to requested dimensions
          int bestw=-1,besth=-1,bestdiff=0x7fffffff;
          for (SDL_Rect **modei=modev;*modei;modei++) {
            int thisdiff=(((*modei)->w>w)?((*modei)->w-w):(w-(*modei)->w));
            thisdiff+=(((*modei)->h>h)?((*modei)->h-h):(h-(*modei)->h));
            if (thisdiff<bestdiff) {
              bestdiff=thisdiff;
              bestw=(*modei)->w;
              besth=(*modei)->h;
            }
          }
          if (bestw<0) sdlflags&=~SDL_FULLSCREEN;
          else { w=bestw; h=besth; }
        }
      }
    }
    if (!(screen=SDL_SetVideoMode(w,h,32,sdlflags))) 
      sitter_throw(SDLError,"SDL_SetVideoMode(%d,%d,32,0x%x): %s",w,h,sdlflags,SDL_GetError());
  #endif
    
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_PROJECTION);
  glOrtho(0,w,h,0,0,1);
  glMatrixMode(GL_MODELVIEW);
  
  /* do we support non-power-of-two texture sizes? */
  const char *verstr=(char*)glGetString(GL_VERSION);
  if (verstr) {
    if ((verstr[0]>='2')&&(verstr[0]<='9')) force_power_of_two=false; // may fail with version 10 (we're just starting 3 now...)
    else goto check_extensions;
  } else goto check_extensions;
  if (0) {
    check_extensions:
    force_power_of_two=true;
    const char *extstr=(char*)glGetString(GL_EXTENSIONS);
    if (extstr) {
      int offset=0;
      while (extstr[offset]) {
        while (extstr[offset]==' ') offset++;
        int extnamelen=0; while (extstr[offset+extnamelen]&&(extstr[offset+extnamelen]!=' ')) extnamelen++;
        if ((extnamelen==31)&&!memcmp(extstr+offset,"GL_ARB_texture_non_power_of_two",31)) {
          force_power_of_two=false;
          break;
        }
        offset+=extnamelen;
      }
    }
  }
  if (force_power_of_two) {
    sitter_log("OpenGL version < 2.0 and GL_ARB_texture_non_power_of_two not found.");
    sitter_log("  Textures will be scaled up at loading to the next power of two.");
    sitter_log("  Some graphics will not be as pretty as I intended.");
  }
  
  debug_goals=game->cfg->getOption_bool("debug-goals");
  highscore_dirty=true;
}

VideoManager::~VideoManager() {
  #if SITTER_LINUX_DRM
    sitter_drm_quit(drm);
  #else
    SDL_Quit();
  #endif
  if (menuv) free(menuv);
  if (texv) free(texv);
  if (gxtexbuf) free(gxtexbuf);
  if (scalebuf) free(scalebuf);
}

void VideoManager::reconfigure() {
  #if !defined(SITTER_LINUX_DRM) //TODO i think this is not needed? check call sites
  int w=game->cfg->getOption_int("winwidth");
  int h=game->cfg->getOption_int("winheight");
  int sdlflags=SDL_OPENGL;
  if (game->cfg->getOption_bool("fullscreen")) {
    w=game->cfg->getOption_int("fswidth");
    h=game->cfg->getOption_int("fsheight");
    sdlflags|=SDL_FULLSCREEN;
    if (game->cfg->getOption_bool("fssmart")) {
      SDL_Rect **modev=SDL_ListModes(NULL,sdlflags);
      if (!modev) sdlflags&=~SDL_FULLSCREEN; // no fullscreen
      else if (modev==(SDL_Rect**)-1) ; // any dimensions ok
      else { // check dimensions based on manhattan distance to requested dimensions
        int bestw=-1,besth=-1,bestdiff=0x7fffffff;
        for (SDL_Rect **modei=modev;*modei;modei++) {
          int thisdiff=(((*modei)->w>w)?((*modei)->w-w):(w-(*modei)->w));
          thisdiff+=(((*modei)->h>h)?((*modei)->h-h):(h-(*modei)->h));
          if (thisdiff<bestdiff) {
            bestdiff=thisdiff;
            bestw=(*modei)->w;
            besth=(*modei)->h;
          }
        }
        if (bestw<0) sdlflags&=~SDL_FULLSCREEN;
        else { w=bestw; h=besth; }
      }
    }
  }
  if (!(screen=SDL_SetVideoMode(w,h,32,sdlflags))) 
    sitter_throw(SDLError,"SDL_SetVideoMode(%d,%d,32,0x%x): %s",w,h,sdlflags,SDL_GetError());
  glViewport(0,0,w,h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0,w,h,0,0,1);
  glMatrixMode(GL_MODELVIEW);
  for (int i=0;i<menuc;i++) menuv[i]->pack(false);
  #endif
}

/******************************************************************************
 * misc lists
 *****************************************************************************/
 
void VideoManager::pushMenu(Menu *menu) {
  if (menuc>=menua) {
    menua+=MENUV_INCREMENT;
    Menu **nv=(Menu**)realloc(menuv,sizeof(void*)*menua);
    if (!nv) sitter_throw(AllocationError,"");
    menuv=nv;
  }
  menuv[menuc++]=menu;
}

/******************************************************************************
 * maintenance
 *****************************************************************************/
 
void VideoManager::update() {
  for (int i=0;i<menuc;i++) menuv[i]->update();
}

/******************************************************************************
 * render, high level
 *****************************************************************************/

void VideoManager::draw() {
  #ifdef SITTER_LINUX_DRM
    sitter_drm_drain(drm);
  #endif
  glClear(GL_COLOR_BUFFER_BIT);
  //if (!game->gameover) {
    if (game->grid) drawGrid(game->grid);
    if (spr_vis) drawSprites(spr_vis);
    if (game->drawradar&&(!game->gameover||game->editing_map)) drawRadar();
    if (game->editing_map) drawEditorDecorations();
    else if (game->grid) drawControl();
    //if (menuc) drawBlotter(BLOTTER_COLOR);
  //}
  if (game->drawhighscores||menuc) drawBlotter(BLOTTER_COLOR);
  if (game->drawhighscores) drawHighScores();
  else highscore_dirty=true;
  for (int i=0;i<menuc;i++) drawMenu(menuv[i]);
  if (game->hakeyboard) drawHAKeyboard();
  glFlush();
  #ifdef SITTER_LINUX_DRM
    if (sitter_drm_swap(drm)<0) sitter_throw(Error,"Failed to swap video");
  #else
    SDL_GL_SwapBuffers();
  #endif
}

/******************************************************************************
 * render, handicapped-accessible keyboard
 *****************************************************************************/
 
void VideoManager::drawHAKeyboard() {

  glDisable(GL_TEXTURE_2D);
    
  glBegin(GL_QUADS);
  for (int i=0;i<game->hakeyboard->keyc;i++) if (HAKeyboard::HAKey *k=game->hakeyboard->keyv+i) {
    glColor4ub(k->bgcolor>>24,k->bgcolor>>16,k->bgcolor>>8,k->bgcolor);
    glVertex2i(k->r.left() ,k->r.top()   );
    glVertex2i(k->r.left() ,k->r.bottom());
    glVertex2i(k->r.right(),k->r.bottom());
    glVertex2i(k->r.right(),k->r.top()   );
  }
  glEnd();
  
  /* This causes problems. I'm not sure we need it.
  glBegin(GL_LINE_STRIP);
  for (int i=0;i<game->hakeyboard->keyc;i++) if (HAKeyboard::HAKey *k=game->hakeyboard->keyv+i) {
    glColor4ub(k->linecolor>>24,k->linecolor>>16,k->linecolor>>8,k->linecolor);
    glVertex2i(k->r.left() ,k->r.top()   );
    glVertex2i(k->r.left() ,k->r.bottom());
    glVertex2i(k->r.right(),k->r.bottom());
    glVertex2i(k->r.right(),k->r.top()   );
  }
  glEnd();
  /**/
  
  glEnable(GL_TEXTURE_2D);
  for (int i=0;i<game->hakeyboard->keyc;i++) if (HAKeyboard::HAKey *k=game->hakeyboard->keyv+i) {
    uint32_t ch=k->val[game->hakeyboard->shiftstate];
    char lbl[4]; lbl[0]=0;
    switch (ch) {
      case 0x08: memcpy(lbl,"DEL",4); break;
      case 0x0d: memcpy(lbl,"OK",3); break;
      case 0x20: memcpy(lbl,"SPC",4); break;
      case HAKEY_SHIFT: memcpy(lbl,"SHF",4); break;
      case HAKEY_CAPS: memcpy(lbl,"CAP",4); break;
      case HAKEY_SYMBOL: memcpy(lbl,"SYM",4); break;
      case HAKEY_PLAIN: memcpy(lbl,"lwr",4); break;
      case HAKEY_PSHIFT: memcpy(lbl,"shf",4); break;
      default: if ((ch>0x20)&&(ch<0x7f)) {
          lbl[0]=ch;
          lbl[1]=0;
        }
    }
    drawBoundedString(k->r.x,k->r.y,k->r.w,k->r.h,lbl,game->hakeyboard->fonttexid,k->fgcolor,12,16,true,true);
  }
}

/******************************************************************************
 * render, radar
 *****************************************************************************/
 
void VideoManager::drawRadar() {
  /* make a projection that puts the whole world onscreen without distorting aspect ratio */
  int screenw=getScreenWidth();
  int screenh=getScreenHeight();
  int worldw=(game->grid?(game->grid->colw*game->grid->colc):screenw);
  int worldh=(game->grid?(game->grid->rowh*game->grid->rowc):screenh);
  double worldaspect=((double)worldw)/((double)worldh);
  double screenaspect=((double)screenw)/((double)screenh);
  int draww,drawh;
  if (worldaspect>screenaspect) { // world wider than screen, match width first
    draww=screenw;//>>1;
    drawh=draww/worldaspect;
  } else {
    drawh=screenh;//>>1;
    draww=drawh*worldaspect;
  }
  int drawx=(screenw>>1)-(draww>>1);
  int drawy=(screenh>>1)-(drawh>>1);
  glViewport(drawx,screenh-(drawy+drawh),draww,drawh);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0,worldw,worldh,0,0,1);
  glMatrixMode(GL_MODELVIEW);
  glBindTexture(GL_TEXTURE_2D,game->radartexid);
  /* blotter */
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
    glColor4ub(0xff,0xff,0xff,0x80);
    glVertex2i(0,0);
    glVertex2i(0,worldh);
    glVertex2i(worldw,worldh);
    glVertex2i(worldw,0);
    glColor4ub(0xff,0xff,0xff,0xff);
  glEnd();
  glEnable(GL_TEXTURE_2D);
  /* draw grid */
  if (game->grid) {
    for (int row=0;row<game->grid->rowc;row++) {
      Rect dstr(0,row*game->grid->rowh,game->grid->colw,game->grid->rowh);
      for (int col=0;col<game->grid->colc;col++) {
        #define DRAWTEX(tc,tr,rot) { \
            double ax,ay,bx,by,cx,cy,dx,dy; \
            switch (rot) { \
              case   0: { ax=bx=tc*0.25; ay=dy=tr*0.25; cx=dx=ax+0.25; by=cy=ay+0.25; } break; \
              case  90: { ax=dx=tc*0.25; cy=dy=tr*0.25; bx=cx=ax+0.25; ay=by=cy+0.25; } break; \
              case 180: { cx=dx=tc*0.25; by=cy=tr*0.25; ax=bx=cx+0.25; ay=dy=by+0.25; } break; \
              case 270: { bx=cx=tc*0.25; ay=by=tr*0.25; ax=dx=bx+0.25; cy=dy=ay+0.25; } break; \
            } \
            glBegin(GL_QUADS); \
              glTexCoord2f(ax,ay); glVertex2i(dstr.left() ,dstr.top()   ); \
              glTexCoord2f(bx,by); glVertex2i(dstr.left() ,dstr.bottom()); \
              glTexCoord2f(cx,cy); glVertex2i(dstr.right(),dstr.bottom()); \
              glTexCoord2f(dx,dy); glVertex2i(dstr.right(),dstr.top()   ); \
            glEnd(); \
          }
        uint32_t prop=game->grid->getCellProperties(col,row);
        if (prop==(SITTER_GRIDCELL_GOAL|SITTER_GRIDCELL_SOLID)) DRAWTEX(0,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL1|SITTER_GRIDCELL_SOLID)) DRAWTEX(0,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL2|SITTER_GRIDCELL_SOLID)) DRAWTEX(1,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL3|SITTER_GRIDCELL_SOLID)) DRAWTEX(2,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL4|SITTER_GRIDCELL_SOLID)) DRAWTEX(3,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL|SITTER_GRIDCELL_SEMISOLID)) DRAWTEX(0,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL1|SITTER_GRIDCELL_SEMISOLID)) DRAWTEX(0,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL2|SITTER_GRIDCELL_SEMISOLID)) DRAWTEX(1,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL3|SITTER_GRIDCELL_SEMISOLID)) DRAWTEX(2,2,0)
        else if (prop==(SITTER_GRIDCELL_GOAL4|SITTER_GRIDCELL_SEMISOLID)) DRAWTEX(3,2,0)
        else if (prop&SITTER_GRIDCELL_ALLGOALS) DRAWTEX(3,1,0) // error, invalid goal
        else if (prop==SITTER_GRIDCELL_SOLID) DRAWTEX(0,3,0)
        else if (prop&SITTER_GRIDCELL_SOLID) DRAWTEX(3,1,0) // error, solid with something else
        else if ((prop&SITTER_GRIDCELL_KILLTOP)&&(prop&SITTER_GRIDCELL_SEMISOLID)) DRAWTEX(3,1,0) // error KILLTOP|SEMISOLID
        else {
          if (prop&SITTER_GRIDCELL_KILLTOP) DRAWTEX(2,3,0)
          if (prop&SITTER_GRIDCELL_KILLRIGHT) DRAWTEX(2,3,90)
          if (prop&SITTER_GRIDCELL_KILLBOTTOM) DRAWTEX(2,3,180)
          if (prop&SITTER_GRIDCELL_KILLLEFT) DRAWTEX(2,3,270)
          if (prop&SITTER_GRIDCELL_SEMISOLID) DRAWTEX(1,3,0)
        }
        #undef DRAWTEX
        dstr.x+=game->grid->colw;
      }
    }
  }
  /* draw sprites */
  for (int i=0;i<game->grp_vis->sprc;i++) if (Sprite *spr=game->grp_vis->sprv[i]) {
    int texcol=-1,texrow;
    if (spr->identity==SITTER_SPRIDENT_PLAYER) {
      if (((PlayerSprite*)spr)->plr) switch (((PlayerSprite*)spr)->plr->plrid) {
        case 1: texcol=0; texrow=0; break;
        case 2: texcol=1; texrow=0; break;
        case 3: texcol=2; texrow=0; break;
        case 4: texcol=3; texrow=0; break;
      }
    } else if (spr->hasGroup(game->grp_quarry)) {
      if (!spr->alive) {
        texcol=1; texrow=1;
      } else if (spr->ongoal) {
        texcol=2; texrow=1;
      } else {
        texcol=0; texrow=1;
      }
    } else if (spr->hasGroup(game->grp_solid)) {
      texcol=3; texrow=3;
    }
    if (texcol<0) continue;
    glBegin(GL_QUADS);
      glTexCoord2f(0.25f*texcol      ,0.25f*texrow      ); glVertex2i(spr->r.left() ,spr->r.top()   );
      glTexCoord2f(0.25f*texcol      ,0.25f*texrow+0.25f); glVertex2i(spr->r.left() ,spr->r.bottom());
      glTexCoord2f(0.25f*texcol+0.25f,0.25f*texrow+0.25f); glVertex2i(spr->r.right(),spr->r.bottom());
      glTexCoord2f(0.25f*texcol+0.25f,0.25f*texrow      ); glVertex2i(spr->r.right(),spr->r.top()   );
    glEnd();
  }
  /* restore projection */
  glViewport(0,0,screenw,screenh);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0,screenw,screenh,0,0,1);
  glMatrixMode(GL_MODELVIEW);
}

/******************************************************************************
 * render, high scores
 *****************************************************************************/
 
void VideoManager::drawHighScores() {
  Rect bounds=getBounds();
  if (highscore_dirty) {
    game->hisc->format(highscore_format_buffer,SITTER_HIGHSCORE_FORMAT_BUFFER_LEN,game->grid_set,
      game->grid_index,game->lvlplayerc,game->hsresult-1);
    highscore_dirty=false;
  }
  int top=bounds.y;
  for (int i=0;i<menuc;i++) if (menuv[i]->y+menuv[i]->h+10>top) top=menuv[i]->y+menuv[i]->h+10;
  drawBoundedString(bounds.x,top,bounds.w,bounds.h-top,highscore_format_buffer,game->hsfonttexid,0xffffffff,12,16,true,false);
}

void VideoManager::drawBlotter(uint32_t rgba) {
  Rect bounds=getBounds();
  glDisable(GL_TEXTURE_2D);
  glBegin(GL_QUADS);
    glColor4ub(rgba>>24,rgba>>16,rgba>>8,rgba);
    glVertex2i(bounds.x,bounds.y);
    glVertex2i(bounds.x,bounds.y+bounds.h);
    glVertex2i(bounds.x+bounds.w,bounds.y+bounds.h);
    glVertex2i(bounds.x+bounds.w,bounds.y);
  glEnd();
  glEnable(GL_TEXTURE_2D);
}

/******************************************************************************
 * render, Grid
 *****************************************************************************/
  
void VideoManager::drawGrid(Grid *grid) {
  if (!grid||!grid->cellv) return;
  Rect bounds=getBounds();
  glScissor(bounds.x,bounds.y,bounds.w,bounds.h);
  glEnable(GL_SCISSOR_TEST);
  /* background */
  int worldw=grid->colc*grid->colw;
  int worldh=grid->rowc*grid->rowh;
  int blotterleft=bounds.x,blottertop=bounds.y,blotterright=bounds.x+bounds.w,blotterbottom=bounds.y+bounds.h;
  if ((worldw<bounds.w)||(worldh<bounds.h)) { // grid smaller than screen on at least one axis
    drawBlotter(0x00000000);
    if (worldw<bounds.w) {
      blotterleft=bounds.x+(bounds.w>>1)-(worldw>>1);
      blotterright=blotterleft+worldw;
    }
    if (worldh<bounds.h) {
      blottertop=bounds.y+(bounds.h>>1)-(worldh>>1);
      blotterbottom=blottertop+worldh;
    }
  }
  if (grid->bgtexid>=0) {
    // TODO -- should the background scroll? possibly on a smaller scale than the full grid, for a depth effect?
    glBindTexture(GL_TEXTURE_2D,grid->bgtexid);
    glBegin(GL_QUADS);
      glColor4ub(0xff,0xff,0xff,0xff);
      glTexCoord2i(0,0); glVertex2i(blotterleft ,blottertop   );
      glTexCoord2i(0,1); glVertex2i(blotterleft ,blotterbottom);
      glTexCoord2i(1,1); glVertex2i(blotterright,blotterbottom);
      glTexCoord2i(1,0); glVertex2i(blotterright,blottertop   );
    glEnd();
  } else {
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
      glColor4ub(grid->bgcolor>>24,grid->bgcolor>>16,grid->bgcolor>>8,grid->bgcolor);
      glVertex2i(blotterleft ,blottertop   );
      glVertex2i(blotterleft ,blotterbottom);
      glVertex2i(blotterright,blotterbottom);
      glVertex2i(blotterright,blottertop   );
    glEnd();
    glEnable(GL_TEXTURE_2D);
  }
  // cx,cy,cw,ch: visible boundaries in cell coordinates
  int cx=grid->scrollx/grid->colw;
  int cy=grid->scrolly/grid->rowh;
  int cw=(bounds.w+grid->colw-1)/grid->colw+1;
  int ch=(bounds.h+grid->rowh-1)/grid->rowh+1;
  int xshift=-grid->scrollx%grid->colw;
  int yshift=-grid->scrolly%grid->rowh;
  for (int row=0;row<ch;row++) {
    int arow=cy+row;
    if ((arow<0)||(arow>=grid->rowc)) continue;
      // left,top,right,bottom: rendering boundaries of 1 cell
    int top=bounds.y+yshift+row*grid->rowh;
    int bottom=top+grid->rowh;
    uint8_t *rowcellv=grid->cellv+arow*grid->colc;
    for (int col=0;col<cw;col++) {
      int acol=cx+col;
      if ((acol<0)||(acol>=grid->colc)) continue;
      int left=bounds.x+xshift+col*grid->colw;
      int right=left+grid->colw;
      if (grid->tidv[rowcellv[acol]]<0) continue;
      glBindTexture(GL_TEXTURE_2D,grid->tidv[rowcellv[acol]]);
      glBegin(GL_QUADS);
        glColor4ub(0xff,0xff,0xff,0xff);
        glTexCoord2i(0,0); glVertex2i(left ,top   );
        glTexCoord2i(0,1); glVertex2i(left ,bottom);
        glTexCoord2i(1,1); glVertex2i(right,bottom);
        glTexCoord2i(1,0); glVertex2i(right,top   );
      glEnd();
    }
  }
  glDisable(GL_SCISSOR_TEST);
}

/******************************************************************************
 * render, Sprites
 *****************************************************************************/

void VideoManager::drawSprites(SpriteGroup *grp) {
  if (!grp->sprc) return;
  Rect bounds=getBounds();
  glScissor(bounds.x,bounds.y,bounds.w,bounds.h);
  glEnable(GL_SCISSOR_TEST);
  /* set up TEV for texture+tint+opacity */
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
  glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_INTERPOLATE);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_PRIMARY_COLOR);
  glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_SRC_COLOR);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE2_RGB,GL_PRIMARY_COLOR);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE2_RGB,GL_SRC_ALPHA);
  glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_MODULATE);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_CONSTANT);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_ALPHA,GL_TEXTURE);
  glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);
  /* draw each sprite */
  for (int i=0;i<grp->sprc;i++) if (Sprite *spr=grp->sprv[i]) {
    if (spr->texid<0) continue;
    GLfloat opcarg[4]={0.0f,0.0f,0.0f,spr->opacity};
    glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,opcarg);
    glBindTexture(GL_TEXTURE_2D,spr->texid);
    glPushMatrix();
    float halfw=spr->r.w/2.0f;
    float halfh=spr->r.h/2.0f;
    glTranslatef(bounds.x+spr->r.x-grp->scrollx+halfw,bounds.y+spr->r.y-grp->scrolly+halfh,0.0f);
    glRotatef(spr->rotation,0,0,1);
    bool flop=(spr->neverflop?false:spr->flop);
    glBegin(GL_QUADS);
      glColor4ub(spr->tint>>24,spr->tint>>16,spr->tint>>8,spr->tint);
        //TEMP: tint based on goal status
        if (debug_goals&&spr->ongoal) {
          switch (spr->goalid) {
            case 0: glColor4ub(0x00,0x00,0x00,0xff); break; 
            case 1: glColor4ub(0xff,0x00,0x00,0xff); break;
            case 2: glColor4ub(0x00,0xff,0x00,0xff); break;
            case 3: glColor4ub(0x00,0x00,0xff,0xff); break;
            case 4: glColor4ub(0xff,0xff,0x00,0xff); break;
            default: glColor4ub(0xff,0xff,0xff,0xff); break;
          }
        }
      glTexCoord2i(flop?1:0,0); glVertex2f(-halfw,-halfh);
      glTexCoord2i(flop?1:0,1); glVertex2f(-halfw, halfh);
      glTexCoord2i(flop?0:1,1); glVertex2f( halfw, halfh);
      glTexCoord2i(flop?0:1,0); glVertex2f( halfw,-halfh);
    glEnd();
    glPopMatrix();
  }
  /* reset TEV */
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  glDisable(GL_SCISSOR_TEST);
}

/******************************************************************************
 * render, Control Panel
 *****************************************************************************/

void VideoManager::drawControl() {
  Rect bounds=getBounds();

  /* clock */
  int seconds=game->play_clock/60; // 60 fps (if we don't hit the target rate, play clock is game time, not real time)
  int minutes=seconds/60; seconds%=60;
  char timestr[15];
  snprintf(timestr,15,"%d:%02d",minutes,seconds);
  timestr[14]=0;
  drawString(bounds.x+10,bounds.y+10,timestr,game->clocktexid,0xffffffc0,12,16);
  
  /* murder counter */
  if (game->murderc||(game->grid&&game->grid->murderlimit)) {
    int x=bounds.x+10,y=bounds.y+31;
    int skulli=0;
    #define SKULLW 16
    #define SKULLH 16
    #define XSTRIDE 20
    #define SKULLTINT_EMPTY 0xff,0xff,0xff,0x55
    #define SKULLTINT_FULL  0xff,0xff,0x00,0xff
    #define SKULLTINT_FATAL 0xff,0x00,0x00,0xff
    glBindTexture(GL_TEXTURE_2D,game->killtexid);
    glBegin(GL_QUADS);
      glColor4ub(SKULLTINT_FULL);
      for (;skulli<game->murderc;skulli++) {
        if (game->grid&&(skulli>=game->grid->murderlimit)) glColor4ub(SKULLTINT_FATAL);
        glTexCoord2i(0,0); glVertex2i(x,y);
        glTexCoord2i(0,1); glVertex2i(x,y+SKULLH);
        glTexCoord2i(1,1); glVertex2i(x+SKULLW,y+SKULLH);
        glTexCoord2i(1,0); glVertex2i(x+SKULLW,y);
        x+=XSTRIDE;
      }
      glColor4ub(SKULLTINT_EMPTY);
      for (;skulli<game->grid->murderlimit;skulli++) {
        glTexCoord2i(0,0); glVertex2i(x,y);
        glTexCoord2i(0,1); glVertex2i(x,y+SKULLH);
        glTexCoord2i(1,1); glVertex2i(x+SKULLW,y+SKULLH);
        glTexCoord2i(1,0); glVertex2i(x+SKULLW,y);
        x+=XSTRIDE;
      }
      glColor4ub(0xff,0xff,0xff,0xff);
    glEnd();
    #undef SKULLTINT_EMPTY
    #undef SKULLTINT_FULL
    #undef SKULLTINT_FATAL
    #undef SKULLW
    #undef SKULLH
    #undef XSTRIDE
  }
  
}

/******************************************************************************
 * render, special junk for editor
 *****************************************************************************/
 
void VideoManager::drawEditorDecorations() {
  if (!game->grid) return;
  int screenw=getScreenWidth();
  int screenh=getScreenHeight();
  uint8_t op8=(game->editor_orn_op*0xff);
  
  /* red square around selected cell */
  glDisable(GL_TEXTURE_2D);
  int x=game->editor_selx*game->grid->colw-game->camerax;
  int y=game->editor_sely*game->grid->rowh-game->cameray;
  glBegin(GL_LINE_LOOP);
    glColor4ub(0xff,0x00,0x00,op8);
    glVertex2i(x                 ,y                 );
    glVertex2i(x                 ,y+game->grid->rowh);
    glVertex2i(x+game->grid->colw,y+game->grid->rowh);
    glVertex2i(x+game->grid->colw,y                 );
  glEnd();
  glEnable(GL_TEXTURE_2D);
  
  if (game->editor_palette) {
    int left=(screenw>>1)-(game->grid->colw*8);
    int y=(screenh>>1)-(game->grid->rowh*8);
    int cellid=0;
    
    /* gray out background, even grayer where palette is going */
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
      glColor4ub(0x80,0x80,0x80,0x80);
      glVertex2i(0,0);
      glVertex2i(0,screenh);
      glVertex2i(screenw,screenh);
      glVertex2i(screenw,0);
      glColor4ub(0x55,0x55,0x55,0x80);
      glVertex2i(left,y);
      glVertex2i(left,y+game->grid->rowh*16);
      glVertex2i(left+game->grid->colw*16,y+game->grid->rowh*16);
      glVertex2i(left+game->grid->colw*16,y);
    glEnd();
    glEnable(GL_TEXTURE_2D);
      
    for (int row=0;row<16;row++) {
      int x=left;
      for (int col=0;col<16;col++) {
        if (game->grid->tidv[cellid]>=0) {
          glBindTexture(GL_TEXTURE_2D,game->grid->tidv[cellid]);
          glBegin(GL_QUADS);
            glColor4ub(0xff,0xff,0xff,0xff);
            glTexCoord2i(0,0); glVertex2i(x                 ,y                 );
            glTexCoord2i(0,1); glVertex2i(x                 ,y+game->grid->rowh);
            glTexCoord2i(1,1); glVertex2i(x+game->grid->colw,y+game->grid->rowh);
            glTexCoord2i(1,0); glVertex2i(x+game->grid->colw,y                 );
          glEnd();
        }
        if (cellid==game->grid->cellv[game->editor_sely*game->grid->colc+game->editor_selx]) {
          glDisable(GL_TEXTURE_2D);
          glBegin(GL_LINE_LOOP);
            glColor4ub(0x00,0xff,0xff,op8);
            glVertex2i(x                 ,y                 );
            glVertex2i(x                 ,y+game->grid->rowh);
            glVertex2i(x+game->grid->colw,y+game->grid->rowh);
            glVertex2i(x+game->grid->colw,y                 );
          glEnd();
          glEnable(GL_TEXTURE_2D);
        }
        x+=game->grid->colw;
        cellid++;
      }
      y+=game->grid->rowh;
    }
  }
  
}

/******************************************************************************
 * render, Menu
 *****************************************************************************/

void VideoManager::drawMenu(Menu *menu) {
  int screenw=getScreenWidth();
  int screenh=getScreenHeight();

  /* background */
  if (menu->bgtexid>=0) {
    glBindTexture(GL_TEXTURE_2D,menu->bgtexid);
    glBegin(GL_QUADS);
      glColor4ub(0xff,0xff,0xff,0xff);
      /* top-left */
      glTexCoord2f(0.0f,0.0f); glVertex2i(menu->x            ,menu->y            );
      glTexCoord2f(0.0f,0.5f); glVertex2i(menu->x            ,menu->y+menu->edgeh);
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew,menu->y+menu->edgeh);
      glTexCoord2f(0.5f,0.0f); glVertex2i(menu->x+menu->edgew,menu->y            );
      /* top-right */
      glTexCoord2f(0.5f,0.0f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y            );
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->edgeh);
      glTexCoord2f(1.0f,0.5f); glVertex2i(menu->x+menu->w            ,menu->y+menu->edgeh);
      glTexCoord2f(1.0f,0.0f); glVertex2i(menu->x+menu->w            ,menu->y            );
      /* bottom-left */
      glTexCoord2f(0.0f,0.5f); glVertex2i(menu->x            ,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(0.0f,1.0f); glVertex2i(menu->x            ,menu->y+menu->h            );
      glTexCoord2f(0.5f,1.0f); glVertex2i(menu->x+menu->edgew,menu->y+menu->h            );
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew,menu->y+menu->h-menu->edgeh);
      /* bottom-right */
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(0.5f,1.0f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->h            );
      glTexCoord2f(1.0f,1.0f); glVertex2i(menu->x+menu->w            ,menu->y+menu->h            );
      glTexCoord2f(1.0f,0.5f); glVertex2i(menu->x+menu->w            ,menu->y+menu->h-menu->edgeh);
      /* top */
      glTexCoord2f(0.5f,0.0f); glVertex2i(menu->x+menu->edgew        ,menu->y            );
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew        ,menu->y+menu->edgeh);
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->edgeh);
      glTexCoord2f(0.5f,0.0f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y            );
      /* bottom */
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew        ,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(0.5f,1.0f); glVertex2i(menu->x+menu->edgew        ,menu->y+menu->h            );
      glTexCoord2f(0.5f,1.0f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->h            );
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->h-menu->edgeh);
      /* left */
      glTexCoord2f(0.0f,0.5f); glVertex2i(menu->x            ,menu->y+menu->edgeh        );
      glTexCoord2f(0.0f,0.5f); glVertex2i(menu->x            ,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew,menu->y+menu->edgeh        );
      /* right */
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->edgeh        );
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(1.0f,0.5f); glVertex2i(menu->x+menu->w            ,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(1.0f,0.5f); glVertex2i(menu->x+menu->w            ,menu->y+menu->edgeh        );
      /* middle */
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew        ,menu->y+menu->edgeh        );
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->edgew        ,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->h-menu->edgeh);
      glTexCoord2f(0.5f,0.5f); glVertex2i(menu->x+menu->w-menu->edgew,menu->y+menu->edgeh        );
    glEnd();
  }
  
  /* bounding rectangle for foreground */
  int b_left=menu->x+menu->edgew;
  int b_top=menu->y+menu->edgeh;
  int b_width=(menu->x+menu->w-menu->edgew)-b_left;
  int b_height=(menu->y+menu->h-menu->edgeh)-b_top;
  
  /* title */
  if (menu->title) {
    drawBoundedString(b_left,b_top,b_width,menu->titlechh,
      menu->title,menu->titlefonttexid,menu->titlecolor,menu->titlechw,menu->titlechh,true,true);
    b_top+=menu->titlechh+menu->titlepad;
    b_height-=menu->titlechh+menu->titlepad;
  }
  
  /* items */
  if (menu->itemc) {
    int itemzoneh=menu->itemc*menu->itemh;
    int scrollbarw=menu->scrollbarw;
    if (itemzoneh>b_height) { // need scroll bar
      int firstitem=menu->itemscroll/menu->itemh;
      int itemc=(b_height+menu->itemh-1)/menu->itemh;
      if (firstitem+itemc>menu->itemc) itemc=menu->itemc-firstitem;
      if (itemc>0) { // shouldn't have allowed scroll this low, but... ok?
        glViewport(b_left,screenh-(b_top+b_height),b_width-scrollbarw,b_height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(b_left,b_left+b_width-scrollbarw,b_top+b_height,b_top,0,1);
        glMatrixMode(GL_MODELVIEW);
        drawMenuItems(menu,firstitem,itemc,b_left,b_top-(menu->itemscroll%menu->itemh),b_width-scrollbarw);
        glViewport(0,0,screenw,screenh);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0,screenw,screenh,0,0,1);
        glMatrixMode(GL_MODELVIEW);
      }
      /* scroll bar */
      drawScrollBar(menu->scrollbartexid,b_left+b_width-scrollbarw,b_top,scrollbarw,
        b_height,scrollbarw,menu->itemscroll,menu->itemscrolllimit,b_height);
    } else { // no scroll bar
      drawMenuItems(menu,0,menu->itemc,b_left,b_top,b_width);
    }
  }
  
}

/******************************************************************************
 * render, menu items
 *****************************************************************************/
 
void VideoManager::drawMenuItems(Menu *menu,int itemp,int itemc,int bx,int by,int bw) {
  int y=by;
  for (int mi=0;mi<itemc;mi++) {
    int x=bx,w=bw;
    if ((menu->indicatortexid>=0)&&(itemp+mi==menu->selection)) { // indicator
      glBindTexture(GL_TEXTURE_2D,menu->indicatortexid);
      int ileft=x;
      int iright=x+menu->indicatorw;
      int itop=y+(menu->itemh>>1)-(menu->indicatorh>>1);
      int ibottom=itop+menu->indicatorh;
      glBegin(GL_QUADS);
        glColor4ub(0xff,0xff,0xff,0xff);
        glTexCoord2i(0,0); glVertex2i(ileft ,itop   );
        glTexCoord2i(0,1); glVertex2i(ileft ,ibottom);
        glTexCoord2i(1,1); glVertex2i(iright,ibottom);
        glTexCoord2i(1,0); glVertex2i(iright,itop   );
      glEnd();
    }
    x+=menu->indicatorw;
    w-=menu->indicatorw;
    MenuItem *item=menu->itemv[itemp+mi];
    if (item->icontexid>=0) { // draw icon
      glBindTexture(GL_TEXTURE_2D,item->icontexid);
      int ileft=x+menu->itemiconpad;
      int iright=ileft+menu->itemiconw;
      int itop=y+(menu->itemh>>1)-(menu->itemiconh>>1);
      int ibottom=itop+menu->itemiconh;
      glBegin(GL_QUADS);
        glColor4ub(0xff,0xff,0xff,0xff);
        glTexCoord2i(0,0); glVertex2i(ileft ,itop   );
        glTexCoord2i(0,1); glVertex2i(ileft ,ibottom);
        glTexCoord2i(1,1); glVertex2i(iright,ibottom);
        glTexCoord2i(1,0); glVertex2i(iright,itop   );
      glEnd();
    }
    x+=menu->itemiconw+(menu->itemiconpad<<1);
    w-=menu->itemiconw+(menu->itemiconpad<<1);
    if (item->label) { // draw label
      uint32_t clr=(menu->itemv[itemp+mi]->btnid?menu->itemcolor:menu->itemdiscolor);
      if ((menu->selection==itemp+mi)&&(menu->selection==menu->maybeselect)) clr=menu->itemclickcolor;
      drawBoundedString(x,y,w,menu->itemh,item->label,menu->itemfonttexid,clr,menu->itemchw,menu->itemchh,false,true);
    }
    y+=menu->itemh;
  }
}

void VideoManager::drawScrollBar(int texid,int x,int y,int w,int h,int btnh,int pos,int limit,int range) {
  bool fake=false;
  if (glIsTexture(texid)) glBindTexture(GL_TEXTURE_2D,texid);
  else { fake=true; glDisable(GL_TEXTURE_2D); }
  Rect bounds=getBounds();
  x+=bounds.x;
  y+=bounds.y;
  int bottom=y+h;
  int upbtnbottom=y+btnh;
  int downbtntop=y+h-btnh;
  
  int minrange=(btnh*limit)/h; // thumb no smaller than btnh
  if (minrange>limit) { limit=minrange; range=minrange; } // sanity check
  if (range<minrange) range=minrange;
  else if (range>limit) range=limit;
  if (pos<0) pos=0; else if (pos+range>limit) pos=limit-range;
  int thumbtop=upbtnbottom+(pos*(h-(btnh<<1)))/limit;
  int thumbbottom=upbtnbottom+((pos+range)*(h-(btnh<<1)))/limit;
  
  if (fake) {
    glBegin(GL_TRIANGLES);
      glColor4ub(0x00,0xff,0xff,0xff);
      glVertex2i(x+(w>>1),y);
      glVertex2i(x,upbtnbottom);
      glVertex2i(x+w,upbtnbottom);
      glVertex2i(x,downbtntop);
      glVertex2i(x+(w>>1),y+h);
      glVertex2i(x+w,downbtntop);
    glEnd();
    glBegin(GL_QUADS);
      glColor4ub(0xff,0xff,0x00,0xff);
      glVertex2i(x,thumbtop);
      glVertex2i(x,thumbbottom);
      glVertex2i(x+w,thumbbottom);
      glVertex2i(x+w,thumbtop);
    glEnd();
    glEnable(GL_TEXTURE_2D);
  } else {
    glBegin(GL_QUADS);
      glColor4ub(0xff,0xff,0xff,0xff);
      glTexCoord2f(0.0f,0.00f); glVertex2i(x,y);
      glTexCoord2f(0.0f,0.33f); glVertex2i(x,upbtnbottom);
      glTexCoord2f(1.0f,0.33f); glVertex2i(x+w,upbtnbottom);
      glTexCoord2f(1.0f,0.00f); glVertex2i(x+w,y);
      glTexCoord2f(0.0f,0.34f); glVertex2i(x,thumbtop);
      glTexCoord2f(0.0f,0.66f); glVertex2i(x,thumbbottom);
      glTexCoord2f(1.0f,0.66f); glVertex2i(x+w,thumbbottom);
      glTexCoord2f(1.0f,0.34f); glVertex2i(x+w,thumbtop);
      glTexCoord2f(0.0f,0.67f); glVertex2i(x,downbtntop);
      glTexCoord2f(0.0f,1.00f); glVertex2i(x,y+h);
      glTexCoord2f(1.0f,1.00f); glVertex2i(x+w,y+h);
      glTexCoord2f(1.0f,0.67f); glVertex2i(x+w,downbtntop);
    glEnd();
  }
}

/******************************************************************************
 * render, text
 *****************************************************************************/
 
#define FR_1_16 0.06250
#define FR_1_6  0.16667
 
void VideoManager::drawString(int x,int y,const char *s,int texid,uint32_t fgclr,int chw,int chh) {
  if ((texid<0)||!s||!s[0]) return;
  int left=x;
  glBindTexture(GL_TEXTURE_2D,texid);
  glBegin(GL_QUADS);
    glColor4ub(fgclr>>24,fgclr>>16,fgclr>>8,fgclr);
    while (uint8_t inch=*s++) {
      if (inch=='\n') { x=left; y+=chh; continue; }
      if (inch==0x01) { glColor4ub(0xff,0xff,0x00,0xff); continue; }
      if (inch==0x02) { glColor4ub(fgclr>>24,fgclr>>16,fgclr>>8,fgclr); continue; }
      if ((inch<0x21)||(inch>0x7e)) { x+=chw; continue; }
      float col=(inch&0xf)*FR_1_16;
      float row=((inch-0x20)>>4)*FR_1_6;
      glTexCoord2f(col        ,row       ); glVertex2i(x    ,y    );
      glTexCoord2f(col        ,row+FR_1_6); glVertex2i(x    ,y+chh);
      glTexCoord2f(col+FR_1_16,row+FR_1_6); glVertex2i(x+chw,y+chh);
      glTexCoord2f(col+FR_1_16,row       ); glVertex2i(x+chw,y    );
      x+=chw;
    }
  glEnd();
}

bool VideoManager::drawBoundedString(int x,int y,int w,int h,const char *s,int texid,uint32_t fgclr,
int chw,int chh,bool xctr,bool yctr) {
  if (!s||!s[0]) return true;
  
  /* adjust chw and chh so it will fit, return if we can't fit it. */
  if ((w<1)||(h<1)) return false;
  int strw,strh; measureString(s,&strw,&strh);
  if (strw*chw>w) {
    chw=w/strw;
    if (chw<1) return false;
  }
  if (strh*chh>h) {
    chh=h/strh;
    if (chh<1) return false;
  }
  
  /* draw centered in the available space */
  int rx=x+(xctr?((w>>1)-((strw*chw)>>1)):0);
  int ry=y+(yctr?((h>>1)-((strh*chh)>>1)):0);
  drawString(rx,ry,s,texid,fgclr,chw,chh);
  return true;
}

void VideoManager::measureString(const char *s,int *maxw,int *h) {
  *maxw=0;
  *h=1;
  int roww=0;
  while (1) {
    uint8_t inch=*s++;
    if ((inch==0)||(inch=='\n')) {
      if (roww>*maxw) *maxw=roww;
      if (!inch) break;
      roww=0;
      (*h)++;
    } else roww++;
  }
}

/******************************************************************************
 * texture scaler
 *****************************************************************************/
 
static inline int nextPowerOfTwo(int n) {
  int p2=1;
  while (p2<n) {
    p2<<=1;
    if (!p2) return 0; // ???
  }
  return p2;
}

static void pxsetter_rgb888(void *dst,int x,int y,int stride,uint32_t px) {
  int offset=y*stride+x*3;
  ((uint8_t*)dst)[offset++]=px>>16;
  ((uint8_t*)dst)[offset++]=px>>8;
  ((uint8_t*)dst)[offset  ]=px;
}

static uint32_t pxgetter_rgb888(const void *src,int x,int y,int stride) {
  int offset=y*stride+x*3;
  uint32_t px=((uint8_t*)src)[offset++]<<16;
  px|=((uint8_t*)src)[offset++]<<8;
  px|=((uint8_t*)src)[offset];
  return px;
}

static void pxsetter_rgba8888(void *dst,int x,int y,int stride,uint32_t px) {
  int offset=y*stride+(x<<2);
  ((uint8_t*)dst)[offset++]=px>>24;
  ((uint8_t*)dst)[offset++]=px>>16;
  ((uint8_t*)dst)[offset++]=px>>8;
  ((uint8_t*)dst)[offset  ]=px;
}

static uint32_t pxgetter_rgba8888(const void *src,int x,int y,int stride) {
  int offset=y*stride+(x<<2);
  uint32_t px=((uint8_t*)src)[offset++]<<24;
  px|=((uint8_t*)src)[offset++]<<16;
  px|=((uint8_t*)src)[offset++]<<8;
  px|=((uint8_t*)src)[offset];
  return px;
}

static void pxsetter_n1(void *dst,int x,int y,int stride,uint32_t px) {
  int offset=y*stride+(x>>3);
  uint8_t mask=(0x80>>(x&7));
  if (px) ((uint8_t*)dst)[offset]|=mask;
  else ((uint8_t*)dst)[offset]&=~mask;
}

static uint32_t pxgetter_n1(const void *src,int x,int y,int stride) {
  int offset=y*stride+(x>>3);
  uint8_t mask=(0x80>>(x&7));
  return ((uint8_t*)src)[offset]&mask;
}

void VideoManager::scaleTexture(const void *src,int fmt,int w,int h,int dstw,int dsth) {
  if ((w<1)||(h<1)||(dstw<1)||(dsth<1)) sitter_throw(IdiotProgrammerError,"scale (%d,%d) to (%d,%d), yeah, great idea wise guy",w,h,dstw,dsth);
  int srcstride,dststride;
  int len;
  void (*setter)(void*,int,int,int,uint32_t);
  uint32_t (*getter)(const void*,int,int,int);
  switch (fmt) {
    case SITTER_TEXFMT_RGB888: {
        srcstride=w*3; 
        dststride=dstw*3; 
        len=dststride*dsth; 
        setter=pxsetter_rgb888;
        getter=pxgetter_rgb888;
      } break;
    case SITTER_TEXFMT_RGBA8888: {
        srcstride=w<<2; 
        dststride=dstw<<2; 
        len=dststride*dsth; 
        setter=pxsetter_rgba8888;
        getter=pxgetter_rgba8888;
      } break;
    case SITTER_TEXFMT_N1: {
        srcstride=((w+7)>>3); 
        dststride=((dstw+7)>>3); 
        len=dststride*dsth; 
        setter=pxsetter_n1;
        getter=pxgetter_n1;
      } break;
    default: sitter_throw(IdiotProgrammerError,"scaleTexture, format=%d",fmt);
  }
  if (len>scalebuflen) {
    if (scalebuf) free(scalebuf);
    if (!(scalebuf=malloc(len))) sitter_throw(AllocationError,"");
    scalebuflen=len;
  }
  int *xmap=(int*)malloc(sizeof(int)*dstw);
  if (!xmap) sitter_throw(AllocationError,"");
  for (int dstx=0;dstx<dstw;dstx++) xmap[dstx]=(dstx*w)/dstw;
  for (int dsty=0;dsty<dsth;dsty++) {
    int srcy=(dsty*h)/dsth;
    for (int dstx=0;dstx<dstw;dstx++) {
      setter(scalebuf,dstx,dsty,dststride,getter(src,xmap[dstx],srcy,srcstride));
    }
  }
  free(xmap);
}

/******************************************************************************
 * texture
 *****************************************************************************/
 
int VideoManager::_loadTexture(int fmt,int w,int h,const void *pixels,int flags,int texid) {
  if (!glIsTexture(texid)) glGenTextures(1,(GLuint*)&texid);
  glBindTexture(GL_TEXTURE_2D,texid);
  int sw=w,sh=h;
  if (force_power_of_two) {
    int p2w=nextPowerOfTwo(w);
    int p2h=nextPowerOfTwo(h);
    if ((p2w!=w)||(p2h!=h)) {
      if ((fmt>=SITTER_TEXFMT_GXRGBA8888)&&(fmt<=SITTER_TEXFMT_GXN4)) { // convert GX textures before scaling
        sw=p2w;
        sh=p2h;
      } else {
        scaleTexture(pixels,fmt,w,h,p2w,p2h);
        pixels=scalebuf;
        w=p2w; h=p2h;
      }
    }
  }
  switch (fmt) {
    case SITTER_TEXFMT_RGBA8888: glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels); break;
    case SITTER_TEXFMT_RGB888: glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,pixels); break;
    case SITTER_TEXFMT_N1: {
        GLushort ctab[2]={0x0000,0xffff};
        glPixelMapusv(GL_PIXEL_MAP_I_TO_R,2,ctab);
        glPixelMapusv(GL_PIXEL_MAP_I_TO_G,2,ctab);
        glPixelMapusv(GL_PIXEL_MAP_I_TO_B,2,ctab);
        glPixelMapusv(GL_PIXEL_MAP_I_TO_A,2,ctab);
        glTexImage2D(GL_TEXTURE_2D,0,GL_INTENSITY,w,h,0,GL_COLOR_INDEX,GL_BITMAP,pixels);
      } break;
    case SITTER_TEXFMT_GXRGBA8888: {
        int len=w*h*4;
        if (len>gxtexbuflen) {
          if (gxtexbuf) free(gxtexbuf);
          if (!(gxtexbuf=malloc(len))) sitter_throw(AllocationError,"");
        }
        sitter_gxdeinterlace_rgba8888_rgba8888(gxtexbuf,pixels,w,h);
        if ((sw!=w)||(sh!=h)) {
          scaleTexture(gxtexbuf,SITTER_TEXFMT_RGBA8888,w,h,sw,sh);
          pixels=scalebuf;
          w=sw; h=sh;
        } else pixels=gxtexbuf;
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
      } break;
    case SITTER_TEXFMT_GXRGB565: {
        int len=w*h*3;
        if (len>gxtexbuflen) {
          if (gxtexbuf) free(gxtexbuf);
          if (!(gxtexbuf=malloc(len))) sitter_throw(AllocationError,"");
        }
        sitter_gxdeinterlace_rgb565_rgb888(gxtexbuf,pixels,w,h);
        if ((sw!=w)||(sh!=h)) {
          scaleTexture(gxtexbuf,SITTER_TEXFMT_RGB888,w,h,sw,sh);
          pixels=scalebuf;
          w=sw; h=sh;
        } else pixels=gxtexbuf;
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,pixels);
      } break;
    case SITTER_TEXFMT_GXN4: {
        int len=(w*h)>>1;
        if (len>gxtexbuflen) {
          if (gxtexbuf) free(gxtexbuf);
          if (!(gxtexbuf=malloc(len))) sitter_throw(AllocationError,"");
        }
        sitter_gxdeinterlace_n4_n1(gxtexbuf,pixels,w,h);
        if ((sw!=w)||(sh!=h)) {
          scaleTexture(gxtexbuf,SITTER_TEXFMT_N1,w,h,sw,sh);
          pixels=scalebuf;
          w=sw; h=sh;
        } else pixels=gxtexbuf;
        GLushort ctab[2]={0x0000,0xffff};
        glPixelMapusv(GL_PIXEL_MAP_I_TO_R,2,ctab);
        glPixelMapusv(GL_PIXEL_MAP_I_TO_G,2,ctab);
        glPixelMapusv(GL_PIXEL_MAP_I_TO_B,2,ctab);
        glPixelMapusv(GL_PIXEL_MAP_I_TO_A,2,ctab);
        glTexImage2D(GL_TEXTURE_2D,0,GL_INTENSITY,w,h,0,GL_COLOR_INDEX,GL_BITMAP,pixels);
      } break;
    default: sitter_throw(ArgumentError,"texture format %d",fmt);
  }
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  switch (game->cfg->getOption_int("tex-filter")) {
    case 1: {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      } break;
    case 2: {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
      } break;
    default:
    case 0: {
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,(flags&SITTER_TEX_FILTER)?GL_LINEAR:GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,(flags&SITTER_TEX_FILTER)?GL_LINEAR:GL_NEAREST);
      } break;
  }
  return texid;
}

int VideoManager::loadTexture(const char *resname,bool safe,int flags) {
  int before;
  uint32_t hash=sitter_strhash(resname);
  int namelen=0; while (resname[namelen]) namelen++;
  if (texc) {
    int lo=0,hi=texc,ck=texc>>1;
    while (hi>lo) {
      if (hash==texv[ck].resnamehash) { 
        int cmp=strcmp(resname,texv[ck].resname);
        if (!cmp) { texv[ck].refc++; return texv[ck].texid; }
        if (cmp<0) hi=ck;
        else lo=ck+1;
      } else {
        if (hash<texv[ck].resnamehash) hi=ck;
        else lo=ck+1;
      }
      ck=(lo+hi)>>1;
    }
    before=hi;
  } else before=0;
  if (!game||!game->res) sitter_throw(IdiotProgrammerError,"game or game->res == NULL");
  int fmt,w,h; void *pixels;
  try {
    if (!game->res->loadImage(resname,&fmt,&w,&h,&pixels)) sitter_throw(FileNotFoundError,"image '%s' not found",resname);
  } catch (FileNotFoundError) {
    if (safe) return -1;
    throw;
  }
  if (texc>=texa) {
    texa+=TEXV_INCREMENT;
    if (!(texv=(TextureReference*)realloc(texv,sizeof(TextureReference)*texa))) sitter_throw(AllocationError,"");
  }
  if (before<texc) memmove(texv+before+1,texv+before,sizeof(TextureReference)*(texc-before));
  texc++;
  texv[before].resnamehash=hash;
  texv[before].refc=1;
  texv[before].texid=_loadTexture(fmt,w,h,pixels,flags);
  if (!(texv[before].resname=(char*)malloc(namelen+1))) sitter_throw(AllocationError,"");
  memcpy(texv[before].resname,resname,namelen+1);
  texv[before].resnamelen=namelen;
  free(pixels);
  return texv[before].texid;
}

// TODO -- I would like to leave textures cached until we *need* to overwrite them.
void VideoManager::unloadTexture(int texid) { // don't throw anything
  for (int i=0;i<texc;i++) if (texv[i].texid==texid) {
    if (--texv[i].refc<=0) {
      glDeleteTextures(1,(GLuint*)&texid);
      free(texv[i].resname);
      texc--;
      if (i<texc) memmove(texv+i,texv+i+1,sizeof(TextureReference)*(texc-i));
    }
    return;
  }
  // wasn't cached? try deleting it anyway...
  glDeleteTextures(1,(GLuint*)&texid);
}

#endif
