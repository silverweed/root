import unittest
import ROOT


RFile = ROOT.Experimental.RFile
RDirectory = ROOT.Experimental.RDirectory

class RFileTests(unittest.TestCase):
    def test_open_for_reading(self):
        """A RFile can read a ROOT file created by TFile"""
        
        fileName = "test_rfile_read_py.root"
    
        # Create a root file to open
        with ROOT.TFile.Open(fileName, "RECREATE") as tfile:
            hist = ROOT.TH1D("hist", "", 100, -10, 10)
            hist.FillRandom("gaus", 1000)
            tfile.WriteObject(hist, "hist")

        with RFile.OpenForReading(fileName) as rfile:
            rdir = RDirectory(rfile)
            hist = rdir.Get("hist")
            self.assertNotEqual(hist, None)
            self.assertEqual(rdir.Get[ROOT.TH1D]("inexistent"), None)
            self.assertEqual(rdir.Get[ROOT.TH1F]("hist"), None)
            self.assertNotEqual(rdir.Get[ROOT.TH1]("hist"), None)

            foo = "foo"
            self.assertRaises(rdir.Put("foo", foo))
        

if __name__ == "__main__":
    unittest.main()
