#pragma once

#include <filesystem>

#include "RPrim.h"

struct RViewportSize {
  float fWidth, fHeight;
};

struct RVulkanConfig {
  std::filesystem::path fAssetPath;
  RViewportSize fViewportSizeMeters;
};

class RVulkan {
  class RVulkanImpl *fImpl;
  
  RVulkan() = default;
  
public:
  // TODO: don't use void*
  static RVulkan Init(const RVulkanConfig &cfg, void *window);

  ~RVulkan();

  void Clear();
  void RenderFrame();

  void PushPrimitives(const RVulkanPrimitiveArray &prims);
};
