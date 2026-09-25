// PSNokia low heap: a malloc-style allocator whose memory is all below 2GB.
//
// The phoneME VM keeps pointers in 4-byte words (see narrow<T> in the VM's
// GlobalDefinitions.hpp), so everything it points to must live below 4GB.
// On the PS4 the eboot and its libc sit at 0x400000, but the libc heap and
// thread stacks are far above 4GB. lowheap reserves one arena at a low
// address and runs dlmalloc (an mspace) inside it.
//
// Staying below 2GB also keeps addresses positive when read back as a
// signed 32-bit int, which some VM and MIDP code does.

#ifndef PSN_LOWHEAP_H
#define PSN_LOWHEAP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reserves the arena. Returns 0 on success. Safe to call more than once.
int lowheap_init(size_t arena_size);

void* lowheap_malloc(size_t size);
void* lowheap_calloc(size_t count, size_t size);
void* lowheap_realloc(void* p, size_t size);
void* lowheap_memalign(size_t alignment, size_t size);
void  lowheap_free(void* p);
size_t lowheap_usable_size(void* p);

// Arena bounds, for diagnostics.
void*  lowheap_base(void);
size_t lowheap_size(void);
size_t lowheap_in_use(void);

#ifdef __cplusplus
}
#endif

#endif // PSN_LOWHEAP_H
