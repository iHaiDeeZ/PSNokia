/*
 * OsFile_ps4.cpp: file access for the PS4, on top of libc stdio.
 *
 * File handles end up in 4-byte VM words (e.g. the int handle field of
 * com.sun.cldchi.jvm.FileDescriptor), but a FILE* lives in the libc heap,
 * far above 4GB. Handles are therefore small table indices; the FILE*
 * stays in this file.
 */

#include "incls/_precompiled.incl"
#include "incls/_OsFile_ps4.cpp.incl"

#include <stdio.h>
#include <sys/stat.h>

#if !ENABLE_PCSL

extern "C" {

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

static FILE* file_of(OsFile_Handle handle) {
  return open_files[(address_word)handle - 1];
}

OsFile_Handle OsFile_open(const PathChar *filename, const char *mode) {
  return handle_of(fopen((const char*)filename, mode));
}

int OsFile_close(OsFile_Handle handle) {
  int result = fclose(file_of(handle));
  open_files[(address_word)handle - 1] = NULL;
  return result;
}

int OsFile_flush(OsFile_Handle handle) {
  return fflush(file_of(handle));
}

size_t OsFile_read(OsFile_Handle handle,
                   void *buffer, size_t size, size_t count) {
  return fread(buffer, size, count, file_of(handle));
}

size_t OsFile_write(OsFile_Handle handle,
                    const void *buffer, size_t size, size_t count) {
  return fwrite(buffer, size, count, file_of(handle));
}

long OsFile_length(OsFile_Handle handle) {
  FILE* file = file_of(handle);
  long pos = ftell(file);
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, pos, SEEK_SET);
  return length;
}

bool OsFile_exists(const PathChar *filename) {
  struct stat st;
  return stat((const char*)filename, &st) == 0 && S_ISREG(st.st_mode);
}

long OsFile_seek(OsFile_Handle handle, long offset, int origin) {
  return fseek(file_of(handle), offset, origin);
}

int OsFile_error(OsFile_Handle handle) {
  return ferror(file_of(handle));
}

int OsFile_eof(OsFile_Handle handle) {
  return feof(file_of(handle));
}

bool OsFile_rename(const PathChar *from, const PathChar *to) {
  remove((const char*)to);
  return rename((const char*)from, (const char*)to) == 0;
}

int OsFile_remove(const PathChar *filename) {
  return remove((const char*)filename);
}

} // extern "C"

#endif // !ENABLE_PCSL
