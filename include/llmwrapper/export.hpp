#pragma once

// Symbol visibility for static (default) and shared (DLL) builds.
//   - Static library: nothing to do, LLMWRAPPER_API expands to nothing.
//   - DLL: CMake defines LLMWRAPPER_SHARED for everyone and
//     LLMWRAPPER_BUILDING only while compiling the library itself.

#if defined(LLMWRAPPER_SHARED)
#  if defined(_WIN32)
#    if defined(LLMWRAPPER_BUILDING)
#      define LLMWRAPPER_API __declspec(dllexport)
#    else
#      define LLMWRAPPER_API __declspec(dllimport)
#    endif
#  else
#    define LLMWRAPPER_API __attribute__((visibility("default")))
#  endif
#else
#  define LLMWRAPPER_API
#endif

#if defined(_MSC_VER) && defined(LLMWRAPPER_SHARED)
// std::string / std::vector members in exported classes are fine as long as
// the application and the DLL are built with the same MSVC toolset.
#  pragma warning(disable : 4251 4275)
#endif
