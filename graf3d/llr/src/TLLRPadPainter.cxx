#include "ROOT/TLLRPadPainter.hxx"
#include "ROOT/LLR.hxx"

#include "TROOT.h"
#include "TPluginManager.h"

// TEMP
#include <iostream>

using namespace ROOT::LLR;

TLLRPadPainter::~TLLRPadPainter() = default;

TLLRPadPainter::TLLRPadPainter()
{
   gROOT->GetPluginManager()->Print();
   auto handler = gROOT->GetPluginManager()->FindHandler("ROOT::RLowLevelRenderer", "wgpu");
   assert(handler);
   auto res = handler->LoadPlugin();
   assert(res == 0); // 0 is success
   fRenderer = std::unique_ptr<ROOT::RLowLevelRenderer>(reinterpret_cast<ROOT::RLowLevelRenderer *>(handler->ExecPlugin(0)));
   std::cout << "renderer: " << fRenderer.get() << "\n";
   assert(fRenderer);
}

void TLLRPadPainter::InitPainter()
{
   
}

void TLLRPadPainter::DrawLine(Double_t x1, Double_t y1, Double_t x2, Double_t y2)
{
   std::cout << "draw line " << x1 << ", " << y1 << ", " << x2 << ", " << y2 << "\n";

   std::vector<ROOT::LLR::RVertex> vertices;

   vertices.emplace_back(V3d{x1, y1, 0.});
   vertices.emplace_back(V3d{x2, y2, 0.});

   fRenderer->AddMesh(vertices);
}

void TLLRPadPainter::DrawLineNDC(Double_t u1, Double_t v1, Double_t u2, Double_t v2)
{
   // TODO
}

void TLLRPadPainter::LockPainter()
{
   fRenderer->Draw();
}

void TLLRPadPainter::SetOpacity(Int_t percent)
{
   // TODO
}

Float_t TLLRPadPainter::GetTextMagnitude() const
{
   // TODO
}

void TLLRPadPainter::OnPad(TVirtualPad *)
{
   // TODO
}

// Overwrite only attributes setters
void TLLRPadPainter::SetAttFill(const TAttFill &att)
{
   // TODO
}
void TLLRPadPainter::SetAttLine(const TAttLine &att)
{
   // TODO
}
void TLLRPadPainter::SetAttMarker(const TAttMarker &att)
{
   // TODO
}

// 2. "Off-screen management" part.
Int_t TLLRPadPainter::CreateDrawable(UInt_t w, UInt_t h)
{
   // TODO
}
void TLLRPadPainter::ClearDrawable()
{
   // TODO
}
void TLLRPadPainter::ClearWindow(Int_t device)
{
   // TODO
}
Int_t TLLRPadPainter::ResizeDrawable(Int_t device, UInt_t w, UInt_t h)
{
   // TODO
}
void TLLRPadPainter::CopyDrawable(Int_t device, Int_t px, Int_t py)
{
   // TODO
}
void TLLRPadPainter::DestroyDrawable(Int_t device)
{
   // TODO
}
void TLLRPadPainter::SelectDrawable(Int_t device)
{
   // TODO
}
void TLLRPadPainter::UpdateDrawable(Int_t mode)
{
   // TODO
}
void TLLRPadPainter::SetDrawMode(Int_t device, Int_t mode)
{
   // TODO
}

void TLLRPadPainter::InvalidateCS()
{
   // TODO
}

Bool_t TLLRPadPainter::HasTTFonts() const
{
   // TODO
}


void TLLRPadPainter::DrawBox(Double_t x1, Double_t y1, Double_t x2, Double_t y2, EBoxMode mode)
{
   // TODO
}
// TPad needs double and float versions.
void TLLRPadPainter::DrawFillArea(Int_t n, const Double_t *x, const Double_t *y)
{
   // TODO
}
void TLLRPadPainter::DrawFillArea(Int_t n, const Float_t *x, const Float_t *y)
{
   // TODO
}

// TPad needs both double and float versions of DrawPolyLine.
void TLLRPadPainter::DrawPolyLine(Int_t n, const Double_t *x, const Double_t *y)
{
   // TODO
}
void TLLRPadPainter::DrawPolyLine(Int_t n, const Float_t *x, const Float_t *y)
{
   // TODO
}
void TLLRPadPainter::DrawPolyLineNDC(Int_t n, const Double_t *u, const Double_t *v)
{
   // TODO
}

// TPad needs both versions.
void TLLRPadPainter::DrawPolyMarker(Int_t n, const Double_t *x, const Double_t *y)
{
   // TODO
}
void TLLRPadPainter::DrawPolyMarker(Int_t n, const Float_t *x, const Float_t *y)
{
   // TODO
}

void TLLRPadPainter::DrawText(Double_t x, Double_t y, const char *text, ETextMode mode)
{
   // TODO
}
void TLLRPadPainter::DrawText(Double_t, Double_t, const wchar_t *, ETextMode)
{
   // TODO
}
void TLLRPadPainter::DrawTextNDC(Double_t x, Double_t y, const char *text, ETextMode mode)
{
   // TODO
}
void TLLRPadPainter::DrawTextNDC(Double_t, Double_t, const wchar_t *, ETextMode)
{
   // TODO
}

void TLLRPadPainter::DrawImage(TImage *img, Int_t x, Int_t y, Int_t flags)
{
   // TODO
}

// jpg, png, gif and bmp output.
void TLLRPadPainter::SaveImage(TVirtualPad *pad, const char *fileName, Int_t type) const
{
   // TODO
}

// TASImage support.
void TLLRPadPainter::DrawPixels(const unsigned char *pixelData, UInt_t width, UInt_t height, Int_t dstX, Int_t dstY,
                Bool_t enableBlending)
{
   // TODO
}
