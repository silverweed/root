#pragma once

// @(#)root/vulkan:$Id$
// Author: Giacomo Parolini, 2024

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "TVirtualX.h"

#include "RVulkan.h"

#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <optional>
#include <unordered_map>

// Wrapper around GLFW window
class RVkWindow {
  friend class TGVulkan;
  
  GLFWwindow *fWindow;
  int fWidth;
  int fHeight;
  std::string fTitle;
  
public:
  RVkWindow(int width, int height, const char *title);
  ~RVkWindow();

  void Clear();

  operator bool() const { return fWindow; }

  void *Get() const { return fWindow; }
};

struct RVkFont {
  FT_Face fFace;

  ~RVkFont() { FT_Done_Face(fFace); }
};

class RVkFonts {
  FT_Library fLibrary;
  std::unordered_map<std::string, RVkFont> fFontCache;

public:
  RVkFonts();
  ~RVkFonts();

  const RVkFont *LoadFont(const char *fontName);
};

// Interface to TVirtualX
class TGVulkan : public TVirtualX {
  // These get created in Init()
  std::unique_ptr<RVkWindow> fWindow;
  std::optional<RVulkan> fVk;

  RVkFonts fFonts;
  
public:
  TGVulkan(const char *name, const char *title);  

  // TVirtualX impl
  // -----------------------------------------
  Int_t OpenDisplay(const char *dpyName) override;
  Bool_t Init(void *display) override;
  void ClearWindow() override;

  FontStruct_t LoadQueryFont(const char *font_name) override;

  ClassDefOverride(TGVulkan, 0);
};
