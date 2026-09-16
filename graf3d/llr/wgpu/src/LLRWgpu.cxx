#include "ROOT/LLRWgpu.hxx"

void ROOT::RLowLevelRendererWgpu::Init()
{
  // TODO
}

void ROOT::RLowLevelRendererWgpu::AddMesh(const std::vector<ROOT::LLR::RVertex> &mesh)
{
  fVertices.insert(fVertices.end(), mesh.begin(), mesh.end());
}

void ROOT::RLowLevelRendererWgpu::Draw()
{
  // TODO
}
