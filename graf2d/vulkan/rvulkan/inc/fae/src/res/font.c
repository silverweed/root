internal
f32 font_scale_factor(Font *font, f32 font_size)
{
  f32 base_line_height = font->metadata.max_glyph_height;
  assert(base_line_height > 0);

  // NOTE: this scale factor is chosen so the maximum possible text height is equal to `font_size` px.
  // We may want to change this and use `font_size` as the "main corpus" size,
  // but for now it seems like a reasonable choice.
  return font_size / base_line_height;
}

internal
Glyph_Data *font_get_glyph_data(Font *font, u8 ch)
{
  return &font->metadata.glyph_data[ch - FONT_FIRST_ASCII];
}

internal
Font_Metadata font_parse_metadata_from_csv(String8 data, u32 atlas_w, u32 atlas_h)
{
  Temp scratch = scratch_begin(0, 0);

  Font_Metadata metadata = {};
  metadata.atlas_size.width = atlas_w;
  metadata.atlas_size.height = atlas_h;

  String8_Node *lines = str8_split(scratch.arena, data, '\n');
  for (String8_Node *node = lines; node; node = node->next) {
    String8 line = node->str;
    String8_Node *tokens = str8_split(scratch.arena, line, ',');
    if (tokens->count != 10) {
      WARN_TAG("Font", "Line in font csv metadata has %u items (expected: 10). Ignoring it.", tokens->count);
      continue;
    }
    
    // parse line
    // @Robustness: this is horrible code, should be made more robust.
#define Parse(T, func) (t = tokens->str, tokens = tokens->next, (T)func(cstr(t)))
    String8 t;
    u8 ch = Parse(u8, atoi);
    if (ch < FONT_FIRST_ASCII || ch > FONT_LAST_ASCII) {
      WARN_TAG("Font", "Font metadata has glyph data for non-printable character %u: skipping.", ch);
      continue;
    }
    Glyph_Data data = {};
    data.advance = Parse(f32, atof);
    data.plane_bounds.left = Parse(f32, atof);
    data.plane_bounds.bot = Parse(f32, atof);
    data.plane_bounds.right = Parse(f32, atof);
    data.plane_bounds.top = Parse(f32, atof);
    f32 atlas_bounds_l = Parse(f32, atof);
    f32 atlas_bounds_b = Parse(f32, atof);
    f32 atlas_bounds_r = Parse(f32, atof);
    f32 atlas_bounds_t = Parse(f32, atof);
#undef Parse
    data.normalized_atlas_bounds.left = atlas_bounds_l / atlas_w;
    data.normalized_atlas_bounds.bot = atlas_bounds_b / atlas_w;
    data.normalized_atlas_bounds.right = atlas_bounds_r / atlas_w;
    data.normalized_atlas_bounds.top = atlas_bounds_t / atlas_w;
    f32 glyph_height = data.plane_bounds.top - data.plane_bounds.bot;
    metadata.max_glyph_height = Max(metadata.max_glyph_height, glyph_height);

    metadata.glyph_data[ch - FONT_FIRST_ASCII] = data;
  }

  scratch_end(scratch);
  return metadata;
}

internal
Font_Metadata font_parse_metadata_from_csv_file(String8 fname, u32 atlas_w, u32 atlas_h)
{
  Font_Metadata metadata = {};
  Temp scratch = scratch_begin(0, 0);
  String8 csv_str = file_read_to_string(scratch.arena, fname);
  if (csv_str.size)
    metadata = font_parse_metadata_from_csv(csv_str, atlas_w, atlas_h);
  scratch_end(scratch);
  return metadata;
}

b8 font_load_from_file(String8 image_file, String8 meta_file, Font *font)
{
  font->image = image_load_from_file(image_file);
  if (font->image.pixels)
    font->metadata = font_parse_metadata_from_csv_file(meta_file, font->image.width, font->image.height);

  return font->image.pixels && font->metadata.atlas_size.width;
}

