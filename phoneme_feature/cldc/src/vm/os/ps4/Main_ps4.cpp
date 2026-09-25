/*
 * Main_ps4.cpp: PS4 entry point for the standalone CLDC VM.
 *
 * Output goes through psn_log (ps4/common/psn_log.h): the file
 * /data/psnokia/vm.txt on the console, plus UDP to the PC.
 */

#include "incls/_precompiled.incl"
#include "incls/_Main_ps4.cpp.incl"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lowheap.h"
#include "psn_log.h"

// Same size as lowheap_malloc.c uses: whichever runs first creates the arena,
// which also serves every malloc in the program.
#define LOWHEAP_ARENA_SIZE (256 * 1024 * 1024)

// The test program is packaged as /app0/vmtest.jar (see ps4/cldc-test)
static const char* default_args[] = {
  "-classpath", "/app0/vmtest.jar", "VmTest"
};

extern "C" {

// The VM prints in fragments; psn_log wants whole lines.
static char line_buffer[1024];
static int  line_length;

void JVMSPI_PrintRaw(const char* s) {
  for (; *s != 0; s++) {
    if (*s == '\n' || line_length == (int)sizeof(line_buffer) - 1) {
      line_buffer[line_length] = 0;
      psn_log("%s", line_buffer);
      line_length = 0;
      if (*s == '\n') {
        continue;
      }
    }
    line_buffer[line_length++] = *s;
  }
}

void JVMSPI_Exit(int code) {
  JVMSPI_PrintRaw("\n");
  psn_log("VM exit(%d)", code);
  exit(code);
}

}

int main(int argc, char **argv) {
  psn_log_open("vm.txt");
  psn_log("PSNokia CLDC VM (PS4) starting");
  // Where things were loaded, to map addresses in later messages back to
  // the ELF (addresses there are relative to 0).
  int stack_local = 0;
  psn_log("main %p, JVMSPI_PrintRaw %p, line_buffer %p, stack %p, malloc %p",
         (void*)&main, (void*)&JVMSPI_PrintRaw, (void*)line_buffer,
         (void*)&stack_local, (void*)&malloc);

  if (lowheap_init(LOWHEAP_ARENA_SIZE) != 0) {
    psn_log("FATAL: could not map the low heap arena");
    for (;;) sleep(1);
  }
  psn_log("low heap: %p, %u MB", lowheap_base(),
         (unsigned)(lowheap_size() >> 20));

  JVM_Initialize();

  // Launched from the home screen there are no arguments; use the defaults.
  if (argc <= 1) {
    argc = (int)(sizeof(default_args) / sizeof(default_args[0]));
    argv = (char**)default_args;
  } else {
    // Ignore arg[0] -- the name of the program.
    argc--;
    argv++;
  }

  while (true) {
    int n = JVM_ParseOneArg(argc, argv);
    if (n < 0) {
      psn_log("Unknown argument: %s", argv[0]);
      break;
    } else if (n == 0) {
      break;
    }
    argc -= n;
    argv += n;
  }

  int result = JVM_Start(NULL, NULL, argc, argv);
  JVMSPI_PrintRaw("\n");
  psn_log("JVM_Start returned %d; low heap in use: %u KB", result,
         (unsigned)(lowheap_in_use() >> 10));

  // Stay alive so the log can be read; close the app from the PS menu.
  for (;;) {
    sleep(1);
  }
  return result;
}
