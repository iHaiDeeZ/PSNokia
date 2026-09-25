/*
 * OsFile_ps4.hpp
 */

extern "C" {

struct OsFile;
#if !ENABLE_PCSL
// A small table index, not a pointer: file handles are kept in 4-byte VM
// words (see OsFile_ps4.cpp).
typedef OsFile* OsFile_Handle;
#endif

const char OsFile_separator_char      = '/';
const char OsFile_path_separator_char = ':';

}
