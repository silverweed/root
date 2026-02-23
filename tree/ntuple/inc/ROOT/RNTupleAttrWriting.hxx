/// \file ROOT/RNTupleAttrWriting.hxx
/// \ingroup NTuple ROOT7
/// \author Giacomo Parolini <giacomo.parolini@cern.ch>
/// \date 2026-01-27
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

#ifndef ROOT7_RNTuple_Attr_Writing
#define ROOT7_RNTuple_Attr_Writing

#include <memory>
#include <string_view>

#include <ROOT/REntry.hxx>
#include <ROOT/RNTupleFillContext.hxx>
#include <ROOT/RNTupleUtils.hxx>

namespace ROOT {

class RNTupleModel;
class RNTuple;
class RNTupleWriter;

namespace Experimental {

class RNTupleAttrSetWriter;

namespace Internal {

// clang-format off
/**
\class ROOT::Experimental::Internal::RNTupleAttrEntryPair
\ingroup NTuple
\brief A pair of scoped + meta entry used by the RNTupleAttrSetWriter.

The meta entry is used to write the "meta fields" that are always present in an Attribute Set (start/len); the scoped
entry is used to write the user-provided fields.
*/
// clang-format on
struct RNTupleAttrEntryPair {
   REntry &fMetaEntry;
   REntry &fScopedEntry;
   ROOT::RNTupleModel &fMetaModel;

   std::size_t Append();
   ROOT::DescriptorId_t GetModelId() const { return fMetaEntry.GetModelId(); }
};

} // namespace Internal

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
\class ROOT::Experimental::RNTupleAttrPendingRange
\ingroup NTuple
\brief A not-yet-finalized Attribute Range used for writing

A range used for writing. It has a well-defined start but not a length/end yet.
It is artificially made non-copyable in order to clarify the semantics of Begin/CommitRange.
For the same reason, it can only be created by the AttrSetWriter.
*/
// clang-format on
class RNTupleAttrPendingRange final {
   friend class ROOT::Experimental::RNTupleAttrSetWriter;

   ROOT::NTupleSize_t fStart = 0;
   ROOT::DescriptorId_t fModelId = kInvalidDescriptorId;
   bool fWasCommitted = false;

   explicit RNTupleAttrPendingRange(ROOT::NTupleSize_t start, ROOT::DescriptorId_t modelId)
      : fStart(start), fModelId(modelId)
   {
   }

public:
   RNTupleAttrPendingRange() = default;
   RNTupleAttrPendingRange(const RNTupleAttrPendingRange &) = delete;
   RNTupleAttrPendingRange &operator=(const RNTupleAttrPendingRange &) = delete;

   RNTupleAttrPendingRange(RNTupleAttrPendingRange &&other) { *this = std::move(other); }

   // NOTE: explicitly implemented to make sure that 'other' gets invalidated upon move.
   RNTupleAttrPendingRange &operator=(RNTupleAttrPendingRange &&other)
   {
      if (&other != this) {
         std::swap(fStart, other.fStart);
         std::swap(fModelId, other.fModelId);
         other.fWasCommitted = true;
      }
      return *this;
   }

   ~RNTupleAttrPendingRange()
   {
      if (R__unlikely(!fWasCommitted))
         R__LOG_WARNING(ROOT::Internal::NTupleLog()) << "A pending attribute range was not committed! If CommitRange() "
                                                        "is not explicitly called before closing the main "
                                                        "Writer, the attributes will not be saved to storage!";
   }

   ROOT::NTupleSize_t GetStart() const
   {
      if (!IsValid())
         throw ROOT::RException(R__FAIL("Tried to get the start of an invalid AttrPendingRange."));
      return fStart;
   }

   ROOT::DescriptorId_t GetModelId() const { return fModelId; }

   /// Returns true if this PendingRange is valid
   operator bool() const { return IsValid(); }
   bool IsValid() const { return fModelId != kInvalidDescriptorId; }
};

// clang-format off
/**
\class ROOT::Experimental::RNTupleAttrSetWriter
\ingroup NTuple
\brief Class used to write an RNTupleAttrSet in the context of an RNTupleWriter.

An Attribute Set is written as a separate RNTuple linked to the "main" RNTuple that created it.
A RNTupleAttrSetWriter only lives as long as the RNTupleWriter that created it (or until CloseAttributeSet() is called).
Users should not use this class directly but rather via RNTupleAttrSetWriterHandle, which is the type returned by
RNTupleWriter::CreateAttributeSet().

~~~ {.cpp}
// Writing attributes via RNTupleAttrSetWriter
// -------------------------------------------

// First define the schema of your Attribute Set:
auto attrModel = ROOT::RNTupleModel::Create();
auto pMyAttr = attrModel->MakeField<std::string>("myAttr");

// Then, assuming `writer` is an RNTupleWriter, create it:
auto attrSet = writer->CreateAttributeSet(std::move(attrModel), "MyAttrSet");

// Attributes are assigned to entry ranges. A range is started via BeginRange():
auto range = attrSet->BeginRange();

// To assign actual attributes, you use the same interface as the main RNTuple:
*pMyAttr = "This is my attribute for this range";

// ... here you can fill your main RNTuple with data ...

// Once you're done, close the range. This will commit the attribute data and bind it to all data written
// between BeginRange() and CommitRange().
attrSet->CommitRange(std::move(range));

// You don't need to explicitly close the AttributeSet, but if you want to do so, use:
// writer->CloseAttributeSet(std::move(attrSet));
~~~
*/
// clang-format on
class RNTupleAttrSetWriter final {
   friend class ROOT::RNTupleWriter;

   /// Our own fill context.
   RNTupleFillContext fFillContext;
   /// Fill context of the main RNTuple being written (i.e. the RNTuple whose attributes we are).
   const RNTupleFillContext *fMainFillContext = nullptr;
   /// The model that the user provided on creation. Used to create user-visible entries.
   std::unique_ptr<RNTupleModel> fUserModel;

   // Cached values of the meta entry pointers.
   std::shared_ptr<ROOT::NTupleSize_t> fRangeStartPtr;
   std::shared_ptr<ROOT::NTupleSize_t> fRangeLenPtr;

   /// Creates an RNTupleAttrSetWriter associated to the RNTupleWriter owning `mainFillContext` and writing
   /// using `sink`. `userModel` is the schema of the AttributeSet.
   static std::unique_ptr<RNTupleAttrSetWriter> Create(const RNTupleFillContext &mainFillContext,
                                                       std::unique_ptr<ROOT::Internal::RPageSink> sink,
                                                       std::unique_ptr<RNTupleModel> userModel);

   RNTupleAttrSetWriter(const RNTupleFillContext &mainFillContext, std::unique_ptr<ROOT::Internal::RPageSink> sink,
                        std::unique_ptr<RNTupleModel> metaModel, std::unique_ptr<RNTupleModel> userModel,
                        std::shared_ptr<ROOT::NTupleSize_t> rangeStartPtr,
                        std::shared_ptr<ROOT::NTupleSize_t> rangeLenPtr);

public:
   /// Returns the descriptor of the underlying attribute RNTuple. This is **NOT** the same descriptor as the
   /// main RNTuple being written!
   const ROOT::RNTupleDescriptor &GetDescriptor() const { return fFillContext.fSink->GetDescriptor(); }
   /// Returns the user-defined model used to create this attribute set.
   const ROOT::RNTupleModel &GetModel() const { return *fUserModel; }

   /// Begins an attribute range. All entries filled in the main RNTupleWriter between BeginRange and CommitRange
   /// will be associated with the set of values of the fields of this attribute set at the moment of CommitRange.
   /// Note that every attribute range must be explicitly committed for it to be stored on disk.
   /// \return An object describing the pending range, which must be passed back to CommitRange to end the attribute
   /// range
   [[nodiscard]] RNTupleAttrPendingRange BeginRange();
   /// Ends an attribute range and associates the current values of the fields of the attribute model's default entry
   /// with all the main RNTuple entries filled since the BeginRange that created the given `range`.
   /// This is only valid if the model used to create this attribute set is not bare.
   void CommitRange(RNTupleAttrPendingRange range);
   /// Like CommitRange(RNTupleAttrPendingRange range) but uses the given entry rather than the default entry.
   /// The given entry must have been created by CreateEntry().
   void CommitRange(RNTupleAttrPendingRange range, REntry &entry);

   /// Creates an REntry fit to pass to CommitRange(RNTupleAttrPendingRange range, REntry entry).
   std::unique_ptr<REntry> CreateEntry() { return fUserModel->CreateEntry(); }

   /// Commits the attributes written so far to disk and disables writing any new ones.
   ROOT::Internal::RNTupleLocatorAndLength Commit();
};

// clang-format off
/**
\class ROOT::Experimental::RNTupleAttrSetWriterHandle
\ingroup NTuple
\brief Non-owning handle to an RNTupleAttrSetWriter

RNTupleAttrSetWriter can only be used through an RNTupleAttrSetWriterHandle, a weak_ptr-like object that allows safe
access to it. The lifetime of an attribute set writer is tied to its parent RNTupleWriter, so the handle handed out
by RNTupleWriter::CreateAttributeSet is invalidated as soon as the parent writer is destructed.

*/
// clang-format on
class RNTupleAttrSetWriterHandle final {
   friend class ROOT::RNTupleWriter;

   std::weak_ptr<RNTupleAttrSetWriter> fWriter;

   explicit RNTupleAttrSetWriterHandle(const std::shared_ptr<RNTupleAttrSetWriter> &range) : fWriter(range) {}

public:
   RNTupleAttrSetWriterHandle(const RNTupleAttrSetWriterHandle &) = delete;
   RNTupleAttrSetWriterHandle &operator=(const RNTupleAttrSetWriterHandle &) = delete;
   RNTupleAttrSetWriterHandle(RNTupleAttrSetWriterHandle &&) = default;
   RNTupleAttrSetWriterHandle &operator=(RNTupleAttrSetWriterHandle &&other) = default;

   /// Retrieves the underlying pointer to the AttrSetWriter, throwing if it's invalid.
   /// This is NOT thread-safe and must be called from the same thread that created the AttrSetWriter.
   RNTupleAttrSetWriter *operator->()
   {
      auto ptr = fWriter.lock();
      if (R__unlikely(!ptr))
         throw ROOT::RException(R__FAIL("Tried to access invalid RNTupleAttrSetWriterHandle"));
      return ptr.get();
   }
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
   std::unique_ptr<RNTupleReader> fReader;
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

   std::unique_ptr<REntry> CreateAttrEntry() { return fUserModel->CreateEntry(); }
   RNTupleAttrRange LoadAttrEntry(NTupleSize_t index);
   RNTupleAttrRange LoadAttrEntry(NTupleSize_t index, REntry &entry);

   /// Returns the number of all attribute entries in this Attribute Set.
   std::size_t GetNAttrEntries() const { return fEntryRanges.size(); }
};

} // namespace Experimental
} // namespace ROOT

#endif
