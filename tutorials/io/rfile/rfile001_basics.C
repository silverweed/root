/// \file
/// \ingroup tutorial_rfile
/// \notebook
/// Example of getting objects from RFile and putting objects into RFile.
///
/// \macro_image
/// \macro_code
///
/// \date February 2026
/// \author The ROOT Team
/// \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
/// is welcome!
#include <ROOT/RFile.hxx>
#include <TCanvas.h>
#include <TH1D.h>

using RFile = ROOT::Experimental::RFile;

static constexpr const char *kFileName = "rfile001.root";
static constexpr const char *kObjName = "hist";

void rfile001_basics()
{
   {
      // Open an RFile for writing, recreating it if it already exists:
      std::unique_ptr<RFile> file = RFile::Recreate(kFileName);

      // Create an object to put into the RFile
      TH1D hist(std::string(kObjName).c_str(), "My Histo", 20, 0, 1);
      hist.FillRandom(100);

      // Put it in the file using RFile::Put(objectPath, object).
      // The object is effectively cloned, so:
      // a) ownership of the object does not change after this, and
      // b) the RFile won't "see" any further modification to the object.
      file->Put(hist.GetName(), hist);

      // When the file goes out of scope it will write itself to disk.
   }

   {
      // Open an existing file for reading. Will throw if the file does not exist or is not readable.
      std::unique_ptr<RFile> file = RFile::Open(kFileName);

      // Retrieve the object from the file.
      // Every time RFile::Get is called you will get a fresh copy of the object and unique ownership over it.
      std::unique_ptr<TH1D> hist = file->Get<TH1D>(kObjName);

      TCanvas *canvas = new TCanvas("", "RFile Basics", 200, 10, 700, 500);
      hist->DrawCopy(); // use DrawCopy so the object in the canvas survives after this scope ends.
   }
}
