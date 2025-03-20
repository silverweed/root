/// \file ROOT/RFile.hxx
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

#ifndef ROOT7_RFile
#define ROOT7_RFile

#include <TFile.h>

#include <string_view>
#include <memory>

namespace ROOT {
namespace Experimental {

// class RFile;

// template <typename T>
// class RFileRef {
//    friend class RFile;
   
//    T *fInner = nullptr;

//    explicit RFileRef(T *inner) : fInner(inner) {}

// public:
//    bool IsValid() const { return !!fInner; }
//    operator bool() const { return IsValid(); }

//    T *Get() { return fInner; }
//    T *operator *() { return fInner; }
//    T *operator ->() { return fInner; }
// };

class RFile {
   std::unique_ptr<TFile> fFile;
   
   explicit RFile(std::unique_ptr<TFile> file) : fFile(std::move(file)) {}
   
   // NOTE: these strings are const char * because they need to be passed to TFile
   /// Gets object `name` from the file and returns an **owning** pointer to it.
   /// The caller should immediately wrap it into a unique_ptr of the type described by `type`.
   [[nodiscard]] void *GetUntyped(const char *name, const TClass *type) const;
   /// Writes `obj` to file, without taking its ownership.
   void PutUntyped(const char *name, const TClass *type, void *obj);

public: 
   ///// Factory methods /////

   /// Opens the file for reading
   static std::unique_ptr<RFile> OpenForReading(std::string_view path);

   /// Opens the file for reading/writing, overwriting it if it already exists
   static std::unique_ptr<RFile> Recreate(std::string_view path);

   /// Opens the file for updating
   static std::unique_ptr<RFile> OpenForUpdate(std::string_view path);

   ///// Instance methods /////

   // Retrieves an object from the file.
   // If the object is not there, returns an invalid ref.
   template <typename T>
   std::unique_ptr<T> Get(std::string_view name) const {
      std::string nameStr(name);
      const TClass *cls = TClass::GetClass(typeid(T));
      void *obj = GetUntyped(nameStr.c_str(), cls);
      return std::unique_ptr<T>(static_cast<T *>(obj));
   }

   // Puts an object into the file.
   // Throws a RException if the file was opened in read-only mode.
   template <typename T>
   void Put(std::string_view name, T &obj) {
      std::string nameStr(name);
      const TClass *cls = TClass::GetClass(typeid(T));
      PutUntyped(nameStr.c_str(), cls, &obj);
   }
};
   
} // namespace Experimental
} // namespace ROOT

#endif
