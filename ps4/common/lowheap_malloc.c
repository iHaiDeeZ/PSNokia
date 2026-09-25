// Replaces the C library's allocator with the low heap (see lowheap.h), so
// every allocation in the program - C++ new, stdio, PCSL, MIDP natives,
// SDL - is below 2GB. MIDP and PCSL keep native pointers (image data, file
// handles) in 4-byte Java fields, which only works if they are low.
//
// Link these objects directly (not from an archive) and before -lc: the
// linker then never pulls in libc.a's malloc.lo, aligned_alloc.lo,
// posix_memalign.lo or malloc_usable_size.lo. musl supports replacing
// malloc this way as long as all of these functions are provided.

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "lowheap.h"

// Big enough for the Java heap, MIDP images and SDL's software surfaces.
#define LOWHEAP_DEFAULT_ARENA (256 * 1024 * 1024)

// malloc can run before main (C++ static constructors, libc start-up).
static int ensure_arena(void) {
  static int ready;
  if (!ready) {
    ready = lowheap_init(LOWHEAP_DEFAULT_ARENA) == 0;
  }
  return ready;
}

void* malloc(size_t size) {
  if (!ensure_arena()) {
    errno = ENOMEM;
    return NULL;
  }
  return lowheap_malloc(size);
}

void free(void* p) {
  if (p != NULL) {
    lowheap_free(p);
  }
}

void* calloc(size_t count, size_t size) {
  if (!ensure_arena()) {
    errno = ENOMEM;
    return NULL;
  }
  return lowheap_calloc(count, size);
}

void* realloc(void* p, size_t size) {
  if (!ensure_arena()) {
    errno = ENOMEM;
    return NULL;
  }
  return lowheap_realloc(p, size);
}

void* aligned_alloc(size_t alignment, size_t size) {
  if (!ensure_arena()) {
    errno = ENOMEM;
    return NULL;
  }
  return lowheap_memalign(alignment, size);
}

void* memalign(size_t alignment, size_t size) {
  return aligned_alloc(alignment, size);
}

int posix_memalign(void** result, size_t alignment, size_t size) {
  if (alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) {
    return EINVAL;
  }
  void* p = aligned_alloc(alignment, size);
  if (p == NULL) {
    return ENOMEM;
  }
  *result = p;
  return 0;
}

size_t malloc_usable_size(void* p) {
  return p == NULL ? 0 : lowheap_usable_size(p);
}
