#ifndef ROOT_LLR_WGPU
#define ROOT_LLR_WGPU

#include "ROOT/LLR.hxx"

#include <vector>

namespace ROOT {
  
class RLowLevelRendererWgpu final : public RLowLevelRenderer {
  std::vector<ROOT::LLR::RVertex> fVertices;

public:
  void Init() override;
  
  void AddMesh(const std::vector<ROOT::LLR::RVertex> &mesh) override;

  void Draw() override;

  ClassDefOverride(RLowLevelRendererWgpu, 0);
};

}

#endif
