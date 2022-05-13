/* sitter_evdev.h
 * Talks direct to evdev but presents an interface that mimics SDL.
 * This is all kinds of dirty.
 * I'm doing it because InputManager is complicated, and tightly coupled to SDL.
 *
 * We are going to read stdin too, since we can assume that if it exists it's the system keyboard.
 * Won't use that for game controls of course, but menus and quit.
 *
 * SDL v1 doesn't detect new joystick connections, so we won't either.
 * That should simplify the system a bit.
 *
 * This code really sucks and i ought to be ashamed of it and i am.
 */
 
#ifndef SITTER_EVDEV_H
#define SITTER_EVDEV_H

class InputManager;

#define SDL_INIT_JOYSTICK 0

#define SDLK_LAST 0xff
#define SDL_JOYAXISMOTION 1
#define SDL_JOYBUTTONDOWN 2
#define SDL_JOYBUTTONUP 3
#define SDL_JOYHATMOTION 4
#define SDL_JOYBALLMOTION 5
#define SDL_KEYDOWN 6
#define SDL_KEYUP 7
#define SDL_MOUSEBUTTONDOWN 8
#define SDL_MOUSEBUTTONUP 9
#define SDL_MOUSEMOTION 10
#define SDL_QUIT 11

#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3
#define SDL_BUTTON_WHEELUP 4
#define SDL_BUTTON_WHEELDOWN 5

// SDLK, just what gets called out by name, which is a lot.
// Leave 15 spaces for F keys and leave G0 open.
#define SDLK_LSHIFT 0xe0
#define SDLK_RSHIFT 0xe1
#define SDLK_LALT 0xe2
#define SDLK_RALT 0xe3
#define SDLK_LCTRL 0xe4
#define SDLK_RCTRL 0xe5
#define SDLK_LSUPER 0xe6
#define SDLK_RSUPER 0xe7
#define SDLK_LMETA 0xe8
#define SDLK_RMETA 0xe9
#define SDLK_F1 0x80
#define SDLK_KP0 0x90
#define SDLK_KP_PERIOD 0xa0
#define SDLK_KP_DIVIDE 0xa1
#define SDLK_KP_MULTIPLY 0xa2
#define SDLK_KP_MINUS 0xa3
#define SDLK_KP_PLUS 0xa4
#define SDLK_KP_EQUALS 0xa5
#define SDLK_UP 0xa6
#define SDLK_DOWN 0xa7
#define SDLK_LEFT 0xa8
#define SDLK_RIGHT 0xa9
#define SDLK_BACKSPACE 0xaa
#define SDLK_TAB 0xab
#define SDLK_CLEAR 0xac
#define SDLK_RETURN 0x0a
#define SDLK_PAUSE 0xad
#define SDLK_ESCAPE 0x1b
#define SDLK_SPACE 0x20
#define SDLK_QUOTE 0x22
#define SDLK_MINUS 0x2d
#define SDLK_PERIOD 0x2e
#define SDLK_SLASH 0x2f
#define SDLK_SEMICOLON 0x3b
#define SDLK_EQUALS 0x3d
#define SDLK_LEFTBRACKET 0xad
#define SDLK_RIGHTBRACKET 0xae
#define SDLK_BACKSLASH 0x5c
#define SDLK_BACKQUOTE 0x60
#define SDLK_DELETE 0x7f
#define SDLK_KP_ENTER 0xb0
#define SDLK_HOME 0xb1
#define SDLK_INSERT 0xb2
#define SDLK_PAGEUP 0xb3
#define SDLK_PAGEDOWN 0xb4
#define SDLK_END 0xb5
#define SDLK_NUMLOCK 0xb6
#define SDLK_CAPSLOCK 0xb7
#define SDLK_SCROLLOCK 0xb8
#define SDLK_MODE 0xb9
#define SDLK_COMPOSE 0xba
#define SDLK_HELP 0xbb
#define SDLK_PRINT 0xbc
#define SDLK_SYSREQ 0xbd
#define SDLK_BREAK 0xbe
#define SDLK_MENU 0xbf
#define SDLK_POWER 0xc0
#define SDLK_EURO 0xc1
#define SDLK_UNDO 0xc2

typedef int SDLKey;

typedef union {
  int type;
  struct {
    int type;
    int which;
    int button;
    int state;
  } jbutton;
  struct {
    int type;
    int which;
    int axis;
    int value;
  } jaxis;
  struct {
    int type;
    int which;
    int hat;
    int value;
  } jhat;
  struct {
    int type;
    int which;
    int ball;
    int xrel;
    int yrel;
  } jball;
  struct {
    int type;
    struct {
      int sym;
      int unicode;
    } keysym;
    int state;
  } key;
  struct {
    int type;
    int button;
    int state;
  } button;
  struct {
    int type;
    int x;
    int y;
    int xrel;
    int yrel;
  } motion;
} SDL_Event;

typedef struct sitter_evdev_device SDL_Joystick;

void sitter_evdev_set_InputManager(InputManager *mgr);

int SDL_Init(int which);
void SDL_EnableUNICODE(int whatever);
const char *SDL_GetError();
int SDL_PollEvent(SDL_Event *event);
int SDL_NumJoysticks();
SDL_Joystick *SDL_JoystickOpen(int p);
void SDL_JoystickClose(SDL_Joystick *joy);
const char *SDL_JoystickName(int p);
int SDL_JoystickNumAxes(SDL_Joystick *joy);
int SDL_JoystickNumButtons(SDL_Joystick *joy);
const char *SDL_GetKeyName(int keysym);

#endif
