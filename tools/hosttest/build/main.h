// Host test stub for samp/main.h: keeps the platform pieces out of the way so
// the real Localization translation table can be compiled and exercised on a
// normal desktop compiler.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

extern uintptr_t g_libGTASA;
extern char* g_pszStorage;

static const bool VER_x32 = false;

void FLog(const char* fmt, ...);

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof(*(a)))
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = NULL; } }
#endif
