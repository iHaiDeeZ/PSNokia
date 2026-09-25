/*
 * JVM_ps4.cpp: PS4 VM startup and shutdown routines.
 */

#include "incls/_precompiled.incl"
#include "incls/_JVM_ps4.cpp.incl"

extern "C" int JVM_Start(const JvmPathChar *classpath, char *main_class,
                         int argc, char **argv) {
  JVM::set_arguments(classpath, main_class, argc, argv);
  return JVM::start();
}

extern "C" int JVM_Start2(const JvmPathChar *classpath, char *main_class,
                          int argc, jchar **u_argv) {
  JVM::set_arguments2(classpath, main_class, argc, NULL, u_argv, true);
  return JVM::start();
}
