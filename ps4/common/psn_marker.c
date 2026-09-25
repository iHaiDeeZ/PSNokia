// write_marker() for the PS4: the render log the phoneME port writes its
// diagnostics to (see midp/src/core/kni_util/include/renderlog.h). Goes
// through psn_log, so it lands in /data/psnokia/renderlog.txt and on UDP.
//
// Callers pass fragments that end in '\n' when a line is complete; psn_log
// wants whole lines, so they are buffered here.

#include <string.h>

#include "psn_log.h"

static char marker_line[512];
static int  marker_length;

void write_marker(const char* text, int len) {
  static int opened;
  if (!opened) {
    psn_log_open("renderlog.txt");
    opened = 1;
  }
  for (int i = 0; i < len; i++) {
    char c = text[i];
    if (c == '\n' || marker_length == (int)sizeof(marker_line) - 1) {
      marker_line[marker_length] = 0;
      psn_log("%s", marker_line);
      marker_length = 0;
      if (c == '\n') {
        continue;
      }
    }
    marker_line[marker_length++] = c;
  }
}

// --- Crash handler ---------------------------------------------------------
//
// Logs fatal signals (bad memory access, illegal instruction, the int3 of a
// failed debug assertion) to the render log with the faulting instruction
// and a frame-pointer backtrace, then lets the crash proceed. Map addresses
// back with: llvm-symbolizer --obj=<elf> <address - 0x400000 - 1>
// (the eboot is loaded at 0x400000).

#include <signal.h>
#include <stdint.h>
#include <stdio.h>

// The PS4 kernel is FreeBSD: its ucontext_t is a 16-byte sigset_t followed
// by FreeBSD's amd64 mcontext_t, a flat array of 8-byte registers. The
// OpenOrbis headers describe musl's Linux layout instead, so read by offset.
#define UC_REG(uc, index) (((const uint64_t*)((const char*)(uc) + 16))[index])
#define FBSD_MC_RBP 9
#define FBSD_MC_RIP 20
#define FBSD_MC_RSP 23

static void crash_handler(int sig, siginfo_t* info, void* context) {
  char line[160];
  uint64_t rip = UC_REG(context, FBSD_MC_RIP);
  uint64_t rsp = UC_REG(context, FBSD_MC_RSP);
  uint64_t rbp = UC_REG(context, FBSD_MC_RBP);
  int n = snprintf(line, sizeof(line),
                   "CRASH signal %d, fault address %p, rip %p, rsp %p, rbp %p\n",
                   sig, info ? info->si_addr : NULL, (void*)rip, (void*)rsp,
                   (void*)rbp);
  write_marker(line, n);
  // Follow saved frame pointers while they stay within 1MB above rsp
  uint64_t* frame = (uint64_t*)rbp;
  for (int i = 0; i < 16; i++) {
    uint64_t f = (uint64_t)frame;
    if (f < rsp || f > rsp + 0x100000 || (f & 7) != 0) {
      break;
    }
    n = snprintf(line, sizeof(line), "  called from %p\n", (void*)frame[1]);
    write_marker(line, n);
    frame = (uint64_t*)frame[0];
  }
  // Let the fault happen again with the default action (system crash report)
  signal(sig, SIG_DFL);
}

void psn_install_crash_handler(void) {
  static const int signals[] = { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP };
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  // OpenOrbis signal.h: its sa_sigaction macro names the wrong union member
  action.__sa_handler.__sa_sigaction = crash_handler;
  action.sa_flags = SA_SIGINFO;
  for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
    sigaction(signals[i], &action, NULL);
  }
}
