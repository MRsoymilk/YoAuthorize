#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

// Windows
#if defined(_WIN32) || defined(_WIN64)
  #define YA_WINDOWS
#endif

// Linux
#if defined(__linux__) || defined(__linux)
  #define YA_LINUX
#endif

// macOS
#if defined(__APPLE__) && defined(__MACH__)
  #define YA_MACOS
#endif

// UNIX-like
#if defined(__unix__) || defined(__unix) || defined(YA_LINUX) || defined(YA_MACOS)
  #define YA_UNIX
#endif

#endif  // !YA_PLATFORM_DEF_H

