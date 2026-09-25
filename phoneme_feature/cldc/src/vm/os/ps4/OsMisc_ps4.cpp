/*
 * OsMisc_ps4.cpp
 */

#include "incls/_precompiled.incl"
#include "incls/_OsMisc_ps4.cpp.incl"

#ifdef __cplusplus
extern "C" {
#endif

const JvmPathChar *OsMisc_get_classpath() {
  // The classpath is always passed to JVM_Start (see Main_ps4.cpp)
  return NULL;
}

void OsMisc_flush_icache(address start, int size) {
  // Nothing to do: the C interpreter generates no code.
}

#if !defined(PRODUCT) || USE_DEBUG_PRINTING

const char *OsMisc_jlong_format_specifier() {
  return "%lld";
}

const char *OsMisc_julong_format_specifier() {
  return "%llu";
}

#endif // PRODUCT

#if ENABLE_PAGE_PROTECTION
void OsMisc_page_protect() {
  UNIMPLEMENTED();
}

void OsMisc_page_unprotect() {
  UNIMPLEMENTED();
}
#endif // ENABLE_PAGE_PROTECTION

#ifdef __cplusplus
}
#endif
