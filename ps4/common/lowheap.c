// PSNokia low heap - see lowheap.h.

#include "lowheap.h"

#include <stdint.h>

// dlmalloc, configured as a single fixed-size mspace we hand it.
#define ONLY_MSPACES 1
#define MSPACES 1
#define HAVE_MMAP 0
#define HAVE_MORECORE 0
#define USE_LOCKS 1
#define MALLOC_ALIGNMENT 16
#define NO_MALLOC_STATS 1
// Never satisfy big requests with a separate mapping outside the arena
#define DEFAULT_MMAP_THRESHOLD ((size_t)-1)
#define DLMALLOC_EXPORT static
#include "dlmalloc/malloc.c"

#ifndef LOWHEAP_EXTERNAL_MAP
#include <sys/mman.h>
#endif

#define LOWHEAP_FIRST_HINT 0x10000000ULL
#define LOWHEAP_STEP       0x01000000ULL
#define LOWHEAP_LIMIT      0x80000000ULL

static mspace arena;
static void*  arena_base;
static size_t arena_size;

// Maps size bytes at exactly hint, or returns NULL. The PC stress test
// (lowheap_test.c) supplies its own, see LOWHEAP_EXTERNAL_MAP.
#ifdef LOWHEAP_EXTERNAL_MAP
void* lowheap_map_at(uint64_t hint, size_t size);
#define map_at lowheap_map_at
#else
static void* map_at(uint64_t hint, size_t size) {
  // A hint, not MAP_FIXED, so nothing already mapped there is clobbered.
  void* p = mmap((void*)(uintptr_t)hint, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == MAP_FAILED) {
    return NULL;
  }
  if ((uint64_t)(uintptr_t)p != hint) {
    munmap(p, size);
    return NULL;
  }
  return p;
}
#endif

int lowheap_init(size_t size) {
  if (arena != 0) {
    return 0;
  }
  for (uint64_t hint = LOWHEAP_FIRST_HINT; hint + size <= LOWHEAP_LIMIT;
       hint += LOWHEAP_STEP) {
    void* p = map_at(hint, size);
    if (p != NULL) {
      arena = create_mspace_with_base(p, size, 1);
      if (arena == 0) {
        return -2;
      }
      arena_base = p;
      arena_size = size;
      return 0;
    }
  }
  return -1;
}

void* lowheap_malloc(size_t size) {
  return mspace_malloc(arena, size);
}

void* lowheap_calloc(size_t count, size_t size) {
  return mspace_calloc(arena, count, size);
}

void* lowheap_realloc(void* p, size_t size) {
  return mspace_realloc(arena, p, size);
}

void* lowheap_memalign(size_t alignment, size_t size) {
  return mspace_memalign(arena, alignment, size);
}

void lowheap_free(void* p) {
  mspace_free(arena, p);
}

size_t lowheap_usable_size(void* p) {
  return mspace_usable_size(p);
}

void* lowheap_base(void) {
  return arena_base;
}

size_t lowheap_size(void) {
  return arena_size;
}

size_t lowheap_in_use(void) {
  struct mallinfo info = mspace_mallinfo(arena);
  return info.uordblks;
}
