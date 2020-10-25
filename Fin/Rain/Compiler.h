#pragma once
//
// file Compiler.h
// author Maximilien M. Cura
//

#define COMPILER(RAIN_FEATURE) (defined RAIN_COMPILER_##RAIN_FEATURE && RAIN_COMPILER_##FEATURE)
#define COMPILER_SUPPORTS(RAIN_FEATURE) (defined RAIN_COMPILER_SUPPORTS##_RAIN_FEATURE && RAIN_COMPILER_SUPPORTS_##RAIN_FEATURE)

#ifdef __has_builtin
#    define COMPILER_HAS_CLANG_BUILTIN(x) __has_builtin (x)
#else
#    define COMPILER_HAS_CLANG_BUILTIN(x) 0
#endif

#ifdef __has_feature
#    define COMPILER_HAS_CLANG_FEATURE(x) __has_feature (x)
#else
#    define COMPILER_HAS_CLANG_FEATURE(x) 0
#endif

#ifdef __has_extension
#    define COMPILER_HAS_CLANG_EXTENSION(x) __has_extension (x)
#else
#    define COMPILER_HAS_CLANG_EXTENSION(x) 0
#endif

#if defined __clang__
#    define RAIN_COMPILER_CLANG 1
#    define RAIN_COMPILER_GCC_OR_CLANG 1
#endif

#if defined __GNUC__
#    define RAIN_COMPILER_GNU_COMPATIBLE
#endif

#if !defined(ALWAYS_INLINE)
#    if COMPILER(GCC_OR_CLANG)
#        define ALWAYS_INLINE inline __attribute__ ((__always_inline__))
#    else
#        define ALWAYS_INLINE inline
#    endif
#endif

#if !defined(NEVER_INLINE)
#    if COMPILER(GCC_OR_CLANG)
#        define NEVER_INLINE __attribute__ ((__noinline__))
#    else
#        define NEVER_INLINE
#    endif
#endif

#if !defined(UNLIKELY)
#    if COMPILER(GCC_OR_CLANG)
#        define UNLIKELY(x) __builtin_expect (!!(x), (long)0)
#    else
#        define UNLIKELY(x) (x)
#    endif
#endif

#if !defined(LIKELY)
#    if COMPILER(GCC_OR_CLANG)
#        define LIKELY(x) __builtin_expect (!!(x), (long)1)
#    else
#        define LIKELY(x) (x)
#    endif
#endif

#if !defined __RAIN_DEFGUARD_VISIBILITY
#define __RAIN_DEFGUARD_VISIBILITY

#define RAIN_API __attribute__((visibility("default")))
#endif
