enum GfxObj_Id {
  GfxObj_Quad,
  GfxObj_Text,
  GfxObj_COUNT
};

static VkDescriptorSetLayout vulk_create_quad_descriptor_set_layout(VkDevice device)
{
  VkDescriptorSetLayoutBinding inst_data_layout_binding = {};
  inst_data_layout_binding.binding = 0;
  inst_data_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  inst_data_layout_binding.descriptorCount = 1;
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

static void vulk_configure_quad_desc_sets(Gfx_Vulkan *vk)
{
  VkDevice device = vk->device;
  Vulk_Buffer *instance_buf = &vk->instance_buf[GfxObj_Quad].buffer;
  Vulk_Pipeline *pipeline = &vk->pipelines[Pipeline_Quad];
  VkDescriptorSet *desc_sets = pipeline->desc_sets;
  u32 n_desc_sets = countof(pipeline->desc_sets);

  VkDescriptorBufferInfo instance_buf_info = {};
  instance_buf_info.buffer = instance_buf->buffer;
  instance_buf_info.range = instance_buf->size;
  instance_buf_info.offset = instance_buf->offset;

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

static VkDescriptorSetLayout vulk_create_text_descriptor_set_layout(VkDevice device)
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

static void vulk_configure_text_desc_sets(Gfx_Vulkan *vk)
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

static VkVertexInputBindingDescription *vulk_make_vertex_binding_desc_default(Arena *arena, u32 *n_desc)
{
  *n_desc = 1;
  VkVertexInputBindingDescription *desc = arena_push_array(VkVertexInputBindingDescription, arena, *n_desc);
  desc[0].binding = 0;
  desc[0].stride = sizeof(Vertex);
  desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return desc;
}

static VkVertexInputAttributeDescription *vulk_make_vertex_attr_desc_default(Arena *arena, u32 *n_desc)
{
  *n_desc = 2;
  VkVertexInputAttributeDescription *desc = arena_push_array(VkVertexInputAttributeDescription, arena, *n_desc);
  desc[0].binding = 0;
  desc[0].location = 0;
  desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  desc[0].offset = offsetof(Vertex, pos);
  desc[1].binding = 0;
  desc[1].location = 1;
  desc[1].format = VK_FORMAT_R32G32_SFLOAT;
  desc[1].offset = offsetof(Vertex, uv);
  return desc;
}

static void vulk_setup_pipeline_configs(Arena *arena, Vulk_Pipeline_Cfg configs[Pipeline_COUNT], b8 wireframe_mode)
{
  u32 pflags = wireframe_mode ? VULK_GFX_PIPELINE_WIREFRAME : 0;

  // Quads
  {
    Vulk_Pipeline_Cfg *cfg = &configs[Pipeline_Quad];
    cfg->shader = (Shader_File) { .vert = str8("quad.vert"), .frag = str8("quad.frag") };
    cfg->n_push_const_ranges = 1;
    cfg->push_const_ranges = arena_push_array(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0].size = sizeof(Push_Const_Gfx);
    cfg->push_const_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    cfg->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    cfg->flags = pflags | VULK_GFX_PIPELINE_NO_CULL | VULK_GFX_PIPELINE_TRANSPARENT;
    cfg->create_desc_set = vulk_create_quad_descriptor_set_layout;
    cfg->configure_desc_sets = vulk_configure_quad_desc_sets;
  }

  // Texts
  {
    Vulk_Pipeline_Cfg *cfg = &configs[Pipeline_Text];
    cfg->shader = (Shader_File) { .vert = str8("base.vert"), .frag = str8("msdf.frag") };
    cfg->n_push_const_ranges = 1;
    cfg->push_const_ranges = arena_push_array(VkPushConstantRange, arena, cfg->n_push_const_ranges);
    cfg->push_const_ranges[0].size = sizeof(Push_Const_Gfx);
    cfg->push_const_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    cfg->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    cfg->flags = pflags | VULK_GFX_PIPELINE_TRANSPARENT | VULK_GFX_PIPELINE_NO_CULL;
    cfg->vert_bindings = vulk_make_vertex_binding_desc_default(arena, &cfg->n_vert_bindings);
    cfg->vert_attrs = vulk_make_vertex_attr_desc_default(arena, &cfg->n_vert_attrs);
    cfg->create_desc_set = vulk_create_text_descriptor_set_layout;
    cfg->configure_desc_sets = vulk_configure_text_desc_sets;
  }
}
