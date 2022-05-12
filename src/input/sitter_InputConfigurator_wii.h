#ifdef SITTER_WII

#ifndef SITTER_INPUTCONFIGURATOR_H
#define SITTER_INPUTCONFIGURATOR_H

/* InputConfigurator for wii is just a wrapper around a menu. It only exists because its
 * SDL counterpart is much more involved.
 */

class Game;
class InputEvent;

class InputConfigurator {
public:

  Game *game;
  
  InputConfigurator(Game *game);
  ~InputConfigurator();
  
  /* If true, I processed the event and you should not look at it.
   */
  bool receiveEvent(InputEvent &evt);
  
  void finalise();
  
};

#endif
#endif
