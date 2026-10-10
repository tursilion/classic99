//
// pico9918-core under a pre-C11 MSVC
//
// (C) 2026 Troy Schrapel (visrealm)
//
// MSVC only gained /std:c11 in VS2019 16.8, and Classic99 builds v141_xp. The
// library's translation units use _Static_assert and _Alignof directly, and
// nothing else in it is newer than C89 plus MSVC's long-standing extensions, so
// these two macros are the whole gap. Force-included ahead of pico9918-core's
// sources only - see ForcedIncludeFiles in classic99.vcxproj.
//
// Gated on the C version, not the compiler: MSVC defines __STDC_VERSION__ only
// when /std:c11 or /std:c17 was asked for, so a toolset that has the real
// keywords uses them.
//

#ifndef PICO9918_MSVC_C89_H
#define PICO9918_MSVC_C89_H

#if defined(_MSC_VER) && !defined(__cplusplus) && \
    (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)

#define PICO9918_C89_CAT2(a, b) a##b
#define PICO9918_C89_CAT(a, b)  PICO9918_C89_CAT2(a, b)

#define _Alignof(T) __alignof(T)

// __COUNTER__, not __LINE__ - two assertions can reach one TU on the same line
#define _Static_assert(cond, msg) \
    typedef char PICO9918_C89_CAT(pico9918_assert_, __COUNTER__)[(cond) ? 1 : -1]

#endif

#endif
