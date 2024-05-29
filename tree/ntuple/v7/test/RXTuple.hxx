#ifndef ROOT7_RNTuple_Test_RXTuple
#define ROOT7_RNTuple_Test_RXTuple

#include <cstdint>
#include "Rtypes.h"

// NOTE: This namespace must be the same as RNTuple!
namespace ROOT::Experimental {
  
// A mock of the RNTuple class, used to write a "future version" of RNTuple to a file.
// The idea is:
//   1. we write a "RXTuple" to a file, with a schema identical to RNTuple + some
//      hypothetical future fields
//   2. we read back the file and patch the name to surreptitiously transmute the on-disk
//      binary to a stored RNTuple with some additional fields
//   3. we read back the patched file to ensure the current version of RNTuple can handle
//      schema evolution in a fwd-compatible way.
// For ease of patching, the name of this struct has the same length as that of RNTuple.
struct RXTuple final {
   std::uint16_t fVersionEpoch = 9;
   std::uint16_t fVersionMajor = 9;
   std::uint16_t fVersionMinor = 9;
   std::uint16_t fVersionPatch = 9;
   // NOTE: these values are arbitrary and here to provide easily recognizable patterns
   // in the binary file.
   std::uint64_t fSeekHeader = 0x42;
   std::uint64_t fNBytesHeader = 0x99;
   std::uint64_t fLenHeader = 0x99;
   std::uint64_t fSeekFooter = 0x34;
   std::uint64_t fNBytesFooter = 0x66;
   std::uint64_t fLenFooter = 0x66;

   // Put an unreasonably high class version (but not too high! Putting 9999 would probably cause 
   // trouble due to its special meaning)
   ClassDefInline(RXTuple, 999);
};

}

#endif
