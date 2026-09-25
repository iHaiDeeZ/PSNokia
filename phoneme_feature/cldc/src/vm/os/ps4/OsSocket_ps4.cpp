/*
 * OsSocket_ps4.cpp: sockets are not implemented on the PS4 yet.
 */

#include "incls/_precompiled.incl"
#include "incls/_OsSocket_ps4.cpp.incl"

void JVMSPI_CheckEvents(JVMSPI_BlockedThreadInfo * blocked_threads,
                        int blocked_threads_count, jlong timeout_ms) {
  (void)blocked_threads;
  (void)blocked_threads_count;
  if (timeout_ms > 0) {
    Os::sleep(timeout_ms);
  }
}

extern "C" {

KNIEXPORT KNI_RETURNTYPE_INT
Java_com_sun_cldc_io_j2me_socket_Protocol_open0() {
  KNI_ReturnInt(-1);
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_com_sun_cldc_io_j2me_socket_Protocol_readByte() {
  KNI_ReturnInt(-1);
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_com_sun_cldc_io_j2me_socket_Protocol_readBuf() {
  KNI_ReturnInt(-1);
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_com_sun_cldc_io_j2me_socket_Protocol_writeByte() {
  KNI_ReturnInt(-1);
}

KNIEXPORT KNI_RETURNTYPE_INT
Java_com_sun_cldc_io_j2me_socket_Protocol_writeBuf() {
  KNI_ReturnInt(-1);
}

jint Java_com_sun_cldc_io_j2me_socket_Protocol_available0(Thread *THREAD) {
  return (jint) 0;
}

void Java_com_sun_cldc_io_j2me_socket_Protocol_close0(Thread *THREAD) {
}

}
