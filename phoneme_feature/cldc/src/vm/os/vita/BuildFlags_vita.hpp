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
 * BuildFlags_vita.hpp: compile-time
 * configuration options for the Vita platform.
 */

// Enable the following flag if you want to test the UNICODE
// FilePath handling under Vita
// #define USE_UNICODE_FOR_FILENAMES 1

// We don't use BSDSocket.cpp to implement sockets on this platform
#define USE_BSD_SOCKET 0

// The Vita port support TIMER_THREAD but not TIMER_INTERRUPT
#define SUPPORTS_TIMER_THREAD        1
#define SUPPORTS_TIMER_INTERRUPT     1

// The Vita port does not support adjustable memory chunks for
// implementing the Java heap.
#define SUPPORTS_ADJUSTABLE_MEMORY_CHUNK 0

// Enable PCSL (Portable Common Services Library) support, required
// by MIDP for file I/O, networking, etc. jvmconfig.h special-cases
// ENABLE_PCSL with an #ifndef guard (see Configurator.java), but in
// our build BuildFlags_vita.hpp is included AFTER jvmconfig.h in the
// precompiled header chain, so the guard does not help us -- we must
// #undef first so this is not seen as a conflicting redefinition.
#undef ENABLE_PCSL
#define ENABLE_PCSL 1
