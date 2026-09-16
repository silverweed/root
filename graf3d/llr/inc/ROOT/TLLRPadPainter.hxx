#ifndef ROOT_TLLRPadPainter
#define ROOT_TLLRPadPainter

#include "ROOT/LLRTypes.hxx"

#include "TPadPainterBase.h"

#include <memory>
#include <vector>

namespace ROOT {

class RLowLevelRenderer;

}

class TLLRPadPainter : public TPadPainterBase {
private:
  std::unique_ptr<ROOT::RLowLevelRenderer> fRenderer;

protected:

public:
   TLLRPadPainter();
   ~TLLRPadPainter();

   void     SetOpacity(Int_t percent) override;
   Float_t  GetTextMagnitude() const override;


   void      OnPad(TVirtualPad *) override;

   // Overwrite only attributes setters
   void      SetAttFill(const TAttFill &att) override;
   void      SetAttLine(const TAttLine &att) override;
   void      SetAttMarker(const TAttMarker &att) override;

   //2. "Off-screen management" part.
   Int_t    CreateDrawable(UInt_t w, UInt_t h) override;
   void     ClearDrawable() override;
   void     ClearWindow(Int_t device) override;
   Int_t    ResizeDrawable(Int_t device, UInt_t w, UInt_t h) override;
   void     CopyDrawable(Int_t device, Int_t px, Int_t py) override;
   void     DestroyDrawable(Int_t device) override;
   void     SelectDrawable(Int_t device) override;
   void     UpdateDrawable(Int_t mode) override;
   void     SetDrawMode(Int_t device, Int_t mode) override;

   void     InitPainter() override;
   void     InvalidateCS() override;
   void     LockPainter() override;

   Bool_t    HasTTFonts() const override;

   void     DrawLine(Double_t x1, Double_t y1, Double_t x2, Double_t y2) override;
   void     DrawLineNDC(Double_t u1, Double_t v1, Double_t u2, Double_t v2) override;

   void     DrawBox(Double_t x1, Double_t y1, Double_t x2, Double_t y2, EBoxMode mode) override;
   //TPad needs double and float versions.
   void     DrawFillArea(Int_t n, const Double_t *x, const Double_t *y) override;
   void     DrawFillArea(Int_t n, const Float_t *x, const Float_t *y) override;

   //TPad needs both double and float versions of DrawPolyLine.
   void     DrawPolyLine(Int_t n, const Double_t *x, const Double_t *y) override;
   void     DrawPolyLine(Int_t n, const Float_t *x, const Float_t *y) override;
   void     DrawPolyLineNDC(Int_t n, const Double_t *u, const Double_t *v) override;

   //TPad needs both versions.
   void     DrawPolyMarker(Int_t n, const Double_t *x, const Double_t *y) override;
   void     DrawPolyMarker(Int_t n, const Float_t *x, const Float_t *y) override;

   void     DrawText(Double_t x, Double_t y, const char *text, ETextMode mode) override;
   void     DrawText(Double_t, Double_t, const wchar_t *, ETextMode) override;
   void     DrawTextNDC(Double_t x, Double_t y, const char *text, ETextMode mode) override;
   void     DrawTextNDC(Double_t, Double_t, const wchar_t *, ETextMode) override;

   void     DrawImage(TImage *img, Int_t x, Int_t y, Int_t flags = 0) override;

   //jpg, png, gif and bmp output.
   void     SaveImage(TVirtualPad *pad, const char *fileName, Int_t type) const override;

   //TASImage support.
   void     DrawPixels(const unsigned char *pixelData, UInt_t width, UInt_t height,
                       Int_t dstX, Int_t dstY, Bool_t enableBlending) override;

   // ClassDefOverride(TLLRPadPainter, 0)
};

#endif

