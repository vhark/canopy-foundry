#include "canopy/units.hpp"
#include <cstdio>
#include <version>

#if defined(_WIN32) && defined(_M_X64)
#define CANOPY_PLATFORM "win64"
#elif defined(__APPLE__) && defined(__aarch64__)
#define CANOPY_PLATFORM "mac-arm64"
#elif defined(__linux__) && defined(__x86_64__)
#define CANOPY_PLATFORM "linux-x64"
#else
#error Unsupported qualification platform
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define CANOPY_COMPILER "MSVC"
#define CANOPY_COMPILER_MAJOR (_MSC_FULL_VER / 10000000)
#define CANOPY_COMPILER_MINOR ((_MSC_FULL_VER / 100000) % 100)
#define CANOPY_COMPILER_PATCH (_MSC_FULL_VER % 100000)
#define CANOPY_CPP_VERSION _MSVC_LANG
#elif defined(__clang__) && defined(__apple_build_version__)
#define CANOPY_COMPILER "AppleClang"
#define CANOPY_COMPILER_MAJOR __clang_major__
#define CANOPY_COMPILER_MINOR __clang_minor__
#define CANOPY_COMPILER_PATCH __clang_patchlevel__
#define CANOPY_CPP_VERSION __cplusplus
#elif defined(__clang__)
#define CANOPY_COMPILER "Clang"
#define CANOPY_COMPILER_MAJOR __clang_major__
#define CANOPY_COMPILER_MINOR __clang_minor__
#define CANOPY_COMPILER_PATCH __clang_patchlevel__
#define CANOPY_CPP_VERSION __cplusplus
#else
#error Unsupported qualification compiler
#endif

#if defined(_CPPRTTI) || defined(__GXX_RTTI)
#define CANOPY_RTTI "true"
#else
#define CANOPY_RTTI "false"
#endif

#if defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#define CANOPY_EXCEPTIONS "true"
#else
#define CANOPY_EXCEPTIONS "false"
#endif

#if defined(_WIN32)
#if !defined(_MT) || !defined(_DLL)
#error Dynamic MSVC CRT is required
#endif
#if defined(_DEBUG)
#define CANOPY_CRT "MDd"
#else
#define CANOPY_CRT "MD"
#endif
#elif defined(_LIBCPP_VERSION)
#define CANOPY_CRT "libc++"
#else
#error Bundled or Apple libc++ is required
#endif

int main()
{
    const double area = canopy::square_feet_to_square_metres(60000.0);
    if (std::printf("{\"area_square_metres\":%.12f,\"platform\":\"%s\",\"cpp_version\":%ld,\"pointer_bits\":%zu,\"rtti\":%s,\"exceptions\":%s,\"crt\":\"%s\",\"compiler_id\":\"%s\",\"compiler_version\":\"%d.%d.%d\"}\n",
                    area, CANOPY_PLATFORM, static_cast<long>(CANOPY_CPP_VERSION),
                    sizeof(void*) * 8, CANOPY_RTTI, CANOPY_EXCEPTIONS, CANOPY_CRT,
                    CANOPY_COMPILER, CANOPY_COMPILER_MAJOR, CANOPY_COMPILER_MINOR,
                    CANOPY_COMPILER_PATCH) < 0)
    {
        return 1;
    }
    return 0;
}
