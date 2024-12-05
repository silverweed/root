#include "RVulkan.h"

#include <cstring>
#include <vulkan/vulkan.h>

#include "RVulkanImpl.cxx"

RVulkan RVulkan::Init(const RVulkanConfig &cfg, void *window)
{
  RVulkan vk = {};
  vk.fImpl = new RVulkanImpl(cfg, window);  
  return vk;
}

RVulkan::~RVulkan()
{
  delete fImpl;
}

void RVulkan::Clear()
{
  fImpl->Clear();
}

void RVulkan::RenderFrame()
{
  fImpl->RenderFrame();
}

void RVulkan::PushPrimitives(const RVulkanPrimitiveArray &prims)
{
  for (auto &&prim : prims) {
    std::visit([this, &paintProps = prim.fPaintProps] (auto &&geom) {
      using T = std::decay_t<decltype(geom)>;
      if constexpr (std::is_same_v<T, Prim::Quad>) {
        fImpl->PushQuad(geom, paintProps);
      } else if constexpr(std::is_same_v<T, Prim::Text>) {
        fImpl->PushTexts(&geom, 1, &paintProps);
      } else {
        static_assert(!sizeof(T));
      }
    }, prim.fGeom);
  }
}
