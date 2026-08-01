//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  target description
//---------------------------------------------------------------------------

#include "elena.h"
// --------------------------------------------------------------------------
#include "targetinfo.h"

#include <string.h>

using namespace _ELENA_;

// --- known targets ---------------------------------------------------------
//
// STATUS: only x86 is exercised today; it is the one the existing code
// generator emits. The rest describe ABIs that the LLVM backend will target,
// and each needs a conformance run before it can be trusted -- particularly
// struct-by-value passing on ppc64 ELFv1 and s390x.
//
// The recommended bring-up order changes ONE variable at a time:
//   x86_64  ->  arm64   (weak ordering, still little endian)
//           ->  s390x   (big endian, still strong ordering)
//           ->  ppc64 / ppc32  (both at once, plus function descriptors)

static const TargetInfo _targets[] =
{
   // name        triple                             arch      os          endian    memory    ptr  align  descr
   { "x86",      "i686-unknown-linux-gnu",           taX86,    toLinux,    teLittle, tmStrong,  32,   8,   false },
   { "x86-win",  "i686-pc-windows-msvc",             taX86,    toWindows,  teLittle, tmStrong,  32,   8,   false },

   { "x64",      "x86_64-unknown-linux-gnu",         taX86_64, toLinux,    teLittle, tmStrong,  64,  16,   false },
   { "x64-win",  "x86_64-pc-windows-msvc",           taX86_64, toWindows,  teLittle, tmStrong,  64,  16,   false },
   { "x64-mac",  "x86_64-apple-darwin",              taX86_64, toMacOS,    teLittle, tmStrong,  64,  16,   false },

   { "arm64",    "aarch64-unknown-linux-gnu",        taArm64,  toLinux,    teLittle, tmWeak,    64,  16,   false },
   { "arm64-mac","aarch64-apple-darwin",             taArm64,  toMacOS,    teLittle, tmWeak,    64,  16,   false },
   { "arm64-win","aarch64-pc-windows-msvc",          taArm64,  toWindows,  teLittle, tmWeak,    64,  16,   false },

   // Big endian from here down.
   { "ppc32",    "powerpc-unknown-linux-gnu",        taPpc32,  toLinux,    teBig,    tmWeak,    32,   8,   false },

   // ppc64 ELFv1: a function pointer is a pointer to {entry, TOC, env}, not to
   // code. VMT slots must hold typed function pointers, and nothing may do
   // address arithmetic on a method address.
   { "ppc64",    "powerpc64-unknown-linux-gnu",      taPpc64,  toLinux,    teBig,    tmWeak,    64,  16,   true  },

   // ppc64le uses ELFv2 and has no descriptors.
   { "ppc64le",  "powerpc64le-unknown-linux-gnu",    taPpc64le,toLinux,    teLittle, tmWeak,    64,  16,   false },

   { "s390x",    "s390x-unknown-linux-gnu",          taS390x,  toLinux,    teBig,    tmStrong,  64,  16,   false },
};

static const size_t _targetCount = sizeof(_targets) / sizeof(_targets[0]);

const TargetInfo* _ELENA_::getTargetByName(const char* name)
{
   if (!name)
      return NULL;

   for (size_t i = 0 ; i < _targetCount ; i++) {
      if (strcmp(_targets[i].name, name) == 0 || strcmp(_targets[i].triple, name) == 0)
         return &_targets[i];
   }

   return NULL;
}

const TargetInfo* _ELENA_::getTargetList(size_t& count)
{
   count = _targetCount;

   return _targets;
}

const TargetInfo* _ELENA_::getDefaultTarget()
{
   // The only place in the compiler where a host property legitimately decides
   // anything: picking a default when --target was not given. Everything after
   // this point reads the TargetInfo, never the host.
#if defined(_WIN32)
   #if defined(__x86_64__) || defined(_M_X64)
      return getTargetByName("x64-win");
   #else
      return getTargetByName("x86-win");
   #endif
#elif defined(__APPLE__)
   #if defined(__aarch64__)
      return getTargetByName("arm64-mac");
   #else
      return getTargetByName("x64-mac");
   #endif
#else
   #if defined(__s390x__)
      return getTargetByName("s390x");
   #elif defined(__powerpc64__)
      #if defined(__LITTLE_ENDIAN__)
         return getTargetByName("ppc64le");
      #else
         return getTargetByName("ppc64");
      #endif
   #elif defined(__powerpc__)
      return getTargetByName("ppc32");
   #elif defined(__aarch64__)
      return getTargetByName("arm64");
   #elif defined(__x86_64__)
      return getTargetByName("x64");
   #else
      return getTargetByName("x86");
   #endif
#endif
}

// --- current target ---

static const TargetInfo* _currentTarget = NULL;

void _ELENA_::setCurrentTarget(const TargetInfo* target)
{
   _currentTarget = target;
}

const TargetInfo* _ELENA_::getCurrentTarget()
{
   if (!_currentTarget)
      _currentTarget = getDefaultTarget();

   return _currentTarget;
}
