#ifndef SITTER_GRID_H
#define SITTER_GRID_H

/* Goals should not be placed in such a way that you can stand on both goal and non-goal solids.
 * (it's undefined whether you will count as "on goal" in that case).
 * So, the edge of a goal terminates either in cliff or wall; does not proceed into platform.
 * Only one of GOAL,GOAL1,GOAL2,GOAL3,GOAL4 may be set; if more than one it behaves as if only
 * GOAL was set.
 */

#include <stdint.h>
#include "sitter_Rect.h"

class Game;

/* cell properties */
#define SITTER_GRIDCELL_SOLID       0x00000001 // can't walk through, in any direction
#define SITTER_GRIDCELL_GOAL        0x00000002 // stand here to win (should be SOLID too)
#define SITTER_GRIDCELL_KILLTOP     0x00000004 // crossing down thru top edge kills (won't work if also SOLID)
#define SITTER_GRIDCELL_KILLBOTTOM  0x00000008 // " up thru bottom
#define SITTER_GRIDCELL_KILLLEFT    0x00000010 // " right thru left
#define SITTER_GRIDCELL_KILLRIGHT   0x00000020 // " left thru right
#define SITTER_GRIDCELL_ALLKILLS    0x0000003c // KILLTOP|KILLBOTTOM|KILLLEFT|KILLRIGHT
#define SITTER_GRIDCELL_GOAL1       0x00000040 // GOAL, for player 1 in competitive modes
#define SITTER_GRIDCELL_GOAL2       0x00000080 // " 2
#define SITTER_GRIDCELL_GOAL3       0x00000100 // " 3
#define SITTER_GRIDCELL_GOAL4       0x00000200 // " 4
#define SITTER_GRIDCELL_ALLGOALS    0x000003c2 // GOAL|GOAL1|GOAL2|GOAL3|GOAL4
#define SITTER_GRIDCELL_SEMISOLID   0x00000400 // stops downward movement from above, all else passes

/* Grid::play_mode */
#define SITTER_GRIDMODE_COOP     1 // quarries must reach any GOAL (normal, cooperative mode)
#define SITTER_GRIDMODE_MOST     2 // player with the most quarries in his goal wins
#define SITTER_GRIDMODE_SURVIVAL 3 // last player alive wins

class Grid {
public:

  typedef struct {
    char *sprresname;
    int plrid; // (playercount<<4)|playerid
    int col,row;
  } SpriteSpawn;
  SpriteSpawn *spawnv; int spawnc,spawna;

  Game *game;
  
  uint8_t *cellv;
  int colc,rowc;
  int colw,rowh;
  int tidv[256];
  uint32_t cellpropv[256];
  
  int bgtexid; // preferred background
  uint32_t bgcolor; // if bgtexid invalid
  char *bgname; // for editing
  
  char *songname;
  
  int play_mode; // SITTER_GRIDMODE_*
  int murderlimit; // kill more than so many quarries and can not win (ie so many is allowed; 0 is sensible)
  int timelimit; // frames (~1/60 s) 0=no limit
  
  int scrollx,scrolly;

  Grid(Game *game,int colc=0,int rowc=0,int colw=32,int rowh=32);
  ~Grid();
  
  void flush();
  
  Rect getBounds() const { return Rect(colc*colw,rowc*rowh); }
  
  uint8_t getCell(int x,int y) const {
    if ((x<0)||(y<0)||(x>=colc)||(y>=rowc)) return 0;
    return cellv[y*colc+x];
  }
  uint32_t getCellProperties(int x,int y) const {
    if ((x<0)||(y<0)||(x>=colc)||(y>=rowc)) return 0;
    return cellpropv[cellv[y*colc+x]];
  }
  int getCellTexture(int x,int y) const {
    if ((x<0)||(y<0)||(x>=colc)||(y>=rowc)) return 0;
    return tidv[cellv[y*colc+x]];
  }
  
  int addSprite(const char *name,int col,int row,int plrid=0);
  void makeSprites(int playerc);
  void removeSpawn(int index);
  
  /* Copy existing cells and delete any spawn points outside the new range.
   * I will call game->grp_all->killAllSprites and makeSprites(0) if anything is removed.
   */
  void safeResize(int colc,int rowc);
  
  /* Shift contents, moving spillover to the opposite side.
   * Move spawn points as well.
   * This is presumed to happen only during editing, and is not very efficient.
   */
  void rotate(int dx,int dy);
  
  /* <bg> may be a loadable texture name, or a hexadecimal number: RRGGBB or RRGGBBAA.
   * (and if none of those, throw whatever video->loadTexture throws without 'safe').
   * If it fails, bgname still gets set and bgtexid still gets unloaded.
   */
  void setBackground(const char *bg);
  
  /* Attempt to load segmented tiles, with names "<texname>-<row><col>.png" where <col> and <row> are
   * hex digits. (our snarkification script outputs these).
   * The top-left tile is written into <cellnw>, and proceed from there.
   * cell properties for all modified cells are reset to 0.
   */
  void loadMultiTexture(const char *texname,uint8_t cellnw);
  
};

#endif
