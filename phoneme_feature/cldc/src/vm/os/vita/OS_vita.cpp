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

/*
 * OS_vita.cpp: vita implementation of the VM
 *               operating system porting interface
 *
 * This file defines the vita-specific implementation
 * of the OS porting interface (class Os). Refer to file
 * "/src/vm/share/runtime/OS.hpp" and the Porting
 * Guide for details.
 */

#include "incls/_precompiled.incl"
#include "incls/_OS_vita.cpp.incl"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>

static bool             ticker_stopping = false;
static bool             ticker_running = false;
static int              sock_initialized = 0;

static bool  _has_offset = false;
static jlong _offset     = 0;

void ads_panic() {
  /* Add this empty method just for building vita_arm smoothly */ 
  return;
}

#if ENABLE_DYNAMIC_NATIVE_METHODS
void* Os::loadLibrary(const char* libName) {
  return 0; //Library loading not supported for this OS
}
void* Os::getSymbol(void* handle, const char* name) {
  return 0; //Library loading not supported for this OS
}
#endif

jlong offset() {
  /*
   * Offset from the monotonic sceKernelGetProcessTimeWide() clock to
   * real Unix-epoch milliseconds, computed once from the Vita's RTC.
   * Without this, java_time_millis() returned small numbers counting
   * up from process start (e.g. ~15000 for "15 seconds after launch")
   * instead of a real ~1.7e12 epoch timestamp — harmless for relative
   * timing (Thread.sleep, event timeouts), but any MIDlet code doing
   * an absolute-date comparison (splash/license gating, "wait until
   * clock reads X") against a real epoch value would then compare
   * against a value that can never be reached, hanging forever.
   */
  if (!_has_offset) {
    SceDateTime dt;
    SceRtcTick tick;
    _offset = 0;
    if (sceRtcGetCurrentClock(&dt, 0) >= 0 && sceRtcGetTick(&dt, &tick) >= 0) {
      /* SceRtcTick.tick is microseconds since 0001-01-01, per vitasdk;
       * convert to Unix-epoch milliseconds (epoch offset in seconds
       * between 0001-01-01 and 1970-01-01 is 62135596800). */
      jlong unixEpochUs = (jlong)tick.tick - ((jlong)62135596800LL * 1000000LL);
      jlong nowMonotonicMs = (jlong)(sceKernelGetProcessTimeWide() / 1000);
      _offset = (unixEpochUs / 1000) - nowMonotonicMs;
    }
    _has_offset = true;
  }
  return _offset;
}

static volatile jlong _cached_millis = 0;

static int millis_ticker_thread(SceSize args, void *argp) {
  (void)args;
  (void)argp;
  while (!ticker_stopping) {
    _cached_millis = (jlong)(sceKernelGetProcessTimeWide() / 1000) + offset();
    /*
     * real_time_tick() is the VM's "timer interrupt" (Thread.cpp) - every
     * other CLDC port wires it to a genuine periodic hardware/OS timer
     * (see e.g. OS_ads.cpp's IRQ handler, OS_javacall.cpp's
     * javacall_time_initialize_timer), which sets Scheduler::_timer_has_ticked.
     * Os::start_ticks()/stop_ticks()/suspend_ticks()/resume_ticks() were all
     * no-ops on Vita, so _timer_has_ticked was PERMANENTLY false on this
     * entire port - nobody ever called real_time_tick() at all.
     *
     * This silently starves native event polling specifically for a Java
     * thread that paces itself with Thread.yield() instead of
     * Thread.sleep(): Scheduler::yield() only calls check_blocked_threads()
     * (-> JVMSPI_CheckEvents -> checkForSystemSignal, the function that
     * actually pumps SDL input) when _timer_has_ticked is true OR the
     * short-lived _estimated_event_readiness counter is still nonzero -
     * neither of which a yield()-only busy loop (which never becomes
     * non-runnable, so never falls into the unconditional
     * wait_for_event_or_timer() path Thread.sleep() uses) can ever
     * re-arm on its own. Confirmed as the root cause of Tower Bloxx's
     * language-select screen (a real GameCanvas/FullCanvas subclass whose
     * main loop calls Thread.yield(), not Thread.sleep()) running at
     * 1600+ FPS while receiving zero input of any kind. Every previously
     * tested game happened to pace its loop with Thread.sleep(), which
     * doesn't depend on this flag at all - that's why this was never
     * seen until now, and why the earlier CLDC-OS-layer audit's "did NOT
     * find evidence it's currently causing a problem" conclusion about
     * these no-op tick functions was incomplete, not wrong for what it
     * actually tested.
     */
    real_time_tick(1);
    sceKernelDelayThread(1000); /* 1ms - far finer than any game needs, still cheap */
  }
  ticker_running = false;
  return sceKernelExitDeleteThread(0);
}

jlong Os::java_time_millis() {
  /*
   * This was previously stubbed to always return 0, which silently broke
   * every time-based scheduling path in the VM (Thread.sleep wakeups,
   * event-wait timeouts, System.currentTimeMillis()): with the clock
   * frozen at 0, a sleeping thread's wakeup_time (computed as millis +
   * java_time_millis()) could never compare <= "now", so threads only
   * ever resumed when an incidental SDL event broke the caller out of
   * its busy-wait loop — producing ~1 FPS regardless of actual workload.
   * sceKernelGetProcessTimeWide() gives a cheap monotonic microsecond
   * clock for the per-call cost; offset() (computed once, via RTC)
   * shifts it to a real Unix-epoch value.
   *
   * Some MIDlets (e.g. Sonic 2 Dash) call System.currentTimeMillis()
   * extremely often — per-object in tight update loops rather than once
   * per frame — and each such call was profiled (via the SAMPLE marker,
   * this function ended up on the stack for ~89% of samples during a
   * gameplay session, an order of magnitude more than any other
   * function) to be expensive enough under Vita3K's emulation of
   * sceKernelGetProcessTimeWide() to single-handedly tank FPS from
   * 200-300+ down to ~10. A background ticker thread (millis_ticker_thread,
   * started in Os::initialize()) now refreshes a cached value once a
   * millisecond via the real syscall; this hot path just reads that
   * cached value — no syscall at all on the common case. Millisecond-
   * level staleness is imperceptible for any real game logic. Falls back
   * to a direct (uncached) read if the ticker thread failed to start or
   * hasn't run yet.
   */
  if (ticker_running) {
    return _cached_millis;
  }
  return (jlong)(sceKernelGetProcessTimeWide() / 1000) + offset();
}

void Os::sleep(jlong ms) {
  /* Previously a no-op; delay the current thread for real. */
  if (ms > 0) {
    sceKernelDelayThread((SceUInt)(ms * 1000));
  }
}


bool Os::start_ticks() {
  /*
   * Real ticks are now driven unconditionally by millis_ticker_thread
   * (started in Os::initialize(), see its real_time_tick() call and the
   * comment there) rather than by these four hooks - a single always-on
   * background thread is simpler and safer on this port than actually
   * honoring suspend/resume here, and nothing currently needs ticks
   * suppressed. Kept as a true/no-op pair so callers get the semantics
   * they expect (ticks are "enabled") without adding real suspend logic
   * nothing on this port relies on yet.
   */
  return true;
}

void Os::stop_ticks() {
  /* See Os::start_ticks() - real shutdown happens in Os::dispose(). */
  return;
}

void Os::suspend_ticks() {
  /* See Os::start_ticks() - intentionally a no-op on this port. */
  return;
}

void Os::resume_ticks() {
  /* See Os::start_ticks() - intentionally a no-op on this port. */
  return;
}

void Os::start_compiler_timer() {
  /*
   * Start a timer to check if the compiler has spent too much time and
   * needs to be suspended. See ../ads/OS_ads.cpp for an example.
   */
  return;
}

bool Os::check_compiler_timer() {
  /*
   * Returns true iff the current compilation has taken too long and
   * should be suspended and resumed later.
   */
  return false;
}

void Os::initialize() {
  /*
   * This function is used to initialize the OS structure.
   * This is where timers and threads get started for the first
   * real_time_tick event, and where signal handlers and other I/O
   * initialization should occur.
   *
   * Starts millis_ticker_thread (see Os::java_time_millis()) so the
   * cached clock value is valid before any real call arrives.
   */
  _cached_millis = (jlong)(sceKernelGetProcessTimeWide() / 1000) + offset();
  SceUID thid = sceKernelCreateThread("millis_ticker", millis_ticker_thread,
                                       0x10000100, 0x4000, 0, 0, NULL);
  if (thid >= 0) {
    ticker_stopping = false;
    ticker_running = true;
    sceKernelStartThread(thid, 0, NULL);
  }
}

void Os::dispose() {
  /*
   * This method needs to correctly clean-up
   * all threads and other OS related activity to allow
   * for a clean and complete restart.  This should undo
   * all the work that initialize does.
   */
  ticker_stopping = true;
}

#ifndef PRODUCT
static bool  _has_performance_frequency = false;
static jlong _performance_frequency     = 0;


jlong Os::elapsed_counter() {
  /*
   * Retrieve the current time in high-resolution.
   * The resolution is decided by elapsed_frequency() method
   */
  return (jlong)sceKernelGetProcessTimeWide();
}

jlong Os::elapsed_frequency() {
  /*
   * Retrieve the system-support highest time resolution,
   * The return value is equal to  1s / the small unit of system-support unit
   * For example, if system support millisecond, the return valuse is 1000
   */
  return 1000000; /* sceKernelGetProcessTimeWide() returns microseconds */
}

#endif // PRODUCT
