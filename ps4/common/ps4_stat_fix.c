// Fixes stat(), fstat() and lstat() for OpenOrbis programs.
//
// The OpenOrbis headers declare FreeBSD's struct stat but with musl's type
// sizes (4-byte mode_t instead of FreeBSD's 2-byte one), so every field
// after st_mode is 8 bytes off from what the PS4 kernel actually writes:
// st_size is read from where the kernel put st_blocks. PCSL, MIDP and the
// VM get file sizes this way, so e.g. a 1MB jar looked 2176 bytes long.
//
// These replacements call the kernel with a buffer in its real layout and
// translate into the header's struct stat. Link this object directly (not
// from an archive) so it takes precedence over libkernel's versions.

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// FreeBSD 9 amd64 struct stat, as the PS4 kernel fills it in
struct kernel_stat {
  uint32_t k_dev;
  uint32_t k_ino;
  uint16_t k_mode;
  uint16_t k_nlink;
  uint32_t k_uid;
  uint32_t k_gid;
  uint32_t k_rdev;
  struct timespec k_atim;
  struct timespec k_mtim;
  struct timespec k_ctim;
  int64_t  k_size;
  int64_t  k_blocks;
  uint32_t k_blksize;
  uint32_t k_flags;
  uint32_t k_gen;
  int32_t  k_lspare;
  struct timespec k_birthtim;
  // Room for anything newer firmware appends
  uint8_t  k_reserved[64];
};

// libkernel entry points that take the kernel layout. The plain names
// (stat/fstat/lstat) are the ones replaced below.
int _fstat(int fd, void* buf);
int sceKernelStat(const char* path, void* buf);

static void translate(const struct kernel_stat* k, struct stat* st) {
  memset(st, 0, sizeof(*st));
  st->st_dev = k->k_dev;
  st->st_ino = k->k_ino;
  st->st_mode = k->k_mode;
  st->st_nlink = k->k_nlink;
  st->st_uid = k->k_uid;
  st->st_gid = k->k_gid;
  st->st_rdev = k->k_rdev;
  st->st_atim = k->k_atim;
  st->st_mtim = k->k_mtim;
  st->st_ctim = k->k_ctim;
  st->st_size = k->k_size;
  st->st_blocks = k->k_blocks;
  st->st_blksize = k->k_blksize;
}

int fstat(int fd, struct stat* st) {
  struct kernel_stat k;
  int rc = _fstat(fd, &k);
  if (rc == 0) {
    translate(&k, st);
  }
  return rc;
}

int stat(const char* path, struct stat* st) {
  struct kernel_stat k;
  int rc = sceKernelStat(path, &k);
  if (rc != 0) {
    // SCE error codes are 0x8002xxxx with the errno in the low byte
    errno = rc & 0xFF;
    return -1;
  }
  translate(&k, st);
  return 0;
}

// Nothing under /app0 or /data is a symbolic link, so following links is
// equivalent here.
int lstat(const char* path, struct stat* st) {
  return stat(path, st);
}
