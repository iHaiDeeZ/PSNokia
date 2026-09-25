/*
 * renderlog.h: appends diagnostic text to the port's render log.
 *
 * Vita: ux0:data/renderlog.txt, written directly with sceIo (as before).
 * Other platforms (PS4): write_marker(), which the platform provides - on
 * the PS4 ps4/common/psn_marker.c, which appends to
 * /data/psnokia/renderlog.txt and mirrors the text over UDP.
 */

#ifndef _RENDERLOG_H_
#define _RENDERLOG_H_

#if __has_include(<psp2/io/fcntl.h>)
#include <psp2/io/fcntl.h>

#define RENDERLOG_WRITE(text, len) do { \
    int renderlog_fd_ = sceIoOpen("ux0:data/renderlog.txt", \
        SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777); \
    if (renderlog_fd_ >= 0) { \
      sceIoWrite(renderlog_fd_, (text), (len)); \
      sceIoClose(renderlog_fd_); \
    } \
  } while (0)
#else

#ifdef __cplusplus
extern "C"
#endif
void write_marker(const char* text, int len);

#define RENDERLOG_WRITE(text, len) write_marker((text), (len))
#endif

#endif /* _RENDERLOG_H_ */
