#ifndef SITTER_HAKEYBOARD_H
#define SITTER_HAKEYBOARD_H

/* "HA" == "Handicapped-Accessible".
 * Yes, it's a joke.
 */
 
#include <stdint.h>
#include "sitter_Rect.h"
 
class Game;
class InputEvent;

/* Each key can map to a few different characters depending on the state of modifiers.
 */
#define SITTER_HAKEYBOARD_DEPTH    3
#define SITTER_HAKEYBOARD_PLAIN    0 // no modifiers
#define SITTER_HAKEYBOARD_SHIFT    1 // shift
#define SITTER_HAKEYBOARD_SYMBOL   2 // special symbols

/* special key symbols, which are not possible unicode */
#define HAKEY_SHIFT         0x40000001 // shift the next keystroke
#define HAKEY_CAPS          0x40000002 // permanent shift
#define HAKEY_SYMBOL        0x40000003 // use "symbols", third code page
#define HAKEY_PLAIN         0x40000004 // return to plain (lowercase) code page
#define HAKEY_PSHIFT        0x40000005 // when in caps-lock or symbol, go to normal code page for one keystroke
 
class HAKeyboard {
public:

  typedef struct {
    Rect r; // position on screen
    uint32_t val[SITTER_HAKEYBOARD_DEPTH]; // unicode values, for now only 0x20..0x7e (ASCII) are printable
    int next_left;  // index of key to jump to when left is pressed, so we don't have to figure it out every time
    int next_right; // " right
    int next_up;    // " up
    int next_down;  // " down
    uint32_t bgcolor,fgcolor,linecolor;
  } HAKey;
  HAKey *keyv; int keyc,keya;
  int fonttexid;
  
  Rect bounds;
  int shiftstate; // SITTER_HAKEYBOARD_{PLAIN,SHIFT,SYMBOL}
  int stickyshiftstate; // -1 == don't change
  int selection;

  Game *game;
  
  HAKeyboard(Game *game);
  ~HAKeyboard();
  
  void update();
  
  /* Return true if we handle this event; it will go no further.
   * There's no querying of the HAKeyboard's state. Our output goes in the form
   * of UNICODE events to InputManager.
   */
  bool checkEvent(InputEvent &evt);
  
  void highlightCharacter(uint32_t ch);
  void moveSelection(int dx,int dy,bool squeak=true);
  void acceptCharacter();
  void deleteCharacter();
  
  void setup_QWERTY();
  
};

#endif
