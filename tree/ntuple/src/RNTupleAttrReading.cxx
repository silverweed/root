#include <ROOT/RNTupleAttrReading.hxx>
#include <ROOT/RNTupleAttrCommon.hxx>

#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RNTupleModel.hxx>

using namespace ROOT::Experimental::Internal::RNTupleAttributes;

ROOT::Experimental::RNTupleAttrSetReader::RNTupleAttrSetReader(std::unique_ptr<RNTupleReader> reader)
   : fReader(std::move(reader))
{
   // Initialize user model
   fUserModel = RNTupleModel::Create();
   const auto *userFieldRoot = fReader->GetModel().GetConstFieldZero().GetConstSubfields()[kUserModelIndex];
   for (const auto *field : userFieldRoot->GetConstSubfields()) {
      fUserModel->AddField(field->Clone(field->GetFieldName()));
   }
   fUserModel->Freeze();

   // Collect all entry ranges
   auto entryRangeStartView = fReader->GetView<ROOT::NTupleSize_t>(kRangeStartName);
   auto entryRangeLenView = fReader->GetView<ROOT::NTupleSize_t>(kRangeLenName);
   fEntryRanges.reserve(fReader->GetNEntries());
   for (auto i : fReader->GetEntryRange()) {
      auto start = entryRangeStartView(i);
      auto len = entryRangeLenView(i);
      fEntryRanges.push_back({RNTupleAttrRange::FromStartLength(start, len), i});
   }

   std::sort(fEntryRanges.begin(), fEntryRanges.end(),
             [](const auto &a, const auto &b) { return a.first.GetStart() < b.first.GetStart(); });

   R__LOG_INFO(ROOT::Internal::NTupleLog()) << "Loaded " << fEntryRanges.size() << " attribute entries.";
}

const ROOT::RNTupleDescriptor &ROOT::Experimental::RNTupleAttrSetReader::GetDescriptor() const
{
   return fReader->GetDescriptor();
}

std::unique_ptr<ROOT::REntry> ROOT::Experimental::RNTupleAttrSetReader::CreateEntry()
{
   return fUserModel->CreateEntry();
}

ROOT::Experimental::RNTupleAttrRange
ROOT::Experimental::RNTupleAttrSetReader::LoadAttrEntry(ROOT::NTupleSize_t index, REntry &entry)
{
   auto &metaModel = const_cast<ROOT::RNTupleModel &>(fReader->GetModel());
   auto &metaEntry = metaModel.GetDefaultEntry();

   if (R__unlikely(entry.GetModelId() != fUserModel->GetModelId()))
      throw RException(R__FAIL("mismatch between entry and model"));

   // Load the meta fields
   metaEntry.fValues[kRangeStartIndex].Read(index);
   metaEntry.fValues[kRangeLenIndex].Read(index);

   // Load the user fields into `entry`
   auto *userRootField = ROOT::Internal::GetFieldZeroOfModel(metaModel).GetMutableSubfields()[kUserModelIndex];
   const auto userFields = userRootField->GetMutableSubfields();
   assert(entry.fValues.size() == userFields.size());
   for (std::size_t i = 0; i < userFields.size(); ++i) {
      auto *field = userFields[i];
      field->Read(index, entry.fValues[i].GetPtr<void>().get());
   }

   auto pStart = metaEntry.GetPtr<NTupleSize_t>(kRangeStartName);
   auto pLen = metaEntry.GetPtr<NTupleSize_t>(kRangeLenName);

   return RNTupleAttrRange::FromStartLength(*pStart, *pLen);
}

ROOT::Experimental::RNTupleAttrRange ROOT::Experimental::RNTupleAttrSetReader::LoadAttrEntry(ROOT::NTupleSize_t index)
{
   auto &entry = fUserModel->GetDefaultEntry();
   return LoadAttrEntry(index, entry);
}


// Entry ranges should be sorted with respect to GetStart by construction.
// TODO: make sure merging attributes preserves the sorting.
bool ROOT::Experimental::RNTupleAttrSetReader::EntryRangesAreSorted(const decltype(fEntryRanges) &ranges)
{
   ROOT::NTupleSize_t prevStart = 0;
   for (const auto &[range, _] : ranges) {
      if (range.GetStart() < prevStart)
         return false;
      prevStart = range.GetStart();
   }
   return true;
};

std::vector<ROOT::NTupleSize_t>
ROOT::Experimental::RNTupleAttrSetReader::GetAttributesRangeInternal(NTupleSize_t startEntry, NTupleSize_t endEntry,
                                                                     bool rangeIsContained)
{
   std::vector<ROOT::NTupleSize_t> result;

   if (endEntry < startEntry) {
      R__LOG_WARNING(ROOT::Internal::NTupleLog())
         << "end < start when getting attributes from Attribute Set '" << GetDescriptor().GetName()
         << "' (range given: [" << startEntry << ", " << endEntry << "].";
      return result;
   }

   assert(EntryRangesAreSorted(fEntryRanges));

   const auto FullyContained = [rangeIsContained](auto startInner, auto endInner, auto startOuter, auto endOuter) {
      if (rangeIsContained) {
         std::swap(startOuter, startInner);
         std::swap(endOuter, endInner);
      }
      return startOuter <= startInner && endInner <= endOuter;
   };

   // TODO: consider using binary search, since fEntryRanges is sorted
   // (maybe it should be done only if the size of the list is bigger than a threshold).
   for (const auto &[range, index] : fEntryRanges) {
      const auto &firstLast = range.GetFirstLast();
      if (!firstLast)
         continue;

      const auto &[first, last] = *firstLast;
      if (first >= endEntry)
         break; // We can break here because fEntryRanges is sorted.

      if (FullyContained(startEntry, endEntry, first, last + 1)) {
         result.push_back(index);
      }
   }

   return result;
}

ROOT::Experimental::RNTupleAttrEntryIterable
ROOT::Experimental::RNTupleAttrSetReader::GetAttributesContainingRange(NTupleSize_t startEntry, NTupleSize_t endEntry)
{
   RNTupleAttrRange range;
   if (endEntry <= startEntry) {
      R__LOG_WARNING(ROOT::Internal::NTupleLog())
         << "empty range given when getting attributes from Attribute Set '" << GetDescriptor().GetName()
         << "' (range given: [" << startEntry << ", " << endEntry << ")).";
      // Make sure we find 0 entries
      range = RNTupleAttrRange::FromStartLength(startEntry, 0);
   } else {
      range = RNTupleAttrRange::FromStartEnd(startEntry, endEntry);
   }
   RNTupleAttrEntryIterable::RFilter filter{range, false};
   return RNTupleAttrEntryIterable{*this, filter};
}

ROOT::Experimental::RNTupleAttrEntryIterable
ROOT::Experimental::RNTupleAttrSetReader::GetAttributesInRange(NTupleSize_t startEntry, NTupleSize_t endEntry)
{
   RNTupleAttrRange range;
   if (endEntry <= startEntry) {
      R__LOG_WARNING(ROOT::Internal::NTupleLog())
         << "empty range given when getting attributes from Attribute Set '" << GetDescriptor().GetName()
         << "' (range given: [" << startEntry << ", " << endEntry << ")).";
      // Make sure we find 0 entries
      range = RNTupleAttrRange::FromStartLength(startEntry, 0);
   } else {
      range = RNTupleAttrRange::FromStartEnd(startEntry, endEntry);
   }
   RNTupleAttrEntryIterable::RFilter filter{range, true};
   return RNTupleAttrEntryIterable{*this, filter};
}

ROOT::Experimental::RNTupleAttrEntryIterable
ROOT::Experimental::RNTupleAttrSetReader::GetAttributes(NTupleSize_t entryIndex)
{
   RNTupleAttrEntryIterable::RFilter filter{RNTupleAttrRange::FromStartEnd(entryIndex, entryIndex + 1), false};
   return RNTupleAttrEntryIterable{*this, filter};
}

ROOT::Experimental::RNTupleAttrEntryIterable ROOT::Experimental::RNTupleAttrSetReader::GetAttributes()
{
   return RNTupleAttrEntryIterable{*this};
}

//
//  RNTupleAttrEntryIterable
//
bool ROOT::Experimental::RNTupleAttrEntryIterable::RIterator::FullyContained(RNTupleAttrRange range) const
{
   assert(fFilter);
   if (fFilter->fIsContained) {
      return fFilter->fRange.GetStart() <= range.GetStart() && range.GetEnd() <= fFilter->fRange.GetEnd();
   } else {
      return range.GetStart() <= fFilter->fRange.GetStart() && fFilter->fRange.GetEnd() <= range.GetEnd();
   }
}

ROOT::Experimental::RNTupleAttrEntryIterable::RIterator::Iter_t
ROOT::Experimental::RNTupleAttrEntryIterable::RIterator::Next() const
{
   // TODO: consider using binary search, since fEntryRanges is sorted
   // (maybe it should be done only if the size of the list is bigger than a threshold).
   for (auto it = fCur; it != fEnd; ++it) {
      const auto &[range, index] = *it;
      // If we have no filter, every entry is valid.
      if (!fFilter)
         return it;

      const auto &firstLast = range.GetFirstLast();
      // If this is nullopt it means this is a zero-length entry: we always skip those except
      // for the "catch-all" GetAttributes() (which is when fFilter is also nullopt).
      if (!firstLast)
         continue;

      const auto &[first, last] = *firstLast;
      if (first >= fFilter->fRange.GetEnd()) {
         // Since fEntryRanges is sorted we know we are at the end of the iteration
         // TODO: tweak fEnd to directly pass the last entry?
         return fEnd;
      }

      if (FullyContained(RNTupleAttrRange::FromStartEnd(first, last + 1)))
         return it;
   }
   return fEnd;
}
