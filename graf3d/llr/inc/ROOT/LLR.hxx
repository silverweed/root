#ifndef ROOT_LLR
#define ROOT_LLR

#include "ROOT/LLRTypes.hxx"

#include "Rtypes.h"

#include <vector>

namespace ROOT {

class RLowLevelRenderer {
public:
  virtual ~RLowLevelRenderer() = default;
  
  virtual void Init() = 0;
  virtual void AddMesh(const std::vector<ROOT::LLR::RVertex> &mesh) = 0;
  virtual void Draw() = 0;

  ClassDef(RLowLevelRenderer, 0);
};
  
}

#endif
