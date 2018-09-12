#pragma once

#include <stdint.h>

// clang-format off

// Auto-detect platform

#if    !defined(RUSH_PLATFORM_WINDOWS) \
	&& !defined(RUSH_PLATFORM_LINUX) \
	&& !defined(RUSH_PLATFORM_MAC) \
	&& !defined(RUSH_PLATFORM_IOS) \
	&& !defined(RUSH_PLATFORM_RPI) \
	&& !defined(RUSH_PLATFORM_EMSCRIPTEN) \
	&& !defined(RUSH_PLATFORM_EXTERNAL)
#   if defined(_WIN32)
#       define RUSH_PLATFORM_WINDOWS
#   endif
#   if defined(__linux__)
#       define RUSH_PLATFORM_LINUX
#   endif
#   if defined(__APPLE__)
#       if defined(__MACH__)
#           define RUSH_PLATFORM_MAC
#       else
#           define RUSH_PLATFORM_IOS
#       endif
#   endif
#endif

// Common macros

#if !defined(NDEBUG) && !defined(RUSH_DEBUG)
#	define RUSH_DEBUG
#endif

#define RUSH_RESTRICT __restrict
#define RUSH_COUNTOF(x) (sizeof(x)/sizeof(x[0]))
#define RUSH_UNUSED( v ) { (void)sizeof(v); }
#define RUSH_DISALLOW_COPY_AND_ASSIGN(T) T(const T&) = delete; void operator=(const T&) = delete;
#define RUSH_IMPLEMENT_FLAG_OPERATORS(T, S)                                                                            \
	inline T    operator^(T a, T b) { return (T)(static_cast<S>(a) ^ static_cast<S>(b)); }                             \
	inline T    operator&(T a, T b) { return (T)(static_cast<S>(a) & static_cast<S>(b)); }                             \
	inline T    operator|(T a, T b) { return (T)(static_cast<S>(a) | static_cast<S>(b)); }                             \
	inline bool operator!(T a) { return !static_cast<S>(a); }

#if defined(_MSC_VER)
#   define RUSH_FORCEINLINE __forceinline
#   define RUSH_NOINLINE __declspec(noinline)
#   define RUSH_BREAK {__debugbreak();}
#   define RUSH_FUNCTION __FUNCTION__
#   define RUSH_DEPRECATED __declspec(deprecated)
#   define RUSH_DEPRECATED_MSG(text) __declspec(deprecated(text))
#   define RUSH_DISABLE_OPTIMIZATION __pragma(optimize("", off))
#   define RUSH_ENABLE_OPTIMIZATION __pragma(optimize("", on))
#elif defined(__GNUC__)
#   define RUSH_FORCEINLINE __attribute__((always_inline))
#   define RUSH_NOINLINE __attribute__((noinline))
#   define RUSH_BREAK {__builtin_trap();}
#   define RUSH_FUNCTION
#   define RUSH_DEPRECATED
#   define RUSH_DEPRECATED_MSG(text)
#   define RUSH_DISABLE_OPTIMIZATION
#   define RUSH_ENABLE_OPTIMIZATION
#endif

// Common types

namespace Rush
{

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;

typedef int8_t		s8;
typedef int16_t		s16;
typedef int32_t		s32;
typedef int64_t		s64;

}

#ifdef RUSH_PLATFORM_EXTERNAL
#include <RushExternal.h>
#endif //RUSH_PLATFORM_EXTERNAL

#ifdef RUSH_USING_NAMESPACE
using namespace Rush;
#endif //RUSH_USING_NAMESPACE
