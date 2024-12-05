#define VULK_MAX_FRAMES_IN_FLIGHT 2

// type used in index buffers
typedef u16 Vulk_Index;
// type used to refer to a specific instance type (e.g. quads, splines, texts, ...)
typedef u16 Vulk_Obj_Type;

// Fwd decls
typedef struct Vulk_Text Vulk_Text;
typedef struct Vulk_Spline Vulk_Spline;
typedef struct Gfx_Vulkan Gfx_Vulkan;

typedef struct Vertex {
  V3 pos;
  V2 uv;
} Vertex;

// This is an opaque type that depends on the windowing system implementation
typedef void Vulk_Window;

typedef union {
  struct {
    String8 vert;
    String8 frag;
  };
  String8 comp;
} Shader_File;

typedef VkDescriptorSetLayout (*Create_Desc_Set_Layout_Fn)(VkDevice);
typedef void (*Configure_Desc_Sets_Fn)(Gfx_Vulkan *);

typedef enum {
  VULK_GFX_PIPELINE_TRANSPARENT = 0x1,
  VULK_GFX_PIPELINE_NO_DEPTH_TEST = 0x2,
  VULK_GFX_PIPELINE_NO_CULL = 0x4,
  VULK_GFX_PIPELINE_NO_DEPTH_WRITE = 0x8,
  VULK_GFX_PIPELINE_WIREFRAME = 0x10,
} Vulk_Create_Gfx_Pipeline_Flags;

typedef struct {
  Shader_File shader;
  VkPushConstantRange *push_const_ranges;
  u32 n_push_const_ranges;
  Create_Desc_Set_Layout_Fn create_desc_set;
  Configure_Desc_Sets_Fn configure_desc_sets;
  b8 is_compute;

  // These are all ignored if `is_compute == true`.
  VkPrimitiveTopology topology;
  u32 flags; // Vulk_Create_Gfx_Pipeline_Flags
  VkVertexInputBindingDescription *vert_bindings;
  u32 n_vert_bindings;
  VkVertexInputAttributeDescription *vert_attrs;
  u32 n_vert_attrs;
} Vulk_Pipeline_Cfg;

typedef struct {
  b8 use_srgb;  
  b8 prefer_integrated_gpu;

  Vulk_Obj_Type n_obj_types;
  u64 *obj_instance_sizes; // size of a single instance on the gpu, per object type

  u64 text_vbuf_size; // total size of the vertex buffer dedicated to text meshes

  Image font_image;
  // XXX: this should not be hardcoded in here
  Image crosshair_image;

  String8 shaders_dir;
  Vulk_Pipeline_Cfg *pipeline_cfgs;
  u32 n_pipeline_cfgs;
} Vulk_Init_Config;

typedef struct {
  // This must be an array with as many elements as `n_pipeline_cfg` passed to init.
  u32 *instance_capacity;
} Vulk_Init_Result;

typedef struct {
  VkPipelineLayout layout;
  VkPipeline pipeline;
  VkDescriptorSetLayout desc_set_layout;
  VkDescriptorSet desc_sets[VULK_MAX_FRAMES_IN_FLIGHT];
} Vulk_Pipeline;

typedef struct {
  u64 start;
  u64 size;  
} Vulk_Buf_Range;

typedef struct Vulk_Buf_Node {
  struct Vulk_Buf_Node *next_by_size, *prev_by_size;
  struct Vulk_Buf_Node *next_by_addr, *prev_by_addr;
  Vulk_Buf_Range rng;
} Vulk_Buf_Node;

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
  VkDeviceSize size;
  VkBufferUsageFlags usage;
  VkMemoryPropertyFlags props;

  void *mapped;

  Vulk_Buf_Node *free_list_head_by_size, *free_list_tail_by_size;
  Vulk_Buf_Node *free_list_head_by_addr, *free_list_tail_by_addr;
} Vulk_Backing_Buffer;

typedef enum Vulk_Buf_Map_Status : b8 {
  Vulk_Buf_Unmapped = 0,
  Vulk_Buf_Mapped   = 1
} Vulk_Buf_Map_Status;

// a sub-allocated buffer
typedef struct {
  // these are duplicated from `backing` for convenience
  VkBuffer buffer;
  VkDeviceMemory memory;

  VkDeviceSize offset;
  VkDeviceSize size;
  void *mapped;
  Vulk_Backing_Buffer *backing;
} Vulk_Buffer;


// =================== API =====================

// Used to allocate enough memory to fit Gfx_Vulkan
FAE_API u64 gfx_vulk_sizeof();

// The `init` function is split in two because it can give the caller an opportunity to tune the level of quality based on the selected device.
FAE_API VkPhysicalDeviceProperties vulk_pre_init(Gfx_Vulkan *vk, Vulk_Window *window, Vulk_Init_Config *cfg);
FAE_API void vulk_init(Gfx_Vulkan *vk, Vulk_Init_Config *cfg, Vulk_Init_Result *result);
FAE_API void vulk_deinit(Gfx_Vulkan *vk);

FAE_API VkDevice vulk_get_device(Gfx_Vulkan *vk);

FAE_API void vulk_wait_idle(Gfx_Vulkan *vk);

FAE_API void vulk_recreate_swapchain(Gfx_Vulkan *vk, Vulk_Window *window);

// Creates `n_texts` texts at once. This is faster (possibly *much* faster) than calling vulk_add_text N times.
// `vertices` must contain all contiguous text vertices, that are split among the various texts according to the
// `n_vertices` array (which must have `n_texts` elements).
FAE_API void vulk_add_texts(Gfx_Vulkan *vk, const Vertex *vertices, u32 n_texts,
                    u32 *n_vertices, u32 *instance_id, b8 is_screenspace,
                    Vulk_Text **out_texts);
FAE_API Vulk_Text *vulk_add_text(Gfx_Vulkan *vk, const Vertex *vertices, u32 n_vertices, u32 instance_id, b8 is_screenspace);
FAE_API void vulk_remove_text(Gfx_Vulkan *vk, Vulk_Text *text);
FAE_API void vulk_set_text_instance_id(Vulk_Text *text, u32 instance_id);
FAE_API void vulk_set_text_vertices(Gfx_Vulkan *vk, Vulk_Text *text, const Vertex *vertices, u32 n_vertices);

// Updates a range of the instance buffer for the given object type.
// `data_size` and `offset` should be multiple of `obj_instance_sizes[obj_type]` as given to `vulk_init`.
FAE_API void vulk_update_instance_buffer(Gfx_Vulkan *vk, Vulk_Obj_Type obj_type, void *data, u64 data_size, u64 offset);

FAE_API b8 vulk_begin_cmd_buf(VkCommandBuffer cmd_buf);
FAE_API b8 vulk_end_cmd_buf(VkCommandBuffer cmd_buf);

FAE_API Vulk_Pipeline *vulk_get_pipeline(Gfx_Vulkan *vk, u32 pipeline_idx);

//// Windowing-system-dependent functions
FAE_API const char **vulkwin_get_required_instance_extensions(u32 *ext_count);
FAE_API VkSurfaceKHR vulkwin_create_surface(VkInstance instance, Vulk_Window *window);
FAE_API VkExtent2D vulkwin_select_swap_extent(Vulk_Window *window, VkSurfaceCapabilitiesKHR *caps);
FAE_API void vulkwin_block_until_visible(Vulk_Window *window);
////

