// Host-compat shim for the SA-MP Mobile C++ tree.
// The real build runs under the Android NDK (arm), where:
//   * __cdecl / __stdcall are calling-convention keywords,
//   * libc++ headers expose size_t/uint8_t transitively.
// On an x86-64 Linux host with libstdc++ neither is true, so we normalize.
#pragma once

#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
