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

#include <kni.h>
#include <midp_logging.h>
#include <lfjport_export.h>
#include <gxj_putpixel.h>
#include <gxj_screen_buffer.h>

#include "SDL.h"
#include "midp_constants_data.h"
#include <stdio.h>
#include <string.h>
#include <psp2/io/fcntl.h>
unsigned int _newlib_heap_size_user = 8*1024*1024; /* override vitasdk 128MiB default that fails to allocate */

static void st_write_marker(const char* text) {
  /* Only called from lfjport_ui_init now (twice, at startup) — cheap,
   * safe to leave as real file I/O for diagnostics. */
  int fd = sceIoOpen("ux0:data/renderlog.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, text, strlen(text));
    sceIoClose(fd);
  }
}

#define SDL_FULLWIDTH	FULLWIDTH
#define SDL_FULLHEIGHT	FULLHEIGHT
void InitGP2XKeys(void);

/*
 * Vita port stubs: the media (MMAPI) and push subsystems are not built
 * for this port (SUBSYSTEM_MMAPI_MODULES / SUBSYSTEM_PUSH_MODULES are
 * empty), but lfjport_st_export.c and the reference socketProtocol.c
 * unconditionally reference these symbols. Provide minimal, functionally
 * correct stand-ins here so linking succeeds:
 *   - InitAudioSubsystem/FinalizeAudioSubsystem: no-op (no audio subsystem
 *     to initialize/tear down yet).
 *   - pushcacheddatasize: always report 0 bytes cached (there is no push
 *     layer caching data), which is exactly correct when push is disabled.
 */
int  InitAudioSubsystem(void) { return 0; }
void FinalizeAudioSubsystem(void) { }
int  pushcacheddatasize(int fd) { (void)fd; return 0; }

SDL_Surface     *Native_SDL_Screen, *Native_SDL_HScreen, *Native_SDL_VScreen;
SDL_Window      *Native_SDL_Window;
static jboolean  Native_SDL_ScreenOrientation;
static jboolean  Native_SDL_Fullscreen;
static int       OriginalOrientation, OriginalWidth, OriginalHeight;

/**
 *
 * Generic Screen Buffer required by putpixel library.
 *
 */
gxj_screen_buffer gxj_system_screen_buffer;

/**
 * @file
 * Additional porting API for Java Widgets based port of abstract
 * command manager.
 */

/**
 * Initializes the lfjport_ui_ native resources.
 *
 * @return <tt>0</tt> upon successful initialization, or
 *         <tt>other value</tt> otherwise
 */
int lfjport_ui_init() 
{ if (SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_VIDEO) != 0) 
     return(-1);
  printf("TRACE: lfjport_ui_init ENTER (SDL_Init OK)\n"); fflush(stdout);
  st_write_marker("RMARKER1: lfjport_ui_init ENTER (SDL_Init OK)\n");
  SDL_ShowCursor(SDL_DISABLE);
  InitAudioSubsystem();
  if (SDL_NumJoysticks() > 0) SDL_JoystickOpen(0);
  OriginalOrientation = 0;
  if (getenv("J2ME_GP2X_REVERSE") != NULL) OriginalOrientation = 1;
  OriginalWidth = OriginalOrientation ? SDL_FULLHEIGHT : SDL_FULLWIDTH;
  OriginalHeight = OriginalOrientation ? SDL_FULLWIDTH : SDL_FULLHEIGHT;
  InitGP2XKeys();
  Native_SDL_Window = SDL_CreateWindow("MIDP", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SDL_FULLWIDTH, SDL_FULLHEIGHT, 0);
  if (Native_SDL_Window == NULL) return(-2);
  Native_SDL_Screen = SDL_GetWindowSurface(Native_SDL_Window);
  if (Native_SDL_Screen == NULL) return(-2);
  Native_SDL_HScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, OriginalWidth, OriginalHeight, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000);
  if (Native_SDL_HScreen == NULL) return(-3);
  Native_SDL_VScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, OriginalHeight, OriginalWidth, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000);
  if (Native_SDL_VScreen == NULL) return(-4);
  SDL_LockSurface(Native_SDL_HScreen);
  SDL_LockSurface(Native_SDL_VScreen);
  gxj_system_screen_buffer.width = OriginalWidth;
  gxj_system_screen_buffer.height = OriginalHeight;
  gxj_system_screen_buffer.alphaData = NULL;
  gxj_system_screen_buffer.pixelData = Native_SDL_HScreen->pixels;
  Native_SDL_ScreenOrientation = KNI_FALSE;
  Native_SDL_Fullscreen = KNI_FALSE;
  atexit(SDL_Quit);
  printf("TRACE: lfjport_ui_init SUCCESS\n"); fflush(stdout);
  st_write_marker("RMARKER2: lfjport_ui_init SUCCESS\n");
  return 0;
}

/**
 * Finalize the lfjport_ui_ native resources.
 */
void lfjport_ui_finalize() 
{ FinalizeAudioSubsystem();
  SDL_Quit();
}

/**
 * Bridge function to request a repaint 
 * of the area specified.
 *
 * @param x1 top-left x coordinate of the area to refresh
 * @param y1 top-left y coordinate of the area to refresh
 * @param x2 bottom-right x coordinate of the area to refresh
 * @param y2 bottom-right y coordinate of the area to refresh
 */

void VideoCopyRotate(unsigned short *Buffer, unsigned short *Video)
{ int x, y, bufinc, rowinc;
  Buffer = &Buffer[(SDL_FULLWIDTH-1)*SDL_FULLHEIGHT];
  bufinc = SDL_FULLHEIGHT;
  rowinc = ((SDL_FULLWIDTH-1)*SDL_FULLHEIGHT)+1;
  for(y=0; y<SDL_FULLHEIGHT; y++, Buffer+=rowinc+bufinc)
     for(x=0; x<SDL_FULLWIDTH; x++, Video++, Buffer-=bufinc)
          *Video = *Buffer;
}

static void fps_report(void) {
  static unsigned int frames = 0;
  static unsigned int totalFrames = 0;
  static Uint32 lastTick = 0;
  static int reportedFirst = 0;
  Uint32 now;
  frames++;
  totalFrames++;
  now = SDL_GetTicks();
  if (lastTick == 0) lastTick = now;
  if (!reportedFirst) {
    /* Log the very first frame immediately so short test runs (closed
     * before a full second elapses) still leave evidence in the log. */
    char buf[48];
    int len = sprintf(buf, "FIRST_FRAME at tick=%u\n", (unsigned int)now);
    int fd = sceIoOpen("ux0:data/renderlog.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd >= 0) {
      sceIoWrite(fd, buf, len);
      sceIoClose(fd);
    }
    reportedFirst = 1;
  }
  if (now - lastTick >= 1000) {
    char buf[48];
    int len = sprintf(buf, "FPS: %u (total=%u)\n", frames, totalFrames);
    int fd = sceIoOpen("ux0:data/renderlog.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
    if (fd >= 0) {
      sceIoWrite(fd, buf, len);
      sceIoClose(fd);
    }
    frames = 0;
    lastTick = now;
  }
}

void lfjport_refresh(int x1, int y1, int x2, int y2)
{ unsigned short *Video, *Buffer;
  fps_report();
  SDL_UnlockSurface(Native_SDL_HScreen);
  SDL_UnlockSurface(Native_SDL_VScreen);
  if (Native_SDL_ScreenOrientation) 
     { if (OriginalOrientation == 1) SDL_BlitSurface(Native_SDL_VScreen, NULL, Native_SDL_Screen, NULL);
       else { SDL_LockSurface(Native_SDL_Screen);
              Video = (unsigned short *)Native_SDL_Screen->pixels;
              Buffer = (unsigned short *)Native_SDL_VScreen->pixels;
              VideoCopyRotate(Buffer, Video);
              SDL_UnlockSurface(Native_SDL_Screen);
            }
     }
  else { if (OriginalOrientation == 0) SDL_BlitSurface(Native_SDL_HScreen, NULL, Native_SDL_Screen, NULL);
         else { SDL_LockSurface(Native_SDL_Screen);
                Video = (unsigned short *)Native_SDL_Screen->pixels;
                Buffer = (unsigned short *)Native_SDL_HScreen->pixels;
                VideoCopyRotate(Buffer, Video);
                SDL_UnlockSurface(Native_SDL_Screen);
              }
       }
  SDL_UpdateWindowSurface(Native_SDL_Window);
  SDL_LockSurface(Native_SDL_HScreen);
  SDL_LockSurface(Native_SDL_VScreen);
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
}

/**
 * Porting API function to update scroll bar.
 *
 * @param scrollPosition current scroll position
 * @param scrollProportion maximum scroll position
 * @return status of this call
 */
int lfjport_set_vertical_scroll(int scrollPosition, int scrollProportion)
{ (void)scrollPosition;
  (void)scrollProportion;
  return 0;
}


/**
 * Turn on or off the full screen mode
 *
 * @param mode true for full screen mode
 *             false for normal
 */
void lfjport_set_fullscreen_mode(jboolean mode) 
{ Native_SDL_Fullscreen = mode;
}

/**
 * Resets native resources when foreground is gained by a new display.
 */
void lfjport_gained_foreground() 
{ // SDL_Quit();
}

/**
 * Change screen orientation flag
 */
jboolean lfjport_reverse_orientation() 
{ Native_SDL_ScreenOrientation = !Native_SDL_ScreenOrientation;
  gxj_system_screen_buffer.pixelData = Native_SDL_ScreenOrientation ? Native_SDL_VScreen->pixels : Native_SDL_HScreen->pixels;
  gxj_system_screen_buffer.width = Native_SDL_ScreenOrientation ? OriginalHeight : OriginalWidth;
  gxj_system_screen_buffer.height = Native_SDL_ScreenOrientation ? OriginalWidth : OriginalHeight;
  return Native_SDL_ScreenOrientation;
}

/**
 * Change screen orientation flag
 */
jboolean lfjport_get_reverse_orientation() 
{ return Native_SDL_ScreenOrientation;        
}

/**
 * Return screen width
 */
int lfjport_get_screen_width() 
{ return Native_SDL_ScreenOrientation ? OriginalHeight : OriginalWidth;
}

/**
 * Return screen height
 */
int lfjport_get_screen_height() 
{ return Native_SDL_ScreenOrientation ? OriginalWidth : OriginalHeight;
}
