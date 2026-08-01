//---------------------------------------------------------------------------
//      E L E N A   P r o j e c t:  target description
//
//      Everything the compiler needs to know about the machine it is generating
//      FOR, as opposed to the machine it is running ON.
//
//      THE RULE
//      --------
//      Target properties are never derived from host properties. No layout
//      decision may consult sizeof(void*), the host's byte order, or the host's
//      alignment. A compiler running on x86-64 Linux must produce byte-identical
//      output for --target=s390x-unknown-linux-gnu as one running on an s390x.
//
//      The 2009 code violated this everywhere -- `#define ref_t size_t` defines a
//      target concept as a host type -- which is why it could only ever generate
//      for the machine it was built on.
//
//      See experimental_version/docs/plan/17-llvm-backend-and-targets.md
//---------------------------------------------------------------------------

#ifndef targetinfoH
#define targetinfoH 1

namespace _ELENA_
{

// --- Architecture ---
enum TargetArch
{
   taUnknown = 0,
   taX86,            // i686
   taX86_64,
   taArm64,
   taPpc32,          // big endian
   taPpc64,          // big endian, ELFv1 -- function pointers are descriptors
   taPpc64le,        // little endian, ELFv2
   taS390x           // big endian
};

// --- Operating system ---
enum TargetOS
{
   toUnknown = 0,
   toLinux,
   toWindows,
   toMacOS,
   toFreestanding    // bare metal: no libc, no loader (the kernel target)
};

// --- Byte order ---
enum TargetEndian
{
   teLittle = 1,
   teBig    = 2
};

// --- Memory ordering model ---
//
// The 2015 runtime contains no barriers because it was written for x86.
// That survives on s390x, which is also strongly ordered, and breaks on ppc and
// arm64 the moment threads exist.
enum TargetMemoryModel
{
   tmStrong = 0,     // x86, s390x  -- TSO-like
   tmWeak            // arm64, ppc  -- needs explicit acquire/release
};

// --- TargetInfo ---
struct TargetInfo
{
   const char*       name;           // short name used by --target
   const char*       triple;         // LLVM target triple

   TargetArch        arch;
   TargetOS          os;
   TargetEndian      endian;
   TargetMemoryModel memoryModel;

   unsigned int      pointerBits;    // 32 | 64
   unsigned int      objectAlign;    // object alignment in bytes

   // On ppc64 ELFv1 a function pointer points at a 3-word descriptor
   // {entry, TOC, environment} rather than at code. VMT slots must therefore
   // hold typed function pointers and must never be subjected to address
   // arithmetic.
   bool              functionDescriptors;

   // Bytes per object slot -- the value that used to be the literal 4 baked
   // into every field offset and VMT index.
   unsigned int slotBytes() const { return pointerBits / 8; }

   bool isBigEndian() const { return endian == teBig; }
   bool is64Bit() const     { return pointerBits == 64; }

   // Name of the configuration file carrying this OS's platform bindings.
   //
   // The forwards that decide WHICH library module implements 'program'output
   // are configuration, not ABI, so they live in targets/<os>.cfg rather than
   // in this table. The axis is the operating system, not the architecture:
   // s390x and x86-64 Linux bind to exactly the same library modules.
   const char* osConfigName() const
   {
      switch (os) {
         case toLinux:        return "linux";
         case toWindows:      return "windows";
         case toMacOS:        return "macos";
         case toFreestanding: return "freestanding";
         default:             return NULL;
      }
   }
};

// --- known targets ---
//
// Deliberately explicit rather than computed: each row is a claim about an ABI
// that someone has to verify, and a table makes the unverified ones visible.

const TargetInfo* getTargetByName(const char* name);
const TargetInfo* getTargetList(size_t& count);

// The target matching the machine this compiler was built for. Used only as
// the default when --target is absent, never as a source of layout decisions.
const TargetInfo* getDefaultTarget();

// --- current target ---
//
// One invocation compiles for exactly one target, so this is compiler-global
// rather than threaded through every layout call site. Set once from the
// command line; read wherever a target property is needed.
void              setCurrentTarget(const TargetInfo* target);
const TargetInfo* getCurrentTarget();

} // _ELENA_

#endif // targetinfoH
