/*
 * BuildFlags_ps4.hpp: compile-time configuration options for the PS4
 * (OpenOrbis) platform.
 */

// Sockets are not implemented on this platform yet (see OsSocket_ps4.cpp)
#define USE_BSD_SOCKET 0

// Ticks come from a background pthread (see OS_ps4.cpp)
#define SUPPORTS_TIMER_THREAD        1
#define SUPPORTS_TIMER_INTERRUPT     1

// The Java heap is one fixed chunk from the low heap (see OsMemory_ps4.cpp)
#define SUPPORTS_ADJUSTABLE_MEMORY_CHUNK 0
