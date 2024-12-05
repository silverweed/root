#define PIXELS_PER_METER 1000
#define GFX_PERSP_NEAR_PLANE 0.001f
// NOTE: using infinite projection, so no persp far plane
#define GFX_ORTHO_NEAR_PLANE 0.f
#define GFX_ORTHO_FAR_PLANE 1000.f

// maybe tune these later
#define GFX_TEXT_VBUF_SIZE MiB(64)
#define GFX_TEXT_MAX_COUNT 15000

#define GFX_SPLINE_THICKNESS 0.05

typedef struct {
  u64 id;
} Gfx_Instance_Id;
#define GFX_INVALID_ID (Gfx_Instance_Id) { 0 }

internal
b8 gfx_is_valid_id(Gfx_Instance_Id id)
{
  return id.id != GFX_INVALID_ID.id;
}

typedef struct {
  V3 pos;
  Quat rot;
  V3 scale;
  V4 color_a;
  V4 color_b;
} Cpu_Instance_Data;

typedef struct {
  _Alignas(16) M44 model; 
  V4 color_a;
  V4 color_b;
} Gpu_Instance_Data;

typedef enum {
  Shader_INVALID,
  Shader_Quad,
  Shader_Msdf,
  Shader_Screenspace_Quad,
  Shader_Skybox,
  // more lightweight version of a skybox, only using a gradient
  Shader_Simple_Skybox,
  Shader_Spline,
  Shader_Spline_Build,
  Shader_COUNT
} Gfx_Shader_Id;

typedef enum {
  Proj_Ortho,
  Proj_Persp
} Gfx_Proj_Mode;

typedef struct {
  // The intended viewport of the application, whose ratio should be preserved by resizes
  V2 viewport_size_meters;
  f32 pixels_per_meter;
  Gfx_Proj_Mode proj_mode;
  b8 srgb;
  b8 prefer_integrated_gpu;
  b8 show_spline_control_points;
  b8 wireframe;
  u32 initial_instances_capacity[GfxObj_COUNT];

  String8 shaders_dir;
  String8 fonts_dir;
  String8 textures_dir;
  String8 font_name;
} Gfx_Config;

typedef struct {
  u32 index_in_data;
  Gfx_Obj_Type obj_type;
} Gfx_Instance_Metadata;

typedef struct {
  // This is always kept contiguous. Removing an instance will cause a swap_remove
  // and a remapping in `id_map`.
  Cpu_Instance_Data *data;
  // Always the same size as `data`. maps { idx_in_data => Gfx_Instance_Id }
  Gfx_Instance_Id *instance_ids;
  u64 count;
  u64 capacity;
  u64 max_capacity;

  // these arenas are alternated whenever we grow `data` and `instance_ids`.
  Arena *arenas[2];
  u8 cur_arena;
} Gfx_Instance_Obj_Data;

// Gfx Instances work this way:
// at high level, a user creates gfx objects such as quads, texts etc and gets back a Gfx_Instance_Id.
// This acts as a handle for the user to interact with the object they created and it is global 
// among all object types.
// Internally, the Gfx layer keeps a mapping between each Gfx_Instance_Id and the instance's metadata,
// i.e. its object type (quad, text, ...) and its index in the Cpu_Instance_Data array.
// There is one such array per object type and it maps 1:1 with a vulkan buffer (in the sense that the index
// in this array is equal to the vulkan instance index for that object type).
typedef struct {
  Gfx_Instance_Obj_Data by_type[GfxObj_COUNT];
  
  // { Gfx_Instance_Id => Gfx_Instance_Metadata }
  Hash_Map id_map;
  // instance id of the last element of `data` (used for remapping when swap-removing)
  Gfx_Instance_Id last_inst_id;
  // The instance_id that the next instance will get.
  Gfx_Instance_Id next_inst_id_available;
} Gfx_Instance_Data;

typedef struct {
  // pointer to a Vulk_Text owned by Gfx_Vulkan
  Vulk_Text *vk_text;
  V2 size;
  i16 char_size;
} Gfx_Text;

typedef struct {
  Vulk_Spline *vk_spline;
  // Optional quads that highlight the control points, for debugging.
  Gfx_Instance_Id cpoints[4];
} Gfx_Spline;

typedef struct {
  Gfx_Vulkan *vk;

  Gfx_Instance_Data instances;
  // maps { Gfx_Instance_Id => Gfx_Text }
  Hash_Map text_map;
  // maps { Gfx_Instance_Id => Gfx_Spline }
  Hash_Map spline_map;

  Font font;

  // view and inv_view are updated by the camera
  M44 view;
  M44 inv_view;
  M44 proj;
  M44 view_proj;
  M44 inv_view_proj;
  // Distance of the projection plane. The projection plane is such that the distance
  // between the top/bottom of it and the center is 1.
  // Its distance is equal to g == 1/tan(fovy/2).
  f32 proj_distance;
  f32 zoom; // @Cleanup: needed?
  V2 viewport_size_meters;
  Rect viewport_px;

  f32 shader_time;

  Gfx_Config config;
} Gfx;

internal
void gfx_update_projection(Gfx *gfx, f32 fov, f32 zoom)
{
  if (gfx->config.proj_mode == Proj_Ortho)
    gfx->proj = rev_ortho_proj(gfx->viewport_size_meters.x * zoom, gfx->viewport_size_meters.y * zoom, GFX_ORTHO_FAR_PLANE);
  else
    gfx->proj = rev_persp_proj(fov, gfx->viewport_size_meters.x / gfx->viewport_size_meters.y, GFX_PERSP_NEAR_PLANE);
  gfx->proj_distance = 1.f / tanf(fov * 0.5f);
  gfx->zoom = zoom;
}

internal
void gfx_update_matrices(Gfx *gfx, Camera *camera)
{
  camera_calc_view_and_inverse(camera, &gfx->view, &gfx->inv_view);

  gfx->view_proj = m44_mul(&gfx->proj, &gfx->view);
  gfx->inv_view_proj = m44_inverse(&gfx->view_proj);

  // @Speed: maybe do only if fov or proj mode changed
  gfx_update_projection(gfx, camera->fov, camera->zoom);
}

internal
Cpu_Instance_Data cpu_inst_data_default()
{
  Cpu_Instance_Data data = {};
  data.scale = v3(1, 1, 1);
  data.color_a = v4(1, 1, 1, 1);
  data.color_b = v4(1, 1, 1, 1);
  return data;
}

internal
void gfx_init_instances(Gfx_Instance_Obj_Data *instances, u64 initial_capacity, u64 max_capacity)
{
  u64 instance_size = sizeof(Gfx_Instance_Id) + sizeof(Cpu_Instance_Data);
  if (initial_capacity > max_capacity) {
    Temp scratch = scratch_begin(0, 0);

    WARN_TAG("Gfx", "Requested initial capacity of %" PRIu64 " is greater than max capacity %" PRIu64 " (%s): capping it.",
             initial_capacity, max_capacity, cstr(to_pretty_size(scratch.arena, max_capacity * instance_size)));
    initial_capacity = max_capacity;

    scratch_end(scratch);
  }
  
  u64 initial_capacity_bytes = initial_capacity * instance_size;
  u64 max_capacity_bytes = max_capacity * instance_size;
  for (u8 i = 0; i < countof(instances->arenas); ++i) {
    // @Speed: consider using a single arena and sub-allocating (we know the max size after all)
    instances->arenas[i] = arena_alloc_sized(max_capacity_bytes, initial_capacity_bytes);
  }
  instances->count = 0; 
  instances->capacity = initial_capacity;
  instances->max_capacity = max_capacity;
  instances->data = arena_push_array(Cpu_Instance_Data, instances->arenas[0], instances->capacity);
  instances->instance_ids = arena_push_array(Gfx_Instance_Id, instances->arenas[0], instances->capacity);
}

internal
void gfx_grow_instance_data_internal(Gfx_Instance_Obj_Data *instances, u64 new_capacity)
{
  u64 old_capacity = instances->capacity;
  assert(new_capacity > old_capacity);
  instances->capacity = new_capacity;

  INFO_TAG("Gfx", "Growing instances arena from %lu to %lu B (arena %u -> %u)", 
           old_capacity, new_capacity, instances->cur_arena, (instances->cur_arena + 1) % 2);
  
  // copy over the existing instance data to the new array and reset the old arena
  Arena *old_arena = instances->arenas[instances->cur_arena];
  u8 arena_id = (instances->cur_arena + 1) % countof(instances->arenas);
  instances->cur_arena = arena_id;
  Arena *arena = instances->arenas[arena_id];
  Cpu_Instance_Data *inst_data = arena_push_array(Cpu_Instance_Data, arena, new_capacity);
  Gfx_Instance_Id *inst_ids = arena_push_array(Gfx_Instance_Id, arena, new_capacity);
  memcpy(inst_data, instances->data, sizeof(Cpu_Instance_Data) * old_capacity);
  memcpy(inst_ids, instances->instance_ids, sizeof(Gfx_Instance_Id) * old_capacity);
  arena_pop_to(old_arena, 0);

  instances->data = inst_data;
  instances->instance_ids = inst_ids;
}

internal
void gfx_instance_reserve(Gfx_Instance_Obj_Data *instances, u64 capacity)
{
  if (capacity >= instances->max_capacity) {
    FATAL_TAG("Gfx", "Instance capacity limit reached! (%" PRIu64 ")", instances->max_capacity);
    os_abort();
  }
  
  if (capacity > instances->capacity) {
    // NOTE: always growing to at least double the current size
    gfx_grow_instance_data_internal(instances, Max(instances->capacity * 2, capacity));
  }
}

internal
Gfx_Instance_Id gfx_next_instance_id(Gfx_Instance_Data *instances)
{
  // next_inst_id_available starts from 0 (which is the invalid id), so we make sure
  // to pre-increment it to get a valid id.
  ++instances->next_inst_id_available.id;
  Gfx_Instance_Id id = instances->next_inst_id_available;
  return id;
}

internal
void gfx_debug_print_id_map(Gfx_Instance_Data *instances, Log_Level lv)
{
  if (g_loglv < lv)
    return;
  
  fae_log(lv, "Gfx", "{"); 
  for (u32 i = 0; i < instances->id_map.n_buckets; ++i) { 
    Hash_Bucket *bucket = hashmap_get_bucket(&instances->id_map, i); 
    if (!bucket->head) continue;

    for (Hash_Node *node = bucket->head; node; node = node->next) { 
      Gfx_Instance_Id *key = (Gfx_Instance_Id *)hashmap_get_node_key(&instances->id_map, node); 
      Gfx_Instance_Metadata *val = (Gfx_Instance_Metadata *)hashmap_get_node_val(&instances->id_map, node); 
      fae_log(lv, "Gfx", "    %lu -> %s, %u", key->id, g_Gfx_Obj_Type_str[val->obj_type], val->index_in_data); 
    } 
  } 
  fae_log(lv, "Gfx", "}"); 
}

internal
void gfx_debug_print_instance_ids(Gfx_Instance_Data *instances, Log_Level lv)
{
  if (g_loglv < lv)
    return;
  
  fae_log(lv, "Gfx", "------------");
  Temp scratch = scratch_begin(0, 0);
  for (u32 i = 0; i < GfxObj_COUNT; ++i) {
    Gfx_Instance_Obj_Data *inst = &instances->by_type[i];
    String8_Node *sn = NULL;
    for (u32 j = 0; j < inst->count; ++j) {
      Gfx_Instance_Id inst_id = inst->instance_ids[j];
      sn = push_str8_node(scratch.arena, sn, "%u", inst_id.id);
    }
    String8_Node *s = push_str8_node(scratch.arena, NULL, "%s: %s",
                                     g_Gfx_Obj_Type_str[i], cstr(str8_node_join(scratch.arena, sn, " | ")));
    fae_log(lv, "Gfx", "%s", cstr(s->str));
  }
  scratch_end(scratch);
}

internal
Gfx_Instance_Id gfx_push_instance_get_meta(Gfx_Instance_Data *instances, Cpu_Instance_Data data, Gfx_Obj_Type obj_type, 
                                           Gfx_Instance_Metadata *metadata)
{
  Gfx_Instance_Obj_Data *inst_by_type = &instances->by_type[obj_type];
  gfx_instance_reserve(inst_by_type, inst_by_type->count + 1);

  Gfx_Instance_Id id = gfx_next_instance_id(instances);
  assert(inst_by_type->count < UINT_MAX); // if this ever trips, change the type of idx_in_data to u64.
  u32 idx_in_data = inst_by_type->count++;
  inst_by_type->data[idx_in_data] = data;
  inst_by_type->instance_ids[idx_in_data] = id;
  metadata->index_in_data = idx_in_data;
  metadata->obj_type = obj_type;
  hashmap_add(&instances->id_map, &id, metadata);

  return id;
}

internal
Gfx_Instance_Id gfx_push_instance(Gfx_Instance_Data *instances, Cpu_Instance_Data data, Gfx_Obj_Type obj_type)
{
  Gfx_Instance_Metadata ignored;
  Gfx_Instance_Id id = gfx_push_instance_get_meta(instances, data, obj_type, &ignored);
  (void)ignored;
  return id;
}

internal
b8 gfx_pop_instance(Gfx_Instance_Data *instances, Gfx_Instance_Id inst_id,
                      Gfx_Instance_Id *swapped_inst_id, Gfx_Instance_Metadata *swapped_inst_metadata)
{
  Gfx_Instance_Metadata *metadata = hashmap_find(Gfx_Instance_Metadata, &instances->id_map, &inst_id);
  if (!metadata) {
    FATAL_TAG("Gfx", "Tried to remove inexistent instance %" PRIu64 "!", inst_id.id);
    os_abort();
  }


  Gfx_Instance_Obj_Data *inst_by_type = &instances->by_type[metadata->obj_type];
  u32 idx_in_data = metadata->index_in_data;
  DEBUG_TAG("Gfx", "popping instance %" PRIu64 " of type %s at idx %u. Instance count before = %" PRIu64,
            inst_id.id, g_Gfx_Obj_Type_str[metadata->obj_type], idx_in_data, inst_by_type->count);
  
  b8 swapped = false;
  if (idx_in_data != inst_by_type->count - 1) {
    // swap-remove the instance
    inst_by_type->data[idx_in_data] = inst_by_type->data[inst_by_type->count - 1];
    
    // remap the swapped id
    Gfx_Instance_Id swapped_id = inst_by_type->instance_ids[inst_by_type->count - 1];
    inst_by_type->instance_ids[idx_in_data] = swapped_id;
    Gfx_Instance_Metadata *swapped_metadata = hashmap_find(Gfx_Instance_Metadata, &instances->id_map, &swapped_id);
    assert(swapped_metadata);
    swapped_metadata->index_in_data = idx_in_data;

    if (swapped_inst_id)
      *swapped_inst_id = swapped_id;
    if (swapped_inst_metadata)
      *swapped_inst_metadata = *swapped_metadata;
    swapped = true;
  }
  --inst_by_type->count;

  hashmap_remove(&instances->id_map, &inst_id);

  return swapped;
}

internal
Cpu_Instance_Data *gfx_get_instance_data(Gfx *gfx, Gfx_Instance_Id instance_id)
{
  Gfx_Instance_Metadata *metadata = hashmap_find(Gfx_Instance_Metadata, &gfx->instances.id_map, &instance_id);
  Cpu_Instance_Data *data = NULL;
  if (metadata) {
    Gfx_Instance_Obj_Data *by_type = &gfx->instances.by_type[metadata->obj_type];
    data = &by_type->data[metadata->index_in_data];
  }
  return data;
}

typedef enum {
  Gfx_World_Space,
  Gfx_Screen_Space
} Gfx_Ref_Frame;

internal
Gfx_Instance_Id gfx_add_quad(Gfx *gfx, Cpu_Instance_Data inst_data)
{
  Gfx_Instance_Id id = gfx_push_instance(&gfx->instances, inst_data, GfxObj_Quad);
  return id;
}

internal
void gfx_remove_quad(Gfx *gfx, Gfx_Instance_Id quad_id)
{
  gfx_pop_instance(&gfx->instances, quad_id, NULL, NULL);
}

// Adds a screenspace quad which is always `size_in_pixels` pixels wide regardless of the viewport size.
internal
Gfx_Instance_Id gfx_add_screenspace_quad(Gfx *gfx, V2 size_in_pixels, Cpu_Instance_Data inst_data)
{
  inst_data.scale.x = size_in_pixels.x;
  inst_data.scale.y = size_in_pixels.y;
  Gfx_Instance_Id id = gfx_push_instance(&gfx->instances, inst_data, GfxObj_Screenspace_Quad);
  return id;
}

internal
void gfx_remove_screenspace_quad(Gfx *gfx, Gfx_Instance_Id quad_id)
{
  gfx_pop_instance(&gfx->instances, quad_id, NULL, NULL);
}

internal
void gfx_add_texts(Gfx *gfx, u32 n_texts, Text_Create_Data *text_data, Cpu_Instance_Data *inst_data, Gfx_Ref_Frame ref_frame,
                   Gfx_Instance_Id **out_text_ids)
{
  Temp scratch = scratch_begin(0, 0);
  
  // NOTE: the vertices are allocated in temporary storage, as they are not needed on CPU side
  // after uploading them to the GPU.
  Text *texts = arena_push_array_nozero(Text, scratch.arena, n_texts);
  // `vertices` contains all contiguous vertices of the created texts.
  Vertex *vertices = texts_create(scratch.arena, &gfx->font, n_texts, text_data, texts);
  
  // Prepare data to feed Vulkan
  u32 *vk_inst_ids = arena_push_array_nozero(u32, scratch.arena, n_texts);
  u32 *n_vertices = arena_push_array_nozero(u32, scratch.arena, n_texts);
  for (u32 i = 0; i < n_texts; ++i) {
    Gfx_Instance_Metadata metadata;
    *out_text_ids[i] = gfx_push_instance_get_meta(&gfx->instances, inst_data[i], GfxObj_Text, &metadata);
    vk_inst_ids[i] = metadata.index_in_data;
    n_vertices[i] = texts[i].n_vertices;
  }

  b8 screenspace = ref_frame == Gfx_Screen_Space;
  Vulk_Text **vtexts = arena_push_array(Vulk_Text*, scratch.arena, n_texts);
  vulk_add_texts(gfx->vk, vertices, n_texts, n_vertices, vk_inst_ids, screenspace, vtexts);

  for (u32 i = 0; i < n_texts; ++i) {
    Gfx_Text gtext = {};
    gtext.vk_text = vtexts[i];
    gtext.char_size = text_data[i].char_size;
    gtext.size = texts[i].local_size;
    hashmap_add(&gfx->text_map, out_text_ids[i], &gtext);
  }

  scratch_end(scratch);
}

internal
Gfx_Instance_Id gfx_add_text(Gfx *gfx, String8 string, Char_Size char_size, Cpu_Instance_Data inst_data, Gfx_Ref_Frame ref_frame)
{
  Temp scratch = scratch_begin(0, 0);
  
  // NOTE: the vertices are allocated in temporary storage, as they are not needed on CPU side
  // after uploading them to the GPU.
  Text text = text_create(scratch.arena, &gfx->font, string, char_size);
  b8 screenspace = ref_frame == Gfx_Screen_Space;
  Gfx_Instance_Metadata metadata;;
  Gfx_Instance_Id text_id = gfx_push_instance_get_meta(&gfx->instances, inst_data, GfxObj_Text, &metadata);
  u32 vk_inst_id = metadata.index_in_data;
  Vulk_Text *vtext = vulk_add_text(gfx->vk, text.vertices, text.n_vertices, vk_inst_id, screenspace);

  Gfx_Text gtext = {};
  gtext.vk_text = vtext;
  gtext.char_size = char_size;
  gtext.size = text.local_size;
  hashmap_add(&gfx->text_map, &text_id, &gtext);

  scratch_end(scratch);
  return text_id;
}

internal 
void gfx_remove_text(Gfx *gfx, Gfx_Instance_Id text_id)
{
  Gfx_Text *gtext = hashmap_find(Gfx_Text, &gfx->text_map, &text_id);
  assert(gtext);
  vulk_remove_text(gfx->vk, gtext->vk_text);

  Gfx_Instance_Id swapped_id;
  Gfx_Instance_Metadata swapped_metadata;
  if (gfx_pop_instance(&gfx->instances, text_id, &swapped_id, &swapped_metadata)) {
    // updated the swapped text's metadata
    Gfx_Text *swapped_gtext = hashmap_find(Gfx_Text, &gfx->text_map, &swapped_id);
    assert(swapped_gtext);
    vulk_set_text_instance_id(swapped_gtext->vk_text, swapped_metadata.index_in_data);
  }

  hashmap_remove(&gfx->text_map, &text_id);
}

internal
V2 gfx_get_text_size(Gfx *gfx, Gfx_Instance_Id text_id)
{
  Gfx_Text *gtext = hashmap_find(Gfx_Text, &gfx->text_map, &text_id);
  assert(gtext);
  return gtext->size;
}

internal
Gfx_Instance_Id gfx_add_spline(Gfx *gfx, V3 control_points[4], Cpu_Instance_Data inst_data)
{
  Temp scratch = scratch_begin(0, 0);
  
  Gfx_Instance_Metadata metadata;
  Gfx_Instance_Id spline_id = gfx_push_instance_get_meta(&gfx->instances, inst_data, GfxObj_Spline, &metadata);
  u32 vk_inst_id = metadata.index_in_data;
  Vulk_Spline *vspline = vulk_add_spline(gfx->vk, control_points, vk_inst_id);

  Gfx_Spline gspline = {};
  gspline.vk_spline = vspline;
  if (gfx->config.show_spline_control_points) {
    for (u32 i = 0; i < countof(gspline.cpoints); ++i) {
      Cpu_Instance_Data idata = {};
      idata.scale = v3(0.05, 0.05, 0.05);
      idata.color_a = idata.color_b = v4(1, 0, 0, 1);
      idata.pos = control_points[i];
      gspline.cpoints[i] = gfx_add_quad(gfx, idata);
    }
  }
  hashmap_add(&gfx->spline_map, &spline_id, &gspline);

  scratch_end(scratch);
  return spline_id;
}

internal 
void gfx_remove_spline(Gfx *gfx, Gfx_Instance_Id spline_id)
{
  Gfx_Spline *gspline = hashmap_find(Gfx_Spline, &gfx->spline_map, &spline_id);
  assert(gspline);
  vulk_remove_spline(gfx->vk, gspline->vk_spline);

  if (gfx_is_valid_id(gspline->cpoints[0])) {
    for (u32 i = 0; i < countof(gspline->cpoints); ++i) {
      gfx_remove_quad(gfx, gspline->cpoints[i]);
    }
  }

  Gfx_Instance_Id swapped_id;
  Gfx_Instance_Metadata swapped_metadata;
  if (gfx_pop_instance(&gfx->instances, spline_id, &swapped_id, &swapped_metadata)) {
    // updated the swapped spline's metadata
    Gfx_Spline *swapped_gspline = hashmap_find(Gfx_Spline, &gfx->spline_map, &swapped_id);
    assert(swapped_gspline);
    vulk_set_spline_instance_id(gfx->vk, swapped_gspline->vk_spline, swapped_metadata.index_in_data);
  }

  hashmap_remove(&gfx->spline_map, &spline_id);
}

internal
void gfx_update_spline(Gfx *gfx, Gfx_Instance_Id spline_id, const V3 control_points[4])
{
  Gfx_Spline *gspline = hashmap_find(Gfx_Spline, &gfx->spline_map, &spline_id);
  assert(gspline);
  vulk_update_spline_points(gfx->vk, gspline->vk_spline, control_points);

  if (gfx_is_valid_id(gspline->cpoints[0])) {
    for (u32 i = 0; i < countof(gspline->cpoints); ++i) {
      Cpu_Instance_Data *idata = gfx_get_instance_data(gfx, gspline->cpoints[i]);
      assert(idata);
      idata->pos = control_points[i];
    }
  }
}

internal
void gfx_set_text_string(Gfx *gfx, Gfx_Instance_Id text_id, String8 string)
{
  Gfx_Text *gtext = hashmap_find(Gfx_Text, &gfx->text_map, &text_id);
  assert(gtext);
  
  Temp scratch = scratch_begin(0, 0);
  
  // NOTE: the vertices are allocated in temporary storage, as they are not needed on CPU side
  // after uploading them to the GPU.
  Text text = text_create(scratch.arena, &gfx->font, string, gtext->char_size);
  vulk_set_text_vertices(gfx->vk, gtext->vk_text, text.vertices, text.n_vertices);

  scratch_end(scratch);
}

internal
void gfx_init(Gfx *gfx, Arena *arena, GLFWwindow *window, Gfx_Config cfg)
{
  gfx->config = cfg;
  gfx->config.proj_mode = cfg.proj_mode;
  // Heap-allocate Gfx_Vulkan because it's quite large
  gfx->vk = arena_push(Gfx_Vulkan, arena);

  Temp scratch = scratch_begin(&arena, 1);

  String8 font_img_file = push_str8f(scratch.arena, "%s/%s_msdf.png", cstr(cfg.fonts_dir), cstr(cfg.font_name));
  String8 font_meta_file = push_str8f(scratch.arena, "%s/%s_meta.csv", cstr(cfg.fonts_dir), cstr(cfg.font_name));
  if (!font_load_from_file(font_img_file, font_meta_file, &gfx->font)) {
    FATAL_TAG("Gfx", "Failed to load font");
    os_abort();
  }

  // Load crosshair image
  String8 crosshair_img_file = push_str8f(scratch.arena, "%s/crosshair.png", cstr(cfg.textures_dir));
  Image crosshair_img = image_load_from_file(crosshair_img_file);

  gfx->instances.id_map = hashmap_init_default(Gfx_Instance_Id, Gfx_Instance_Metadata, arena, 4096);

  Vulk_Init_Config vk_cfg = {};
  vk_cfg.use_srgb = gfx->config.srgb;
  vk_cfg.prefer_integrated_gpu = gfx->config.prefer_integrated_gpu;
  vk_cfg.n_obj_types = GfxObj_COUNT;
  vk_cfg.spline_obj_type = GfxObj_Spline;
  vk_cfg.obj_instance_sizes = arena_push_array_nozero(u64, scratch.arena, vk_cfg.n_obj_types);
  for (u32 i = 0; i < vk_cfg.n_obj_types; ++i)
    vk_cfg.obj_instance_sizes[i] = sizeof(Gpu_Instance_Data);
  vk_cfg.text_vbuf_size = GFX_TEXT_VBUF_SIZE;
  vk_cfg.font_image = gfx->font.image;
  vk_cfg.crosshair_image = crosshair_img;
  vk_cfg.shaders_dir = cfg.shaders_dir;
  VkPhysicalDeviceProperties phd_props = vulk_pre_init(gfx->vk, window, &vk_cfg);

  b8 use_simple_skybox = phd_props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
  if (use_simple_skybox)
    INFO_TAG("Gfx", "Non-discrete GPU detected: using lower graphics settings");

  vk_cfg.n_pipeline_cfgs = vulk_setup_pipeline_configs(scratch.arena, &vk_cfg.pipeline_cfgs, use_simple_skybox, cfg.wireframe);

  Vulk_Init_Result vk_res = {};
  vk_res.instance_capacity = arena_push_array_nozero(u32, scratch.arena, vk_cfg.n_obj_types);
  DEBUG_TAG("Gfx", "Initting Vulkan...");
  vulk_init(gfx->vk, &vk_cfg, &vk_res);

  // NOTE: initialize instances after calling vulk_init(), so we know the exact instances capacity.
  DEBUG_TAG("Gfx", "Initting gfx instances...");
  for (u32 i = 0; i < GfxObj_COUNT; ++i)
    gfx_init_instances(&gfx->instances.by_type[i], cfg.initial_instances_capacity[i], vk_res.instance_capacity[i]);
  
  gfx->text_map = hashmap_init_default(Gfx_Instance_Id, Gfx_Text, arena, 2048);
  gfx->spline_map = hashmap_init_default(Gfx_Instance_Id, Gfx_Spline, arena, 2048);

  // Crosshair
  Cpu_Instance_Data idata = cpu_inst_data_default();
  gfx_add_screenspace_quad(gfx, v2(crosshair_img.width, crosshair_img.height), idata);

  // Images can be unloaded after initting vulk (they have been uploaded to the GPU already)
  image_unload(crosshair_img);
  image_unload(gfx->font.image);

  scratch_end(scratch);
}

internal
void gfx_deinit(Gfx *gfx)
{  
  for (u32 type = 0; type < countof(gfx->instances.by_type); ++type) {
    for (u8 i = 0; i < countof(gfx->instances.by_type[type].arenas); ++i) {
      arena_release(gfx->instances.by_type[type].arenas[i]);
    }
  }

  vulk_deinit(gfx->vk);
}

internal
void gfx_resized(Gfx *gfx, GLFWwindow *window)
{
  i32 width, height;
  glfwGetFramebufferSize(window, &width, &height);

  f32 ppm = gfx->config.pixels_per_meter;
  gfx->viewport_size_meters = v2(width / ppm, height / ppm);
  gfx->viewport_px = rect_pos_size(v2(0, 0), v2(width, height));
  
  gfx->vk->swapchain_stale = true;
}

internal
void gfx_upload_instance_data(Gfx *gfx, Gfx_Instance_Obj_Data *instances, Gfx_Obj_Type obj_type, u64 instance_idx)
{
  Cpu_Instance_Data *cpu_data = &instances->data[instance_idx];
  Gpu_Instance_Data gpu_data = {};
  // if (obj_type == GfxObj_Spline || obj_type == GfxObj_Text)
  //   INFO_TAG("Gfx", "%s  %p: %f", g_Gfx_Obj_Type_str[obj_type], cpu_data, cpu_data->pos.z);
  gpu_data.model = transform_from_pos_rot_scale(cpu_data->pos, cpu_data->rot, cpu_data->scale);
  gpu_data.color_a = cpu_data->color_a;
  gpu_data.color_b = cpu_data->color_b;

  vulk_update_instance_buffer(gfx->vk, obj_type, &gpu_data, sizeof(Gpu_Instance_Data), instance_idx * sizeof(Gpu_Instance_Data));
}

internal
void gfx_update_instance_data(Gfx *gfx)
{
  // @Speed
  // We should selectively copy only things that changed from last frame.
  for (Gfx_Obj_Type obj_type = 0; obj_type < GfxObj_COUNT; ++obj_type) {
    Gfx_Instance_Obj_Data *inst_by_type = &gfx->instances.by_type[obj_type];
    for (u64 i = 0; i < inst_by_type->count; ++i)
      gfx_upload_instance_data(gfx, inst_by_type, obj_type, i);
  }
}

internal
Frustum gfx_build_frustum(Gfx *gfx)
{  
  if (gfx->config.proj_mode == Proj_Ortho) {
    V2 viewp_size = v2(
      gfx->viewport_size_meters.x * gfx->zoom * 0.5,
      gfx->viewport_size_meters.y * gfx->zoom * 0.5);
    Frustum f = build_ortho_frustum(&gfx->inv_view, viewp_size, GFX_ORTHO_NEAR_PLANE);
    return f;
  } else {
    f32 aspect_ratio = gfx->viewport_size_meters.x / gfx->viewport_size_meters.y;
    Frustum f = build_persp_frustum(&gfx->inv_view, &gfx->view,
                                    gfx->proj_distance, aspect_ratio,
                                    GFX_PERSP_NEAR_PLANE);
    return f;
  }
}

internal
void gfx_update_push_constants(Gfx *gfx, Vulk_Draw_Data *draw_data)
{
  Vulk_Draw_Push_Const_Gfx *pc_gfx = &draw_data->pc.gfx;
  pc_gfx->view_proj = m44_mul(&gfx->proj, &gfx->view);

  Vulk_Draw_Push_Const_Spline *pc_spline = &draw_data->pc.spline;
  pc_spline->view_proj = pc_gfx->view_proj;
  // pc_spline->camera_wpos = gfx->inv_view.col[3];
  pc_spline->inv_view_proj = gfx->inv_view_proj;

  V4 view_pos = gfx->inv_view.col[3];
  Vulk_Draw_Push_Const_Spline_Build *pc_spline_build = &draw_data->pc.spline_build;
  pc_spline_build->view_pos_ws_thickness = v4(
    view_pos.x, view_pos.y, view_pos.z,
    GFX_SPLINE_THICKNESS
  );

  // Screenspace text needs a special view-projection
  Vulk_Draw_Push_Const_Gfx *pc_ss_text = &draw_data->pc.ss_text;
  // INFO("vp size: %f,%f px (%f,%f m)", gfx->viewport_px.width, gfx->viewport_px.height,
  //      gfx->viewport_size_meters.x, gfx->viewport_size_meters.y);
  // FIXME: it only really works as intended when the text position is (0, 0). Probably
  // we're not handling resizes correctly.
  // The idea is that we want a text in (0, 0) to be on the bottom left (or top left, whatever)
  // and one in (1, 1) to be in the bottom right (or top right).
  // The text vertices are defined in meters in their local space.
  pc_ss_text->view_proj = m44(
    2.f / gfx->viewport_size_meters.x, 0, 0, -1,
    0, 0, -2.f / gfx->viewport_size_meters.y, 1,
    0, 0, 0, 0,
    0, 0, 0, 1
  );

  Vulk_Draw_Push_Const_Skybox *pc_skybox = &draw_data->pc.skybox;
  M44 skybox_view = m44(0, 0, 1, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1);
  skybox_view = m44_mul(&gfx->view, &skybox_view);
  pc_skybox->view_proj = m44_mul(&gfx->proj, &skybox_view);
  // A nice sky gradient
  // pc_skybox->color_a = v4(.9f, 1.f, .98f, 1.f);
  // pc_skybox->color_b = v4(.98f, .745f, 1.f, 1.f);
  // pc_skybox->color_c = v4(.65f, .65f, 1.f, 1.f);
  pc_skybox->color_b = v4(.06f, .14f, .29f, 1.f);
  pc_skybox->color_a = v4(.1f, .13f, .20f, 1.f);
  pc_skybox->color_c = v4(.06f, 0.20f, 0.21f, 1.f);
  pc_skybox->time = gfx->shader_time;

  Vulk_Draw_Push_Const_Screenspace_Quad *pc_ss_quad = &draw_data->pc.ss_quad;
  pc_ss_quad->inv_viewport_size = v2(1.f / gfx->viewport_px.width, 
                                     1.f / gfx->viewport_px.height);
}

internal
void gfx_draw(Gfx *gfx, GLFWwindow *window, f32 delta_seconds)
{
  gfx->shader_time += delta_seconds;

  gfx_update_instance_data(gfx);

  Vulk_Draw_Data draw_data = {};
  draw_data.n_quads = gfx->instances.by_type[GfxObj_Quad].count;
  // Only show the crosshair in perspective mode.
  // XXX: currently assuming the only screenspace quad is the Pshair.
  draw_data.n_screenspace_quads = (gfx->config.proj_mode == Proj_Persp) * gfx->instances.by_type[GfxObj_Screenspace_Quad].count;
  // draw_data.frustum_ws = gfx_build_frustum(gfx);
  draw_data.cam_pos = v3_from_v4(gfx->inv_view.col[3]);
  draw_data.cull_distance = 100.f;
  draw_data.proj_is_ortho = gfx->config.proj_mode == Proj_Ortho;
  draw_data.clear_color = v3(0.012, 0.015, 0.04);
  gfx_update_push_constants(gfx, &draw_data);

  vulk_do_frame(gfx->vk, window, &draw_data);
}

internal
void gfx_end_main_loop(Gfx *gfx)
{
  vulk_wait_idle(gfx->vk);
}
