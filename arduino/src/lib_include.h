// File is only expected to be included once per translation unit
#ifdef LIB_PATH
#error File was included more than once
#endif // LIB_PATH

// Includes configuration header from parent directory
#include "../user_config.h"

// Gets library path from file
#include "../lib_path.h"

// Ensures path to library macro is defined
#ifndef LIB_PATH
#error Unable to find LIB_PATH
#endif // LIB_PATH

// Macro function to include a file from the waccat repository defined elsewhere on disk
#define LIB_INCLUDE(file) <LIB_PATH/file>
