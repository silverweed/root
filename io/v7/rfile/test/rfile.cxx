#include "gtest/gtest.h"

#include <TFile.h>
#include <TH1D.h>
#include <TROOT.h>
#include <ROOT/RFile.hxx>

using ROOT::Experimental::RFile;

namespace {

/**
 * An RAII wrapper around an open temporary file on disk. It cleans up the guarded file when the wrapper object
 * goes out of scope.
 */
class FileRaii {
private:
   std::string fPath;
   bool fPreserveFile = false;

public:
   explicit FileRaii(const std::string &path) : fPath(path) {}
   FileRaii(FileRaii &&) = default;
   FileRaii(const FileRaii &) = delete;
   FileRaii &operator=(FileRaii &&) = default;
   FileRaii &operator=(const FileRaii &) = delete;
   ~FileRaii()
   {
      if (!fPreserveFile)
         std::remove(fPath.c_str());
   }
   std::string GetPath() const { return fPath; }

   // Useful if you want to keep a test file after the test has finished running
   // for debugging purposes. Should only be used locally and never pushed.
   void PreserveFile() { fPreserveFile = true; }
};

} // anonymous namespace

TEST(RFile, OpenForReading)
{
   FileRaii fileGuard("test_rfile_read.root");

   // Create a root file to open
   {
      auto tfile = std::unique_ptr<TFile>(TFile::Open(fileGuard.GetPath().c_str(), "RECREATE"));
      TH1D hist("hist", "", 100, -10, 10);
      hist.FillRandom("gaus", 1000);
      tfile->WriteObject(&hist, "hist");
   }

   auto file = RFile::OpenForReading(fileGuard.GetPath());
   auto hist = file->Get<TH1D>("hist");
   EXPECT_TRUE(hist.IsValid());

   EXPECT_FALSE(file->Get<TH1D>("inexistent").IsValid());
   EXPECT_FALSE(file->Get<TH1F>("hist").IsValid());
   EXPECT_FALSE(file->Get<TH1>("hist").IsValid());

   // We do NOT want to globally register RFiles ever.
   EXPECT_EQ(ROOT::GetROOT()->GetListOfFiles()->GetSize(), 0);
}

TEST(RFile, OpenForWriting)
{
   FileRaii fileGuard("test_rfile_write.root");

   auto hist = std::make_unique<TH1D>("hist", "", 100, -10, 10);
   hist->FillRandom("gaus", 1000);

   auto file = RFile::Recreate(fileGuard.GetPath());
   file->Put("hist", std::move(hist));
   EXPECT_TRUE(file->Get<TH1D>("hist").IsValid());

   EXPECT_EQ(ROOT::GetROOT()->GetListOfFiles()->GetSize(), 0);
}
