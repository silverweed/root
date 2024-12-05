#pragma once

#include "fae/core/str.h"
#include "fae/res/image.h"

typedef struct {
  f32 left, bot, right, top;
} Glyph_Bounds;

typedef struct {
  f32 advance;
  
  // Bounding box relative to the baseline
  Glyph_Bounds plane_bounds;

  // Normalized coordinates (uv) inside atlas
  Glyph_Bounds normalized_atlas_bounds;
} Glyph_Data;

#define FONT_FIRST_ASCII 32
#define FONT_LAST_ASCII 254

typedef struct {
  // NOTE: supporting only ASCII for now
  // Storing only data for printable ASCII characters.
  Glyph_Data glyph_data[FONT_LAST_ASCII - FONT_FIRST_ASCII];
  struct {
    u32 width;
    u32 height;
  } atlas_size;
  f32 max_glyph_height;
} Font_Metadata;

typedef struct {
  Image image;
  Font_Metadata metadata;  
} Font;

FAE_API b8 font_load_from_file(String8 image_file, String8 meta_file, Font *font);
