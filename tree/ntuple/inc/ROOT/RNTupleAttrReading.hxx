/// \file ROOT/RNTupleAttrWriting.hxx
/// \ingroup NTuple ROOT7
/// \author Giacomo Parolini <giacomo.parolini@cern.ch>
/// \date 2026-01-27
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

#ifndef ROOT7_RNTuple_Attr_Reading
#define ROOT7_RNTuple_Attr_Reading

#include <memory>
#include <optional>
#include <vector>

#include <ROOT/RNTupleTypes.hxx>
#include <ROOT/REntry.hxx>

#include <TError.h>

namespace ROOT {

class RNTupleDescriptor;
class RNTupleModel;
class RNTupleReader;

namespace Experimental {

// clang-format off
/**
\class ROOT::Experimental::RNTupleAttrRange
\ingroup NTuple
\brief A range of entries linked to an Attribute Entry
*/
// clang-format on
class RNTupleAttrRange final {
   ROOT::NTupleSize_t fStart = 0;
   ROOT::NTupleSize_t fLength = 0;

   RNTupleAttrRange(ROOT::NTupleSize_t start, ROOT::NTupleSize_t length) : fStart(start), fLength(length) {}

public:
   static RNTupleAttrRange FromStartLength(ROOT::NTupleSize_t start, ROOT::NTupleSize_t length)
   {
      return RNTupleAttrRange{start, length};
   }

   /// Creates an AttributeRange from [start, end), where `end` is one past the last valid entry of the range
   /// (`FromStartEnd(0, 10)` will create a range whose last valid index is 9).
   static RNTupleAttrRange FromStartEnd(ROOT::NTupleSize_t start, ROOT::NTupleSize_t end)
   {
      R__ASSERT(end >= start);
      return RNTupleAttrRange{start, end - start};
   }

   RNTupleAttrRange() = default;

   /// Returns the first valid entry index in the range. Returns nullopt if the range has zero length.
   std::optional<ROOT::NTupleSize_t> GetFirst() const { return fLength ? std::make_optional(fStart) : std::nullopt; }
   /// Returns the beginning of the range. Note that this is *not* a valid index in the range if the range has zero
   /// length.
   ROOT::NTupleSize_t GetStart() const { return fStart; }
   /// Returns the last valid entry index in the range. Returns nullopt if the range has zero length.
   std::optional<ROOT::NTupleSize_t> GetLast() const
   {
      return fLength ? std::make_optional(fStart + fLength - 1) : std::nullopt;
   }
   /// Returns one past the last valid index of the range, equal to `GetStart() + GetLength()`.
   ROOT::NTupleSize_t GetEnd() const { return fStart + fLength; }
   ROOT::NTupleSize_t GetLength() const { return fLength; }

   /// Returns the pair { firstEntryIdx, lastEntryIdx } (inclusive). Returns nullopt if the range has zero length.
   std::optional<std::pair<ROOT::NTupleSize_t, ROOT::NTupleSize_t>> GetFirstLast() const
   {
      return fLength ? std::make_optional(std::make_pair(fStart, fStart + fLength - 1)) : std::nullopt;
   }
   /// Returns the pair { start, length }.
   std::pair<ROOT::NTupleSize_t, ROOT::NTupleSize_t> GetStartLength() const { return {GetStart(), GetLength()}; }
};

// clang-format off
/**
\class ROOT::Experimental::RNTupleAttrSetReader
\ingroup NTuple
\brief Class used to read a RNTupleAttrSet in the context of a RNTupleReader

An RNTupleAttrSetReader is created via RNTupleReader::OpenAttributeSet. Once created, it may outlive its parent Reader.
Reading Attributes works similarly to reading regular RNTuple entries: you can either create entries or just use the
AttrSetReader Model's default entry and load data into it via LoadAttrEntry.

~~ {.cpp}
// Reading Attributes via RNTupleAttrSetReader
// -------------------------------------------

// Assuming `reader` is a RNTupleReader:
auto attrSet = reader->OpenAttributeSet("MyAttrSet");

// Just like how you would read a regular RNTuple, first get the pointer to the fields you want to read:
auto &attrEntry = attrSet->GetModel().GetDefaultEntry();
auto pAttr = attrEntry->GetPtr<std::string>("myAttr");

// Then select which attributes you want to read. E.g. read all attributes linked to the entry at index 10:
for (auto idx : attrSet->GetAttributes(10)) {
   attrSet->LoadAttrEntry(idx);
   cout << "entry " << idx << " has attribute " << *pAttr << "\n";
}
~~
*/
// clang-format on
class RNTupleAttrSetReader final {
   friend class ROOT::RNTupleReader;

   /// List containing pairs { entryRange, entryIndex }, used to quickly find out which entries in the Attribute
   /// RNTuple contain entries that overlap a given range. The list is sorted by range start, i.e.
   /// entryRange.first.Start().
   std::vector<std::pair<RNTupleAttrRange, NTupleSize_t>> fEntryRanges;
   /// The internal Reader used to read the AttributeSet RNTuple
   std::unique_ptr<ROOT::RNTupleReader> fReader;
   /// The reconstructed user model
   std::unique_ptr<ROOT::RNTupleModel> fUserModel;

   explicit RNTupleAttrSetReader(std::unique_ptr<RNTupleReader> reader);

public:
   RNTupleAttrSetReader(const RNTupleAttrSetReader &) = delete;
   RNTupleAttrSetReader &operator=(const RNTupleAttrSetReader &) = delete;
   RNTupleAttrSetReader(RNTupleAttrSetReader &&) = default;
   RNTupleAttrSetReader &operator=(RNTupleAttrSetReader &&) = default;
   ~RNTupleAttrSetReader() = default;

   const ROOT::RNTupleDescriptor &GetDescriptor() const;
   const ROOT::RNTupleModel &GetModel() const { return *fUserModel; }

   std::unique_ptr<REntry> CreateEntry();
   RNTupleAttrRange LoadAttrEntry(NTupleSize_t index);
   RNTupleAttrRange LoadAttrEntry(NTupleSize_t index, REntry &entry);

   /// Returns the number of all attribute entries in this Attribute Set.
   std::size_t GetNAttrEntries() const { return fEntryRanges.size(); }
};

} // namespace Experimental
} // namespace ROOT

#endif
