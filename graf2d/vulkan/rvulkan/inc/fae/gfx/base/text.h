#pragma once 

#include "fae/core/mem.h"
#include "fae/res/font.h"

struct Vertex;

typedef i16 Char_Size;

typedef struct Text {
  Vertex *vertices;
  u32 n_vertices;
  V2 local_size;
} Text;

typedef struct {
  String8 string;
  Char_Size char_size;
} Text_Create_Data;

FAE_API Text text_create(Arena *arena, Font *font, String8 string, Char_Size char_size);
FAE_API Vertex *texts_create(Arena *arena, Font *font, u32 n_texts, Text_Create_Data *tdata, Text *out_texts);
