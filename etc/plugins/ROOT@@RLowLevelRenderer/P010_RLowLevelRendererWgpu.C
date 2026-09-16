void P010_RLowLevelRendererWgpu()
{
   gPluginMgr->AddHandler("ROOT::RLowLevelRenderer", "wgpu", "ROOT::RLowLevelRendererWgpu",
      "ROOTLLRWgpu", "RLowLevelRendererWgpu()");
}
