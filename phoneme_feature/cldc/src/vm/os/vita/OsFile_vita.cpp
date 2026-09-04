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

/** \file OsFile_vita.cpp
 *
 * Real vitasdk sceIo* filesystem access (ux0:, etc). Replaces the earlier
 * embedded-fake-file stub used for interpreter bring-up.
 */

#include "incls/_precompiled.incl"
#include "incls/_OsFile_vita.cpp.incl"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

extern "C" {

#if !ENABLE_PCSL
struct OsFile {
    SceUID fd;
    bool in_use;
};

static OsFile file_handles[10];

OsFile_Handle OsFile_open(const PathChar *fn_filename, const char *mode) {
  bool has_r = false, has_w = false, has_plus = false;

  for (const char *m = mode; *m; m++) {
    if (*m == 'r') has_r = true;
    else if (*m == 'w') has_w = true;
    else if (*m == '+') has_plus = true;
  }
  (void)has_r;

  int flags;
  if (has_w) {
    flags = has_plus ? (SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC)
                      : (SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC);
  } else {
    flags = has_plus ? SCE_O_RDWR : SCE_O_RDONLY;
  }

  SceUID fd = sceIoOpen(fn_filename, flags, 0777);
  if (fd < 0) {
    return NULL;
  }

  for (int j = 0; j < (int)ARRAY_SIZE(file_handles); j++) {
    if (!file_handles[j].in_use) {
      file_handles[j].fd = fd;
      file_handles[j].in_use = true;
      return &file_handles[j];
    }
  }

  // out of OsFile handles
  sceIoClose(fd);
  return NULL;
}

int OsFile_close(OsFile_Handle handle) {
  int result = sceIoClose(handle->fd);
  handle->in_use = false;
  return (result < 0) ? -1 : 0;
}

int OsFile_flush(OsFile_Handle handle) {
  // sceIoWrite goes straight through -- no user-space buffering here
  (void)handle;
  return 0;
}

size_t OsFile_read(OsFile_Handle handle,
                   void *buffer, size_t size, size_t count) {
  if (size == 0) {
    return 0;
  }
  SceSSize got = sceIoRead(handle->fd, buffer, (SceSize)(size * count));
  if (got < 0) {
    return 0;
  }
  return (size_t)got / size;
}

size_t OsFile_write(OsFile_Handle handle,
                    const void *buffer, size_t size, size_t count) {
  if (size == 0) {
    return 0;
  }
  SceSSize written = sceIoWrite(handle->fd, buffer, (SceSize)(size * count));
  if (written < 0) {
    return 0;
  }
  return (size_t)written / size;
}

long OsFile_length(OsFile_Handle handle) {
  SceIoStat stat;
  if (sceIoGetstatByFd(handle->fd, &stat) < 0) {
    return -1;
  }
  return (long)stat.st_size;
}

bool OsFile_exists(const PathChar *fn_filename) {
  SceIoStat stat;
  return sceIoGetstat(fn_filename, &stat) >= 0;
}

long OsFile_seek(OsFile_Handle handle, long offset, int origin) {
  int whence;
  switch (origin) {
  case SEEK_CUR:
    whence = SCE_SEEK_CUR;
    break;
  case SEEK_SET:
    whence = SCE_SEEK_SET;
    break;
  case SEEK_END:
    whence = SCE_SEEK_END;
    break;
  default:
    return -1;
  }

  long result = sceIoLseek32(handle->fd, offset, whence);
  if (result < 0) {
    return -1;
  }
  return 0;
}

int OsFile_error(OsFile_Handle handle) {
  (void)handle;
  return 0;
}

int OsFile_eof(OsFile_Handle handle) {
  long cur = sceIoLseek32(handle->fd, 0, SCE_SEEK_CUR);
  SceIoStat stat;
  if (sceIoGetstatByFd(handle->fd, &stat) < 0) {
    return 1;
  }
  return cur >= (long)stat.st_size;
}

bool OsFile_rename(const char *from, const char *to) {
  return sceIoRename(from, to) >= 0;
}

int OsFile_remove(const char *filename) {
  return (sceIoRemove(filename) >= 0) ? 0 : -1;
}

#endif // !ENABLE_PCSL

} // extern "C"
