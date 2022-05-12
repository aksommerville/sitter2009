#ifndef SITTER_GAME_H
#define SITTER_GAME_H

#include <stdint.h>

class Configuration;
class InputManager;
class VideoManager;
class ResourceManager;
class Player;
class Timer;
class Grid;
class SpriteGroup;
class InputEvent;
class AudioManager;
class InputConfigurator;
class HighScores;
class HAKeyboard;

class Game {

  void updateInput();
  Player **plrv; int plrc,plra;

public:

  Configuration *cfg;
  ResourceManager *res;
  VideoManager *video;
  InputManager *input;
  AudioManager *audio;
  Timer *timer;
  InputConfigurator *inputcfg;
  HAKeyboard *hakeyboard;
  HighScores *hisc;
  
  Grid *grid;
  SpriteGroup *grp_vis,*grp_upd,*grp_all;
  SpriteGroup *grp_solid; // sprites participating in sprite-on-sprite collision testing
  SpriteGroup *grp_carry; // sprites that bill can pick up
  SpriteGroup *grp_quarry; // when all dead, you lose; when all on goal, you win
  SpriteGroup *grp_crushable; // crush-o-matic can kill these (even if they're solid)
  SpriteGroup *grp_slicable; // other instant-kills work (eg chainsaw)
  SpriteGroup *grp_alwaysempty; // for assorted purposes, an always-empty, never-NULL group
  
  /* Center of camera's focus. We need to track this so we can limit its speed.
   * dx and dy are only used in editor.
   */
  int camerax,cameray;
  int cameradx,camerady;
  
  bool quit;
  int murderc,murderlimit; // how many children have we killed in this level?, how many are we allowed to?
  int victorydelay; // managed by getVictoryState; don't report win/loss instantly (for user-friendliness)
  char *grid_set; // grid set name
  int grid_index; // grid number, within set, from 1
  int lvlplayerc;
  int play_clock; // counting frames from level start
  int clocktexid;
  int killtexid;
  bool gameover; // more accurately "round over"
  bool rabbit_mode; // make all players jump unstoppably
  bool menu_fast; // speed up choices in integer menu item
  int hsresult; // used briefly during high-score name entry
  bool drawhighscores;
  int hsfonttexid;
  bool drawradar;
  int radartexid;
  char *highscore_name; // previous name entered for high score, may be NULL. will suggest as next name.
  
  /* editor */
  bool editing_map;
  char *editor_set,*editor_grid;
  bool editor_mdown;
  int editor_selx,editor_sely;
  double editor_orn_op; // ornament opacity; everything other than the grid fades in and out for contrast
  double editor_orn_dop;
  bool editor_palette; // viewing/editing palette, not cells
  char *editor_instr;
  uint8_t editor_pent_cell;
  
  int getVictoryState();
  
  /* A 4-player limit is hard-coded at several points, particularly input.
   * Game's Player list can handle any size at all.
   */
  int addPlayer(Player *plr); // return 1-based plrid, checks for already-added, i take ownership
  Player *getPlayer(int plrid) const; // plrid is 1-based, may return NULL
  void removePlayer(int plrid); // plrid is 1-based

  Game(Configuration *cfg);
  ~Game();
  
  void mmain(); // fucking ridiculous. i586-mingw32msvc-g++ refuses to link a MEMBER FUNCTION called "main". Why?
  
  void update();
  void update_editor();
  
  void openMainMenu();
  void openPauseMenu();
  void openCreditsMenu();
  void openConfigMenu();
  void openAllDeadMenu();
  void openAllGoalMenu();
  void openMurderMenu();
  void openPlayersDeadMenu();
  void openCompetitiveWinMenu(int winnerid);
  void openCompetitiveDrawMenu();
  void openEditorMenu();
  void openEditorSetMenu(const char *setname);
  void openQueryDialog(const char *prompt,char **dst,int respid,bool replace=true);
  void openAudioMenu();
  void openVideoMenu();
  void closeVideoMenu();
  
  void resetGame(const char *gridset,int gridindex,int playerc);
  void restartLevel();
  void nextLevel();
  void highScoreCheck(int plrid); // call at end of level, just after opening the dialogue
  void receiveHighScoreName();
  
  void beginEditor(const char *path);
  void editorNew();
  void editorNewSet();
  void editorEvent(InputEvent &evt);
  
  void runDebugTest(); // TEMP
  
};

#endif
