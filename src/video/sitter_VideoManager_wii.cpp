#ifdef SITTER_WII

#include <malloc.h>
#include <string.h>
#include "sitter_Error.h"
#include "sitter_string.h"
#include "sitter_Menu.h"
#include "sitter_Game.h"
#include "sitter_surfacecvt.h"
#include "sitter_ResourceManager.h"
#include "sitter_Grid.h"
#include "sitter_Sprite.h"
#include "sitter_PlayerSprite.h"
#include "sitter_HighScores.h"
#include "sitter_Player.h"
#include "sitter_Configuration.h"
#include "sitter_HAKeyboard.h"
#include "sitter_VideoManager.h"

#ifndef PI
  #define PI 3.1415926535897931
#endif

#define BLOTTER_COLOR 0x40206080

#define MENUV_INCREMENT 4
#define TEXV_INCREMENT 32
#define EDGEMAPV_INCREMENT 16

#define GXFIFO_SIZE (256*1024)

/******************************************************************************
 * init / kill
 *****************************************************************************/
 
VideoManager::VideoManager(Game *game,int w,int h,int flags):game(game) {

  spr_vis=NULL;
  menuv=NULL; menuc=menua=0;
  texv=NULL; texc=0;
  whichfb=0;
  highscore_dirty=true;
  debug_goals=game->cfg->getOption_bool("debug-goals");
  
  rmode=VIDEO_GetPreferredMode(0);
  if (CONF_GetAspectRatio()) { // pepsiman's recommendation to MPlayer, found on wiibrew
    rmode->viWidth=678;        // ...ok, it works on mom's tv. :)
    rmode->viXOrigin=(VI_MAX_WIDTH_PAL-678)/2;
  }
  VIDEO_Configure(rmode);
  
  vscreenw=game->cfg->getOption_int("fswidth");
  if (vscreenw<1) vscreenw=rmode->fbWidth;
  vscreenh=game->cfg->getOption_int("fsheight");
  if (vscreenh<1) vscreenh=rmode->efbHeight;
  
  xfb[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  xfb[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  gxfifo=memalign(32,GXFIFO_SIZE);
  if (!xfb[0]||!xfb[1]||!gxfifo) sitter_throw(AllocationError,"");
  memset(gxfifo,0,GXFIFO_SIZE);
  
  GX_Init(gxfifo,GXFIFO_SIZE);
  
  // copied from troon. i don't think this block is necessary:
  float yscale=GX_GetYScaleFactor(rmode->efbHeight,rmode->xfbHeight);
  int xfbheight_tmp=GX_SetDispCopyYScale(yscale);
  GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
  GX_SetDispCopyDst(rmode->fbWidth,xfbheight_tmp);
  GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
  GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
  GX_SetDispCopyGamma(GX_GM_1_0);
  GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
  
  GX_SetCullMode(GX_CULL_NONE);
  Mtx44 prjmtx;
  guOrtho(prjmtx,0,vscreenh,0,vscreenw,0,1);
  GX_LoadProjectionMtx(prjmtx,GX_ORTHOGRAPHIC);
  Mtx44 mdlmtx;
  guMtxIdentity(mdlmtx);
  GX_LoadPosMtxImm(mdlmtx,GX_PNMTX0);
  GXColor bgcolor={0,0,0,0xff};
  GX_SetCopyClear(bgcolor,0xffffff);
  GX_SetBlendMode(GX_BM_BLEND,GX_BL_SRCALPHA,GX_BL_INVSRCALPHA,GX_LO_CLEAR);
  GX_SetNumTevStages(1);
  GX_SetNumChans(2);
  GX_SetNumTexGens(1);
  GX_SetTexCoordGen(GX_TEXCOORD0,GX_TG_MTX2x4,GX_TG_TEX0,GX_IDENTITY);
  tevBlend();
  viewportFull();
  
  GX_InvalidateTexAll();
  GX_InvVtxCache();
  
  GX_DrawDone();
  GX_CopyDisp(xfb[0],GX_TRUE);
  VIDEO_SetNextFramebuffer(xfb[0]);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if (rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
  VIDEO_SetBlack(0);
}

VideoManager::~VideoManager() {
  if (menuv) free(menuv);
  for (int i=0;i<texc;i++) {
    if (texv[i].pixels) free(texv[i].pixels);
    if (texv[i].resname) free(texv[i].resname);
  }
  if (texv) free(texv);
}

void VideoManager::reconfigure() {
  vscreenw=game->cfg->getOption_int("fswidth");
  if (vscreenw<1) vscreenw=rmode->fbWidth;
  vscreenh=game->cfg->getOption_int("fsheight");
  if (vscreenh<1) vscreenh=rmode->efbHeight;
  Mtx44 prjmtx;
  guOrtho(prjmtx,0,vscreenh,0,vscreenw,0,1);
  GX_LoadProjectionMtx(prjmtx,GX_ORTHOGRAPHIC);
  viewportFull();
  for (int i=0;i<menuc;i++) menuv[i]->pack(false);
}

void VideoManager::viewportFull() {
  int xover=game->cfg->getOption_int("wii-overscan-x");
  int yover=game->cfg->getOption_int("wii-overscan-y");
  GX_SetViewport(xover>>1,yover>>1,rmode->fbWidth-xover,rmode->efbHeight-yover,0,1);
  GX_SetScissor(xover>>1,yover>>1,rmode->fbWidth-xover,rmode->efbHeight-yover);
}

/******************************************************************************
 * misc lists
 *****************************************************************************/
 
void VideoManager::pushMenu(Menu *menu) {
  if (menuc>=menua) {
    menua+=MENUV_INCREMENT;
    if (!(menuv=(Menu**)realloc(menuv,sizeof(void*)*menua))) sitter_throw(AllocationError,"");
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
  GX_InvVtxCache();
  
  if (game->grid) drawGrid(game->grid);
  if (spr_vis) drawSprites(spr_vis);
  if (game->drawradar&&!game->gameover) drawRadar();
  if (game->editing_map) drawEditorDecorations();
  else if (game->grid) drawControl();
  if (game->drawhighscores||menuc) drawBlotter(BLOTTER_COLOR);
  if (game->drawhighscores) drawHighScores();
  else highscore_dirty=true;
  for (int i=0;i<menuc;i++) drawMenu(menuv[i]);
  if (game->hakeyboard) drawHAKeyboard();

  GX_DrawDone();
  GX_CopyDisp(xfb[whichfb],GX_TRUE);
  VIDEO_SetNextFramebuffer(xfb[whichfb]);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  whichfb^=1;
}

/******************************************************************************
 * render, handicapped-accessible keyboard
 *****************************************************************************/
 
void VideoManager::drawHAKeyboard() {
  for (int i=0;i<game->hakeyboard->keyc;i++) if (HAKeyboard::HAKey *k=game->hakeyboard->keyv+i) {
    #define CLR(lbl) GX_Color4u8(k->lbl##color>>24,k->lbl##color>>16,k->lbl##color>>8,k->lbl##color);
    /* background */
    tevRasterOnly();
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      GX_Position2s16(k->r.left() ,k->r.top()   ); CLR(bg)
      GX_Position2s16(k->r.left() ,k->r.bottom()); CLR(bg)
      GX_Position2s16(k->r.right(),k->r.bottom()); CLR(bg)
      GX_Position2s16(k->r.right(),k->r.top()   ); CLR(bg)
    GX_End();
    /* outline */
    GX_Begin(GX_LINES,GX_VTXFMT0,5);
      GX_Position2s16(k->r.left() ,k->r.top()   ); CLR(line)
      GX_Position2s16(k->r.left() ,k->r.bottom()); CLR(line)
      GX_Position2s16(k->r.right(),k->r.bottom()); CLR(line)
      GX_Position2s16(k->r.right(),k->r.top()   ); CLR(line)
      GX_Position2s16(k->r.left() ,k->r.top()   ); CLR(line)
    GX_End();
    #undef CLR
    /* foreground */
    tevBlend();
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
  int worldw=(game->grid?(game->grid->colw*game->grid->colc):rmode->fbWidth);
  int worldh=(game->grid?(game->grid->rowh*game->grid->rowc):rmode->efbHeight);
  double worldaspect=((double)worldw)/((double)worldh);
  double screenaspect=((double)rmode->fbWidth)/((double)rmode->efbHeight);
  int draww,drawh;
  if (worldaspect>screenaspect) { // world wider than screen, match width first
    draww=rmode->fbWidth;//>>1;
    drawh=draww/worldaspect;
  } else {
    drawh=rmode->efbHeight;//>>1;
    draww=drawh*worldaspect;
  }
  int drawx=(rmode->fbWidth>>1)-(draww>>1);
  int drawy=(rmode->efbHeight>>1)-(drawh>>1);
  GX_SetViewport(drawx,drawy,draww,drawh,0,1);
  GX_SetScissor(drawx,drawy,draww,drawh);
  Mtx44 prjmtx;
  guOrtho(prjmtx,0,worldh,0,worldw,0,1);
  GX_LoadProjectionMtx(prjmtx,GX_ORTHOGRAPHIC);
  bindTexture(game->radartexid);
  /* blotter */
  tevRasterOnly();
  GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    GX_Position2s16(     0,     0); GX_Color4u8(0xff,0xff,0xff,0x80);
    GX_Position2s16(     0,worldh); GX_Color4u8(0xff,0xff,0xff,0x80);
    GX_Position2s16(worldw,worldh); GX_Color4u8(0xff,0xff,0xff,0x80);
    GX_Position2s16(worldw,     0); GX_Color4u8(0xff,0xff,0xff,0x80);
  GX_End();
  /* draw grid */
  tevTextureSegment();
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
            GX_Begin(GX_QUADS,GX_VTXFMT0,4); \
              GX_Position2s16(dstr.left() ,dstr.top()   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2f32(ax,ay); \
              GX_Position2s16(dstr.left() ,dstr.bottom()); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2f32(bx,by); \
              GX_Position2s16(dstr.right(),dstr.bottom()); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2f32(cx,cy); \
              GX_Position2s16(dstr.right(),dstr.top()   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2f32(dx,dy); \
            GX_End(); \
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
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      #define NOCLR GX_Color4u8(0xff,0xff,0xff,0xff);
      GX_Position2s16(spr->r.left() ,spr->r.top()   ); NOCLR GX_TexCoord2f32(0.25f*texcol      ,0.25f*texrow      );
      GX_Position2s16(spr->r.left() ,spr->r.bottom()); NOCLR GX_TexCoord2f32(0.25f*texcol      ,0.25f*texrow+0.25f);
      GX_Position2s16(spr->r.right(),spr->r.bottom()); NOCLR GX_TexCoord2f32(0.25f*texcol+0.25f,0.25f*texrow+0.25f);
      GX_Position2s16(spr->r.right(),spr->r.top()   ); NOCLR GX_TexCoord2f32(0.25f*texcol+0.25f,0.25f*texrow      );
      #undef NOCLR
    GX_End();
  }
  tevBlend();
  /* restore projection */
  viewportFull();
  guOrtho(prjmtx,0,vscreenh,0,vscreenw,0,1);
  GX_LoadProjectionMtx(prjmtx,GX_ORTHOGRAPHIC);
}

/******************************************************************************
 * render, high scores
 *****************************************************************************/
 
void VideoManager::drawHighScores() {
  if (highscore_dirty) {
    game->hisc->format(highscore_format_buffer,SITTER_HIGHSCORE_FORMAT_BUFFER_LEN,game->grid_set,
      game->grid_index,game->lvlplayerc,game->hsresult-1);
    highscore_dirty=false;
  }
  int top=0;
  for (int i=0;i<menuc;i++) if (menuv[i]->y+menuv[i]->h+10>top) top=menuv[i]->y+menuv[i]->h+10;
  drawBoundedString(0,top,vscreenw,vscreenh-top,highscore_format_buffer,game->hsfonttexid,0xffffffff,12,16,true,false);
}

void VideoManager::drawBlotter(uint32_t rgba) {
  tevRasterOnly();
  GX_Begin(GX_QUADS,GX_VTXFMT0,4);
    #define CLR GX_Color4u8(rgba>>24,rgba>>16,rgba>>8,rgba);
    GX_Position2s16(0,0); CLR
    GX_Position2s16(0,vscreenh); CLR
    GX_Position2s16(vscreenw,vscreenh); CLR
    GX_Position2s16(vscreenw,0); CLR
    #undef CLR
  GX_End();
  tevBlend();
}

/******************************************************************************
 * render, Grid
 *****************************************************************************/
  
void VideoManager::drawGrid(Grid *grid) {
  if (!grid||!grid->cellv) return;
  /* background */
  int worldw=grid->colc*grid->colw;
  int worldh=grid->rowc*grid->rowh;
  int blotterleft=0,blottertop=0,blotterright=vscreenw,blotterbottom=vscreenh;
  if ((worldw<vscreenw)||(worldh<vscreenh)) { // grid smaller than screen on at least one axis
    drawBlotter(0x00000000);
    if (worldw<vscreenw) {
      blotterleft=(vscreenw>>1)-(worldw>>1);
      blotterright=blotterleft+worldw;
    }
    if (worldh<vscreenh) {
      blottertop=(vscreenh>>1)-(worldh>>1);
      blotterbottom=blottertop+worldh;
    }
  }
  if (isTexture(grid->bgtexid)) {
    bindTexture(grid->bgtexid);
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      GX_Position2s16(blotterleft ,blottertop   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(0,0);
      GX_Position2s16(blotterleft ,blotterbottom); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(0,1);
      GX_Position2s16(blotterright,blotterbottom); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(1,1);
      GX_Position2s16(blotterright,blottertop   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(1,0);
    GX_End();
  } else {
    tevRasterOnly();
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      #define CLR grid->bgcolor>>24,grid->bgcolor>>16,grid->bgcolor>>8,grid->bgcolor
      GX_Position2s16(blotterleft ,blottertop   ); GX_Color4u8(CLR);
      GX_Position2s16(blotterleft ,blotterbottom); GX_Color4u8(CLR);
      GX_Position2s16(blotterright,blotterbottom); GX_Color4u8(CLR);
      GX_Position2s16(blotterright,blottertop   ); GX_Color4u8(CLR);
      #undef CLR
    GX_End();
    tevBlend();
  }
  // cx,cy,cw,ch: visible boundaries in cell coordinates
  int cx=grid->scrollx/grid->colw;
  int cy=grid->scrolly/grid->rowh;
  int cw=(vscreenw+grid->colw-1)/grid->colw+1;
  int ch=(vscreenh+grid->rowh-1)/grid->rowh+1;
  int xshift=-grid->scrollx%grid->colw;
  int yshift=-grid->scrolly%grid->rowh;
  tevTextureOnly();
  for (int row=0;row<ch;row++) {
    int arow=cy+row;
    if ((arow<0)||(arow>=grid->rowc)) continue;
      // left,top,right,bottom: rendering boundaries of 1 cell
    int top=yshift+row*grid->rowh;
    int bottom=top+grid->rowh;
    uint8_t *rowcellv=grid->cellv+arow*grid->colc;
    for (int col=0;col<cw;col++) {
      int acol=cx+col;
      if ((acol<0)||(acol>=grid->colc)) continue;
      int left=xshift+col*grid->colw;
      int right=left+grid->colw;
      if (grid->tidv[rowcellv[acol]]<0) continue;
      bindTexture(grid->tidv[rowcellv[acol]]);
      GX_Begin(GX_QUADS,GX_VTXFMT0,4);
        GX_Position2s16(left ,top   ); GX_TexCoord2u8(0,0);
        GX_Position2s16(left ,bottom); GX_TexCoord2u8(0,1);
        GX_Position2s16(right,bottom); GX_TexCoord2u8(1,1);
        GX_Position2s16(right,top   ); GX_TexCoord2u8(1,0);
      GX_End();
    }
  }
  tevBlend();
}

/******************************************************************************
 * render, Sprites
 *****************************************************************************/

void VideoManager::drawSprites(SpriteGroup *grp) {
  if (!grp->sprc) return;
  tevSprite();
  for (int i=0;i<grp->sprc;i++) if (Sprite *spr=grp->sprv[i]) {
    if (spr->texid<0) continue;
    GXColor kclr={0,0,0,spr->opacity*0xff};
    GX_SetTevColor(GX_TEVREG0,kclr);
    bindTexture(spr->texid);
    float halfw=spr->r.w/2.0f;
    float halfh=spr->r.h/2.0f;
    Mtx44 mtxt,mtxr,sprmtx;
    guMtxTrans(mtxt,spr->r.x-grp->scrollx+halfw,spr->r.y-grp->scrolly+halfh,0);
    guMtxRotRad(mtxr,'z',(spr->rotation*PI)/180.0);
    guMtxConcat(mtxt,mtxr,sprmtx);
    GX_LoadPosMtxImm(sprmtx,GX_PNMTX0);
    bool flop=(spr->neverflop?false:spr->flop);
    uint8_t r,g,b,a;
    if (debug_goals&&spr->ongoal) switch (spr->goalid) {
      case 0: r=0x00; g=0x00; b=0x00; a=0xff; break;
      case 1: r=0xff; g=0x00; b=0x00; a=0xff; break;
      case 2: r=0x00; g=0xff; b=0x00; a=0xff; break;
      case 3: r=0x00; g=0x00; b=0xff; a=0xff; break;
      case 4: r=0xff; g=0xff; b=0x00; a=0xff; break;
      default:r=0xff; g=0xff; b=0xff; a=0xff; break;
    } else { r=spr->tint>>24; g=spr->tint>>16; b=spr->tint>>8; a=spr->tint; }
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      GX_Position2f32(-halfw,-halfh); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(flop?1:0,0);
      GX_Position2f32(-halfw, halfh); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(flop?1:0,1);
      GX_Position2f32( halfw, halfh); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(flop?0:1,1);
      GX_Position2f32( halfw,-halfh); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(flop?0:1,0);
    GX_End();
  }
  tevBlend();
  Mtx44 imtx;
  guMtxIdentity(imtx);
  GX_LoadPosMtxImm(imtx,GX_PNMTX0);
}

/******************************************************************************
 * render, Control Panel
 *****************************************************************************/

void VideoManager::drawControl() {

  /* clock */
  int seconds=game->play_clock/60; // 60 fps (if we don't hit the target rate, play clock is game time, not real time)
  int minutes=seconds/60; seconds%=60;
  char timestr[15];
  snprintf(timestr,15,"%d:%02d",minutes,seconds);
  timestr[14]=0;
  drawString(10,10,timestr,game->clocktexid,0x4080ffff,12,16);
  
  /* murder counter */
  if (game->murderc||(game->grid&&game->grid->murderlimit)) {
    int x=10,y=31;
    int skulli=0;
    #define SKULLW 16
    #define SKULLH 16
    #define XSTRIDE 20
    #define SKULLTINT_EMPTY 0xff,0xff,0xff,0x55
    #define SKULLTINT_FULL  0xff,0xff,0x00,0xff
    #define SKULLTINT_FATAL 0xff,0x00,0x00,0xff
    bindTexture(game->killtexid);
    uint8_t r=0xff,g=0xff,b=0x00,a=0xff; // FULL
    GX_Begin(GX_QUADS,GX_VTXFMT0,MAX(game->murderc,game->grid->murderlimit)*4);
      for (;skulli<game->murderc;skulli++) {
        if (game->grid&&(skulli>=game->grid->murderlimit)) { r=0xff; g=0x00; b=0x00; a=0xff; } // FATAL
        GX_Position2s16(x       ,y       ); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(0,0);
        GX_Position2s16(x       ,y+SKULLH); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(0,1);
        GX_Position2s16(x+SKULLW,y+SKULLH); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(1,1);
        GX_Position2s16(x+SKULLW,y       ); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(1,0);
        x+=XSTRIDE;
      }
      r=0xff; g=0xff; b=0xff; a=0x55; // EMPTY
      for (;skulli<game->grid->murderlimit;skulli++) {
        GX_Position2s16(x       ,y       ); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(0,0);
        GX_Position2s16(x       ,y+SKULLH); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(0,1);
        GX_Position2s16(x+SKULLW,y+SKULLH); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(1,1);
        GX_Position2s16(x+SKULLW,y       ); GX_Color4u8(r,g,b,a); GX_TexCoord2u8(1,0);
        x+=XSTRIDE;
      }
    GX_End();
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
  uint8_t op8=(game->editor_orn_op*0xff);
  
  /* red square around selected cell */
  tevRasterOnly();
  int x=game->editor_selx*game->grid->colw-game->camerax;
  int y=game->editor_sely*game->grid->rowh-game->cameray;
  GX_Begin(GX_LINESTRIP,GX_VTXFMT0,5);
    GX_Position2s16(x                 ,y                 ); GX_Color4u8(0xff,0x00,0x00,op8);
    GX_Position2s16(x                 ,y+game->grid->rowh); GX_Color4u8(0xff,0x00,0x00,op8);
    GX_Position2s16(x+game->grid->colw,y+game->grid->rowh); GX_Color4u8(0xff,0x00,0x00,op8);
    GX_Position2s16(x+game->grid->colw,y                 ); GX_Color4u8(0xff,0x00,0x00,op8);
    GX_Position2s16(x                 ,y                 ); GX_Color4u8(0xff,0x00,0x00,op8);
  GX_End();
  tevBlend();
  
  if (game->editor_palette) {
    int left=(vscreenw>>1)-(game->grid->colw*8);
    int y=(vscreenh>>1)-(game->grid->rowh*8);
    int cellid=0;
    
    /* gray out background, even grayer where palette is going */
    tevRasterOnly();
    GX_Begin(GX_QUADS,GX_VTXFMT0,8);
      GX_Position2s16(0       ,0       ); GX_Color4u8(0x80,0x80,0x80,0x80);
      GX_Position2s16(0       ,vscreenh); GX_Color4u8(0x80,0x80,0x80,0x80);
      GX_Position2s16(vscreenw,vscreenh); GX_Color4u8(0x80,0x80,0x80,0x80);
      GX_Position2s16(vscreenw,0       ); GX_Color4u8(0x80,0x80,0x80,0x80);
      GX_Position2s16(left                    ,y                    ); GX_Color4u8(0x55,0x55,0x55,0x80);
      GX_Position2s16(left                    ,y+game->grid->rowh*16); GX_Color4u8(0x55,0x55,0x55,0x80);
      GX_Position2s16(left+game->grid->colw*16,y+game->grid->rowh*16); GX_Color4u8(0x55,0x55,0x55,0x80);
      GX_Position2s16(left+game->grid->colw*16,y                    ); GX_Color4u8(0x55,0x55,0x55,0x80);
    GX_End();
    tevTextureOnly();
      
    for (int row=0;row<16;row++) {
      int x=left;
      for (int col=0;col<16;col++) {
        if (game->grid->tidv[cellid]>=0) {
          bindTexture(game->grid->tidv[cellid]);
          GX_Begin(GX_QUADS,GX_VTXFMT0,4);
            GX_Position2s16(x                 ,y                 ); GX_TexCoord2u8(0,0);
            GX_Position2s16(x                 ,y+game->grid->rowh); GX_TexCoord2u8(0,1);
            GX_Position2s16(x+game->grid->colw,y+game->grid->rowh); GX_TexCoord2u8(1,1);
            GX_Position2s16(x+game->grid->colw,y                 ); GX_TexCoord2u8(1,0);
          GX_End();
        }
        if (cellid==game->grid->cellv[game->editor_sely*game->grid->colc+game->editor_selx]) {
          tevRasterOnly();
          GX_Begin(GX_LINESTRIP,GX_VTXFMT0,5);
            GX_Position2s16(x                 ,y                 ); GX_Color4u8(0x00,0xff,0xff,op8);
            GX_Position2s16(x                 ,y+game->grid->rowh); GX_Color4u8(0x00,0xff,0xff,op8);
            GX_Position2s16(x+game->grid->colw,y+game->grid->rowh); GX_Color4u8(0x00,0xff,0xff,op8);
            GX_Position2s16(x+game->grid->colw,y                 ); GX_Color4u8(0x00,0xff,0xff,op8);
            GX_Position2s16(x                 ,y                 ); GX_Color4u8(0x00,0xff,0xff,op8);
          GX_End();
          tevTextureOnly();
        }
        x+=game->grid->colw;
        cellid++;
      }
      y+=game->grid->rowh;
    }
    
    tevBlend();
  }
  
}

/******************************************************************************
 * render, Menu
 *****************************************************************************/

void VideoManager::drawMenu(Menu *menu) {

  /* background */
  if (menu->bgtexid>=0) {
    bindTexture(menu->bgtexid);
    tevTextureSegment();
    GX_Begin(GX_QUADS,GX_VTXFMT0,36);
      #define x0 menu->x
      #define x1 menu->x+menu->edgew
      #define x2 menu->x+menu->w-menu->edgew
      #define x3 menu->x+menu->w
      #define y0 menu->y
      #define y1 menu->y+menu->edgeh
      #define y2 menu->y+menu->h-menu->edgeh
      #define y3 menu->y+menu->h
      #define NOCLR GX_Color4u8(0xff,0xff,0xff,0xff);
      GX_Position2s16(x0,y0); NOCLR GX_TexCoord2f32(0.0f,0.0f); // top-left...
      GX_Position2s16(x0,y1); NOCLR GX_TexCoord2f32(0.0f,0.5f);
      GX_Position2s16(x1,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x1,y0); NOCLR GX_TexCoord2f32(0.5f,0.0f);
      GX_Position2s16(x2,y0); NOCLR GX_TexCoord2f32(0.5f,0.0f); // top-right...
      GX_Position2s16(x2,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x3,y1); NOCLR GX_TexCoord2f32(1.0f,0.5f);
      GX_Position2s16(x3,y0); NOCLR GX_TexCoord2f32(1.0f,0.0f);
      GX_Position2s16(x0,y2); NOCLR GX_TexCoord2f32(0.0f,0.5f); // bottom-left...
      GX_Position2s16(x0,y3); NOCLR GX_TexCoord2f32(0.0f,1.0f); 
      GX_Position2s16(x1,y3); NOCLR GX_TexCoord2f32(0.5f,1.0f);
      GX_Position2s16(x1,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x2,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f); // bottom-right...
      GX_Position2s16(x2,y3); NOCLR GX_TexCoord2f32(0.5f,1.0f);
      GX_Position2s16(x3,y3); NOCLR GX_TexCoord2f32(1.0f,1.0f);
      GX_Position2s16(x3,y2); NOCLR GX_TexCoord2f32(1.0f,0.5f);
      GX_Position2s16(x1,y0); NOCLR GX_TexCoord2f32(0.5f,0.0f); // top...
      GX_Position2s16(x1,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x2,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x2,y0); NOCLR GX_TexCoord2f32(0.5f,0.0f);
      GX_Position2s16(x1,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f); // bottom...
      GX_Position2s16(x1,y3); NOCLR GX_TexCoord2f32(0.5f,1.0f);
      GX_Position2s16(x2,y3); NOCLR GX_TexCoord2f32(0.5f,1.0f); 
      GX_Position2s16(x2,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x0,y1); NOCLR GX_TexCoord2f32(0.0f,0.5f); // left...
      GX_Position2s16(x0,y2); NOCLR GX_TexCoord2f32(0.0f,0.5f); 
      GX_Position2s16(x1,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x1,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x2,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f); // right...
      GX_Position2s16(x2,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x3,y2); NOCLR GX_TexCoord2f32(1.0f,0.5f);
      GX_Position2s16(x3,y1); NOCLR GX_TexCoord2f32(1.0f,0.5f);
      GX_Position2s16(x1,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f); // middle...
      GX_Position2s16(x1,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x2,y2); NOCLR GX_TexCoord2f32(0.5f,0.5f);
      GX_Position2s16(x2,y1); NOCLR GX_TexCoord2f32(0.5f,0.5f);
    GX_End();
    tevBlend();
    #undef x0
    #undef x1
    #undef x2
    #undef x3
    #undef y0
    #undef y1
    #undef y2
    #undef y3
    #undef NOCLR
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
        GX_SetViewport(b_left,b_top,b_width-scrollbarw,b_height,0,1);
        GX_SetScissor(b_left,b_top,b_width-scrollbarw,b_height);
        Mtx44 prjmtx;
        guOrtho(prjmtx,b_top,b_top+b_height,b_left,b_left+b_height,0,1);
        GX_LoadProjectionMtx(prjmtx,GX_ORTHOGRAPHIC);
        drawMenuItems(menu,firstitem,itemc,b_left,b_top-(menu->itemscroll%menu->itemh),b_width-scrollbarw);
        viewportFull();
        guOrtho(prjmtx,0,vscreenh,0,vscreenw,0,1);
        GX_LoadProjectionMtx(prjmtx,GX_ORTHOGRAPHIC);
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
      bindTexture(menu->indicatortexid);
      int ileft=x;
      int iright=x+menu->indicatorw;
      int itop=y+(menu->itemh>>1)-(menu->indicatorh>>1);
      int ibottom=itop+menu->indicatorh;
      GX_Begin(GX_QUADS,GX_VTXFMT0,4);
        GX_Position2s16(ileft ,itop   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(0,0);
        GX_Position2s16(ileft ,ibottom); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(0,1);
        GX_Position2s16(iright,ibottom); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(1,1);
        GX_Position2s16(iright,itop   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(1,0);
      GX_End();
    }
    x+=menu->indicatorw;
    w-=menu->indicatorw;
    MenuItem *item=menu->itemv[itemp+mi];
    if (item->icontexid>=0) { // draw icon
      bindTexture(item->icontexid);
      int ileft=x+menu->itemiconpad;
      int iright=ileft+menu->itemiconw;
      int itop=y+(menu->itemh>>1)-(menu->itemiconh>>1);
      int ibottom=itop+menu->itemiconh;
      GX_Begin(GX_QUADS,GX_VTXFMT0,4);
        GX_Position2s16(ileft ,itop   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(0,0);
        GX_Position2s16(ileft ,ibottom); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(0,1);
        GX_Position2s16(iright,ibottom); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(1,1);
        GX_Position2s16(iright,itop   ); GX_Color4u8(0xff,0xff,0xff,0xff); GX_TexCoord2u8(1,0);
      GX_End();
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
  if (isTexture(texid)) { bindTexture(texid); }
  else { fake=true; tevRasterOnly(); }
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
    GX_Begin(GX_TRIANGLES,GX_VTXFMT0,6);
      GX_Position2s16(x+(w>>1),y          ); GX_Color4u8(0x00,0xff,0xff,0xff);
      GX_Position2s16(x       ,upbtnbottom); GX_Color4u8(0x00,0xff,0xff,0xff);
      GX_Position2s16(x+w     ,upbtnbottom); GX_Color4u8(0x00,0xff,0xff,0xff);
      GX_Position2s16(x       ,downbtntop ); GX_Color4u8(0x00,0xff,0xff,0xff);
      GX_Position2s16(x+(w>>1),y+h        ); GX_Color4u8(0x00,0xff,0xff,0xff);
      GX_Position2s16(x+w     ,downbtntop ); GX_Color4u8(0x00,0xff,0xff,0xff);
    GX_End();
    GX_Begin(GX_QUADS,GX_VTXFMT0,4);
      GX_Position2s16(x  ,thumbtop   ); GX_Color4u8(0xff,0xff,0x00,0xff);
      GX_Position2s16(x  ,thumbbottom); GX_Color4u8(0xff,0xff,0x00,0xff);
      GX_Position2s16(x+w,thumbbottom); GX_Color4u8(0xff,0xff,0x00,0xff);
      GX_Position2s16(x+w,thumbtop   ); GX_Color4u8(0xff,0xff,0x00,0xff);
    GX_End();
    tevBlend();
  } else {
    tevTextureSegment();
    GX_Begin(GX_QUADS,GX_VTXFMT0,12);
      #define NOCLR GX_Color4u8(0xff,0xff,0xff,0xff);
      GX_Position2s16(x  ,y          ); NOCLR GX_TexCoord2f32(0.0f,0.00f);
      GX_Position2s16(x  ,upbtnbottom); NOCLR GX_TexCoord2f32(0.0f,0.33f);
      GX_Position2s16(x+w,upbtnbottom); NOCLR GX_TexCoord2f32(1.0f,0.33f);
      GX_Position2s16(x+w,y          ); NOCLR GX_TexCoord2f32(1.0f,0.00f);
      GX_Position2s16(x  ,thumbtop   ); NOCLR GX_TexCoord2f32(0.0f,0.34f);
      GX_Position2s16(x  ,thumbbottom); NOCLR GX_TexCoord2f32(0.0f,0.66f);
      GX_Position2s16(x+w,thumbbottom); NOCLR GX_TexCoord2f32(1.0f,0.66f);
      GX_Position2s16(x+w,thumbtop   ); NOCLR GX_TexCoord2f32(1.0f,0.34f);
      GX_Position2s16(x  ,downbtntop ); NOCLR GX_TexCoord2f32(0.0f,0.67f);
      GX_Position2s16(x  ,y+h        ); NOCLR GX_TexCoord2f32(0.0f,1.00f);
      GX_Position2s16(x+w,y+h        ); NOCLR GX_TexCoord2f32(1.0f,1.00f);
      GX_Position2s16(x+w,downbtntop ); NOCLR GX_TexCoord2f32(1.0f,0.67f);
      #undef NOCLR
    GX_End();
    tevBlend();
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
  int vtxc=0; for (uint8_t *si=(uint8_t*)s;*si;si++) if ((((*si)>0x20)&&((*si)<0x7f))&&((*si)!='\n')) vtxc+=4;
  if (!vtxc) return;
  bindTexture(texid);
  tevTextureSegment();
  uint8_t r=fgclr>>24,g=fgclr>>16,b=fgclr>>8,a=fgclr;
  GX_Begin(GX_QUADS,GX_VTXFMT0,vtxc);
    while (uint8_t inch=*s++) {
      if (inch=='\n') { x=left; y+=chh; continue; }
      if (inch==0x01) { r=g=a=0x0ff; b=0x00; continue; }
      if (inch==0x02) { r=fgclr>>24; g=fgclr>>16; b=fgclr>>8; a=fgclr; continue; }
      if ((inch<0x21)||(inch>0x7e)) { x+=chw; continue; }
      float col=(inch&0xf)*FR_1_16;
      float row=((inch-0x20)>>4)*FR_1_6;
      GX_Position2s16(x    ,y    ); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(col        ,row       );
      GX_Position2s16(x    ,y+chh); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(col        ,row+FR_1_6);
      GX_Position2s16(x+chw,y+chh); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(col+FR_1_16,row+FR_1_6);
      GX_Position2s16(x+chw,y    ); GX_Color4u8(r,g,b,a); GX_TexCoord2f32(col+FR_1_16,row       );
      x+=chw;
    }
  GX_End();
  tevBlend();
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
 * TEV and vertex format
 *****************************************************************************/

void VideoManager::tevTextureOnly() {
  GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_TEXC);
  GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_SetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_TEXA);
  GX_SetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_ClearVtxDesc();
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XY,GX_S16,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_U8,0);
  GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
  GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLORNULL);
}

void VideoManager::tevRasterOnly() {
  GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_ZERO,GX_CC_ZERO,GX_CC_RASC);
  GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_SetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_ZERO,GX_CA_ZERO,GX_CA_RASA);
  GX_SetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_ClearVtxDesc();
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XY,GX_S16,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
  GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
  GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
}

void VideoManager::tevBlend() {
  GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_TEXC,GX_CC_RASC,GX_CC_ZERO);
  GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_SetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_TEXA,GX_CA_RASA,GX_CA_ZERO);
  GX_SetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_ClearVtxDesc();
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XY,GX_S16,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_U8,0);
  GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
  GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
}

void VideoManager::tevSprite() {
  GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_TEXC,GX_CC_RASC,GX_CC_RASA,GX_CC_ZERO);
  GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_SetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_TEXA,GX_CA_A0,GX_CA_ZERO);
  GX_SetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_ClearVtxDesc();
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XY,GX_F32,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_U8,0);
  GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
  GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
}

void VideoManager::tevTextureSegment() {
  GX_SetTevColorIn(GX_TEVSTAGE0,GX_CC_ZERO,GX_CC_TEXC,GX_CC_RASC,GX_CC_ZERO);
  GX_SetTevColorOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_SetTevAlphaIn(GX_TEVSTAGE0,GX_CA_ZERO,GX_CA_TEXA,GX_CA_RASA,GX_CA_ZERO);
  GX_SetTevAlphaOp(GX_TEVSTAGE0,GX_TEV_ADD,GX_TB_ZERO,GX_CS_SCALE_1,GX_TRUE,GX_TEVPREV);
  GX_ClearVtxDesc();
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XY,GX_S16,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
  GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_TEX0,GX_TEX_ST,GX_F32,0);
  GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
  GX_SetVtxDesc(GX_VA_TEX0,GX_DIRECT);
  GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORD0,GX_TEXMAP0,GX_COLOR0A0);
}

/******************************************************************************
 * texture list
 *****************************************************************************/
 
bool VideoManager::findTextureByName(const char *resname,int *texid) const {
  uint32_t resnamehash=sitter_strhash(resname);
  int available=-1,zref=-1;
  for (int i=0;i<texc;i++) {
    if (!texv[i].valid) {
      if (available<0) available=i;
      continue;
    }
    if ((texv[i].refc==0)&&(zref<0)) zref=i;
    if (texv[i].resnamehash!=resnamehash) continue;
    if (!texv[i].resname) continue;
    if (strcmp(texv[i].resname,resname)) continue;
    *texid=i;
    return true;
  }
  if (available>=0) { 
    *texid=available;
    return false;
  }
  if (zref>=0) {
    *texid=zref;
    return false;
  }
  *texid=texc;
  return false;
}

void VideoManager::insertTextureReference(int texid,const char *resname) {
  if (texid>=texc) {
    int ntexc=texc+TEXV_INCREMENT;
    if (!(texv=(TextureReference*)realloc(texv,sizeof(TextureReference)*ntexc))) sitter_throw(AllocationError,"");
    memset(texv+texc,0,sizeof(TextureReference)*TEXV_INCREMENT);
    texc=ntexc;
  }
  if (!(texv[texid].resname=strdup(resname))) sitter_throw(AllocationError,"");
  texv[texid].resnamehash=sitter_strhash(resname);
  texv[texid].valid=true;
  texv[texid].pixels=NULL;
  texv[texid].pixelslen=0;
  texv[texid].fmt=-1;
  texv[texid].w=0;
  texv[texid].h=0;
  texv[texid].refc=0;
  memset(&(texv[texid].gxobj),0,sizeof(GXTexObj));
}

void VideoManager::unloadTexture(int texid) {
  if (texid<0) return;
  if (texid>=texc) return;
  if (!texv[texid].refc) return;
  texv[texid].refc--;
}

/******************************************************************************
 * texture loader
 *****************************************************************************/
 
int VideoManager::loadTexture(const char *resname,bool safe,int flags) {

  /* is it cached? */
  int texid;
  if (findTextureByName(resname,&texid)) {
    texv[texid].refc++;
    return texid;
  }
  
  /* load file */
  int fmt,w,h; void *pixels;
  try {
    if (!game->res->loadImage(resname,&fmt,&w,&h,&pixels)) sitter_throw(FileNotFoundError,"image %s",resname);
  } catch (FileNotFoundError) {
    if (safe) {
      return -1;
    }
    throw;
  }
  insertTextureReference(texid,resname);
  texv[texid].refc=1;
  texv[texid].w=w;
  texv[texid].h=h;
  
  /* convert to GX format */
  int gxfmt,nlen;
  switch (fmt) {
    case SITTER_TEXFMT_RGBA8888: {
        nlen=((w&~3)*(h&~3))<<2;
        if (nlen>texv[texid].pixelslen) {
          if (texv[texid].pixels) free(texv[texid].pixels);
          if (!(texv[texid].pixels=memalign(32,nlen))) sitter_throw(AllocationError,"");
          texv[texid].pixelslen=nlen;
        }
        sitter_gxinterlace_rgba8888_rgba8888(texv[texid].pixels,pixels,w,h);
        free(pixels);
        gxfmt=GX_TF_RGBA8;
      } break;
    case SITTER_TEXFMT_RGB888: {
        nlen=((w&~3)*(h&~3))<<1;
        if (nlen>texv[texid].pixelslen) {
          if (texv[texid].pixels) free(texv[texid].pixels);
          if (!(texv[texid].pixels=memalign(32,nlen))) sitter_throw(AllocationError,"");
          texv[texid].pixelslen=nlen;
        }
        sitter_gxinterlace_rgb888_rgb565(texv[texid].pixels,pixels,w,h);
        free(pixels);
        gxfmt=GX_TF_RGB565;
      } break;
    case SITTER_TEXFMT_N1: {
        nlen=((w&~7)*(h&~7))>>1;
        if (nlen>texv[texid].pixelslen) {
          if (texv[texid].pixels) free(texv[texid].pixels);
          if (!(texv[texid].pixels=memalign(32,nlen))) sitter_throw(AllocationError,"");
          texv[texid].pixelslen=nlen;
        }
        sitter_gxinterlace_n1_n4(texv[texid].pixels,pixels,w,h);
        free(pixels);
        gxfmt=GX_TF_I4;
      } break;
    case SITTER_TEXFMT_GXRGBA8888: {
        nlen=((w&~3)*(h&~3))<<2;
        if (nlen>texv[texid].pixelslen) {
          if (texv[texid].pixels) free(texv[texid].pixels);
          texv[texid].pixels=pixels;
          texv[texid].pixelslen=nlen;
        } else {
          memcpy(texv[texid].pixels,pixels,nlen);
          free(pixels);
        }
        gxfmt=GX_TF_RGBA8;
      } break;
    case SITTER_TEXFMT_GXRGB565: {
        nlen=((w&~3)*(h&~3))<<1;
        if (nlen>texv[texid].pixelslen) {
          if (texv[texid].pixels) free(texv[texid].pixels);
          texv[texid].pixels=pixels;
          texv[texid].pixelslen=nlen;
        } else {
          memcpy(texv[texid].pixels,pixels,nlen);
          free(pixels);
        }
        gxfmt=GX_TF_RGB565;
      } break;
    case SITTER_TEXFMT_GXN4: {
        nlen=((w&~7)*(h&~7))>>1;
        if (nlen>texv[texid].pixelslen) {
          if (texv[texid].pixels) free(texv[texid].pixels);
          texv[texid].pixels=pixels;
          texv[texid].pixelslen=nlen;
        } else {
          memcpy(texv[texid].pixels,pixels,nlen);
          free(pixels);
        }
        gxfmt=GX_TF_I4;
      } break;
    default: sitter_throw(FormatError,"%s: texture format %d",resname,fmt);
  }
  texv[texid].fmt=gxfmt;
  
  /* push to GPU */
  DCFlushRange(texv[texid].pixels,nlen);
  GX_InitTexObj(&(texv[texid].gxobj),texv[texid].pixels,w,h,gxfmt,GX_CLAMP,GX_CLAMP,0);
  switch (game->cfg->getOption_int("tex-filter")) {
    case 1: {
        GX_InitTexObjLOD(&(texv[texid].gxobj),GX_NEAR,GX_NEAR,-1000.0f,1000.0f,0.0f,0,0,0);
      } break;
    case 2: {
        GX_InitTexObjLOD(&(texv[texid].gxobj),GX_LINEAR,GX_LINEAR,-1000.0f,1000.0f,0.0f,0,0,0);
      } break;
    default:
    case 0: {
        GX_InitTexObjLOD(&(texv[texid].gxobj),(flags&SITTER_TEX_FILTER)?GX_LINEAR:GX_NEAR,
          (flags&SITTER_TEX_FILTER)?GX_LINEAR:GX_NEAR,-1000.0f,1000.0f,0.0f,0,0,0);
      }
  }
  
  return texid;
}

#endif
