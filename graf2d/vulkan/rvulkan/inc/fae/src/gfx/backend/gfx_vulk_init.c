typedef enum {
  GfxObj_Quad,
  GfxObj_Text,
  GfxObj_Screenspace_Quad,
  GfxObj_Spline,
  GfxObj_COUNT
} Gfx_Obj_Type;

internal const char *const g_Gfx_Obj_Type_str[GfxObj_COUNT] = {
  "Quad", "Screenspace Quad", "Text", "Spline"
};

typedef enum {
  Pipeline_Quad,
  Pipeline_Text,
  Pipeline_Screenspace_Quad,
  Pipeline_Skybox,
  Pipeline_Spline,
  Pipeline_Spline_Build,
  Pipeline_COUNT
} Vulk_Init_Pipeline_Id;

internal
VkDescriptorSetLayout vulk_create_text_descriptor_set_layout(VkDevice device)
{
  VkDescriptorSetLayoutBinding storage_buf_layout_binding = {};
  storage_buf_layout_binding.binding = 0;
  storage_buf_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  storage_buf_layout_binding.descriptorCount = 1;
  storage_buf_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding sampler_layout_binding = {};
  sampler_layout_binding.binding = 1;
  sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_layout_binding.descriptorCount = 1;
  sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBinding layout_bindings[] = {
    storage_buf_layout_binding,
    sampler_layout_binding,
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = countof(layout_bindings);
  layout_info.pBindings = layout_bindings;

  VkDescriptorSetLayout desc_set_layout;
  VkResult res = vkCreateDescriptorSetLayout(device, &layout_info, NULL, &desc_set_layout);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create text descriptor set layout");
    os_abort();
  }

  return desc_set_layout;
}

internal
VkDescriptorSetLayout vulk_create_quad_descriptor_set_layout(VkDevice device)
{
  VkDescriptorSetLayoutBinding inst_data_layout_binding = {};
  inst_data_layout_binding.binding = 0;
  inst_data_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  inst_data_layout_binding.descriptorCount = 1;
  inst_data_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding layout_bindings[] = {
    inst_data_layout_binding,
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = countof(layout_bindings);
  layout_info.pBindings = layout_bindings;

  VkDescriptorSetLayout desc_set_layout;
  VkResult res = vkCreateDescriptorSetLayout(device, &layout_info, NULL, &desc_set_layout);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create descriptor set layout");
    os_abort();
  }

  return desc_set_layout;
}

internal
VkDescriptorSetLayout vulk_create_screenspace_quad_descriptor_set_layout(VkDevice device)
{
  VkDescriptorSetLayoutBinding inst_data_layout_binding = {};
  inst_data_layout_binding.binding = 0;
  inst_data_layout_binding.descriptorCount = 1;
  inst_data_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  inst_data_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding sampler_layout_binding = {};
  sampler_layout_binding.binding = 1;
  sampler_layout_binding.descriptorCount = 1;
  sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_layout_binding.pImmutableSamplers = NULL;
  sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBinding layout_bindings[] = {
    inst_data_layout_binding,
    sampler_layout_binding
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = countof(layout_bindings);
  layout_info.pBindings = layout_bindings;

  VkDescriptorSetLayout desc_set_layout;
  VkResult res = vkCreateDescriptorSetLayout(device, &layout_info, NULL, &desc_set_layout);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create descriptor set layout");
    os_abort();
  }

  return desc_set_layout;
}

internal
VkDescriptorSetLayout vulk_create_skybox_descriptor_set_layout(VkDevice device)
{
  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

  VkDescriptorSetLayout desc_set_layout;
  VkResult res = vkCreateDescriptorSetLayout(device, &layout_info, NULL, &desc_set_layout);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create descriptor set layout");
    os_abort();
  }
  return desc_set_layout;
}

internal
VkDescriptorSetLayout vulk_create_spline_descriptor_set_layout(VkDevice device)
{
  VkDescriptorSetLayoutBinding inst_data_layout_binding = {};
  inst_data_layout_binding.binding = 0;
  inst_data_layout_binding.descriptorCount = 1;
  inst_data_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  inst_data_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutBinding layout_bindings[] = {
    inst_data_layout_binding,
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = countof(layout_bindings);
  layout_info.pBindings = layout_bindings;

  VkDescriptorSetLayout desc_set_layout;
  VkResult res = vkCreateDescriptorSetLayout(device, &layout_info, NULL, &desc_set_layout);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create descriptor set layout");
    os_abort();
  }
  return desc_set_layout;
}

internal
VkDescriptorSetLayout vulk_create_spline_build_descriptor_set_layout(VkDevice device)
{
  VkDescriptorSetLayoutBinding cpoints_layout_binding = {};
  cpoints_layout_binding.binding = 0;
  cpoints_layout_binding.descriptorCount = 1;
  cpoints_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  cpoints_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding out_buf_layout_binding = {};
  out_buf_layout_binding.binding = 1;
  out_buf_layout_binding.descriptorCount = 1;
  out_buf_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  out_buf_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutBinding layout_bindings[] = {
    cpoints_layout_binding,
    out_buf_layout_binding
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = countof(layout_bindings);
  layout_info.pBindings = layout_bindings;

  VkDescriptorSetLayout desc_set_layout;
  VkResult res = vkCreateDescriptorSetLayout(device, &layout_info, NULL, &desc_set_layout);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create descriptor set layout");
    os_abort();
  }
  return desc_set_layout;
}

internal
void vulk_configure_quad_desc_sets(Gfx_Vulkan *vk)
{
  VkDevice device = vk->device;
  Vulk_Buffer *quad_instance_buf = &vk->instance_buf[GfxObj_Quad].buffer;
  VkDescriptorSet *desc_sets = vk->pipelines[Pipeline_Quad].desc_sets;
  u32 n_desc_sets = countof(vk->pipelines[Pipeline_Quad].desc_sets);

  VkDescriptorBufferInfo instance_buf_info = {};
  instance_buf_info.buffer = quad_instance_buf->buffer;
  instance_buf_info.range = quad_instance_buf->size;
  instance_buf_info.offset = quad_instance_buf->offset;

  for (u64 i = 0; i < n_desc_sets; ++i) {
    VkWriteDescriptorSet inst_data_desc_write = {};
    inst_data_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inst_data_desc_write.dstSet = desc_sets[i];
    inst_data_desc_write.dstBinding = 0;
    inst_data_desc_write.dstArrayElement = 0;
    inst_data_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inst_data_desc_write.descriptorCount = 1;
    inst_data_desc_write.pBufferInfo = &instance_buf_info;

    VkWriteDescriptorSet desc_writes[] = {
      inst_data_desc_write,
    };

    vkUpdateDescriptorSets(device, countof(desc_writes), desc_writes, 0, NULL);
  }
}

internal
void vulk_configure_text_desc_sets(Gfx_Vulkan *vk)
{
  VkDevice device = vk->device;
  Vulk_Buffer *instance_buf = &vk->instance_buf[GfxObj_Text].buffer;
  VkDescriptorSet *desc_sets = vk->pipelines[Pipeline_Text].desc_sets;

  VkDescriptorBufferInfo storage_buf_info = {};
  storage_buf_info.buffer = instance_buf->buffer;
  storage_buf_info.range = instance_buf->size;
  storage_buf_info.offset = instance_buf->offset;

  VkDescriptorImageInfo image_info = {};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = vk->font_image.view;
  image_info.sampler = vk->font_image_sampler;

  for (u64 i = 0; i < countof(vk->pipelines[Pipeline_Text].desc_sets); ++i) {
    VkWriteDescriptorSet storage_desc_write = {};
    storage_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    storage_desc_write.dstSet = desc_sets[i];
    storage_desc_write.dstBinding = 0;
    storage_desc_write.dstArrayElement = 0;
    storage_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storage_desc_write.descriptorCount = 1;
    storage_desc_write.pBufferInfo = &storage_buf_info;

    VkWriteDescriptorSet image_desc_write = {};
    image_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image_desc_write.dstSet = desc_sets[i];
    image_desc_write.dstBinding = 1;
    image_desc_write.dstArrayElement = 0;
    image_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image_desc_write.descriptorCount = 1;
    image_desc_write.pImageInfo = &image_info;

    VkWriteDescriptorSet desc_writes[] = {
      storage_desc_write,
      image_desc_write
    };

    vkUpdateDescriptorSets(device, countof(desc_writes), desc_writes, 0, NULL);
  }
}

internal
void vulk_configure_screenspace_quad_desc_sets(Gfx_Vulkan *vk)
{
  VkDevice device = vk->device;
  Vulk_Buffer *instance_buf = &vk->instance_buf[GfxObj_Screenspace_Quad].buffer;
  VkDescriptorSet *desc_sets = vk->pipelines[Pipeline_Screenspace_Quad].desc_sets;
  u32 n_desc_sets = countof(vk->pipelines[Pipeline_Screenspace_Quad].desc_sets);

  VkDescriptorBufferInfo instance_buf_info = {};
  instance_buf_info.buffer = instance_buf->buffer;
  instance_buf_info.range = instance_buf->size;
  instance_buf_info.offset = instance_buf->offset;

  // @Incomplete
  Vulk_Image *image = &vk->images.images[0];
  VkSampler *sampler = &vk->images.samplers[0];

  VkDescriptorImageInfo image_info = {};
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_info.imageView = image->view;
  image_info.sampler = *sampler;

  for (u64 i = 0; i < n_desc_sets; ++i) {
    VkWriteDescriptorSet inst_data_desc_write = {};
    inst_data_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inst_data_desc_write.dstSet = desc_sets[i];
    inst_data_desc_write.dstBinding = 0;
    inst_data_desc_write.dstArrayElement = 0;
    inst_data_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inst_data_desc_write.descriptorCount = 1;
    inst_data_desc_write.pBufferInfo = &instance_buf_info;

    VkWriteDescriptorSet image_desc_write = {};
    image_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image_desc_write.dstSet = desc_sets[i];
    image_desc_write.dstBinding = 1;
    image_desc_write.dstArrayElement = 0;
    image_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    image_desc_write.descriptorCount = 1;
    image_desc_write.pImageInfo = &image_info;

    VkWriteDescriptorSet desc_writes[] = {
      inst_data_desc_write,
      image_desc_write
    };

    vkUpdateDescriptorSets(device, countof(desc_writes), desc_writes, 0, NULL);
  }
}

internal
void vulk_configure_spline_desc_sets(Gfx_Vulkan *vk)
{
  VkDevice device = vk->device;
  Vulk_Buffer *spline_instance_buf = &vk->instance_buf[GfxObj_Spline].buffer;
  VkDescriptorSet *desc_sets = vk->pipelines[Pipeline_Spline].desc_sets;
  u32 n_desc_sets = countof(vk->pipelines[Pipeline_Spline].desc_sets);

  VkDescriptorBufferInfo instance_buf_info = {};
  instance_buf_info.buffer = spline_instance_buf->buffer;
  instance_buf_info.range = spline_instance_buf->size;
  instance_buf_info.offset = spline_instance_buf->offset;

  for (u64 i = 0; i < n_desc_sets; ++i) {
    VkWriteDescriptorSet inst_data_desc_write = {};
    inst_data_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inst_data_desc_write.dstSet = desc_sets[i];
    inst_data_desc_write.dstBinding = 0;
    inst_data_desc_write.dstArrayElement = 0;
    inst_data_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inst_data_desc_write.descriptorCount = 1;
    inst_data_desc_write.pBufferInfo = &instance_buf_info;

    VkWriteDescriptorSet desc_writes[] = {
      inst_data_desc_write,
    };

    vkUpdateDescriptorSets(device, countof(desc_writes), desc_writes, 0, NULL);
  }
}

internal
void vulk_configure_spline_build_desc_sets(Gfx_Vulkan *vk)
{
  VkDevice device = vk->device;
  VkDescriptorSet *desc_sets = vk->pipelines[Pipeline_Spline_Build].desc_sets;
  u32 n_desc_sets = countof(vk->pipelines[Pipeline_Spline_Build].desc_sets);

  VkDescriptorBufferInfo cpoints_buf_info = {};
  cpoints_buf_info.buffer = vk->splines.cpoints_buf.buffer;
  cpoints_buf_info.range = vk->splines.cpoints_buf.size;
  cpoints_buf_info.offset = vk->splines.cpoints_buf.offset;

  VkDescriptorBufferInfo out_buf_info = {};
  out_buf_info.buffer = vk->splines.vbuffer.buffer;
  out_buf_info.range = vk->splines.vbuffer.size;
  out_buf_info.offset = vk->splines.vbuffer.offset;

  for (u64 i = 0; i < n_desc_sets; ++i) {
    VkWriteDescriptorSet cpoints_desc_write = {};
    cpoints_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cpoints_desc_write.dstSet = desc_sets[i];
    cpoints_desc_write.dstBinding = 0;
    cpoints_desc_write.dstArrayElement = 0;
    cpoints_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cpoints_desc_write.descriptorCount = 1;
    cpoints_desc_write.pBufferInfo = &cpoints_buf_info;

    VkWriteDescriptorSet out_buf_desc_write = {};
    out_buf_desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    out_buf_desc_write.dstSet = desc_sets[i];
    out_buf_desc_write.dstBinding = 1;
    out_buf_desc_write.dstArrayElement = 0;
    out_buf_desc_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    out_buf_desc_write.descriptorCount = 1;
    out_buf_desc_write.pBufferInfo = &out_buf_info;

    VkWriteDescriptorSet desc_writes[] = {
      cpoints_desc_write,
      out_buf_desc_write
    };

    vkUpdateDescriptorSets(device, countof(desc_writes), desc_writes, 0, NULL);
  }
}

internal
VkVertexInputBindingDescription *vulk_make_vertex_binding_desc_default(Arena *arena, u32 *n_desc)
{
  *n_desc = 1;
  VkVertexInputBindingDescription *desc = arena_push_array(VkVertexInputBindingDescription, arena, *n_desc);
  desc[0].binding = 0;
  desc[0].stride = sizeof(Vertex);
  desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return desc;
}

internal
VkVertexInputAttributeDescription *vulk_make_vertex_attr_desc_default(Arena *arena, u32 *n_desc)
{
  *n_desc = 2;
  VkVertexInputAttributeDescription *desc = arena_push_array(VkVertexInputAttributeDescription, arena, *n_desc);
  desc[0] = (VkVertexInputAttributeDescription){
    .binding = 0,
    .location = 0,
    .format = VK_FORMAT_R32G32B32_SFLOAT,
    .offset = offsetof(Vertex, pos)
  };
  desc[1] = (VkVertexInputAttributeDescription){
    .binding = 0,
    .location = 1,
    .format = VK_FORMAT_R32G32_SFLOAT,
    .offset = offsetof(Vertex, uv)
  };
  return desc;
}

internal
VkVertexInputBindingDescription *vulk_make_vertex_binding_desc_spline(Arena *arena, u32 *n_desc)
{
  *n_desc = 1;
  VkVertexInputBindingDescription *desc = arena_push_array(VkVertexInputBindingDescription, arena, *n_desc);
  desc[0].binding = 0;
  desc[0].stride = sizeof(V4);
  desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return desc;
}

internal
VkVertexInputAttributeDescription *vulk_make_vertex_attr_desc_spline(Arena *arena, u32 *n_desc)
{
  *n_desc = 1;
  VkVertexInputAttributeDescription *desc = arena_push_array(VkVertexInputAttributeDescription, arena, *n_desc);
  desc[0] = (VkVertexInputAttributeDescription){
    .binding = 0,
    .location = 0,
    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
    .offset = 0
  };
  return desc;
}

typedef struct {
  V2 viewport_size_px;  
  f32 thickness;
} Vulk_Draw_Spline_Ubo;

typedef struct {
  _Alignas(16) M44 view_proj;
} Vulk_Draw_Push_Const_Gfx;

typedef struct {
  // vertex
  _Alignas(16) M44 view_proj;
  // fragment
  M44 inv_view_proj;
} Vulk_Draw_Push_Const_Spline;

typedef struct {
  V2 inv_viewport_size;
} Vulk_Draw_Push_Const_Screenspace_Quad;

typedef struct {
  V4 view_pos_ws_thickness;
} Vulk_Draw_Push_Const_Spline_Build;

typedef struct {
  // Vertex
  M44 view_proj;

  // Fragment
  V4 color_a;
  V4 color_b;
  V4 color_c;
  f32 time; // XXX: this might make sense to pass as an app-global descriptor
} Vulk_Draw_Push_Const_Skybox;

internal
u32 vulk_setup_pipeline_configs(Arena *arena, Vulk_Pipeline_Cfg **configs, b8 use_simple_skybox, b8 wireframe_mode)
{
  u32 pflags = wireframe_mode ? VULK_GFX_PIPELINE_WIREFRAME : 0;

  *configs = arena_push_array(Vulk_Pipeline_Cfg, arena, Pipeline_COUNT);
  
  // Quads
  {
    Vulk_Pipeline_Cfg *cfg = &(*configs)[Pipeline_Quad];
    cfg->shader = (Shader_File) { .vert = str8("quad.vert"), .frag = str8("node.frag") };
    cfg->n_push_const_ranges = 1;
    cfg->push_const_ranges = arena_push_array_nozero(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0] = (VkPushConstantRange) {
      .size = sizeof(Vulk_Draw_Push_Const_Gfx),
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    cfg->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    cfg->flags = pflags | VULK_GFX_PIPELINE_NO_CULL;
    cfg->create_desc_set = vulk_create_quad_descriptor_set_layout;
    cfg->configure_desc_sets = vulk_configure_quad_desc_sets;
  }

  // Text
  {
    Vulk_Pipeline_Cfg *cfg = &(*configs)[Pipeline_Text];
    cfg->shader = (Shader_File) { .vert = str8("base.vert"), .frag = str8("msdf.frag") };
    cfg->n_push_const_ranges = 1;
    cfg->push_const_ranges = arena_push_array_nozero(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0] = (VkPushConstantRange) {
      .size = sizeof(Vulk_Draw_Push_Const_Gfx),
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    cfg->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    cfg->flags = pflags | VULK_GFX_PIPELINE_TRANSPARENT | VULK_GFX_PIPELINE_NO_CULL;
    cfg->vert_bindings = vulk_make_vertex_binding_desc_default(arena, &cfg->n_vert_bindings);
    cfg->vert_attrs = vulk_make_vertex_attr_desc_default(arena, &cfg->n_vert_attrs);
    cfg->create_desc_set = vulk_create_text_descriptor_set_layout;
    cfg->configure_desc_sets = vulk_configure_text_desc_sets;
  }

  // Screenspace quads
  {
    Vulk_Pipeline_Cfg *cfg = &(*configs)[Pipeline_Screenspace_Quad];
    cfg->shader = (Shader_File) { .vert = str8("screenspace_quad.vert"), .frag = str8("textured.frag") };
    cfg->n_push_const_ranges = 1;
    cfg->push_const_ranges = arena_push_array_nozero(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0] = (VkPushConstantRange) {
      .size = sizeof(Vulk_Draw_Push_Const_Screenspace_Quad),
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    cfg->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    cfg->flags = pflags | VULK_GFX_PIPELINE_TRANSPARENT | VULK_GFX_PIPELINE_NO_DEPTH_WRITE 
                        | VULK_GFX_PIPELINE_NO_DEPTH_TEST | VULK_GFX_PIPELINE_NO_CULL;
    cfg->create_desc_set = vulk_create_screenspace_quad_descriptor_set_layout;
    cfg->configure_desc_sets = vulk_configure_screenspace_quad_desc_sets;
  }

  // Skybox
  {
    Vulk_Pipeline_Cfg *cfg = &(*configs)[Pipeline_Skybox];
    cfg->shader = (Shader_File) { 
      .vert = str8("skybox.vert"), 
      .frag = use_simple_skybox ? str8("skybox_simple.frag") : str8("skybox.frag") 
    };
    cfg->n_push_const_ranges = 2;
    cfg->push_const_ranges = arena_push_array_nozero(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0] = (VkPushConstantRange) {
      .size = sizeof(M44),
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    cfg->push_const_ranges[1] = (VkPushConstantRange) {
      .offset = offsetof(Vulk_Draw_Push_Const_Skybox, color_a),
      .size = sizeof(Vulk_Draw_Push_Const_Skybox) - offsetof(Vulk_Draw_Push_Const_Skybox, color_a),
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    cfg->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // XXX: we should do backface culling (or rather, frontface), but currently it doesn't work in ortho!
    cfg->flags = pflags | VULK_GFX_PIPELINE_NO_DEPTH_WRITE | VULK_GFX_PIPELINE_NO_CULL;
    cfg->vert_bindings = vulk_make_vertex_binding_desc_default(arena, &cfg->n_vert_bindings);
    cfg->vert_attrs = vulk_make_vertex_attr_desc_default(arena, &cfg->n_vert_attrs);
    cfg->create_desc_set = vulk_create_skybox_descriptor_set_layout;
  }

  // Splines
  {
    Vulk_Pipeline_Cfg *cfg = &(*configs)[Pipeline_Spline];
    cfg->shader = (Shader_File) { .vert = str8("spline.vert"), .frag = str8("spline.frag") };
    cfg->n_push_const_ranges = 2;
    cfg->push_const_ranges = arena_push_array_nozero(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0] = (VkPushConstantRange) {
      .size = sizeof(M44),
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
    };
    cfg->push_const_ranges[1] = (VkPushConstantRange) {
      .offset = sizeof(M44),
      .size = sizeof(M44),
      .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };
    cfg->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    cfg->flags = pflags | VULK_GFX_PIPELINE_NO_CULL;
    cfg->vert_bindings = vulk_make_vertex_binding_desc_spline(arena, &cfg->n_vert_bindings);
    cfg->vert_attrs = vulk_make_vertex_attr_desc_spline(arena, &cfg->n_vert_attrs);
    cfg->create_desc_set = vulk_create_spline_descriptor_set_layout;
    cfg->configure_desc_sets = vulk_configure_spline_desc_sets;
  }

  // Spline Build
  {
    Vulk_Pipeline_Cfg *cfg = &(*configs)[Pipeline_Spline_Build];
    cfg->shader = (Shader_File) { .comp = str8("spline_build.comp") };
    cfg->n_push_const_ranges = 1;
    cfg->push_const_ranges = arena_push_array_nozero(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0] = (VkPushConstantRange) {
      .size = sizeof(Vulk_Draw_Push_Const_Spline_Build),
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };
    cfg->is_compute = true;
    cfg->create_desc_set = vulk_create_spline_build_descriptor_set_layout;
    cfg->configure_desc_sets = vulk_configure_spline_build_desc_sets;
  }

  return Pipeline_COUNT;
}
