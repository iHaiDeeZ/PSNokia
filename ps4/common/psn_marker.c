// write_marker() for the PS4: the render log the phoneME port writes its
// diagnostics to (see midp/src/core/kni_util/include/renderlog.h). Goes
// through psn_log, so it lands in /data/psnokia/renderlog-<title id>.txt
// and on UDP.
//
// Callers pass fragments that end in '\n' when a line is complete; psn_log
// wants whole lines, so they are buffered here.

#include <string.h>

#include "psn_log.h"

static char marker_line[512];
static int  marker_length;

// Each game keeps its own log, renderlog-<title id>.txt, so running another
// game does not replace it. The title ID is in the package's titleid.txt.
static void open_game_log(void) {
  char id[16] = "", name[48] = "renderlog.txt";
  FILE* f = fopen("/app0/titleid.txt", "r");
  if (f) {
    if (fscanf(f, "%15[A-Za-z0-9]", id) == 1) {
      snprintf(name, sizeof(name), "renderlog-%s.txt", id);
    }
    fclose(f);
  }
  psn_log_open(name);
}

void write_marker(const char* text, int len) {
  static int opened;
  if (!opened) {
    open_game_log();
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
// and the return addresses on the stack, then lets the crash proceed. Map
// addresses back with: llvm-symbolizer --obj=<elf> <address - 0x400000>
// (the eboot is loaded at 0x400000; subtract 1 more for return addresses).

#include <stdint.h>
#include <stdio.h>

// The PS4 kernel is FreeBSD, but the OpenOrbis signal.h describes musl's
// Linux struct sigaction, flag values and signal numbers. Going through
// it, the kernel never saw SA_SIGINFO and bus errors were not caught, so
// crashes went unlogged. Use libkernel's _sigaction with FreeBSD's types.
struct fbsd_sigaction {
  void (*handler)(int, void*, void*);
  int flags;
  uint32_t mask[4];
};
#define FBSD_SA_SIGINFO 0x40
#define FBSD_SIGILL 4
#define FBSD_SIGTRAP 5
#define FBSD_SIGABRT 6
#define FBSD_SIGFPE 8
#define FBSD_SIGBUS 10
#define FBSD_SIGSEGV 11
#define FBSD_SIGSYS 12
int _sigaction(int sig, const struct fbsd_sigaction* action,
               struct fbsd_sigaction* old);

// FreeBSD's siginfo_t has si_addr at byte 24. The registers are FreeBSD's
// amd64 mcontext_t, a flat array of 8-byte slots, but the PS4's ucontext_t
// has more before it than FreeBSD's 16-byte sigset_t, so crash_handler
// finds it: mc_addr (the fault address) is slot 17, with rip at 20 and rsp
// at 23.
#define SI_ADDR(info) (*(void* const*)((const char*)(info) + 24))
#define MC_RDI 1
#define MC_RSI 2
#define MC_RDX 3
#define MC_RCX 4
#define MC_RAX 7
#define MC_RBX 8
#define MC_RBP 9
#define MC_ADDR 17
#define MC_RIP 20
#define MC_RSP 23

// The eboot's code (it is loaded at 0x400000) and the main thread's stack
#define CODE_START 0x400000ULL
#define CODE_END 0x1400000ULL
#define IS_CODE(v) ((v) >= CODE_START && (v) < CODE_END)
#define IS_STACK(v) ((v) >= 0x700000000ULL && (v) < 0x800000000ULL)

static void crash_handler(int sig, void* info, void* context) {
  static int crashed;
  char line[160];
  const uint64_t* uc = (const uint64_t*)context;
  uint64_t fault = info ? (uint64_t)SI_ADDR(info) : 0;
  const uint64_t* mc = NULL;
  int n;
  if (crashed++) {
    return;
  }
  // The raw context, to check the layout
  for (int i = 0; i < 48; i += 4) {
    n = snprintf(line, sizeof(line), "  uc[%2d] %lx %lx %lx %lx\n", i,
                 (unsigned long)uc[i], (unsigned long)uc[i + 1],
                 (unsigned long)uc[i + 2], (unsigned long)uc[i + 3]);
    write_marker(line, n);
  }
  for (int base = 0; base <= 16 && mc == NULL; base++) {
    const uint64_t* m = uc + base;
    if (m[MC_ADDR] == fault && IS_STACK(m[MC_RSP])) {
      mc = m;
    }
  }
  if (mc == NULL) {
    n = snprintf(line, sizeof(line), "CRASH signal %d, fault address %p (registers not found)\n",
                 sig, (void*)fault);
    write_marker(line, n);
    goto done;
  }
  n = snprintf(line, sizeof(line),
               "CRASH signal %d, fault address %p, rip %p, rsp %p (registers at uc+%d)\n",
               sig, (void*)fault, (void*)mc[MC_RIP], (void*)mc[MC_RSP],
               (int)((mc - uc) * 8));
  write_marker(line, n);
  n = snprintf(line, sizeof(line),
               "  rax %lx rbx %lx rcx %lx rdx %lx rsi %lx rdi %lx rbp %lx\n",
               (unsigned long)mc[MC_RAX], (unsigned long)mc[MC_RBX],
               (unsigned long)mc[MC_RCX], (unsigned long)mc[MC_RDX],
               (unsigned long)mc[MC_RSI], (unsigned long)mc[MC_RDI],
               (unsigned long)mc[MC_RBP]);
  write_marker(line, n);
  // Optimized code keeps no frame pointers: list the return addresses
  // into our code found on the stack instead (some may be stale)
  {
    const uint64_t* stack = (const uint64_t*)mc[MC_RSP];
    int found = 0;
    for (int i = 0; i < 1024 && found < 24; i++) {
      if (IS_CODE(stack[i])) {
        n = snprintf(line, sizeof(line), "  stack+%d: %p\n", i * 8, (void*)stack[i]);
        write_marker(line, n);
        found++;
      }
    }
  }
done: {
    // Let the fault happen again with the default action (system crash report)
    struct fbsd_sigaction dfl = { 0 };
    _sigaction(sig, &dfl, NULL);
  }
}

void psn_install_crash_handler(void) {
  static const int signals[] = { FBSD_SIGSEGV, FBSD_SIGBUS, FBSD_SIGILL,
                                 FBSD_SIGFPE, FBSD_SIGTRAP, FBSD_SIGABRT,
                                 FBSD_SIGSYS };
  struct fbsd_sigaction action = { 0 };
  action.handler = crash_handler;
  action.flags = FBSD_SA_SIGINFO;
  for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
    if (_sigaction(signals[i], &action, NULL) != 0) {
      char line[64];
      int n = snprintf(line, sizeof(line), "crash handler: signal %d not caught\n",
                       signals[i]);
      write_marker(line, n);
    }
  }
}
