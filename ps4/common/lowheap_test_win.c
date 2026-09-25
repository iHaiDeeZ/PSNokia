// Windows mapping for the PC stress test of lowheap.
#include <windows.h>
#include <stdint.h>

void* lowheap_map_at(uint64_t hint, size_t size) {
  return VirtualAlloc((LPVOID)(uintptr_t)hint, size,
                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}
