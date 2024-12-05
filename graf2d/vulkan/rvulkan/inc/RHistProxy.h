#pragma once

#include <vector>

#include "RPrim.h"

/// bridge from TH1 to vulkan primitives (TEMP)

namespace RVulkanProxy { 

struct RHist2DProps {
  int fNSubdivsY;
  std::string fTitle;
  std::vector<int> fBins;
};

void DrawHist2D(const RHist2DProps &props, RVulkanPrimitiveArray &outPrims);

}
