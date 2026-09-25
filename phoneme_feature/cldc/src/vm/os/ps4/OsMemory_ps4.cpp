/*
 * OsMemory_ps4.cpp: all VM memory comes from the low heap.
 *
 * The VM keeps pointers in 4-byte words, so everything it allocates must be
 * below 4GB. The PS4 libc heap is far above that; lowheap (a dlmalloc
 * mspace in an arena mapped below 2GB, see ps4/common) is not.
 * Main_ps4.cpp initializes it before the VM starts.
 *
 * With ENABLE_PCSL, share/runtime/OsMemory.cpp uses pcsl_mem_malloc instead,
 * which is malloc, which is the low heap too (see lowheap_malloc.c).
 */

#include "incls/_precompiled.incl"
#include "incls/_OsMemory_ps4.cpp.incl"

#include "lowheap.h"

#if !ENABLE_PCSL

#ifdef __cplusplus
extern "C" {
#endif

void *OsMemory_allocate(size_t size) {
  return lowheap_malloc(size);
}

void OsMemory_free(void *p) {
  lowheap_free(p);
}

#ifdef __cplusplus
}
#endif

#endif // !ENABLE_PCSL
