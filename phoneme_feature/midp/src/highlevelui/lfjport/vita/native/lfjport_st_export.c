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
#include <stdlib.h>
#include <string.h>
#include <renderlog.h>
unsigned int _newlib_heap_size_user = 8*1024*1024; /* override vitasdk 128MiB default that fails to allocate */

static void st_write_marker(const char* text) {
  /* Only called from lfjport_ui_init now (twice, at startup) — cheap,
   * safe to leave as real file I/O for diagnostics. */
  RENDERLOG_WRITE(text, (int)strlen(text));
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
#ifdef PS4
/* The PS4 always outputs 1080p. The MIDP framebuffer is converted to the
 * window's pixel format (PS4_Converted) and scaled to fit the TV. */
#define PS4_SCREEN_WIDTH  1920
#define PS4_SCREEN_HEIGHT 1080
static SDL_Surface *PS4_Converted;
static void ps4_display_load(void);
#endif
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
#ifdef PS4
  /* Per-game screen size ("WxH", set by runMidlet_md.c): e.g. 128x128 for
   * early Nokia games, which otherwise draw into one corner */
  { const char *size = getenv("J2ME_SCREEN_SIZE");
    int w, h;
    if (size != NULL && sscanf(size, "%dx%d", &w, &h) == 2 &&
        w >= 96 && h >= 64 && w <= 640 && h <= 640)
       { OriginalWidth = w;
         OriginalHeight = h;
       }
  }
  ps4_display_load();
#endif
  InitGP2XKeys();
#ifdef PS4
  Native_SDL_Window = SDL_CreateWindow("MIDP", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, PS4_SCREEN_WIDTH, PS4_SCREEN_HEIGHT, 0);
#else
  Native_SDL_Window = SDL_CreateWindow("MIDP", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SDL_FULLWIDTH, SDL_FULLHEIGHT, 0);
#endif
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
    RENDERLOG_WRITE(buf, len);
    reportedFirst = 1;
  }
  if (now - lastTick >= 1000) {
    char buf[48];
    int len = sprintf(buf, "FPS: %u (total=%u)\n", frames, totalFrames);
    RENDERLOG_WRITE(buf, len);
    frames = 0;
    lastTick = now;
  }
}

#ifdef PS4
/*
 * How the phone screen is shown on the TV, changed live from the
 * controller (L1 + Options: shape, L1 + Touchpad: filter) and kept per
 * game in $MIDP_HOME/display.txt, e.g. "full smooth". The game always
 * draws at its own resolution; these only change the scaling.
 *   fit   - as large as fits, original aspect ratio
 *   4:3   - stretched to a 4:3 box
 *   full  - stretched to the whole 16:9 screen
 *   pixel - largest whole-number scale (every phone pixel the same size)
 *   sharp - plain pixel scaling; smooth - Scale2x first (rounded edges)
 */
enum { VIEW_FIT, VIEW_4_3, VIEW_FULL, VIEW_PIXEL, VIEW_COUNT };
static const char *const view_names[VIEW_COUNT] = { "fit", "4:3", "full", "pixel" };
static int ps4_view = VIEW_FIT;
static int ps4_smooth;
static SDL_Surface *PS4_Smoothed;

void psn_scale2x(const Uint32 *src, int w, int h, int spitch,
                 Uint32 *dst, int dpitch);

static void ps4_display_path(char *path, int size)
{ const char *home = getenv("MIDP_HOME");
  snprintf(path, size, "%s/display.txt", home != NULL ? home : "/data/psnokia");
}

static void ps4_display_load(void)
{ char path[128], word[16], buf[64];
  int n;
  FILE *f;
  ps4_display_path(path, sizeof(path));
  f = fopen(path, "r");
  if (f == NULL) f = fopen("/data/psnokia/display.txt", "r"); /* all games */
  if (f != NULL)
     { while (fscanf(f, "%15s", word) == 1)
          { int i;
            for (i = 0; i < VIEW_COUNT; i++)
                 if (strcmp(word, view_names[i]) == 0) ps4_view = i;
            if (strcmp(word, "smooth") == 0) ps4_smooth = 1;
            if (strcmp(word, "sharp") == 0) ps4_smooth = 0;
          }
       fclose(f);
     }
  n = snprintf(buf, sizeof(buf), "DISPLAY at start: %s %s\n",
               view_names[ps4_view], ps4_smooth ? "smooth" : "sharp");
  RENDERLOG_WRITE(buf, n);
}

static void ps4_display_save(void)
{ char path[128], buf[64];
  int n;
  FILE *f;
  ps4_display_path(path, sizeof(path));
  f = fopen(path, "w");
  if (f != NULL)
     { fprintf(f, "%s %s\n", view_names[ps4_view], ps4_smooth ? "smooth" : "sharp");
       fclose(f);
     }
  n = snprintf(buf, sizeof(buf), "DISPLAY: %s %s\n", view_names[ps4_view],
               ps4_smooth ? "smooth" : "sharp");
  RENDERLOG_WRITE(buf, n);
}

/* The rectangle of the 1080p window the phone screen is scaled into */
static void ps4_view_rect(int w, int h, SDL_Rect *dst)
{ int sw = Native_SDL_Screen->w, sh = Native_SDL_Screen->h;
  switch (ps4_view)
     { case VIEW_FULL:
            dst->w = sw;
            dst->h = sh;
            break;
       case VIEW_4_3:
            dst->h = sh;
            dst->w = sh * 4 / 3;
            break;
       case VIEW_PIXEL:
          { int scale = sw / w < sh / h ? sw / w : sh / h;
            if (scale < 1) scale = 1;
            dst->w = w * scale;
            dst->h = h * scale;
            break;
          }
       default:
            if (w * sh > h * sw)
               { dst->w = sw;
                 dst->h = h * sw / w;
               }
            else
               { dst->h = sh;
                 dst->w = w * sh / h;
               }
     }
  dst->x = (sw - dst->w) / 2;
  dst->y = (sh - dst->h) / 2;
}

/* Scales the last converted frame (PS4_Converted) into the window */
static void ps4_draw(void)
{ SDL_Surface *image = PS4_Converted;
  SDL_Rect dst;
  if (image == NULL) return;
  if (ps4_smooth && image->format->BytesPerPixel == 4)
     { if (PS4_Smoothed == NULL || PS4_Smoothed->w != image->w * 2 ||
           PS4_Smoothed->h != image->h * 2)
          { if (PS4_Smoothed != NULL) SDL_FreeSurface(PS4_Smoothed);
            PS4_Smoothed = SDL_CreateRGBSurface(0, image->w * 2, image->h * 2, 32,
                 image->format->Rmask, image->format->Gmask,
                 image->format->Bmask, image->format->Amask);
          }
       if (PS4_Smoothed != NULL)
          { psn_scale2x((const Uint32 *)image->pixels, image->w, image->h,
                        image->pitch / 4, (Uint32 *)PS4_Smoothed->pixels,
                        PS4_Smoothed->pitch / 4);
            image = PS4_Smoothed;
          }
     }
  ps4_view_rect(PS4_Converted->w, PS4_Converted->h, &dst);
  /* Clear the borders every frame: the window surface may be double
   * buffered */
  SDL_FillRect(Native_SDL_Screen, NULL, SDL_MapRGB(Native_SDL_Screen->format, 0, 0, 0));
  SDL_BlitScaled(image, NULL, Native_SDL_Screen, &dst);
}

/* Controller shortcuts (midp_msgQueue_md.c). The new setting shows at
 * once, even when the game is not repainting, and is saved. */
static void ps4_display_changed(void)
{ ps4_display_save();
  ps4_draw();
  SDL_UpdateWindowSurface(Native_SDL_Window);
  SDL_SaveBMP(Native_SDL_Screen, "/data/psnokia/display.bmp");
}

void ps4_display_next_view(void)
{ ps4_view = (ps4_view + 1) % VIEW_COUNT;
  ps4_display_changed();
}

void ps4_display_toggle_smooth(void)
{ ps4_smooth = !ps4_smooth;
  ps4_display_changed();
}

/* Converts the current MIDP framebuffer and shows it */
static void ps4_present(SDL_Surface *source)
{ if (PS4_Converted == NULL || PS4_Converted->w != source->w ||
      PS4_Converted->h != source->h)
     { if (PS4_Converted != NULL) SDL_FreeSurface(PS4_Converted);
       PS4_Converted = SDL_ConvertSurface(source, Native_SDL_Screen->format, 0);
       if (PS4_Converted == NULL) return;
     }
  SDL_BlitSurface(source, NULL, PS4_Converted, NULL);
  /* Snapshots of the MIDP screen for diagnosis (fetch over FTP) */
  { static int frames;
    frames++;
    if (frames == 60 || frames == 250 || frames == 600 || frames == 1500)
       { char name[64];
         snprintf(name, sizeof(name), "/data/psnokia/frame%d.bmp", frames);
         SDL_SaveBMP(PS4_Converted, name);
       }
  }
  ps4_draw();
}
#endif

void lfjport_refresh(int x1, int y1, int x2, int y2)
{ unsigned short *Video, *Buffer;
  fps_report();
  SDL_UnlockSurface(Native_SDL_HScreen);
  SDL_UnlockSurface(Native_SDL_VScreen);
#ifdef PS4
  (void)Video;
  (void)Buffer;
  ps4_present(Native_SDL_ScreenOrientation ? Native_SDL_VScreen : Native_SDL_HScreen);
#else
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
#endif
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
