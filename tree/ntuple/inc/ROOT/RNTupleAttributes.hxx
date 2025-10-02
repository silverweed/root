/// \file ROOT/RNTupleAttributes.hxx
/// \ingroup NTuple ROOT7
/// \author Giacomo Parolini <giacomo.parolini@cern.ch>
/// \date 2025-02-25
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

#ifndef ROOT7_RNTuple_Attributes
#define ROOT7_RNTuple_Attributes

#include <memory>
#include <string_view>

#include <ROOT/REntry.hxx>
#include <ROOT/RNTupleFillContext.hxx>
#include <ROOT/RNTupleUtils.hxx>

namespace ROOT {

class RNTupleModel;
class RNTuple;

namespace Experimental {

class RNTupleAttrSetWriter;

namespace Internal {

/**
\class ROOT::Experimental::Internal::RNTupleAttrEntryPair
\ingroup NTuple
\brief A wrapper class that properly Appends values to the "meta entry" and the "scoped entry".

The "meta entry" is a REntry referring to the "meta model", i.e. the internal RNTupleModel used by the
RNTupleAttrSetWriter which includes both the user-defined attribute set model and the internal meta fields (such as the
entry range start and len). Since we want to keep the user-defined fields valid when they pass the model to create the
attribute set, we need to do some rewiring to make sure that we write values in the proper places: the values of meta
fields are appended through the meta entry, while the values of user-defined fields are written through fUserValues,
which are bound to the original user-created pointers.
*/
class RNTupleAttrEntryPair {
   REntry &fMetaEntry;
   std::vector<RFieldBase::RValue> fUserValues;

public:
   RNTupleAttrEntryPair(REntry &metaEntry, REntry &scopedEntry, ROOT::RNTupleModel &metaModel);

   std::size_t Append();
   ROOT::DescriptorId_t GetModelId() const { return fMetaEntry.GetModelId(); }
};

namespace RNTupleAttributes {

const char *const kRangeStartName = "_rangeStart";
const char *const kRangeLenName = "_rangeLen";
const char *const kUserModelName = "_userModel";

constexpr NTupleSize_t kRangeStartFieldIdx = 0;
constexpr NTupleSize_t kRangeLenFieldIdx = 1;
constexpr NTupleSize_t kUserModelFieldIdx = 2;

} // namespace RNTupleAttributes

} // namespace Internal

/**
\class ROOT::Experimental::RNTupleAttrRange
\ingroup NTuple
\brief An entry range linked to an Attribute
*/
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
   std::optional<ROOT::NTupleSize_t> First() const { return fLength ? std::make_optional(fStart) : std::nullopt; }
   /// Returns the start of the range. Note that this is *not* a valid index in the range if the range has zero length.
   ROOT::NTupleSize_t Start() const { return fStart; }
   /// Returns the last valid entry index in the range. Returns nullopt if the range has zero length.
   std::optional<ROOT::NTupleSize_t> Last() const
   {
      return fLength ? std::make_optional(fStart + fLength - 1) : std::nullopt;
   }
   /// Returns one past the last valid index of the range, equal to `Start() + Length()`.
   ROOT::NTupleSize_t End() const { return fStart + fLength; }
   ROOT::NTupleSize_t Length() const { return fLength; }

   /// Returns the pair { firstEntryIdx, lastEntryIdx } (inclusive). Returns nullopt if the range has zero length.
   std::optional<std::pair<ROOT::NTupleSize_t, ROOT::NTupleSize_t>> GetFirstLast() const
   {
      return fLength ? std::make_optional(std::make_pair(fStart, fStart + fLength - 1)) : std::nullopt;
   }
   /// Returns the pair { start, length }.
   std::pair<ROOT::NTupleSize_t, ROOT::NTupleSize_t> GetStartLength() const { return {Start(), Length()}; }
};

/**
\class ROOT::Experimental::RNTupleAttrPendingRange
\ingroup NTuple
\brief An attribute range used for writing. It has a well-defined start but not a length/end yet.

It is artificially made non-copyable in order to clarify the semantics of Begin/CommitRange.
For the same reason, it can only be created by the AttrSetWriter.
*/
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
      std::swap(fStart, other.fStart);
      std::swap(fModelId, other.fModelId);
      other.fWasCommitted = true;
      return *this;
   }

   ~RNTupleAttrPendingRange()
   {
      if (R__unlikely(!fWasCommitted))
         R__LOG_WARNING(ROOT::Internal::NTupleLog()) << "A pending attribute range was not committed! If CommitRange() "
                                                        "is not explicitly called before closing the main "
                                                        "Writer, the attributes will not be saved to storage!";
   }

   ROOT::NTupleSize_t Start() const
   {
      if (fModelId == kInvalidDescriptorId)
         throw ROOT::RException(R__FAIL("Tried to get the start of an invalid AttrPendingRange."));
      return fStart;
   }

   ROOT::DescriptorId_t GetModelId() const { return fModelId; }

   /// Returns true if this PendingRange is valid
   operator bool() const { return IsValid(); }
   bool IsValid() const { return fModelId != kInvalidDescriptorId; }
};

/**
\class ROOT::Experimental::RNTupleAttrSetWriter
\ingroup NTuple
\brief Class used to write a RNTupleAttrSet in the context of a RNTupleWriter.

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
class RNTupleAttrSetWriter final {
   friend class ROOT::RNTupleFillContext;

   /// Our own fill context.
   RNTupleFillContext fFillContext;
   /// Fill context of the main RNTuple being written (i.e. the RNTuple whose attributes we are).
   const RNTupleFillContext *fMainFillContext = nullptr;
   /// The model that the user provided on creation. Used to create user-visible entries.
   std::unique_ptr<RNTupleModel> fUserModel;

   /// Creates a RNTupleAttrSetWriter associated to the RNTupleWriter owning `mainFillContext` and writing
   /// in `dir`. `model` is the schema of the attribute set.
   static std::unique_ptr<RNTupleAttrSetWriter> Create(std::string_view name, std::unique_ptr<RNTupleModel> model,
                                                       const RNTupleFillContext &mainFillContext, TDirectory &dir);

   RNTupleAttrSetWriter(const RNTupleFillContext &mainFillContext, RNTupleFillContext fillContext,
                        std::unique_ptr<RNTupleModel> userModel);

   /// Flushes any remaining open range and writes the attribute RNTuple to storage.
   void Commit();

public:
   RNTupleAttrSetWriter(const RNTupleAttrSetWriter &) = delete;
   RNTupleAttrSetWriter &operator=(const RNTupleAttrSetWriter &) = delete;
   RNTupleAttrSetWriter(RNTupleAttrSetWriter &&) = default;
   RNTupleAttrSetWriter &operator=(RNTupleAttrSetWriter &&) = default;
   ~RNTupleAttrSetWriter() = default;

   /// Retrieves the descriptor of the underlying attribute set RNTuple
   const ROOT::RNTupleDescriptor &GetDescriptor() const { return fFillContext.fSink->GetDescriptor(); }

   /// Retrieves the model used to create this RNTupleAttrSetWriter
   const ROOT::RNTupleModel &GetModel() const { return *fUserModel; }

   /// Opens an attribute range, making sure that all future Fills in the main writer will be associated to the set
   /// of attribute values at the moment of CommitRange(). The returned RNTupleAttrPendingRange should be treated as
   /// a transaction token that uniquely identifies the opened range and needs to be relinquished upon commit.
   /// Note that the range committing will not happen automatically but needs to be explicitly invoked, otherwise the
   /// attributes will not be written to the RNTuple.
   [[nodiscard]] RNTupleAttrPendingRange BeginRange();
   /// Like CommitRange(RNTupleAttrPendingRange range, REntry &entry) but uses the default entry.
   void CommitRange(RNTupleAttrPendingRange range);
   /// Closes an attribute range and commits the attributes' value contained in `entry` to the RNTuple. `entry` must
   /// have been created by this RNTupleAttrSetWriter.
   /// The values are not necessarily written immediately to the underlying storage.
   void CommitRange(RNTupleAttrPendingRange range, REntry &entry);

   /// Creates a REntry linked to the model that was used to create this RNTupleAttrSetWriter.
   /// This entry is suited for setting the attribute values between a BeginRange() and CommitRange().
   /// If attributes are set through an explicit entry (as opposed to using the default entry), the overload of
   /// CommitRange taking a REntry as the second argument must be used to close the attribute range.
   ///
   /// Example usage:
   /// ~~~{.cpp}
   /// // attrEntry may be created before or after BeginRange():
   /// auto attrEntry = attrSet->CreateAttrEntry();
   /// auto attrRange = attrSet->BeginRange();
   /// auto pMyAttr = attrEntry->GetPtr<int>("myAttr");
   /// // The following line may happen at any moment before CommitRange():
   /// *pMyAttr = 42;
   ///
   /// // ...some code filling some regular entries...
   ///
   /// attrSet->CommitRange(std::move(attrRange), *attrEntry);
   /// ~~~
   ///
   /// Note that this entry will remain valid after CommitRange() and can be reused (same goes for all pointers
   /// fetched from it).
   std::unique_ptr<REntry> CreateAttrEntry() { return fUserModel->CreateEntry(); }
};

/**
\class ROOT::Experimental::RNTupleAttrSetWriterHandle
\ingroup NTuple
\brief A non-owning reference to an RNTupleAttrSetWriter.

The RNTupleAttrSetWriters are owned by their parent RNTupleFillContext and can only be accessed by the user
during the Context's lifetime.
This class ensures that invalid accesses do not result in undefined behavior but rather in an exception.
*/
class RNTupleAttrSetWriterHandle final {
   std::weak_ptr<RNTupleAttrSetWriter> fWriter;

   explicit RNTupleAttrSetWriterHandle(const std::shared_ptr<RNTupleAttrSetWriter> &range) : fWriter(range) {}

public:
   RNTupleAttrSetWriterHandle(const RNTupleAttrSetWriterHandle &) = delete;
   RNTupleAttrSetWriterHandle &operator=(const RNTupleAttrSetWriterHandle &) = delete;
   RNTupleAttrSetWriterHandle(RNTupleAttrSetWriterHandle &&) = default;
   RNTupleAttrSetWriterHandle &operator=(RNTupleAttrSetWriterHandle &&other) = default;

   /// Retrieves the underlying pointer to the AttrSetWriter, throwing if it's invalid.
   /// This is NOT thread-safe and must be called from the same thread that created the RNTupleAttrSetWriter.
   RNTupleAttrSetWriter *operator->()
   {
      if (R__unlikely(fWriter.expired()))
         throw ROOT::RException(R__FAIL("Tried to access invalid RNTupleAttrSetWriterHandle"));
      return fWriter.lock().get();
   }
};

} // namespace Experimental
} // namespace ROOT

#endif
