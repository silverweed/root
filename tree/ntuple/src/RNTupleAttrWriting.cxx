/// \file RNTupleAttrWriting.cxx
/// \ingroup NTuple ROOT7
/// \author Giacomo Parolini <giacomo.parolini@cern.ch>
/// \date 2026-01-27
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

#include <ROOT/RNTupleAttrWriting.hxx>
#include <ROOT/RNTupleAttrCommon.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleFillContext.hxx>
#include <ROOT/RNTupleReader.hxx>
#include <ROOT/RPageStorageFile.hxx>
#include <ROOT/StringUtils.hxx>

using namespace ROOT::Experimental::Internal::RNTupleAttributes;

static ROOT::RResult<void> ValidateAttributeModel(const ROOT::RNTupleModel &model)
{
   const auto &projFields = ROOT::Internal::GetProjectedFieldsOfModel(model);
   if (!projFields.IsEmpty())
      return R__FAIL("The Model used to create an AttributeSet cannot contain projected fields.");

   for (const auto &field : model.GetConstFieldZero()) {
      if (field.GetStructure() == ROOT::ENTupleStructure::kStreamer)
         return R__FAIL(std::string("The Model used to create an AttributeSet cannot contain Streamer field '") +
                        field.GetQualifiedFieldName() + "'");
   }
   return ROOT::RResult<void>::Success();
}

//
//  RNTupleAttrEntryPair
//
std::size_t ROOT::Experimental::Internal::RNTupleAttrEntryPair::Append()
{
   std::size_t bytesWritten = 0;
   // Write the meta entry values
   bytesWritten += fMetaEntry.fValues[kRangeStartIndex].Append();
   bytesWritten += fMetaEntry.fValues[kRangeLenIndex].Append();

   // Bind the user model's memory to the meta model's subfields
   const auto &userFields =
      ROOT::Internal::GetFieldZeroOfModel(fMetaModel).GetMutableSubfields()[kUserModelIndex]->GetMutableSubfields();
   assert(userFields.size() == fScopedEntry.fValues.size());
   for (std::size_t i = 0; i < fScopedEntry.fValues.size(); ++i) {
      std::shared_ptr<void> userPtr = fScopedEntry.fValues[i].GetPtr<void>();
      auto value = userFields[i]->BindValue(userPtr);
      bytesWritten += value.Append();
   }
   return bytesWritten;
}

//
//  RNTupleAttrSetWriter
//
std::unique_ptr<ROOT::Experimental::RNTupleAttrSetWriter>
ROOT::Experimental::RNTupleAttrSetWriter::Create(const RNTupleFillContext &mainFillContext,
                                                 std::unique_ptr<ROOT::Internal::RPageSink> sink,
                                                 std::unique_ptr<RNTupleModel> userModel)

{
   ValidateAttributeModel(*userModel).ThrowOnError();

   // We create a "meta model" that's what we'll use to write data to storage. This meta model has 3 fields:
   // the "meta fields" _rangeStart / _rangeLen and an untyped Record field which contains all the top-level fields
   // from the user model as its children. This is done to "namespace" all user-defined attribute fields so that we
   // are free to use whichever name we want for our meta fields.
   // Note that the user model is preserved as-is to allow the user to create entries from it or use its default
   // entry. When we actually write data to storage, we do some pointer trickery to correctly read the values from
   // the user model and store them under the meta model's fields (see RNTupleAttrEntryPair::Append())
   auto metaModel = RNTupleModel::Create();
   metaModel->SetDescription(userModel->GetDescription());
   auto rangeStartPtr = metaModel->MakeField<ROOT::NTupleSize_t>(kRangeStartName);
   auto rangeLenPtr = metaModel->MakeField<ROOT::NTupleSize_t>(kRangeLenName);
   std::vector<std::unique_ptr<RFieldBase>> fields;
   auto subfields = userModel->GetConstFieldZero().GetConstSubfields();
   fields.reserve(subfields.size());
   for (const auto *field : subfields) {
      fields.push_back(field->Clone(field->GetFieldName()));
   }
   auto userRootField = std::make_unique<ROOT::RRecordField>(kUserModelName, std::move(fields));
   metaModel->AddField(std::move(userRootField));

   metaModel->Freeze();
   userModel->Freeze();

   return std::unique_ptr<RNTupleAttrSetWriter>(
      new RNTupleAttrSetWriter(mainFillContext, std::move(sink), std::move(metaModel), std::move(userModel),
                               std::move(rangeStartPtr), std::move(rangeLenPtr)));
}

ROOT::Experimental::RNTupleAttrSetWriter::RNTupleAttrSetWriter(const RNTupleFillContext &mainFillContext,
                                                               std::unique_ptr<ROOT::Internal::RPageSink> sink,
                                                               std::unique_ptr<RNTupleModel> metaModel,
                                                               std::unique_ptr<RNTupleModel> userModel,
                                                               std::shared_ptr<ROOT::NTupleSize_t> rangeStartPtr,
                                                               std::shared_ptr<ROOT::NTupleSize_t> rangeLenPtr)
   : fFillContext(std::move(metaModel), std::move(sink)),
     fMainFillContext(&mainFillContext),
     fUserModel(std::move(userModel)),
     fRangeStartPtr(std::move(rangeStartPtr)),
     fRangeLenPtr(std::move(rangeLenPtr))
{
   (void)fMainFillContext;
}

ROOT::Experimental::RNTupleAttrPendingRange ROOT::Experimental::RNTupleAttrSetWriter::BeginRange()
{
   const auto start = fMainFillContext->GetNEntries();
   return RNTupleAttrPendingRange{start, fFillContext.GetModel().GetModelId()};
}

void ROOT::Experimental::RNTupleAttrSetWriter::CommitRange(ROOT::Experimental::RNTupleAttrPendingRange pendingRange,
                                                           REntry &entry)
{
   pendingRange.fWasCommitted = true;

   if (pendingRange.GetModelId() != fFillContext.GetModel().GetModelId())
      throw ROOT::RException(R__FAIL("Range passed to CommitRange() of AttributeSet '" + GetDescriptor().GetName() +
                                     "' was not created by it or was already committed."));

   // Get current entry number from the writer and use it as end of entry range
   const auto end = fMainFillContext->GetNEntries();
   auto &metaEntry = fFillContext.fModel->GetDefaultEntry();
   R__ASSERT(end >= pendingRange.GetStart());
   *fRangeStartPtr = pendingRange.GetStart();
   *fRangeLenPtr = end - pendingRange.GetStart();
   Internal::RNTupleAttrEntryPair pair{metaEntry, entry, *fFillContext.fModel};
   fFillContext.FillImpl(pair);
}

void ROOT::Experimental::RNTupleAttrSetWriter::CommitRange(ROOT::Experimental::RNTupleAttrPendingRange pendingRange)
{
   CommitRange(std::move(pendingRange), fUserModel->GetDefaultEntry());
}

ROOT::Internal::RNTupleLocatorAndLength ROOT::Experimental::RNTupleAttrSetWriter::Commit()
{
   fFillContext.FlushCluster();
   fFillContext.fSink->CommitClusterGroup();
   return fFillContext.fSink->CommitDataset();
}

//
//  RNTupleAttrSetReader
//
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
             [](const auto &a, const auto &b) { return a.first.Start() < b.first.Start(); });

   R__LOG_INFO(ROOT::Internal::NTupleLog()) << "Loaded " << fEntryRanges.size() << " attribute entries.";
}

const ROOT::RNTupleDescriptor &ROOT::Experimental::RNTupleAttrSetReader::GetDescriptor() const
{
   return fReader->GetDescriptor();
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
