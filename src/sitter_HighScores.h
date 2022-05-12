#ifndef SITTER_HIGHSCORES_H
#define SITTER_HIGHSCORES_H

#include <stdint.h>

class Game;

class HighScores {

  typedef struct {
    int play_clock;
    int murderc;
    int64_t realtime;
    char *name;
  } ScoreEntry;

  typedef struct {
    int gridindex,playerc;
    ScoreEntry *entv; // sorted by rank
    int entc,enta;
  } GridListEntry;
  
  typedef struct {
    char *setname;
    GridListEntry *entv; // unsorted
    int entc,enta;
  } SetListEntry;
  
  SetListEntry *setv; int setc,seta; // unsorted
  int scorelimit;
  
  SetListEntry *findSet(const char *setname) const;
  SetListEntry *addSet(const char *setname);
  GridListEntry *findGrid(SetListEntry *set,int gridindex,int playerc) const;
  GridListEntry *addGrid(SetListEntry *set,int gridindex,int playerc);
  int addScore(GridListEntry *grid,int play_clock,int murderc,int64_t realtime,const char *name); // return index

public:

  Game *game;
  
  HighScores(Game *game);
  ~HighScores();
  
  void readStoredScores();
  void writeScores();
  
  /* Return the placement of new score: 1=first place, 2=second place, ...
   * <=0 means it doesn't rank.
   */
  int registerScore(const char *setname,int gridindex,int playerc,int time,int murderc);
  void setScoreName(const char *setname,int gridindex,int playerc,int scoreid,const char *name);
  
  void flush();
  
  /* Always NUL-terminates. Do not call with dstlen<1.
   * If highlight>=0, insert 0x01 (begin highlight) and 0x02 (end highlight) at the given entry.
   */
  void format(char *dst,int dstlen,const char *setname,int gridindex,int playerc,int highlight=-1);
  
};

#endif
