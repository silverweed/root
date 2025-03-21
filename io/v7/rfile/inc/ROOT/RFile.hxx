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

#include <ROOT/RError.hxx>
#include <string_view>
#include <memory>

namespace ROOT {
namespace Experimental {

class RFile;

namespace Internal {
struct RFileProxy {
   const RFile *fFile;
   RFileProxy(const RFile *file) : fFile(file) {}
};
} // namespace Internal

template <typename T>
class RFileRef {
   friend class RFile;

   std::weak_ptr<const Internal::RFileProxy> fParentFile;
   std::weak_ptr<T> fInner;
   std::string fName;

   explicit RFileRef(std::weak_ptr<const Internal::RFileProxy> parentFile, std::string_view name,
                     const std::shared_ptr<T> &inner = nullptr)
      : fParentFile(parentFile), fInner(inner), fName(name)
   {
   }

public:
   operator bool() const { return !fInner.expired() && !!fInner.lock(); }

   T *Get() { return fInner.expired() ? nullptr : fInner.lock().get(); }
   T *operator*() { return fInner.lock().get(); }
   T *operator->() { return fInner.lock().get(); }

   std::unique_ptr<T> Clone() const;
};

class RFile : std::enable_shared_from_this<RFile> {
public: // XXX: should not be public, but the dictionary fails to compile..
   struct RFileEntry {
      const TClass *fClass;
      std::shared_ptr<void> fData;
   };

private:
   std::shared_ptr<const Internal::RFileProxy> fSelf;
   std::unique_ptr<TFile> fFile;
   mutable std::unordered_map<std::string, RFileEntry> fCache;

   explicit RFile(std::unique_ptr<TFile> file)
      : fSelf(std::make_shared<Internal::RFileProxy>(this)), fFile(std::move(file))
   {
   }

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
   RFileRef<T> Get(std::string_view name) const
   {
      std::string nameStr(name);
      const TClass *cls = TClass::GetClass(typeid(T));
      if (auto it = fCache.find(nameStr); it != fCache.end()) {
         if (!it->second.fClass->InheritsFrom(cls)) {
            return RFileRef<T>{fSelf, name};
         }
         return RFileRef{fSelf, name, std::static_pointer_cast<T>(it->second.fData)};
      }
      void *obj = GetUntyped(nameStr.c_str(), cls);
      if (!obj)
         return RFileRef<T>{fSelf, name};

      auto entry = RFileEntry{cls, std::shared_ptr<T>(static_cast<T *>(obj))};
      fCache[nameStr] = entry;
      return RFileRef{fSelf, name, std::static_pointer_cast<T>(entry.fData)};
   }

   template <typename T>
   std::unique_ptr<T> GetCopy(std::string_view name) const
   {
      std::string nameStr(name);
      const TClass *cls = TClass::GetClass(typeid(T));
      void *obj = GetUntyped(nameStr.c_str(), cls);
      return std::unique_ptr<T>{static_cast<T *>(obj)};
   }

   // Puts an object into the file.
   // Throws a RException if the file was opened in read-only mode.
   template <typename T>
   void Put(std::string_view name, T &obj)
   {
      std::string nameStr(name);
      const TClass *cls = TClass::GetClass(typeid(T));
      PutUntyped(nameStr.c_str(), cls, &obj);
   }
};

template <typename T>
inline std::unique_ptr<T> RFileRef<T>::Clone() const
{
   auto parentFile = fParentFile.lock();
   auto inner = fInner.lock();
   if (!parentFile || !inner)
      return nullptr;

   return parentFile->fFile->GetCopy<T>(fName);
}

} // namespace Experimental
} // namespace ROOT

#endif
