/// \file v7/src/RFile.cxx
/// \ingroup Base ROOT7
/// \author Giacomo Parolini <giacomo.parolini@cern.ch>
/// \date 2025-03-19
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!

/*************************************************************************
 * Copyright (C) 1995-2016, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "ROOT/RFile.hxx"

using ROOT::Experimental::RFile;
using ROOT::Experimental::RFileRef;
 
std::unique_ptr<RFile> RFile::OpenForReading(std::string_view path)
{
   auto tfile = std::unique_ptr<TFile>(TFile::Open(std::string(path).c_str(), "READ"));//_WITHOUT_GLOBALREGISTRATION"));
   auto rfile = std::unique_ptr<RFile>(new RFile(std::move(tfile)));
   return rfile;
}

std::unique_ptr<RFile> RFile::Recreate(std::string_view path)
{
   auto tfile = std::unique_ptr<TFile>(TFile::Open(std::string(path).c_str(), "RECREATE"));
   auto rfile = std::unique_ptr<RFile>(new RFile(std::move(tfile)));
   return rfile;
}

void *RFile::GetUntyped(const char *name, const TClass *type) const
{
   void *obj = fFile->GetObjectChecked(name, type);
   return obj;
}

void RFile::PutUntyped(const char *name, const TClass *type, void *obj)
{
   fFile->WriteObjectAny(obj, type, name);
}
