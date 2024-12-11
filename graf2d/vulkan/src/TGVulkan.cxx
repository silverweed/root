#include "TGVulkan.h"

#include "XLFDParser.h"

#include <ROOT/RError.hxx>
#include <ROOT/RLogger.hxx>

using ROOT::RException;

// RVkWindow
// -----------------------------------------
RVkWindow::RVkWindow(int width, int height, const char *title) : fWidth(width), fHeight(height), fTitle(title)
{
   glfwInit();
   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
   glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

   fWindow = glfwCreateWindow(fWidth, fHeight, title, NULL, NULL);
}

RVkWindow::~RVkWindow()
{
   glfwDestroyWindow(fWindow);
}

// RVkFonts
// -----------------------------------------
RVkFonts::RVkFonts()
{
   FT_Error err = FT_Init_FreeType(&fLibrary);
   if (err) {
      throw RException(R__FAIL("failed to initialize freetype"));
   }
}

RVkFonts::~RVkFonts()
{
   FT_Error err = FT_Done_FreeType(fLibrary);
   if (err) {
      R__LOG_ERROR() << "failed to deinitialize freetype";
   }
}

const RVkFont *RVkFonts::LoadFont(const char *fontName)
{
   auto it = fFontCache.find(fontName);
   if (it != fFontCache.end()) {
      return &it->second;
   }

   // VERY TEMP
   std::string fontPath = std::string("/home/jp/root/fonts/") + fontName + ".ttf";
   RVkFont &font = fFontCache[fontName];
   // See here for explanation of faceIndex:
   // https://freetype.org/freetype2/docs/reference/ft2-face_creation.html#ft_open_face
   FT_Long faceIndex = 0;
   FT_Error err = FT_New_Face(fLibrary, fontPath.c_str(), faceIndex, &font.fFace);
   if (err) {
      R__LOG_ERROR() << "failed to load font" << fontPath;
      return nullptr;
   }

   return &font;
}

// TGVulkan
// -----------------------------------------
TGVulkan::TGVulkan(const char *name, const char *title) : TVirtualX(name, title) {}

Bool_t TGVulkan::Init(void *display)
{
   (void)display;

   // init GLFW
   fWindow = RVkWindow(800, 600, "RVulkan test");
   if (!fWindow)
      return false;

   // init vulkan
   RVulkanConfig vk_cfg;
   vk_cfg.fAssetPath = "assets";
   vk_cfg.fViewportSizeMeters = {2, 2};
   fVk = RVulkan::Init(vk_cfg, fWindow->Get());

   return true;
}

void TGVulkan::ClearWindow() {}

FontStruct_t TGVulkan::LoadQueryFont(const char *font_name)
{
   using namespace ROOT::XLFD;

   XLFDName xlfd;
   if (ParseXLFDName(font_name, xlfd)) {
      return static_cast<FontStruct_t>(reinterpret_cast<uintptr_t>(fFonts.LoadFont(xlfd.fFamilyName.c_str())));
   } else {
      R__LOG_ERROR() << "failed to parse xlfd name " << font_name;
      return static_cast<FontStruct_t>(reinterpret_cast<uintptr_t>(fFonts.LoadFont(font_name)));
   }
}
