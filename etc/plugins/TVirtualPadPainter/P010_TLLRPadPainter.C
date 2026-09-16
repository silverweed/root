void P010_TLLRPadPainter()
{
   gPluginMgr->AddHandler("TVirtualPadPainter", "llr", "TLLRPadPainter",
                          "ROOTLLR", "TLLRPadPainter()");
}
