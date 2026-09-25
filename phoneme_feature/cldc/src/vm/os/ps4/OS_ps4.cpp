/*
 * OS_ps4.cpp: PS4 implementation of the VM operating system porting
 * interface (class Os). Refer to "/src/vm/share/runtime/OS.hpp" and the
 * Porting Guide for details.
 */

#include "incls/_precompiled.incl"
#include "incls/_OS_ps4.cpp.incl"

#include <pthread.h>
#include <time.h>
#include <unistd.h>

static volatile bool  ticker_stopping = false;
static volatile bool  ticker_running  = false;
static volatile jlong cached_millis   = 0;

void ads_panic() {
}

#if ENABLE_DYNAMIC_NATIVE_METHODS
void* Os::loadLibrary(const char* libName) {
  return 0; // Library loading is not supported
}
void* Os::getSymbol(void* handle, const char* name) {
  return 0;
}
#endif

static jlong read_millis() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (jlong)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/*
 * The VM's "timer interrupt": real_time_tick() sets the scheduler's
 * _timer_has_ticked. Without it a Java thread that paces itself with
 * Thread.yield() never polls native events (see the Vita port's
 * OS_vita.cpp for the whole story). The same thread keeps a cached clock
 * for Os::java_time_millis(), which some MIDlets call very often.
 */
static void* ticker_thread(void* arg) {
  (void)arg;
  while (!ticker_stopping) {
    cached_millis = read_millis();
    real_time_tick(1);
    usleep(1000);
  }
  ticker_running = false;
  return NULL;
}

jlong Os::java_time_millis() {
  return ticker_running ? cached_millis : read_millis();
}

void Os::sleep(jlong ms) {
  if (ms > 0) {
    usleep((useconds_t)(ms * 1000));
  }
}

// Ticks run unconditionally in ticker_thread; nothing on this port needs
// them suspended.
bool Os::start_ticks() {
  return true;
}

void Os::stop_ticks() {
}

void Os::suspend_ticks() {
}

void Os::resume_ticks() {
}

void Os::start_compiler_timer() {
}

bool Os::check_compiler_timer() {
  return false;
}

void Os::initialize() {
  cached_millis = read_millis();
  ticker_stopping = false;
  pthread_t thread;
  if (pthread_create(&thread, NULL, ticker_thread, NULL) == 0) {
    ticker_running = true;
    pthread_detach(thread);
  }
}

void Os::dispose() {
  ticker_stopping = true;
}

#ifndef PRODUCT

jlong Os::elapsed_counter() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (jlong)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

jlong Os::elapsed_frequency() {
  return 1000000; // elapsed_counter() is in microseconds
}

#endif // PRODUCT
