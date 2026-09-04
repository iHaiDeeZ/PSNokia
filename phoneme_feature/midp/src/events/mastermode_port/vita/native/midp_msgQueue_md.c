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
#include <string.h>
#include <psp2/io/fcntl.h>

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
  int fd = sceIoOpen("ux0:data/renderlog.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, buf, len);
    sceIoClose(fd);
  }
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

void JoystickCheck(SDL_Event *event, MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent)
{ int btn = event->jbutton.button;
  int isPress = (event->jbutton.state == SDL_PRESSED);
  int Key = KEYMAP_KEY_INVALID;

  if (btn == 4 || btn == 5) { /* L1 or R1 = Shift modifier, no key of its own */
    shiftHeld = isPress;
  } else if (btn == 9 && isPress) { /* R2 - DEBUG_TRACE1 (temporary), direct call */
    log_input_event("pss_before", 0, 0, 0);
    pss();
    log_input_event("pss_after", 0, 0, 0);
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
     { HatCheck(event, pNewSignal, pNewMidpEvent);
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
   * diagnostics; log anything else unrecognized. */
  if (event->type != SDL_MOUSEMOTION && event->type != SDL_JOYAXISMOTION &&
      event->type != SDL_WINDOWEVENT && event->type != SDL_TEXTINPUT) {
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
void checkForSystemSignal(MidpReentryData* pNewSignal, MidpEvent* pNewMidpEvent, jlong timeout) 
{ SDL_Event event;
  jlong currentTime = JVM_JavaMilliSeconds(), stopTime;
  mq_write_marker("QMARKER1: checkForSystemSignal ENTER\n");
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

