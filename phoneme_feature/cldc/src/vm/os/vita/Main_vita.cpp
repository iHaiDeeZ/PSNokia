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

#include "incls/_precompiled.incl"
#include "incls/_Main_vita.cpp.incl"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>


#include "../utilities/JVM_Malloc.hpp"

extern "C" void write_marker(const char* text, int len) {
  /* Was ux0:data/vmmarker.txt - switched to renderlog.txt, which has
   * proven reliable all session, to rule out anything filename/path
   * specific to vmmarker.txt while chasing why pss()'s tty output
   * (which routes through this function via JVMSPI_PrintRaw) wasn't
   * showing up anywhere. */
  int fd = sceIoOpen("ux0:data/renderlog.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, text, len);
    sceIoClose(fd);
  }
}

void JVMSPI_PrintRaw(const char* s) {
  /* printf("%s", s); fflush(stdout); used to run here before write_marker.
   * This port has no real controlling console for stdout - suspected
   * (not fully confirmed) of silently blocking/misbehaving on this
   * homebrew target, which would explain why every tty-based print
   * (DefaultStream -> JVMSPI_PrintRaw, used by e.g. System.out) produced
   * no observable output, while the direct-to-write_marker MarkerStream
   * bypass used by pss() this session worked fine. Removed. */
  write_marker(s, (int)strlen(s));
}

void JVMSPI_Exit(int code) {
  sceKernelExitProcess(code);
}

int main(int argc, char **argv) {

  {
    static const char m0[] = "MARKER0: main() entered\n";
    write_marker(m0, sizeof(m0)-1);
  }

  // Call this before any other Jvm_ functions.
    char* theHeap;
        int   size = 0x00200000;
    int   res;

    JVM_Initialize();

    theHeap = (char*)malloc(size);
    if (theHeap ==NULL) {
        printf("No memory\n");
        return -1;
    }

    memset(theHeap,'Z',size);

    memset(theHeap,'K',size-4);

    memset(theHeap,'A',4);


    res = JVM_InitializeMemory(theHeap,size-8);

    if (res < 0 ) {
        printf("Can't init the heap\n");
        return -1;
    } else if (res >= 0) {
        printf("Already initialized\n");
    }

  {
    static const char m1[] = "MARKER1: heap initialized\n";
    write_marker(m1, sizeof(m1)-1);
  }

  // Ignore arg[0] -- the name of the program.
  argc --;
  argv ++;

  while (true) {
    int n = JVM_ParseOneArg(argc, argv);
    if (n < 0) {
      printf("Unknown argument: %s\n", argv[0]);
      JVMSPI_DisplayUsage(NULL);
      return -1;
    } else if (n == 0) {
      break;
    }
    argc -= n;
    argv += n;
  }

  {
    static const char m2[] = "MARKER2: args parsed\n";
    write_marker(m2, sizeof(m2)-1);
  }

  if (JVM_GetConfig(JVM_CONFIG_SLAVE_MODE) == KNI_FALSE) {
    // Run the VM in regular mode -- JVM_Start won't return until
    // the VM completes execution.
    {
      static const char m3[] = "MARKER3: before JVM_Start\n";
      write_marker(m3, sizeof(m3)-1);
    }
    {
      int __result = JVM_Start(NULL, NULL, argc, argv);
      static char __buf[80];
      int __len = sprintf(__buf, "MARKER4: after JVM_Start, result=%d\n", __result);
      write_marker(__buf, __len);
      return __result;
    }
  } else {
    // Run the VM in slave mode -- we keep calling JVM_TimeSlice(),
    // which executes bytecodes for a small amount and returns. This
    // mode is necessary for platforms that need to keep the main
    // control loop outside of of the VM.
    //
    // Note that this mode is not necessary on Win32. We do it here
    // just as a demo.

    JVM_Start(NULL, NULL, argc, argv);

    for (;;) {
      jlong timeout = JVM_TimeSlice();
      if (timeout <= -2) {
        break;
      } else {
        int blocked_threads_count;
        JVMSPI_BlockedThreadInfo * blocked_threads;

        blocked_threads = SNI_GetBlockedThreads(&blocked_threads_count);
        JVMSPI_CheckEvents(blocked_threads, blocked_threads_count, timeout);
      }
    }

    return JVM_CleanUp();
  }
}
