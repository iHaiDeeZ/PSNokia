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
 * OsFile_win32.cpp: Win32 implementation of File I/O.
 */

#include "incls/_precompiled.incl"
#include "incls/_OsFile_win32.cpp.incl"


#ifdef CYGWIN
#define _stat stat
#endif
#include <sys/stat.h>

#if defined (_S_IFREG) || !defined(S_IFREG)
#define S_IFREG _S_IFREG
#endif

extern "C" {

#ifndef _MSC_VER
// We should really use things like _wfopen(), _wrename() and _wremove(),
// but these are part of  VC++ C library, and may not be available on
// Cygwin and CodeWarrior/Win32
#if USE_UNICODE_FOR_FILENAMES
#define PATHCHAR_TO_ASCII(from, to) \
  char to[256]; \
  { \
      int len = fn_strlen(from); \
      for (int i=0; i<len; i++) { \
        to[i] = (char)from[i]; \
      } \
      to[len] = 0; \
  }
#else
#define PATHCHAR_TO_ASCII(from, to) char *to = (char *) from
#endif
#endif

#if !ENABLE_PCSL

#if USE_NARROW_POINTERS
// File handles are kept in 4-byte VM words (e.g. the int handle field of
// com.sun.cldchi.jvm.FileDescriptor), but a FILE* can live above 4GB. Hand
// out small table indices instead and keep the FILE* here.
enum { MAX_OPEN_FILES = 256 };
static FILE* open_files[MAX_OPEN_FILES];

static OsFile_Handle handle_of(FILE* file) {
  if (file == NULL) {
    return NULL;
  }
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (open_files[i] == NULL) {
      open_files[i] = file;
      return (OsFile_Handle)(address_word)(i + 1);
    }
  }
  fclose(file);
  return NULL;
}

static void release_handle(OsFile_Handle handle) {
  open_files[(address_word)handle - 1] = NULL;
}

#define FILE_OF(handle) (open_files[(address_word)(handle) - 1])
#else
#define handle_of(file) (file)
#define release_handle(handle)
#define FILE_OF(handle) (handle)
#endif

OsFile_Handle OsFile_open(const PathChar *fn_filename, const char *mode) {
#ifdef _MSC_VER
  // The VM just opens files for reading
  wchar_t mymode[10+1];
  int modelen = strlen(mode);
  if (modelen > 10) {
      modelen = 10;
  }
  for (int i=0; i<modelen; i++) {
    mymode[i] = (wchar_t)mode[i];
  }
  mymode[modelen] = 0;

  OsFile_Handle result =
      (OsFile_Handle) _wfopen((const wchar_t *)fn_filename, mymode);

  //tty->print("Open: ");
  //while (*fn_filename) {
  //  tty->print("%c", *fn_filename++);
  //}
  //tty->print_cr(" 0x%x", result);

  return result;
#else
  PATHCHAR_TO_ASCII(fn_filename, filename);
  return handle_of(fopen(filename, mode));
#endif
}

int OsFile_close(OsFile_Handle handle) {
  //tty->print_cr("Close: 0x%x", handle);
  int result = fclose(FILE_OF(handle));
  release_handle(handle);
  return result;
}

int OsFile_flush(OsFile_Handle handle) {
  return fflush(FILE_OF(handle));
}

size_t OsFile_read(OsFile_Handle handle,
                   void *buffer, size_t size, size_t count) {
  return fread(buffer, size, count, FILE_OF(handle));
}

size_t OsFile_write(OsFile_Handle handle,
                    const void *buffer, size_t size, size_t count) {
  return fwrite(buffer, size, count, FILE_OF(handle));
}

long OsFile_length(OsFile_Handle handle) {
  FILE* file = FILE_OF(handle);
  fseek(file, 0, SEEK_END);
  long res = ftell(file);
  fseek(file, 0, SEEK_SET);// reset file
  return res;
}

bool OsFile_exists(const PathChar *fn_filename) {
  struct _stat statBuf;

#ifdef _MSC_VER
  if (_wstat((const wchar_t *)fn_filename, &statBuf) != 0 ||
      (statBuf.st_mode & S_IFREG) == 0) {
    return false;
  } else {
    return true;
  }
#else
  PATHCHAR_TO_ASCII(fn_filename, filename);

  if (_stat(filename, &statBuf) != 0 || (statBuf.st_mode & S_IFREG) == 0) {
    return false;
  } else {
    return true;
  }
#endif
}

long OsFile_seek(OsFile_Handle handle, long offset, int origin) {
  return fseek(FILE_OF(handle), offset, origin);
}

int OsFile_error(OsFile_Handle handle) {
  return ferror(FILE_OF(handle));
}

int OsFile_eof(OsFile_Handle handle) {
  return feof(FILE_OF(handle));
}

bool OsFile_rename(const JvmPathChar *fn_from, const JvmPathChar *fn_to) {
#ifdef _MSC_VER
  _wremove((const wchar_t *)fn_to);
  _wrename((const wchar_t *)fn_from, (const wchar_t *)fn_to);
#else
  PATHCHAR_TO_ASCII(fn_from, from);
  PATHCHAR_TO_ASCII(fn_to, to);
  remove(to);
  rename(from, to);
#endif
  return true;
}

int OsFile_remove(const JvmPathChar *fn_filename) {
#ifdef _MSC_VER
  return _wremove((const wchar_t *)fn_filename);
#else
  PATHCHAR_TO_ASCII(fn_filename, filename);
  return remove(filename);
#endif
}

#endif // !ENABLE_PCSL

} // extern "C"
