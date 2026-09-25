// Stress test for lowheap (runs on the PC): random malloc/realloc/free with
// content checks, alignment and "below 2GB" checks.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lowheap.h"

#define SLOTS 4096
#define ROUNDS 200000

static unsigned char* ptrs[SLOTS];
static size_t sizes[SLOTS];
static unsigned char tags[SLOTS];

static unsigned rng = 12345;
static unsigned next_rand(void) {
  rng = rng * 1103515245u + 12345u;
  return rng >> 8;
}

static int check(int i) {
  for (size_t k = 0; k < sizes[i]; k++) {
    if (ptrs[i][k] != tags[i]) {
      printf("CORRUPT slot %d at byte %zu\n", i, k);
      return 0;
    }
  }
  return 1;
}

static int valid(void* p) {
  uintptr_t a = (uintptr_t)p;
  if (a >= 0x80000000u) {
    printf("HIGH pointer %p\n", p);
    return 0;
  }
  if (a % 16 != 0) {
    printf("MISALIGNED pointer %p\n", p);
    return 0;
  }
  return 1;
}

int main(void) {
  if (lowheap_init(64 * 1024 * 1024) != 0) {
    printf("init failed\n");
    return 1;
  }
  printf("arena %p size %zu\n", lowheap_base(), lowheap_size());
  for (int r = 0; r < ROUNDS; r++) {
    int i = next_rand() % SLOTS;
    if (ptrs[i] != NULL) {
      if (!check(i)) return 1;
      if (next_rand() % 4 == 0) {
        size_t n = next_rand() % 8192;
        unsigned char* p = lowheap_realloc(ptrs[i], n);
        if (n != 0 && p == NULL) { printf("realloc failed\n"); return 1; }
        size_t keep = n < sizes[i] ? n : sizes[i];
        for (size_t k = 0; k < keep; k++) {
          if (p[k] != tags[i]) { printf("realloc lost data\n"); return 1; }
        }
        if (!valid(p)) return 1;
        memset(p, tags[i], n);
        ptrs[i] = p;
        sizes[i] = n;
      } else {
        lowheap_free(ptrs[i]);
        ptrs[i] = NULL;
      }
    } else {
      size_t n = (next_rand() % 16 == 0) ? next_rand() % 200000 : next_rand() % 256;
      unsigned char* p = (next_rand() % 8 == 0) ? lowheap_memalign(64, n)
                                               : lowheap_malloc(n);
      if (p == NULL) { printf("malloc(%zu) failed\n", n); return 1; }
      if (!valid(p)) return 1;
      tags[i] = (unsigned char)(next_rand() & 0xff);
      memset(p, tags[i], n);
      ptrs[i] = p;
      sizes[i] = n;
    }
  }
  for (int i = 0; i < SLOTS; i++) {
    if (ptrs[i] != NULL) {
      if (!check(i)) return 1;
      lowheap_free(ptrs[i]);
    }
  }
  printf("in use after freeing everything: %zu\n", lowheap_in_use());
  printf("lowheap_test PASSED\n");
  return 0;
}
