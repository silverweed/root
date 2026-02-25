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
