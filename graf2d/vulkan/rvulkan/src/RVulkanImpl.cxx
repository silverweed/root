#include "RVulkan.h"

#include <assert.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vulkan/vulkan.h>

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "fae/core/types.h"
#include "fae/core/mem.h"
#include "fae/core/lists.h"
#include "fae/core/str.h"
#include "fae/core/misc.h"
#include "fae/core/log.h"
#include "fae/core/files.h"
#include "fae/math/math.h"
#include "fae/res/image.h"
#include "fae/res/font.h"
#include "fae/platform/platform.h"
#include "fae/gfx/backend/gfx_vulk.h"
#include "fae/gfx/base/text.h"

#define Min    Fae_Min
#define Max    Fae_Max
#define Clamp  Fae_Clamp
#define Abs    Fae_Abs
#define Square Fae_Square

#include "fae/src/core/mem.c"
#include "fae/src/core/str.c"
#include "fae/src/core/files.c"
#include "fae/src/res/image.c"
#include "fae/src/res/font.c"
#include "fae/src/gfx/base/text.c"
#include "fae/src/gfx/backend/gfx_vulk_buf.c"
#include "fae/src/gfx/backend/gfx_vulk.c"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "fae/src/gfx/backend/gfx_vulk_glfw.c"
#include "fae/src/gfx/backend/gfx_vulk_draw.c"
#include "vulk_init.c"

#undef Min
#undef Max
#undef Clamp
#undef Abs
#undef Square

#include <vector>

#include "RPrim.h"

struct Thread_Ctx_Wrapper {
  Thread_Ctx tctx;

  Thread_Ctx_Wrapper()
  {
    tctx_init(&tctx);    
  }

  ~Thread_Ctx_Wrapper()
  {
    tctx_release();
  }
};

static Thread_Ctx_Wrapper &ensure_tctx_initted()
{
  thread_local Thread_Ctx_Wrapper wrapper;
  return wrapper;
}

struct Gpu_Instance_Data {
  alignas(16) M44 model;
  V4 color;
};

class RVulkanImpl {
  Gfx_Vulkan vk;
  void *fWindow;

  V2 fViewportSizeMeters;

  std::vector<Gpu_Instance_Data> fQuadsIdata;

  std::vector<Text> fTexts;
  std::vector<Vulk_Text *> fVulkTexts;
  std::vector<Gpu_Instance_Data> fTextsIdata;

  // TODO: handle multiple fonts
  Font fFont;

  void UpdatePushConstants(Vulk_Draw_Push_Constants &pcs) const;

public:
  RVulkanImpl(const RVulkanConfig &cfg, void *window);

  void PushQuad(const Prim::Quad &quad, const Prim::PaintProps &paintProps);
  void PushTexts(const Prim::Text *texts, size_t nTexts, const Prim::PaintProps *paintProps);

  void Clear();
  void RenderFrame();
};

static b8 load_font(Arena *arena, String8 fonts_dir, String8 font_name, Font *font)
{
  String8 font_img_file = push_str8f(arena, "%s/%s_msdf.png", cstr(fonts_dir), cstr(font_name));
  String8 font_meta_file = push_str8f(arena, "%s/%s_meta.csv", cstr(fonts_dir), cstr(font_name));
  if (!font_load_from_file(font_img_file, font_meta_file, font)) {
    return false;
  }
  return true;
}

RVulkanImpl::RVulkanImpl(const RVulkanConfig &vk_cfg, void *window)
  : fWindow(window)
  , fViewportSizeMeters(v2(vk_cfg.fViewportSizeMeters.fWidth, vk_cfg.fViewportSizeMeters.fHeight))
{
  ensure_tctx_initted();

  memset(&vk, 0, sizeof(vk));

  Vulk_Init_Config cfg = {};
  cfg.use_srgb = false;
  vulk_pre_init(&vk, window, &cfg);

  Temp scratch = scratch_begin(0, 0);

  Vulk_Pipeline_Cfg pipeline_cfgs[Pipeline_COUNT] = {};
  vulk_setup_pipeline_configs(scratch.arena, pipeline_cfgs, false);

  load_font(scratch.arena, push_str8f(scratch.arena, "%s/fonts", vk_cfg.fAssetPath.c_str()), str8("Hack-Regular"), &fFont);

  cfg.pipeline_cfgs = pipeline_cfgs;
  cfg.n_pipeline_cfgs = countof(pipeline_cfgs);
  cfg.shaders_dir = push_str8f(scratch.arena, "%s/shaders", vk_cfg.fAssetPath.c_str());
  cfg.font_image = fFont.image;
  cfg.n_obj_types = GfxObj_COUNT;
  cfg.obj_instance_sizes = arena_push_array_nozero(u64, scratch.arena, cfg.n_obj_types);
  for (u32 i = 0; i < cfg.n_obj_types; ++i)
    cfg.obj_instance_sizes[i] = sizeof(Gpu_Instance_Data);
  Vulk_Init_Result res = {};
  res.instance_capacity = arena_push_array(u32, scratch.arena, cfg.n_pipeline_cfgs);
  vulk_init(&vk, &cfg, &res);

  image_unload(fFont.image);

  scratch_end(scratch);
}

void RVulkanImpl::Clear()
{
  for (auto *text : fVulkTexts)
  {
    vulk_remove_text(&vk, text);
  }

  fQuadsIdata.clear();
  fTexts.clear();
  fTextsIdata.clear();
  fVulkTexts.clear();
}

void RVulkanImpl::UpdatePushConstants(Vulk_Draw_Push_Constants &pcs) const
{
  const float kFarPlaneDist = 1000.f;

  Push_Const_Gfx &pc = pcs.gfx;
  // Flip Y direction since vulkan has clip Y growing downward.
  M44 view = m44(1, 0, 0, 0,
                 0, -1, 0, 0,
                 0, 0, 1, 0,
                 0, 0, 0, 1);
  V2 vpSize = fViewportSizeMeters;
  M44 proj = rev_ortho_proj(vpSize.x, vpSize.y, kFarPlaneDist);
  pc.view_proj = m44_mul(&proj, &view);
}

void RVulkanImpl::RenderFrame()
{
  Vulk_Draw_Data draw_data = {};
  draw_data.clear_color = v3(1, 1, 1);

  /// Update quads
  draw_data.n_quads = fQuadsIdata.size();
  // @Speed: don't necessarily update the whole buffer every frame
  vulk_update_instance_buffer(&vk, GfxObj_Quad, fQuadsIdata.data(), sizeof(Gpu_Instance_Data) * fQuadsIdata.size(), 0);
  vulk_update_instance_buffer(&vk, GfxObj_Text, fTextsIdata.data(), sizeof(Gpu_Instance_Data) * fTextsIdata.size(), 0);

  UpdatePushConstants(draw_data.pc);

  vulk_do_frame(&vk, (GLFWwindow *)fWindow, &draw_data);
}

static Gpu_Instance_Data BuildQuadInstanceData(const Prim::Quad &quad, const Prim::Color &color)
{
  Gpu_Instance_Data idata;
  M44 quadSize = m44_scale(v3(quad.fWidth, quad.fHeight, 1));
  idata.model = m44_mul(&quad.fTransform, &quadSize);
  idata.color = *reinterpret_cast<const V4 *>(&color);
  return idata;
}

void RVulkanImpl::PushQuad(const Prim::Quad &localQuad, const Prim::PaintProps &paintProps)
{
  using Prim::Quad;
  
  bool needsBg = paintProps.fBgColor.a > 0;
  bool needsBorder = paintProps.fBorderColor.a > 0 && paintProps.fBorderThickness > 0;
  int nQuads = needsBg + 4 * needsBorder;
  fQuadsIdata.reserve(fQuadsIdata.size() + nQuads);

  // convert quad from "canvas coordinates" to world coordinates.
  // Canvas coordinates have the origin in the bottom-left corner and they are clipped to (1, 1)
  // World coordinates have the origin in the center of the screen and are clipped to the viewport size
  auto quad = localQuad;
  quad.SetPos((quad.X() - 0.5) * fViewportSizeMeters.x, (quad.Y() - 0.5) * fViewportSizeMeters.y);
  quad.fWidth *= fViewportSizeMeters.x;
  quad.fHeight *= fViewportSizeMeters.y;

  if (needsBg) {
    Gpu_Instance_Data idata = BuildQuadInstanceData(quad, paintProps.fBgColor);
    fQuadsIdata.push_back(idata);
  }

  if (needsBorder) {
    V2 pixelsToMeters = v2(fViewportSizeMeters.x / vk.swapchain.extent.width, fViewportSizeMeters.y / vk.swapchain.extent.height);
    V2 thickness = v2_muls(pixelsToMeters, paintProps.fBorderThickness);

    // Bottom
    {
      Quad bottom = Quad::MinMax(quad.Left(), quad.Bottom(), quad.Right(), quad.Bottom() + thickness.y);
      Gpu_Instance_Data idata = BuildQuadInstanceData(bottom, paintProps.fBorderColor);
      fQuadsIdata.push_back(idata);
    }

    // Top
    {
      Quad top = Quad::MinMax(quad.Left(), quad.Top() - thickness.y, quad.Right(), quad.Top());
      Gpu_Instance_Data idata = BuildQuadInstanceData(top, paintProps.fBorderColor);
      fQuadsIdata.push_back(idata);
    }

    // Left
    {
      Quad left = Quad::MinMax(quad.Left(), quad.Bottom(), quad.Left() + thickness.x, quad.Top());
      Gpu_Instance_Data idata = BuildQuadInstanceData(left, paintProps.fBorderColor);
      fQuadsIdata.push_back(idata);
    }

    // Right
    {
      Quad right = Quad::MinMax(quad.Right() - thickness.x, quad.Bottom(), quad.Right(), quad.Top());
      Gpu_Instance_Data idata = BuildQuadInstanceData(right, paintProps.fBorderColor);
      fQuadsIdata.push_back(idata);
    }
  }
}

void RVulkanImpl::PushTexts(const Prim::Text *texts, size_t nTexts, const Prim::PaintProps *paintProps)
{
  Temp scratch = scratch_begin(0, 0);

  Text_Create_Data *createData = arena_push_array_nozero(Text_Create_Data, scratch.arena, nTexts);
  for (size_t i = 0; i < nTexts; ++i) {
    createData[i].string = str8_from_buf(scratch.arena, (const u8 *)texts[i].fString.c_str(), texts[i].fString.length());
    createData[i].char_size = texts[i].fCharSize;
  }

  assert(fTexts.size() == fVulkTexts.size());
  assert(fTextsIdata.size() == fVulkTexts.size());

  size_t nTextsBefore = fTexts.size();
  fTexts.resize(fTexts.size() + nTexts);
  Text *outTexts = fTexts.data() + nTextsBefore;
  Vertex *vertices = texts_create(scratch.arena, &fFont, nTexts, createData, outTexts);

  u32 *textInstIds = arena_push_array_nozero(u32, scratch.arena, nTexts);
  u32 *nVertices = arena_push_array_nozero(u32, scratch.arena, nTexts);
  for (u32 i = 0; i < nTexts; ++i) {
    // TODO: proper instance id mapping
    // Gfx_Instance_Metadata metadata;
    // *out_text_ids[i] = gfx_push_instance_get_meta(&gfx->instances, inst_data[i], GfxObj_Text, &metadata);
    // vk_inst_ids[i] = metadata.index_in_data;
    textInstIds[i] = nTextsBefore + i;
    nVertices[i] = outTexts[i].n_vertices;
  }

  fVulkTexts.resize(fVulkTexts.size() + nTexts);
  Vulk_Text **vtexts = fVulkTexts.data() + nTextsBefore;
  vulk_add_texts(&vk, vertices, nTexts, nVertices, textInstIds, false, vtexts);

  fTextsIdata.reserve(fTextsIdata.size() + nTexts);
  for (u32 i = 0; i < nTexts; ++i) {
    Gpu_Instance_Data idata;
    idata.model = texts[i].fTransform;
    idata.model.col[3].x = (texts[i].X() - 0.5f) * fViewportSizeMeters.x - outTexts[i].local_size.x * 0.5f;
    idata.model.col[3].y = (texts[i].Y() - 0.5f) * fViewportSizeMeters.y - outTexts[i].local_size.y * 0.5f;
    idata.color = *reinterpret_cast<const V4 *>(&paintProps[i].fBgColor);
    fTextsIdata.emplace_back(idata);
  }
  
  scratch_end(scratch);
}
