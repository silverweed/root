// `vertices` must be an array of at least `string.size * 6` elements.
internal
Text text_create_noalloc(Vertex *vertices, Font *font, String8 string, Char_Size char_size)
{
  Text text = {};

  f32 f_char_size = char_size / 100.; // XXX: figure out a proper way to handle char size.

  u32 n_vertices = string.size * 6; // one quad per glyph
  u32 v_idx = 0;
  f32 pos_x = 0.f;
  f32 scale_factor = font_scale_factor(font, f_char_size);
  for (u64 i = 0; i < string.size; ++i) {
    u8 ch = string.str[i];
    assert(ch >= FONT_FIRST_ASCII);
    Glyph_Data *glyph_data = font_get_glyph_data(font, ch);
    assert(glyph_data); // currently assuming we have all the (ASCII) characters available
    Glyph_Bounds *pb = &glyph_data->plane_bounds;
    Rect rect = (Rect){ 
      pos_x + pb->left * scale_factor,
      pb->bot * scale_factor,
      (pb->right - pb->left) * scale_factor,
      (pb->top - pb->bot) * scale_factor
    };

    pos_x += scale_factor * glyph_data->advance;
    
    Glyph_Bounds *atlas_bounds = &glyph_data->normalized_atlas_bounds;
    Vertex v_1 = (Vertex){ 
      .pos = v3(rect.x, rect.y + rect.height, 0),
      .uv = v2(atlas_bounds->left, 1.f - atlas_bounds->top)
    };
    Vertex v_2 = (Vertex){ 
      .pos = v3(rect.x + rect.width, rect.y + rect.height, 0),
      .uv = v2(atlas_bounds->right, 1.f - atlas_bounds->top)
    };
    Vertex v_3 = (Vertex){ 
      .pos = v3(rect.x + rect.width, rect.y, 0),
      .uv = v2(atlas_bounds->right, 1.f - atlas_bounds->bot)
    };
    Vertex v_4 = (Vertex){ 
      .pos = v3(rect.x, rect.y, 0),
      .uv = v2(atlas_bounds->left, 1.f - atlas_bounds->bot)
    };
    
    text.local_size.x = Max(text.local_size.x, rect.x + rect.width);
    text.local_size.y = Max(text.local_size.y, rect.y + rect.height);

    vertices[v_idx++] = v_1;
    vertices[v_idx++] = v_2;
    vertices[v_idx++] = v_3;
    vertices[v_idx++] = v_3;
    vertices[v_idx++] = v_4;
    vertices[v_idx++] = v_1;
  }

  assert(v_idx == n_vertices);

  text.vertices = vertices;
  text.n_vertices = n_vertices;

  return text;
}

Vertex *texts_create(Arena *arena, Font *font, u32 n_texts, Text_Create_Data *tdata, Text *out_texts)
{
  // NOTE: currently assuming ASCII string. We could do better, but eh, maybe later.

  u32 n_vertices = 0;
  for (u32 i = 0; i < n_texts; ++i)
    n_vertices += tdata[i].string.size * 6;
  
  Vertex *vertices = arena_push_array_nozero(Vertex, arena, n_vertices);
  Vertex *cur_vertices = vertices;
  for (u32 i = 0; i < n_texts; ++i) {
    if (tdata[i].string.size > 0) {
      out_texts[i] = text_create_noalloc(cur_vertices, font, tdata[i].string, tdata[i].char_size);
      cur_vertices += tdata[i].string.size * 6;
    }
  }
  return vertices;
}

Text text_create(Arena *arena, Font *font, String8 string, Char_Size char_size)
{
  Text text = {};
  Text_Create_Data data = {};
  data.string = string;
  data.char_size = char_size;
  texts_create(arena, font, 1, &data, &text);

  return text;
}
