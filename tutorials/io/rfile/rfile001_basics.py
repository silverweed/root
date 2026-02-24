# \file
# \ingroup tutorial_rfile
# \notebook
# Example of getting objects from RFile and putting objects into RFile.
#
# \macro_image
# \macro_code
#
# \date February 2026
# \author The ROOT Team
# \warning This is part of the ROOT 7 prototype! It will change without notice. It might trigger earthquakes. Feedback
# is welcome!
import ROOT

kFileName = "rfile001.root"
kObjName = "hist"

def rfile001_basics():
  # Open an RFile for writing, recreating it if it already exists:
  with ROOT.Experimental.RFile.Recreate(kFileName) as file:
      # Create an object to put into the RFile
      hist = ROOT.TH1D(kObjName, "My Histo", 20, 0, 1)
      hist.FillRandom(100)

      # Put it in the file using RFile::Put(objectPath, object).
      # The object is effectively cloned, so the RFile won't "see" any further modification to the object.
      file.Put(hist.GetName(), hist)

      # After the `with` block `file` will write itself to disk.

  # Open an existing file for reading. Will throw if the file does not exist or is not readable.
  with ROOT.Experimental.RFile.Open(kFileName) as file:
      # Retrieve the object from the file.
      # Every time RFile.Get is called you will get a fresh copy of the object.
      hist = file.Get(kObjName)

      # You can also explicitly request the type of the object to return:
      # hist = file.Get[ROOT.TH1D](kObjName)

      canvas = ROOT.TCanvas("", "RFile Basics", 200, 10, 700, 500)
      hist.DrawCopy()

