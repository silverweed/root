#include "RHistProxy.h"

using namespace Prim;

void RVulkanProxy::DrawHist2D(const RHist2DProps &hProps, RVulkanPrimitiveArray &outPrims)
{
  // draw hist frame
  auto frame = RVulkanPrimitiveBuilder(Quad::MinMax(0.1, 0.1, 0.9, 0.9))
    .BorderColor(Color::kBlack)
    .BorderThickness(1.f)
    .BackgroundColor(Color::kTransparent)
    // .BackgroundColor(Color::kBlack)
    .Build();

  outPrims.emplace_back(frame);

  Quad frameQuad = std::get<Quad>(frame.fGeom);

  // draw axes subdivisions
  {
    // FIXME: we should use pixel sizes, not meters, in some cases (such as the width of a subdiv)
    float subdivLen = 0.01f; // TODO make configurable
    float subdivThickness = 0.001f;

    /// X axis
    int nSubdivsX = hProps.fBins.size();
    if (nSubdivsX > 0)
    {
      auto subdivSpacing = frameQuad.fWidth / nSubdivsX;

      outPrims.reserve(outPrims.size() + nSubdivsX + 1);
      for (int i = 0 ; i < nSubdivsX + 1; ++i) {
        auto subdiv = RVulkanPrimitiveBuilder::Quad(subdivThickness, subdivLen)
          .Position(frameQuad.Left() + subdivSpacing * i, frameQuad.Bottom() - subdivLen * 0.5f)
          .BackgroundColor(Color::kBlack)
          .Build();
        outPrims.emplace_back(subdiv);
      }
    }
    /// Y axis
    int nSubdivsY = hProps.fNSubdivsY;
    if (nSubdivsY > 0)
    {
      auto subdivSpacing = frameQuad.fHeight / nSubdivsY;

      outPrims.reserve(outPrims.size() + nSubdivsY + 1);
      for (int i = 0 ; i < nSubdivsY + 1; ++i) {
        auto subdiv = RVulkanPrimitiveBuilder::Quad(subdivLen, subdivThickness)
          .Position(frameQuad.Left() - subdivLen * 0.5f, frameQuad.Bottom() + subdivSpacing * i)
          .BackgroundColor(Color::kBlack)
          .Build();
        outPrims.emplace_back(subdiv);
      }
    }
  }

  // draw title
  if (hProps.fTitle.length() > 0) {
    auto title = RVulkanPrimitiveBuilder::Text(hProps.fTitle, 8)
      .Position(0.5f, 0.95f)
      .BackgroundColor(Color::kBlack)
      .Build();
    outPrims.emplace_back(title);
  }

  // Draw bins
  const auto &bins = hProps.fBins;
  if (bins.size() > 0) {
    // TEMP: deduce min/max from bins
    int max = 0;
    for (int bin : hProps.fBins) {
      max = std::max(bin, max);
    }
    max *= 1.2f;

    float binWidth = frameQuad.fWidth / bins.size();
    float binStartLeft = frameQuad.Left() + binWidth * 0.5f;

    outPrims.reserve(outPrims.size() + bins.size());
    for (int i = 0; i < bins.size(); ++i) {
      int bin = bins[i];
      float binHeight = (float)bin / max * frameQuad.fHeight;
      auto binQuad = RVulkanPrimitiveBuilder::Quad(binWidth, binHeight)
        .Position(binStartLeft + binWidth * i, frameQuad.Bottom() + binHeight * 0.5)
        .BackgroundColor(Color { 0, 0, 0.6, 1 })
        .Build();
      outPrims.emplace_back(binQuad);
    }
  }
}
