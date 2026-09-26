/*
 *   
 *
 * Copyright  1990-2007 Sun Microsystems, Inc. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 only, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details (a copy is
 * included at /legal/license.txt).
 * 
 * You should have received a copy of the GNU General Public License
 * version 2 along with this work; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 or visit www.sun.com if you need additional
 * information or have any questions.
 */

#include <midp_logging.h>
#include <midp_mastermode_port.h>
#include <keymap_input.h>
#include <midp_constants_data.h>

#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <renderlog.h>

static void mq_write_marker(const char* text) {
  /* Diagnostic-only: disabled — checkForSystemSignal is called on every
   * VM idle checkpoint, so file-I/O here throttles the event loop badly. */
  (void)text;
}

/* Diagnostic-only: logs actual key/button/hat events as they're generated.
 * Safe to leave enabled — only fires on real input, not every poll/frame. */
static void log_input_event(const char* source, int rawtype, int chr, int action) {
  char buf[96];
  int len = sprintf(buf, "INPUT src=%s raw=%d CHR=%d ACTION=%d\n", source, rawtype, chr, action);
  RENDERLOG_WRITE(buf, len);
}

/*
 * pspkvm-style mapping (PSP has no numeric keypad, so its J2ME port
 * repurposes the face buttons + D-pad as digits, with L1/R1 acting as
 * a Shift modifier for the second layer, and reserves the analog stick
 * for actual UP/DOWN/LEFT/RIGHT game-action navigation):
 *
 *   Cross=0  Square=1  Triangle=3           (unshifted)
 *   Shift+Circle=5  Shift+Square=7  Shift+Triangle=9
 *   Shift+Select=*  Shift+Start=#  Shift+Cross=CLEAR
 *   Circle=SELECT(fire)   Select(system)=SOFT1   Start(system)=SOFT2
 *   D-pad Up/Down/Left/Right = 2/8/4/6 (numpad-arranged digits)
 *   Left stick = real UP/DOWN/LEFT/RIGHT game actions
 *   L2/R2/L3/R3 (Vita-only, no PSP equivalent) = GAME_A/B/C/D
 *
 * SDL's Vita joystick backend (src/joystick/vita/SDL_sysjoystick.c)
 * exposes exactly 12 face/shoulder buttons in this fixed order:
 *   0 Triangle, 1 Circle, 2 Cross, 3 Square, 4 L1, 5 R1,
 *   6 Select, 7 Start, 8 L2, 9 R2, 10 L3, 11 R3
 */
static int shiftHeld = 0; /* L1 or R1 currently held */

void InitGP2XKeys()
{ /* No per-index table to initialize any more (see mapping comment
   * above) — kept as a no-op so lfjport_ui_init's existing call site
   * doesn't need to change. */
}

void KeyboardCheck(SDL_Event *event, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ int Key = KEYMAP_KEY_INVALID;
  switch(event->key.keysym.sym)
        { case SDLK_LGUI: Key = KEYMAP_KEY_SOFT1; break;
          case SDLK_MENU:   Key = KEYMAP_KEY_SOFT2; break;
          case SDLK_UP:     Key = KEYMAP_KEY_UP; break;
          case SDLK_DOWN:   Key = KEYMAP_KEY_DOWN; break;
          case SDLK_LEFT:   Key = KEYMAP_KEY_LEFT; break;
          case SDLK_RIGHT:  Key = KEYMAP_KEY_RIGHT; break;
          case SDLK_RETURN: Key = KEYMAP_KEY_SELECT; break;
          case SDLK_CLEAR:  Key = KEYMAP_KEY_CLEAR; break;
          case SDLK_KP_0:    Key = KEYMAP_KEY_0; break;
          case SDLK_KP_1:    Key = KEYMAP_KEY_7; break; // KEYPADNUM reverse KEYPHONE
          case SDLK_KP_2:    Key = KEYMAP_KEY_8; break; // KEYPADNUM reverse KEYPHONE
          case SDLK_KP_3:    Key = KEYMAP_KEY_9; break; // KEYPADNUM reverse KEYPHONE
          case SDLK_KP_4:    Key = KEYMAP_KEY_4; break;
          case SDLK_KP_5:    Key = KEYMAP_KEY_5; break;
          case SDLK_KP_6:    Key = KEYMAP_KEY_6; break;
          case SDLK_KP_7:    Key = KEYMAP_KEY_1; break; // KEYPADNUM reverse KEYPHONE
          case SDLK_KP_8:    Key = KEYMAP_KEY_2; break; // KEYPADNUM reverse KEYPHONE
          case SDLK_KP_9:    Key = KEYMAP_KEY_3; break; // KEYPADNUM reverse KEYPHONE
          case SDLK_KP_MULTIPLY: Key = KEYMAP_KEY_ASTERISK; break;
          case SDLK_KP_PLUS:     Key = KEYMAP_KEY_POUND; break;
          case SDLK_HOME:   Key = KEYMAP_KEY_SEND; break;
          case SDLK_END:    Key = KEYMAP_KEY_END; break;
          case SDLK_PAGEUP: Key = -9; break;
          default: Key = KEYMAP_KEY_INVALID;
        }
  pNewSignal->waitingFor = UI_SIGNAL;
  pNewMidpEvent->type = MIDP_KEY_EVENT;
  pNewMidpEvent->CHR = Key;
  pNewMidpEvent->ACTION = (event->key.state == SDL_PRESSED) ? KEYMAP_STATE_PRESSED : KEYMAP_STATE_RELEASED;
  log_input_event("key", event->key.keysym.sym, pNewMidpEvent->CHR, pNewMidpEvent->ACTION);
}

/*
 * SDL's Vita joystick backend reports the D-pad as a hat switch
 * (SDL_JOYHATMOTION), not as buttons — CheckEvent previously had no
 * handler for this event type at all, so D-pad presses never reached
 * Java. pspkvm maps D-pad directions to digits 2/8/4/6 (numpad-style,
 * matching their position around 5) rather than UP/DOWN/LEFT/RIGHT —
 * real navigation comes from the analog stick instead (see AxisCheck).
 */
static int prevHatValue = SDL_HAT_CENTERED;

void HatCheck(SDL_Event *event, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ int newHat = event->jhat.value;
  int changed = newHat ^ prevHatValue;
  int Key = KEYMAP_KEY_INVALID;
  int pressed = 0;
  if (changed & SDL_HAT_UP)         { Key = KEYMAP_KEY_2;    pressed = (newHat & SDL_HAT_UP)    != 0; }
  else if (changed & SDL_HAT_DOWN)  { Key = KEYMAP_KEY_8;  pressed = (newHat & SDL_HAT_DOWN)  != 0; }
  else if (changed & SDL_HAT_LEFT)  { Key = KEYMAP_KEY_4;  pressed = (newHat & SDL_HAT_LEFT)  != 0; }
  else if (changed & SDL_HAT_RIGHT) { Key = KEYMAP_KEY_6; pressed = (newHat & SDL_HAT_RIGHT) != 0; }
  prevHatValue = newHat;
  if (Key != KEYMAP_KEY_INVALID) {
    pNewSignal->waitingFor = UI_SIGNAL;
    pNewMidpEvent->type = MIDP_KEY_EVENT;
    pNewMidpEvent->CHR = Key;
    pNewMidpEvent->ACTION = pressed ? KEYMAP_STATE_PRESSED : KEYMAP_STATE_RELEASED;
    log_input_event("hat", newHat, pNewMidpEvent->CHR, pNewMidpEvent->ACTION);
  }
}

/*
 * Left analog stick -> real UP/DOWN/LEFT/RIGHT game-action navigation
 * (pspkvm's own convention: the D-pad is reserved for digit entry, so
 * the stick is what games actually use to move). Axis 0 = X, axis 1 =
 * Y, matching SDL's standard joystick axis order.
 */
#define AXIS_DEADZONE 16000
static int axisXState = 0; /* -1 left, 0 center, 1 right */
static int axisYState = 0; /* -1 up, 0 center, 1 down */

void AxisCheck(SDL_Event *event, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ int axis = event->jaxis.axis;
  int value = event->jaxis.value;
  int Key = KEYMAP_KEY_INVALID;
  int pressed = 0;
  int newState = (value > AXIS_DEADZONE) ? 1 : (value < -AXIS_DEADZONE) ? -1 : 0;
  if (axis == 0 && newState != axisXState) {
    if (axisXState == 1)       { Key = KEYMAP_KEY_RIGHT; pressed = 0; }
    else if (axisXState == -1) { Key = KEYMAP_KEY_LEFT;  pressed = 0; }
    else if (newState == 1)    { Key = KEYMAP_KEY_RIGHT; pressed = 1; }
    else if (newState == -1)   { Key = KEYMAP_KEY_LEFT;  pressed = 1; }
    axisXState = newState;
  } else if (axis == 1 && newState != axisYState) {
    if (axisYState == 1)       { Key = KEYMAP_KEY_DOWN; pressed = 0; }
    else if (axisYState == -1) { Key = KEYMAP_KEY_UP;   pressed = 0; }
    else if (newState == 1)    { Key = KEYMAP_KEY_DOWN; pressed = 1; }
    else if (newState == -1)   { Key = KEYMAP_KEY_UP;   pressed = 1; }
    axisYState = newState;
  }
  /* Most axis motion events are jitter that doesn't cross the deadzone
   * (Key stays INVALID) - only dispatch a real MIDP event, and only
   * write the diagnostic log, when something actually changed. Setting
   * waitingFor is what makes midp_check_events() dispatch a Java-side
   * event at all; leaving it untouched is a safe, cheap no-op. */
  if (Key != KEYMAP_KEY_INVALID) {
    pNewSignal->waitingFor = UI_SIGNAL;
    pNewMidpEvent->type = MIDP_KEY_EVENT;
    pNewMidpEvent->CHR = Key;
    pNewMidpEvent->ACTION = pressed ? KEYMAP_STATE_PRESSED : KEYMAP_STATE_RELEASED;
    log_input_event("axis", (axis << 20) | (value & 0xFFFFF), pNewMidpEvent->CHR, pNewMidpEvent->ACTION);
  }
}

/*
 * pss() is CLDC's own "print all Java thread stacks" debug hook
 * (Debug.cpp). Calling it directly from here, instead of going through
 * System.getProperty("__debug.only.pss") -> DisplayEventListener.java
 * -> getProperty0() native, sidesteps a stale-Java-class-cache issue
 * in this build's MIDP javac step that silently keeps recompiling to
 * an unchanged output, and doesn't depend on the game's own thread
 * ever properly dispatching the event either.
 */
extern void pss(void);

#ifdef PS4
/*
 * The OpenOrbis SDL2 PS4 joystick numbers the DualShock 4 buttons
 *   0 Cross, 1 Circle, 2 Square, 3 Triangle, 4 L1, 5 R1, 9 Options,
 *   11 L3, 12 R3, 13-16 D-pad Up/Down/Left/Right, 17 Touchpad, 18 L2, 19 R2
 * (see OpenOrbis samples/SDL2/SDL2/Game.h). Translate to the Vita indices
 * the mapping below is written for: Options acts as Start and the
 * touchpad click as Select (the two softkeys).
 */
static int ps4_to_vita_button(int btn)
{ switch (btn) {
    case 0:  return 2;   /* Cross    */
    case 1:  return 1;   /* Circle   */
    case 2:  return 3;   /* Square   */
    case 3:  return 0;   /* Triangle */
    case 4:  return 4;   /* L1       */
    case 5:  return 5;   /* R1       */
    case 9:  return 7;   /* Options -> Start  */
    case 17: return 6;   /* Touchpad -> Select */
    case 18: return 8;   /* L2       */
    case 19: return 9;   /* R2       */
    case 11: return 10;  /* L3       */
    case 12: return 11;  /* R3       */
    case 13: return 12;  /* D-pad Up    */
    case 14: return 13;  /* D-pad Down  */
    case 15: return 14;  /* D-pad Left  */
    case 16: return 15;  /* D-pad Right */
    default: return -1;
  }
}

/* Display settings shortcuts and the menu (lfjport_st_export.c) */
void ps4_display_next_view(void);
void ps4_display_toggle_smooth(void);
void ps4_open_menu(void);

static int ps4_touch_click(int isPress, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent);

/*
 * The buttons the player can assign in the menu, by Vita index, and the
 * phone key each sends by default. L1/R1 (the second set, below) and the
 * touchpad (the touchscreen) are fixed. The choice is saved per game in
 * $MIDP_HOME/buttons.txt as soon as it changes.
 */
typedef struct { int vita; const char *name; int defaultKey; } ps4_button;
static const ps4_button ps4_buttons[] = {
  { 2,  "Cross",       KEYMAP_KEY_SELECT },
  { 1,  "Circle",      KEYMAP_KEY_SOFT2 },   /* right soft key: Back / Exit */
  { 3,  "Square",      KEYMAP_KEY_SOFT1 },   /* left soft key */
  { 0,  "Triangle",    KEYMAP_KEY_0 },
  { 7,  "Options",     KEYMAP_KEY_SOFT1 },
  { 8,  "L2",          KEYMAP_KEY_1 },
  { 9,  "R2",          KEYMAP_KEY_3 },
  { 10, "L3",          KEYMAP_KEY_7 },
  { 11, "R3",          KEYMAP_KEY_9 },
  { 12, "D-pad up",    KEYMAP_KEY_2 },
  { 13, "D-pad down",  KEYMAP_KEY_8 },
  { 14, "D-pad left",  KEYMAP_KEY_4 },
  { 15, "D-pad right", KEYMAP_KEY_6 },
};
#define PS4_BUTTON_COUNT ((int)(sizeof(ps4_buttons) / sizeof(ps4_buttons[0])))
static int ps4_keys[PS4_BUTTON_COUNT];
static int ps4_keys_loaded;

static void ps4_buttons_path(char *path, int size)
{ const char *home = getenv("MIDP_HOME");
  snprintf(path, size, "%s/buttons.txt", home != NULL ? home : "/data/psnokia");
}

void ps4_save_buttons(void)
{ char path[128];
  FILE *f;
  int i;
  ps4_buttons_path(path, sizeof(path));
  f = fopen(path, "w");
  if (f == NULL) return;
  for (i = 0; i < PS4_BUTTON_COUNT; i++)
       fprintf(f, "%d %d\n", i, ps4_keys[i]);
  fclose(f);
}

static void ps4_load_buttons(void)
{ char path[128];
  FILE *f;
  int i, key;
  for (i = 0; i < PS4_BUTTON_COUNT; i++)
       ps4_keys[i] = ps4_buttons[i].defaultKey;
  ps4_keys_loaded = 1;
  ps4_buttons_path(path, sizeof(path));
  f = fopen(path, "r");
  if (f == NULL) return;
  while (fscanf(f, "%d %d", &i, &key) == 2)
       if (i >= 0 && i < PS4_BUTTON_COUNT) ps4_keys[i] = key;
  fclose(f);
}

/* For the menu (lfjport_st_export.c) */
int ps4_button_count(void) { return PS4_BUTTON_COUNT; }
const char *ps4_button_name(int i) { return ps4_buttons[i].name; }

int ps4_button_key(int i)
{ if (!ps4_keys_loaded) ps4_load_buttons();
  return ps4_keys[i];
}

void ps4_set_button_key(int i, int key)
{ if (!ps4_keys_loaded) ps4_load_buttons();
  ps4_keys[i] = key;
  ps4_save_buttons();
}

void ps4_reset_buttons(void)
{ int i;
  for (i = 0; i < PS4_BUTTON_COUNT; i++)
       ps4_keys[i] = ps4_buttons[i].defaultKey;
  ps4_keys_loaded = 1;
  ps4_save_buttons();
}

/* The phone key assigned to a button (Vita index) */
static int ps4_key_for(int vita)
{ int i;
  if (!ps4_keys_loaded) ps4_load_buttons();
  for (i = 0; i < PS4_BUTTON_COUNT; i++)
       if (ps4_buttons[i].vita == vita) return ps4_keys[i];
  return KEYMAP_KEY_INVALID;
}

static int l3Held, r3Held;
#endif

void JoystickCheck(SDL_Event *event, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ int btn = event->jbutton.button;
  int isPress = (event->jbutton.state == SDL_PRESSED);
  int Key = KEYMAP_KEY_INVALID;

#ifdef PS4
  btn = ps4_to_vita_button(btn);
#endif
  if (btn == 4 || btn == 5) { /* L1 or R1 = Shift modifier, no key of its own */
    shiftHeld = isPress;
#ifndef PS4
  } else if (btn == 9 && isPress) { /* R2 - DEBUG_TRACE1 (temporary), direct call */
    log_input_event("pss_before", 0, 0, 0);
    pss();
    log_input_event("pss_after", 0, 0, 0);
#endif
#ifdef PS4
  } else if (shiftHeld && (btn == 7 || btn == 6)) {
    /* L1 + Options / L1 + Touchpad: display shape / filter, not keys for
     * the game (lfjport_st_export.c) */
    if (isPress) {
      if (btn == 7) ps4_display_next_view();
      else ps4_display_toggle_smooth();
    }
  } else if (btn == 6) {
    /* Touchpad click: touch the phone screen under the cursor */
    if (ps4_touch_click(isPress, pNewSignal, pNewMidpEvent)) return;
  } else if ((btn == 10 && isPress && r3Held) || (btn == 11 && isPress && l3Held)) {
    /* L3 + R3: the menu. The game is paused while it is open; afterwards
     * release the key the first of the two buttons pressed. */
    int first = btn == 10 ? 11 : 10;
    l3Held = r3Held = 0;
    shiftHeld = 0;
    ps4_open_menu();
    Key = ps4_key_for(first);
    isPress = 0;
  } else {
    /* The assigned keys (by default Cross confirms and Circle is the right
     * soft key, usually Back/Exit; Square is the left soft key). L1/R1
     * held gives the second set on the face buttons. */
    if (btn == 10) l3Held = isPress;
    if (btn == 11) r3Held = isPress;
    if (shiftHeld && btn >= 0 && btn <= 3) {
      static const int second[4] = { KEYMAP_KEY_CLEAR, KEYMAP_KEY_POUND,
                                     KEYMAP_KEY_5, KEYMAP_KEY_ASTERISK };
      Key = second[btn];    /* Triangle, Circle, Cross, Square */
    } else {
      Key = ps4_key_for(btn);
    }
  }
#else
  } else {
    switch (btn) {
      case 0: Key = shiftHeld ? KEYMAP_KEY_9 : KEYMAP_KEY_3; break;            /* Triangle */
      case 1: Key = shiftHeld ? KEYMAP_KEY_5 : KEYMAP_KEY_SELECT; break;       /* Circle   */
      case 2: Key = shiftHeld ? KEYMAP_KEY_CLEAR : KEYMAP_KEY_0; break;        /* Cross    */
      case 3: Key = shiftHeld ? KEYMAP_KEY_7 : KEYMAP_KEY_1; break;            /* Square   */
      case 6: Key = shiftHeld ? KEYMAP_KEY_ASTERISK : KEYMAP_KEY_SOFT1; break; /* Select   */
      case 7: Key = shiftHeld ? KEYMAP_KEY_POUND : KEYMAP_KEY_SOFT2; break;    /* Start    */
      case 8: Key = KEYMAP_KEY_GAMEA; break;  /* L2 (Vita-only extra) */
      case 9: Key = KEYMAP_KEY_GAMEB; break;  /* R2 - press already handled above (pss()); this only covers release */
      case 10: Key = KEYMAP_KEY_GAMEC; break; /* L3 (Vita-only extra) */
      case 11: Key = KEYMAP_KEY_GAMED; break; /* R3 (Vita-only extra) */
      /* When a real PC gamepad is passed through Vita3K (as opposed to
       * running on real Vita hardware / Vita3K's own virtual input),
       * the D-pad arrives as extra button indices here instead of a
       * hat switch (SDL_JOYHATMOTION, handled in HatCheck, apparently
       * never fires in that case) - confirmed empirically: index 15
       * fired for D-pad Right. Standard SDL controller-DB order is
       * 12=up, 13=down, 14=left, 15=right. Same digit mapping as the
       * hat path, for consistency. */
      case 12: Key = KEYMAP_KEY_2; break; /* D-pad Up    */
      case 13: Key = KEYMAP_KEY_8; break; /* D-pad Down  */
      case 14: Key = KEYMAP_KEY_4; break; /* D-pad Left  */
      case 15: Key = KEYMAP_KEY_6; break; /* D-pad Right */
      default: Key = KEYMAP_KEY_INVALID;
    }
  }
#endif
  pNewSignal->waitingFor = UI_SIGNAL;
  pNewMidpEvent->type = MIDP_KEY_EVENT;
  pNewMidpEvent->CHR = Key;
  pNewMidpEvent->ACTION = isPress ? KEYMAP_STATE_PRESSED : KEYMAP_STATE_RELEASED;
  log_input_event("btn", btn, pNewMidpEvent->CHR, pNewMidpEvent->ACTION);
}

/*
 * Touch was never handled at all before this — SDL_FINGERDOWN/UP/MOTION
 * fell through CheckEvent unrecognized. Any game whose splash/loading
 * screen needs a tap to continue would hang forever, since no pointer
 * event ever reached Java. SDL's touch coordinates are normalized
 * (0.0-1.0), so scale by the real MIDP canvas size (FULLWIDTH/HEIGHT,
 * not the host window size) to get pixel coordinates.
 */
void TouchCheck(SDL_Event *event, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ int x = (int)(event->tfinger.x * FULLWIDTH);
  int y = (int)(event->tfinger.y * FULLHEIGHT);
  int action;
  if (x < 0) x = 0; if (x >= FULLWIDTH) x = FULLWIDTH - 1;
  if (y < 0) y = 0; if (y >= FULLHEIGHT) y = FULLHEIGHT - 1;
  if (event->type == SDL_FINGERDOWN) action = KEYMAP_STATE_PRESSED;
  else if (event->type == SDL_FINGERUP) action = KEYMAP_STATE_RELEASED;
  else action = KEYMAP_STATE_DRAGGED;
  pNewSignal->waitingFor = UI_SIGNAL;
  pNewMidpEvent->type = MIDP_PEN_EVENT;
  pNewMidpEvent->X_POS = x;
  pNewMidpEvent->Y_POS = y;
  pNewMidpEvent->ACTION = action;
  /* FINGERMOTION (drag) can fire continuously/rapidly - only log the
   * low-frequency press/release edges to avoid a repeat of the earlier
   * hot-path file-I/O throttling problem. */
  if (event->type != SDL_FINGERMOTION) {
    log_input_event("touch", event->type, x, y);
  }
}

void CheckEvent(SDL_Event *event, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ if (event->type == SDL_QUIT)
     { SDL_Quit();
       exit(0);
     }
  if ((event->type == SDL_KEYDOWN) || (event->type == SDL_KEYUP))
     { KeyboardCheck(event, pNewSignal, pNewMidpEvent);
       return;
     }
  if ((event->type == SDL_JOYBUTTONDOWN) || (event->type == SDL_JOYBUTTONUP))
     { JoystickCheck(event, pNewSignal, pNewMidpEvent);
       return;
     }
  if (event->type == SDL_JOYHATMOTION)
     {
#ifndef PS4
       /* On the PS4 the D-pad also arrives as buttons 13-16 (JoystickCheck);
        * handling the hat as well would send every press twice */
       HatCheck(event, pNewSignal, pNewMidpEvent);
#endif
       return;
     }
  if (event->type == SDL_JOYAXISMOTION)
     { AxisCheck(event, pNewSignal, pNewMidpEvent);
       return;
     }
  if (event->type == SDL_FINGERDOWN || event->type == SDL_FINGERUP || event->type == SDL_FINGERMOTION)
     { TouchCheck(event, pNewSignal, pNewMidpEvent);
       return;
     }
  /* Skip logging known-noisy, non-actionable event types to avoid
   * flooding the log (and re-throttling the event loop) during
   * diagnostics; log anything else unrecognized.
   * SDL_MULTIGESTURE/DOLLARGESTURE/DOLLARRECORD added after a real-hardware
   * renderlog.txt (session 6, 2026-09-04) showed SDL_MULTIGESTURE
   * (raw=2050) firing continuously - hundreds of times - almost certainly
   * from the Vita's rear touchpad picking up incidental contact from how
   * the device is held. This port doesn't implement multi-touch gestures
   * at all, so these were pure noise, but each one still triggered a real
   * blocking write_marker() file write - on real hardware's actual flash
   * storage (unlike Vita3K's host-backed, effectively-instant file I/O),
   * a flood of these is a very plausible cause of the real-hardware-only
   * "black screen, nothing loading" hangs this project chased for two
   * sessions without a reproducible native-side lead. */
  if (event->type != SDL_MOUSEMOTION && event->type != SDL_JOYAXISMOTION &&
      event->type != SDL_WINDOWEVENT && event->type != SDL_TEXTINPUT &&
      event->type != SDL_MULTIGESTURE && event->type != SDL_DOLLARGESTURE &&
      event->type != SDL_DOLLARRECORD) {
    log_input_event("unhandled", event->type, -999, -999);
  }
}

/*
 * This function is called by the VM periodically. It has to check if
 * system has sent a signal to MIDP and return the result in the
 * structs given.
 *
 * Values for the <timeout> paramater:
 *  >0 = Block until a signal sent to MIDP, or until <timeout> milliseconds
 *       has elapsed.
 *   0 = Check the system for a signal but do not block. Return to the
 *       caller immediately regardless of the if a signal was sent.
 *  -1 = Do not timeout. Block until a signal is sent to MIDP.
 */
#ifdef PS4
/*
 * Phone vibration (Display.vibrate, Nokia's DeviceControl.startVibra) as
 * DS4 rumble. A timer thread turns the motors off when the duration is up;
 * a newer request supersedes it.
 */
#include <pthread.h>
#include <unistd.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>

static int ps4_pad_handle = -1;
static int ps4_vibra_result;  /* last scePadSetVibration result, for the log */
static volatile unsigned int ps4_vibra_generation;

/* The pad SDL opened for the logged-in user; negative if there is none */
static int ps4_pad(void)
{ if (ps4_pad_handle < 0)
     { int32_t user = -1;
       if (sceUserServiceGetInitialUser(&user) == 0)
          ps4_pad_handle = scePadGetHandle(user, 0, 0);
     }
  return ps4_pad_handle;
}

static void ps4_set_motors(int level)
{ OrbisPadVibeParam param;
  if (ps4_pad() < 0) { ps4_vibra_result = ps4_pad_handle; return; }
  param.lgMotor = (uint8_t)level;
  param.smMotor = (uint8_t)level;
  ps4_vibra_result = scePadSetVibration(ps4_pad_handle, &param);
}

struct ps4_vibra_timer { unsigned int generation; int ms; };

static void *ps4_vibra_stop(void *arg)
{ struct ps4_vibra_timer *t = (struct ps4_vibra_timer *)arg;
  usleep((useconds_t)t->ms * 1000);
  if (t->generation == ps4_vibra_generation) ps4_set_motors(0);
  free(t);
  return NULL;
}

/* level 0..255; level or ms <= 0 stops */
void ps4_vibrate(int level, int ms)
{ struct ps4_vibra_timer *t;
  pthread_t thread;
  unsigned int generation = ++ps4_vibra_generation;
  if (level <= 0 || ms <= 0)
     { ps4_set_motors(0);
       return;
     }
  if (level > 255) level = 255;
  ps4_set_motors(level);
  {
    char line[80];
    int n = snprintf(line, sizeof(line), "VIBRATE level %d, %d ms: pad %d, result %d", level, ms,
                     ps4_pad_handle, ps4_vibra_result);
    if (n > (int)sizeof(line) - 2) n = (int)sizeof(line) - 2;
    line[n] = 10;
    RENDERLOG_WRITE(line, n + 1);
  }
  t = (struct ps4_vibra_timer *)malloc(sizeof(*t));
  if (t == NULL) return;
  t->generation = generation;
  t->ms = ms;
  if (pthread_create(&thread, NULL, ps4_vibra_stop, t) == 0)
     pthread_detach(thread);
  else
     free(t);
}

/*
 * The DS4 touchpad as the phone's touchscreen. The whole touchpad maps
 * onto the phone screen: a finger on it moves a cursor, clicking it
 * touches the screen under the cursor, and sliding while it is held
 * down drags.
 */
void ps4_screen_size(int *w, int *h);                 /* lfjport_st_export.c */
void ps4_show_cursor(int x, int y, int state);

static int ps4_touch_res_x, ps4_touch_res_y;
static int ps4_touch_x = -1, ps4_touch_y = -1;  /* phone pixels */
static int ps4_touch_down;
static Uint32 ps4_touch_polled;

/* Reads the finger position; returns 1 if it moved */
static int ps4_touch_read(void)
{ OrbisPadData data;
  int w, h, x, y;
  if (ps4_pad() < 0 || scePadReadState(ps4_pad_handle, &data) != 0) return 0;
  if (data.touch.fingers == 0)
     { ps4_show_cursor(ps4_touch_x, ps4_touch_y, ps4_touch_down ? 2 : 0);
       return 0;
     }
  if (ps4_touch_res_x <= 0)
     { OrbisPadInformation info;
       ps4_touch_res_x = 1920;
       ps4_touch_res_y = 943;
       if (scePadGetControllerInformation(ps4_pad_handle, &info) == 0 &&
           info.touchResolutionX > 0 && info.touchResolutionY > 0)
          { ps4_touch_res_x = info.touchResolutionX;
            ps4_touch_res_y = info.touchResolutionY;
          }
     }
  ps4_screen_size(&w, &h);
  x = data.touch.touch[0].x * w / ps4_touch_res_x;
  y = data.touch.touch[0].y * h / ps4_touch_res_y;
  if (x < 0) x = 0; if (x >= w) x = w - 1;
  if (y < 0) y = 0; if (y >= h) y = h - 1;
  ps4_show_cursor(x, y, ps4_touch_down ? 2 : 1);
  if (x == ps4_touch_x && y == ps4_touch_y) return 0;
  ps4_touch_x = x;
  ps4_touch_y = y;
  return 1;
}

static void ps4_pen_event(int action, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ pNewSignal->waitingFor = UI_SIGNAL;
  pNewMidpEvent->type = MIDP_PEN_EVENT;
  pNewMidpEvent->X_POS = ps4_touch_x;
  pNewMidpEvent->Y_POS = ps4_touch_y;
  pNewMidpEvent->ACTION = action;
  if (action != KEYMAP_STATE_DRAGGED)
     log_input_event("touch", action, ps4_touch_x, ps4_touch_y);
}

/* Touchpad click; returns 1 if it made a pen event */
static int ps4_touch_click(int isPress, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ ps4_touch_read();
  if (isPress && !ps4_touch_down && ps4_touch_x >= 0)
     { ps4_touch_down = 1;
       ps4_show_cursor(ps4_touch_x, ps4_touch_y, 2);
       ps4_pen_event(KEYMAP_STATE_PRESSED, pNewSignal, pNewMidpEvent);
       return 1;
     }
  if (!isPress && ps4_touch_down)
     { ps4_touch_down = 0;
       ps4_show_cursor(ps4_touch_x, ps4_touch_y, 1);
       ps4_pen_event(KEYMAP_STATE_RELEASED, pNewSignal, pNewMidpEvent);
       return 1;
     }
  return 0;
}

/* Called while waiting for events: moves the cursor, and drags while the
 * touchpad is held down. Returns 1 if it made a pen event. */
static int ps4_touch_poll(MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ Uint32 now = SDL_GetTicks();
  if (now - ps4_touch_polled < 16) return 0;
  ps4_touch_polled = now;
  if (!ps4_touch_read() || !ps4_touch_down) return 0;
  ps4_pen_event(KEYMAP_STATE_DRAGGED, pNewSignal, pNewMidpEvent);
  return 1;
}
#endif

void checkForSystemSignal(MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent, jlong timeout) 
{ SDL_Event event;
  jlong currentTime = JVM_JavaMilliSeconds(), stopTime;
  mq_write_marker("QMARKER1: checkForSystemSignal ENTER\n");
#ifdef PS4
  /* The touchpad is not an SDL event source: poll it alongside SDL */
  for (;;)
     { if (ps4_touch_poll(pNewSignal, pNewMidpEvent)) return;
       if (SDL_PollEvent(&event))
          { CheckEvent(&event, pNewSignal, pNewMidpEvent);
            return;
          }
       if (timeout == 0 ||
           (timeout > 0 && JVM_JavaMilliSeconds() - currentTime >= timeout))
          return;
       SDL_Delay(1);
     }
#endif
  if (timeout == -1)
     { mq_write_marker("QMARKER2: calling SDL_WaitEvent (BLOCKING)\n");
       if (SDL_WaitEvent(&event))
          { mq_write_marker("QMARKER3: SDL_WaitEvent got an event\n");
            CheckEvent(&event, pNewSignal, pNewMidpEvent);
          }
       return;
     }
  do { if (SDL_PollEvent(&event))
          { CheckEvent(&event, pNewSignal, pNewMidpEvent);
            return;
          }
       stopTime = JVM_JavaMilliSeconds();
     } while((stopTime - currentTime) < timeout);
}

