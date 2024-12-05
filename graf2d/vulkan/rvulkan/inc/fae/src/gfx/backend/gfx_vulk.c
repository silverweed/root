#define VULK_MAX_SWAPCHAIN_IMAGES 6
#define VULK_INSTANCE_BUF_SIZE (50 * 1024 * 1024)
#define VULK_SCREENSPACE_QUAD_INSTANCE_BUF_SIZE (10 * 1024)
#define VULK_MAX_IMAGES 64

#define VULK_SPLINE_SUBDIVS 32
#define VULK_VERTICES_PER_SPLINE VULK_SPLINE_SUBDIVS * 2
#define VULK_SPLINE_CPOINTS_NUM_V4 3

typedef struct {
  VkSwapchainKHR handle;
  VkImage images[VULK_MAX_SWAPCHAIN_IMAGES];
  VkImageView image_views[VULK_MAX_SWAPCHAIN_IMAGES];
  VkFramebuffer framebuffers[VULK_MAX_SWAPCHAIN_IMAGES];
  u8 images_count;
  VkFormat format;
  VkExtent2D extent;
} Vulk_Swapchain;

typedef struct {
  u32 graphics;
  u32 present;
  u32 compute;
  b8 graphics_found;
  b8 present_found;
  b8 compute_found;
} Queue_Family_Idx;

typedef struct {
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR *formats;
  u32 formats_count;
  VkPresentModeKHR *present_modes;
  u32 present_modes_count;
} Swapchain_Support;

typedef struct {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkFormat format;
} Vulk_Image;

typedef struct {
  Vulk_Image images[VULK_MAX_IMAGES];
  VkSampler samplers[VULK_MAX_IMAGES];
  u32 n_images;
} Vulk_Images;

typedef struct {
  u32 id;
} Vulk_Image_Id;

typedef struct Vulk_Text {
  struct Vulk_Text *next, *prev;

  Vulk_Buffer vbuffer;
  u32 instance_id; // index of instance_data in the text instance buffer
  u32 n_vertices;
  b8 screenspace;
} Vulk_Text;

typedef struct {
  Vulk_Text *ws_head, *ws_tail;
  Vulk_Text *ss_head, *ss_tail;
  Vulk_Text *free;
} Vulk_Texts;

typedef struct {
  // Buffer containing a contiguous range of Gpu_Instance_Data. This maps 1:1 with Gfx_Instance_Data_Obj->data.
  Vulk_Buffer buffer;
  VkDeviceSize instance_size; // size of a single instance
} Vulk_Instances;

typedef struct Gfx_Vulkan {
  // Used to allocate support structs such as Vulk_Text, Vulk_Spline, etc
  Arena *arena;

  VkInstance instance;
  b8 vlayers_enabled;
  b8 srgb_enabled;
  VkPhysicalDevice phys_device;
  VkDevice device;

  Vulk_Buf_Alloc buf_alloc;

  u32 min_storage_buf_offset_align;

  VkQueue gfx_queue;
  VkQueue present_queue;
  VkQueue compute_queue;

  VkSurfaceKHR surface;
  Vulk_Swapchain swapchain;

  VkDescriptorPool desc_pool;

  VkRenderPass renderpass;
  Vulk_Pipeline *pipelines;
  u32 n_pipelines;

  Vulk_Image depth_image;

  Vulk_Instances *instance_buf; // array of len `n_obj_types`
  Vulk_Obj_Type n_obj_types;

  Vulk_Texts texts;
  Vulk_Images images;

  VkCommandPool cmd_pool;
  VkCommandPool transient_cmd_pool;

  Vulk_Image font_image;
  VkSampler font_image_sampler;

  // per-frame resources
  VkCommandBuffer cmd_buf[VULK_MAX_FRAMES_IN_FLIGHT];
  VkCommandBuffer compute_cmd_buf[VULK_MAX_FRAMES_IN_FLIGHT];

  VkSemaphore semph_image_available[VULK_MAX_FRAMES_IN_FLIGHT];
  VkSemaphore semph_render_finished[VULK_MAX_FRAMES_IN_FLIGHT];
  VkSemaphore semph_compute_finished[VULK_MAX_FRAMES_IN_FLIGHT];
  VkFence fence_frame_in_flight[VULK_MAX_FRAMES_IN_FLIGHT];  
  VkFence fence_compute_in_flight[VULK_MAX_FRAMES_IN_FLIGHT];  

  u8 cur_frame;
  b8 swapchain_stale;

  VkDebugUtilsMessengerEXT debug_messenger;
} Gfx_Vulkan;

#ifndef NDEBUG
#define VULK_ENABLE_VLD_LAYERS true
#else
#define VULK_ENABLE_VLD_LAYERS false
#endif

internal
const char *const g_vlayers_wanted[] = {
  "VK_LAYER_KHRONOS_validation"
};

internal
const char *const g_required_device_extensions[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

internal
b8 vulk_vlayers_available(Arena *scratch)
{
  u32 layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, NULL);

  VkLayerProperties *layers = arena_push_array_nozero(VkLayerProperties, scratch, layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, layers);

  if (log_get_lv() >= Log_Debug)
    for (u32 i = 0; i < layer_count; ++i)
      DEBUG_TAG("Vulkan", "Layer available: %s", layers[i].layerName);

  for (u32 wi = 0; wi < countof(g_vlayers_wanted); ++wi) {
    b8 found = false;
    for (u32 i = 0; i < layer_count; ++i) {
      if (strcmp(layers[i].layerName, g_vlayers_wanted[wi]) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

typedef struct {
  const char **data;
  u32 size;  
} Extensions_Array;

internal
Extensions_Array vulk_get_required_extensions(Arena *arena, b8 vlayers_enabled)
{
  Extensions_Array extensions = {};
  
  u32 win_ext_count;
  const char **win_exts = vulkwin_get_required_instance_extensions(&win_ext_count);

  extensions.size = win_ext_count + vlayers_enabled;
  if (win_ext_count) {
    assert(win_exts);
    extensions.data = arena_push_array(const char *, arena, win_ext_count + vlayers_enabled);
    memcpy(extensions.data, win_exts, sizeof(win_exts[0]) * win_ext_count);
  } else if (vlayers_enabled) {
    extensions.data = arena_push_array(const char *, arena, 1);
  }
  if (vlayers_enabled)
    extensions.data[win_ext_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

  return extensions;
}

internal
VKAPI_ATTR VkBool32 VKAPI_CALL vulk_debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT type,
  const VkDebugUtilsMessengerCallbackDataEXT* cb_data,
  void* user_data)
{
  (void)user_data;

  // map vulkan severity to our log level
  Log_Level lv = Log_None;
  if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)      lv = Log_Debug;
  else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)    lv = Log_Info;
  else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) lv = Log_Warn;
  else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)   lv = Log_Err;

  const char *type_str = "Unknown";
  if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)          type_str = "General";
  else if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)  type_str = "Validation";
  else if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) type_str = "Performance";

  fae_log(lv, "Vulkan", "%s: %s", type_str, cb_data->pMessage);

  return VK_FALSE;
}

internal
VkInstance vulk_create_instance(Arena *arena, b8 *vlayers_enabled)
{
  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Fae";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;
  if (VULK_ENABLE_VLD_LAYERS) {
    if (vulk_vlayers_available(arena)) {
      create_info.enabledLayerCount = countof(g_vlayers_wanted);
      create_info.ppEnabledLayerNames = g_vlayers_wanted;
      INFO_TAG("Vulkan", "enabled %u validations layers.", create_info.enabledLayerCount);
    } else {
      WARN_TAG("Vulkan", "validation layers requested but not available!");
    }
  }
  *vlayers_enabled = create_info.enabledLayerCount > 0;
  Extensions_Array extensions = vulk_get_required_extensions(arena, *vlayers_enabled);
  create_info.enabledExtensionCount = extensions.size;
  create_info.ppEnabledExtensionNames = extensions.data;

  VkInstance instance;
  VkResult res = vkCreateInstance(&create_info, NULL, &instance);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create Vulkan instance");
    os_abort();
  }

  return instance;
}

internal
void vulk_init_debug_callback(VkInstance instance, VkDebugUtilsMessengerEXT *messenger)
{ 
  VkDebugUtilsMessengerCreateInfoEXT cb_create_info = {};
  cb_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  cb_create_info.messageSeverity = (VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT * g_loglv >= Log_Verbose)
                                 | (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT * g_loglv >= Log_Info)
                                 | (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT * g_loglv >= Log_Warn)
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  cb_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  cb_create_info.pfnUserCallback = vulk_debug_callback;
  PFN_vkCreateDebugUtilsMessengerEXT create_dbg_cb = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (create_dbg_cb) {
    create_dbg_cb(instance, &cb_create_info, NULL, messenger);
  } else {
    ERR_TAG("Vulkan", "Failed to create debug messenger: proc not found.");
  }
}

internal
void vulk_deinit_debug_callback(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
  PFN_vkDestroyDebugUtilsMessengerEXT destroy_dbg_cb = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  destroy_dbg_cb(instance, messenger, NULL);
}

internal
b8 vulk_check_device_extensions_support(Arena *arena, VkPhysicalDevice device)
{
  u32 ext_count;
  vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, NULL);

  VkExtensionProperties *extensions = arena_push_array_nozero(VkExtensionProperties, arena, ext_count);
  vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, extensions);

  for (u32 wi = 0; wi < countof(g_required_device_extensions); ++wi) {
    b8 found = false;
    for (u32 i = 0; i < ext_count; ++i) {
      if (strcmp(extensions[i].extensionName, g_required_device_extensions[wi]) == 0) {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }

  return true;
}

internal
b8 vulk_check_device_features_support(VkPhysicalDevice device)
{
  VkPhysicalDeviceFeatures features;
  vkGetPhysicalDeviceFeatures(device, &features);

  // TODO: most of these are optional, we should be able to run even without
  b8 has_all_features = true;
  // has_all_features &= features.multiDrawIndirect;
  // has_all_features &= features.drawIndirectFirstInstance;
  has_all_features &= features.samplerAnisotropy;
  has_all_features &= features.fillModeNonSolid;

  return has_all_features;
}

internal
Swapchain_Support vulk_query_swapchain_support(Arena *arena, VkPhysicalDevice device, VkSurfaceKHR surface)
{
  Swapchain_Support details = {};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formats_count, NULL);
  if (details.formats_count) {
    details.formats = arena_push_array_nozero(VkSurfaceFormatKHR, arena, details.formats_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formats_count, details.formats);
  }
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_modes_count, NULL);
  if (details.present_modes_count) {
    details.present_modes = arena_push_array_nozero(VkPresentModeKHR, arena, details.present_modes_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.present_modes_count, details.present_modes);
  }
  
  return details;
}

internal
Queue_Family_Idx vulk_find_queue_families(Arena *arena, VkPhysicalDevice device, VkSurfaceKHR surface)
{
  Queue_Family_Idx queue_family_idx = {};
  
  u32 queue_family_count;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

  if (!queue_family_count)
    return queue_family_idx;

  VkQueueFamilyProperties *queue_families = arena_push_array_nozero(VkQueueFamilyProperties, arena, queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

  for (u32 i = 0; i < queue_family_count; ++i) {
    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      queue_family_idx.graphics = i;
      queue_family_idx.graphics_found = true;
    }

    if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
      queue_family_idx.compute = i;
      queue_family_idx.compute_found = true;
    }

    VkBool32 present_support;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
    if (present_support) {
      queue_family_idx.present = i;
      queue_family_idx.present_found = true;
    }

    if (queue_family_idx.graphics_found && queue_family_idx.present_found && queue_family_idx.compute_found) {
      break;
    }
  }

  return queue_family_idx;
}

internal
b8 vulk_is_device_suitable(Arena *arena, VkPhysicalDevice device, VkSurfaceKHR surface, Queue_Family_Idx *queue_family_idx, Swapchain_Support *ss)
{
  *queue_family_idx = vulk_find_queue_families(arena, device, surface);
  if (!(queue_family_idx->graphics_found && queue_family_idx->present_found))
    return false;

  b8 feat_and_ext_supported = vulk_check_device_features_support(device);
  feat_and_ext_supported = feat_and_ext_supported && vulk_check_device_extensions_support(arena, device);
  if (feat_and_ext_supported) {
    Swapchain_Support swapchain_support = vulk_query_swapchain_support(arena, device, surface);
    if (!swapchain_support.formats_count || !swapchain_support.present_modes_count)
      return false;
    *ss = swapchain_support;
  }

  return true;
}

internal
VkPhysicalDevice vulk_select_physical_device(Arena *arena, VkInstance instance, VkSurfaceKHR surface, b8 prefer_integrated_gpu,
                                             Queue_Family_Idx *queue_family_idx, Swapchain_Support *ss, u32 *min_storage_buf_off_align,
                                             VkPhysicalDeviceProperties *device_props)
{
  VkPhysicalDevice device = VK_NULL_HANDLE;

  u32 device_count;
  vkEnumeratePhysicalDevices(instance, &device_count, NULL);
  if (!device_count) {
    FATAL_TAG("Vulkan", "No suitable GPUs found!");
    os_abort();
  }

  VkPhysicalDevice *devices = arena_push_array_nozero(VkPhysicalDevice, arena, device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, devices);

  // Prefer discrete GPU over integrated
  VkPhysicalDevice *discrete = arena_push_array_nozero(VkPhysicalDevice, arena, device_count);
  VkPhysicalDevice *non_discrete = arena_push_array_nozero(VkPhysicalDevice, arena, device_count);
  u32 n_discrete = 0;
  u32 n_non_discrete = 0;
  for (u32 i = 0; i < device_count; ++i) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(devices[i], &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
      discrete[n_discrete++] = devices[i];
    else
      non_discrete[n_non_discrete++] = devices[i];
  }

  if (!prefer_integrated_gpu) {
    for (u32 i = 0; i < n_discrete; ++i) {
      if (vulk_is_device_suitable(arena, discrete[i], surface, queue_family_idx, ss)) {
        device = discrete[i];
        break;
      }
    }
  }
  if (device == VK_NULL_HANDLE) {
    for (u32 i = 0; i < n_non_discrete; ++i) {
      if (vulk_is_device_suitable(arena, non_discrete[i], surface, queue_family_idx, ss)) {
        device = non_discrete[i];
        break;
      }
    }
  }
  if (device == VK_NULL_HANDLE) {
    for (u32 i = 0; i < n_discrete; ++i) {
      if (vulk_is_device_suitable(arena, discrete[i], surface, queue_family_idx, ss)) {
        device = discrete[i];
        break;
      }
    }
  }

  if (device == VK_NULL_HANDLE) {
    FATAL_TAG("Vulkan", "No suitable GPUs found (%u tried)!", device_count);
    os_abort();
  }

  vkGetPhysicalDeviceProperties(device, device_props);
  *min_storage_buf_off_align = (u32)device_props->limits.minStorageBufferOffsetAlignment;

  INFO_TAG("Vulkan", "GPU selected: %s", device_props->deviceName);
#ifdef VK_API_VERSION_MAJOR
  INFO_TAG("Vulkan", "Vulkan API version: %d.%d.%d", 
           VK_API_VERSION_MAJOR(device_props->apiVersion),
           VK_API_VERSION_MINOR(device_props->apiVersion),
           VK_API_VERSION_PATCH(device_props->apiVersion));
#else
  INFO_TAG("Vulkan", "Vulkan API version: %d.%d.%d",
      VK_VERSION_MAJOR(device_props->apiVersion),
      VK_VERSION_MINOR(device_props->apiVersion),
      VK_VERSION_PATCH(device_props->apiVersion));
#endif

  return device;
}

internal
VkDevice vulk_create_logical_device(VkPhysicalDevice phys_device, Queue_Family_Idx queue_family_idx,
                                    VkQueue *gfx_queue, VkQueue *present_queue, VkQueue *compute_queue)
{
  u32 queue_indices[] = { queue_family_idx.graphics, queue_family_idx.present };
  u32 queue_indices_count = 1 + (queue_family_idx.graphics != queue_family_idx.present);
  f32 q_prio = 1.f;

  VkDeviceQueueCreateInfo q_create_infos[countof(queue_indices)] = {};
  
  // create a queue for each unique queue
  for (u32 i = 0; i < queue_indices_count; ++i) {
    VkDeviceQueueCreateInfo *q_create_info = &q_create_infos[i];
    q_create_info->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q_create_info->queueFamilyIndex = queue_indices[i];
    q_create_info->queueCount = 1;
    q_create_info->pQueuePriorities = &q_prio;
  }

  VkPhysicalDeviceFeatures features = {};
  // features.multiDrawIndirect = true;
  // features.drawIndirectFirstInstance = true;
  features.samplerAnisotropy = true;
  features.fillModeNonSolid = true;

  VkDeviceCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.pQueueCreateInfos = q_create_infos;
  create_info.queueCreateInfoCount = queue_indices_count;
  create_info.pEnabledFeatures = &features;
  create_info.enabledExtensionCount = countof(g_required_device_extensions);
  create_info.ppEnabledExtensionNames = g_required_device_extensions;

  VkDevice device;
  VkResult res = vkCreateDevice(phys_device, &create_info, NULL, &device); 
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create logical device.\n'");
    os_abort();
  }

  vkGetDeviceQueue(device, queue_family_idx.graphics, 0, gfx_queue);
  vkGetDeviceQueue(device, queue_family_idx.present, 0, present_queue);
  vkGetDeviceQueue(device, queue_family_idx.compute, 0, compute_queue);

  return device;
}

internal
VkSurfaceFormatKHR vulk_select_swap_surface_format(VkSurfaceFormatKHR *available_formats, u32 available_formats_count, b8 enable_srgb)
{
  if (available_formats_count == 0) {
    FATAL_TAG("Vulkan", "No swapchain formats available!");
    os_abort();
  }

  if (enable_srgb) {
    for (u32 i = 0; i < available_formats_count; ++i) {
      if (available_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && available_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        return available_formats[i];
    }
  }
  return available_formats[0];
}

internal
VkPresentModeKHR vulk_select_present_mode(VkPresentModeKHR *present_modes, u32 present_modes_count)
{
  for (u32 i = 0; i < present_modes_count; ++i) {
    if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
      return present_modes[i];
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

internal
Vulk_Swapchain vulk_create_swapchain(Vulk_Window *window, VkDevice device, VkSurfaceKHR surface, 
                                     Queue_Family_Idx *queue_family_idx, Swapchain_Support *ss,
                                     VkSwapchainKHR old_swapchain, b8 enable_srgb)
{
  VkSurfaceFormatKHR surf_format = vulk_select_swap_surface_format(ss->formats, ss->formats_count, enable_srgb);
  VkPresentModeKHR present_mode = vulk_select_present_mode(ss->present_modes, ss->present_modes_count);
  VkExtent2D extent = vulkwin_select_swap_extent(window, &ss->capabilities);
  u32 image_count = ss->capabilities.minImageCount + 1;
  if (ss->capabilities.maxImageCount > 0 && image_count > ss->capabilities.maxImageCount)
    image_count = ss->capabilities.maxImageCount;

  VkSwapchainCreateInfoKHR create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surf_format.format;
  create_info.imageColorSpace = surf_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (queue_family_idx->graphics != queue_family_idx->present) {
    u32 q_fam_idx[] = { queue_family_idx->graphics, queue_family_idx->present };
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;    
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = q_fam_idx;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  create_info.preTransform = ss->capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = old_swapchain;

  VkSwapchainKHR swapchain;
  VkResult res = vkCreateSwapchainKHR(device, &create_info, NULL, &swapchain);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create swapchain!");
    os_abort();
  }

  Vulk_Swapchain swp;
  swp.handle = swapchain;
  swp.extent = extent;
  swp.format = surf_format.format;

  vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
  assert_always(0 < image_count && image_count <= VULK_MAX_SWAPCHAIN_IMAGES);
  // NOTE: we MUST set this value to the correct image count before calling vkGetSwapchainImagesKHR
  // again, otherwise the call will fail!
  swp.images_count = image_count;
  vkGetSwapchainImagesKHR(device, swapchain, &image_count, swp.images);
  assert_always(image_count == swp.images_count);
  
  return swp;
}

internal
void vulk_create_swapchain_image_views(VkDevice device, Vulk_Swapchain *swapchain)
{
  for (u8 i = 0; i < swapchain->images_count; ++i) {
    VkImageViewCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = swapchain->images[i];
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = swapchain->format;  
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    VkResult res = vkCreateImageView(device, &create_info, NULL, &swapchain->image_views[i]);
    if (res != VK_SUCCESS) {
      FATAL_TAG("Vulkan", "Failed to create image view");
      os_abort();
    }
  }
}

internal
b8 vulk_create_shader_module(VkDevice device, String8 fname, String8 code, VkShaderModule *module)
{
  VkShaderModuleCreateInfo  create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = code.size;
  create_info.pCode = (u32 *)code.str;

  VkResult res = vkCreateShaderModule(device, &create_info, NULL, module);
  if (res != VK_SUCCESS) {
    ERR_TAG("Vulkan", "Failed to create shader module for %s", cstr(fname));
    return false;
  }
  return true;
}

internal
b8 vulk_create_shader_modules(Arena *scratch_arena, VkDevice device,
                              String8 vert_fname, String8 frag_fname, VkShaderModule *vert, VkShaderModule *frag)
{
  String8 vert_code = file_read_to_string(scratch_arena, vert_fname);
  String8 frag_code = file_read_to_string(scratch_arena, frag_fname);

  if (!vert_code.size || !frag_code.size)
    return false;

  b8 ok = vulk_create_shader_module(device, vert_fname, vert_code, vert);
  if (!ok)
    return false;

  ok = vulk_create_shader_module(device, frag_fname, frag_code, frag);
  if (!ok) {
    vkDestroyShaderModule(device, *vert, NULL);
    return false;
  }

  return true;
}

internal
VkFormat vulk_find_supported_format(VkPhysicalDevice phys_device, const VkFormat *candidates, u32 n_candidates,
                                    VkImageTiling tiling, VkFormatFeatureFlags features)
{
  for (u32 i = 0; i < n_candidates; ++i) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(phys_device, candidates[i], &props);

    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
      return candidates[i];

    if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
      return candidates[i];
  }

  FATAL_TAG("Vulkan", "Failed to find supported format");
  os_abort();

  return candidates[0];
}

internal
VkFormat vulk_select_depth_format(VkPhysicalDevice phys_device) 
{
  const VkFormat candidates[] = {
    VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT,
  };
  return vulk_find_supported_format(phys_device, candidates, countof(candidates), VK_IMAGE_TILING_OPTIMAL,
                                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

internal
VkRenderPass vulk_create_renderpass(VkPhysicalDevice phys_device, VkDevice device, Vulk_Swapchain *swapchain)
{
  VkAttachmentDescription color_attachment = {};
  color_attachment.format = swapchain->format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_attachment_ref = {};
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depth_attachment = {};
  depth_attachment.format = vulk_select_depth_format(phys_device);
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref = {};
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  // NOTE: these correspond to the `layout (location = X) out` in the fragment shader
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[] = { color_attachment, depth_attachment };
  VkRenderPassCreateInfo renderpass_info = {};
  renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderpass_info.attachmentCount = countof(attachments);
  renderpass_info.pAttachments = attachments;
  renderpass_info.subpassCount = 1;
  renderpass_info.pSubpasses = &subpass;
  renderpass_info.dependencyCount = 1;
  renderpass_info.pDependencies = &dependency;

  VkRenderPass renderpass;
  VkResult res = vkCreateRenderPass(device, &renderpass_info, NULL, &renderpass);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create render pass");
    os_abort();
  }

  return renderpass;
}

internal
VkPipelineLayout vulk_create_pipeline_layout(VkDevice device, VkDescriptorSetLayout desc_set_layout, VkPushConstantRange *pc_ranges, u32 n_pc_ranges)
{
  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = 1;
  pipeline_layout_info.pSetLayouts = &desc_set_layout;
  pipeline_layout_info.pushConstantRangeCount = n_pc_ranges;
  pipeline_layout_info.pPushConstantRanges = pc_ranges;

  VkPipelineLayout pipeline_layout;
  VkResult res = vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create pipeline layout");
    os_abort();
  }
  
  return pipeline_layout;
}

internal
VkPipeline vulk_create_graphics_pipeline(Arena *scratch_arena, VkDevice device, VkRenderPass renderpass,
                                         VkPipelineLayout pipeline_layout, String8 shader_dir, Shader_File shader,
                                         VkPrimitiveTopology topology, u32 flags,
                                         VkVertexInputBindingDescription *vert_binding, u32 n_vert_binding,
                                         VkVertexInputAttributeDescription *vert_attr, u32 n_vert_attr)
{
  const VkDynamicState dyn_states[] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dyn_state_info = {};
  dyn_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dyn_state_info.dynamicStateCount = countof(dyn_states);
  dyn_state_info.pDynamicStates = dyn_states;

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
  vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_info.vertexBindingDescriptionCount = n_vert_binding;
  vertex_input_info.pVertexBindingDescriptions = vert_binding;
  vertex_input_info.vertexAttributeDescriptionCount = n_vert_attr;
  vertex_input_info.pVertexAttributeDescriptions = vert_attr;

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = topology;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewport_state = {};
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = (flags & VULK_GFX_PIPELINE_WIREFRAME) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = (flags & VULK_GFX_PIPELINE_NO_CULL) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState color_blend_attachment = {};
  color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  if (flags & VULK_GFX_PIPELINE_TRANSPARENT) {
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
  } else {
    color_blend_attachment.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo color_blending = {};
  color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  String8 vert_fname = push_str8f(scratch_arena, "%s/%s.spv", cstr(shader_dir), cstr(shader.vert));
  String8 frag_fname = push_str8f(scratch_arena, "%s/%s.spv", cstr(shader_dir), cstr(shader.frag));
  VkShaderModule vert_module, frag_module;
  if (!vulk_create_shader_modules(scratch_arena, device, vert_fname, frag_fname, &vert_module, &frag_module)) {
    FATAL_TAG("Vulkan", "Failed to create shader modules");
    os_abort();
  }

  VkPipelineShaderStageCreateInfo vert_stage_info = {};
  vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vert_stage_info.module = vert_module;
  vert_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo frag_stage_info = {};
  frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  frag_stage_info.module = frag_module;
  frag_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage_info, frag_stage_info };

  VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
  depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = (flags & VULK_GFX_PIPELINE_NO_DEPTH_TEST) ? VK_FALSE : VK_TRUE;
  depth_stencil.depthWriteEnable = (flags & VULK_GFX_PIPELINE_NO_DEPTH_WRITE) ? VK_FALSE : VK_TRUE;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  VkGraphicsPipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = countof(shader_stages);
  pipeline_info.pStages = shader_stages;
  pipeline_info.pVertexInputState = &vertex_input_info;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dyn_state_info;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.renderPass = renderpass;
  pipeline_info.pDepthStencilState = &depth_stencil;

  VkPipeline pipeline;
  VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create graphics pipeline");
    os_abort();
  }

  vkDestroyShaderModule(device, frag_module, NULL);
  vkDestroyShaderModule(device, vert_module, NULL);

  return pipeline;
}

internal
VkPipeline vulk_create_compute_pipeline(Arena *scratch_arena, VkDevice device, VkPipelineLayout pipeline_layout,
                                        String8 shader_dir, Shader_File shader)
{
  String8 comp_fname = push_str8f(scratch_arena, "%s/%s.spv", cstr(shader_dir), cstr(shader.comp));
  String8 comp_code = file_read_to_string(scratch_arena, comp_fname);
  VkShaderModule comp_module;
  if (!vulk_create_shader_module(device, comp_fname, comp_code, &comp_module)) {
    FATAL_TAG("Vulkan", "Failed to create compute shader module");
    os_abort();
  }

  VkPipelineShaderStageCreateInfo comp_stage_info = {};
  comp_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  comp_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  comp_stage_info.module = comp_module;
  comp_stage_info.pName = "main";

  VkComputePipelineCreateInfo pipeline_info = {};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.layout = pipeline_layout;
  pipeline_info.stage = comp_stage_info;

  VkPipeline pipeline;
  VkResult res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create compute pipeline");
    os_abort();
  }

  vkDestroyShaderModule(device, comp_module, NULL);
  return pipeline;
}

internal
void vulk_create_framebuffers(VkDevice device, VkRenderPass renderpass, Vulk_Swapchain *swapchain, VkImageView depth_img_view)
{
  for (u32 i = 0; i < swapchain->images_count; ++i) {
    VkImageView attachments[] = {
      swapchain->image_views[i],
      depth_img_view
    };
    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = renderpass;
    fb_info.attachmentCount = countof(attachments);
    fb_info.pAttachments = attachments;
    fb_info.width = swapchain->extent.width;
    fb_info.height = swapchain->extent.height;
    fb_info.layers = 1;

    VkResult res = vkCreateFramebuffer(device, &fb_info, NULL, &swapchain->framebuffers[i]);
    if (res != VK_SUCCESS) {
      FATAL_TAG("Vulkan", "Failed to create framebuffer %u", i);
      os_abort();
    }
  }
}

internal
VkCommandPool vulk_create_cmd_pool(VkDevice device, Queue_Family_Idx queue_family_idx, b8 transient)
{
  VkCommandPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = queue_family_idx.graphics;

  VkCommandPool cmd_pool;
  VkResult res = vkCreateCommandPool(device, &pool_info, NULL, &cmd_pool);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create command pool");
    os_abort();
  }
  return cmd_pool;
}

internal
void vulk_alloc_cmd_buffers(VkDevice device, VkCommandPool cmd_pool, VkCommandBuffer *cmd_bufs, u32 n_cmd_bufs)
{
  VkCommandBufferAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = cmd_pool;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = n_cmd_bufs;

  VkResult res = vkAllocateCommandBuffers(device, &alloc_info, cmd_bufs);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to allocate command buffers");
    os_abort();
  }
}

internal
VkSemaphore vulk_create_semaphore(VkDevice device)
{
  VkSemaphoreCreateInfo semph_info = {};
  semph_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkSemaphore semph;
  VkResult res = vkCreateSemaphore(device, &semph_info, NULL, &semph);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create semaphore");
    os_abort();
  }

  return semph;
}

internal
VkFence vulk_create_fence(VkDevice device, b8 signaled)
{
  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = signaled * VK_FENCE_CREATE_SIGNALED_BIT;

  VkFence fence;
  VkResult res = vkCreateFence(device, &fence_info, NULL, &fence);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create fence");
    os_abort();
  }

  return fence;
}

internal
VkDescriptorPool vulk_create_descriptor_pool(VkDevice device)
{
  VkDescriptorPoolSize inst_buf_pool_size = {};
  inst_buf_pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  inst_buf_pool_size.descriptorCount = (u32)VULK_MAX_FRAMES_IN_FLIGHT * 10; 

  VkDescriptorPoolSize sampler_pool_size = {};
  sampler_pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_pool_size.descriptorCount = (u32)VULK_MAX_FRAMES_IN_FLIGHT * (VULK_MAX_IMAGES + 3); 

  // VkDescriptorPoolSize ubo_pool_size = {};
  // ubo_pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  // ubo_pool_size.descriptorCount = (u32)VULK_MAX_FRAMES_IN_FLIGHT; 

  VkDescriptorPoolSize pool_sizes[] = { inst_buf_pool_size, sampler_pool_size };

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.poolSizeCount = countof(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;
  pool_info.maxSets = (u32)VULK_MAX_FRAMES_IN_FLIGHT * 6;

  VkDescriptorPool pool;
  VkResult res = vkCreateDescriptorPool(device, &pool_info, NULL, &pool);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create descriptor pool");
    os_abort();
  }

  return pool;
}

internal
void vulk_create_descriptor_sets(VkDevice device, VkDescriptorPool desc_pool, VkDescriptorSetLayout desc_layout, VkDescriptorSet *desc_sets)
{
  VkDescriptorSetLayout layouts[VULK_MAX_FRAMES_IN_FLIGHT];
  for (u32 i = 0; i < countof(layouts); ++i)
    layouts[i] = desc_layout;
  
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = desc_pool;
  alloc_info.descriptorSetCount = (u32)VULK_MAX_FRAMES_IN_FLIGHT;
  alloc_info.pSetLayouts = layouts;

  VkResult res = vkAllocateDescriptorSets(device, &alloc_info, desc_sets);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to allocate descriptor sets");
    os_abort();
  }
}

internal
Vulk_Buffer vulk_create_instance_buffer(Gfx_Vulkan *vk, u64 size)
{
  Vulk_Buffer buf = vulk_alloc_buffer(&vk->buf_alloc, size,
                                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       Vulk_Buf_Mapped);
  INFO_TAG("Vulkan", "Allocated %lu kiB of instance data buffer", buf.size >> 10);
  return buf;
}

internal
VkCommandBuffer vulk_begin_transient_commands(Gfx_Vulkan *vk)
{
  VkCommandBuffer cmd_buf;
  vulk_alloc_cmd_buffers(vk->device, vk->transient_cmd_pool, &cmd_buf, 1);

  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd_buf, &begin_info);

  return cmd_buf;
}

internal
void vulk_end_transient_commands(Gfx_Vulkan *vk, VkCommandBuffer cmd_buf)
{  
  vkEndCommandBuffer(cmd_buf);

  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd_buf;

  // @Speed: if we need to optimize this in the future, we may pass a fence here and wait
  // on that instead of calling QueueWaitIdle.
  vkQueueSubmit(vk->gfx_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(vk->gfx_queue);

  vkFreeCommandBuffers(vk->device, vk->transient_cmd_pool, 1, &cmd_buf);
}

internal
void vulk_copy_buffer(Gfx_Vulkan *vk, VkBuffer src, VkBuffer dst, u32 n_copies, VkBufferCopy *copies)
{
  VkCommandBuffer cmd_buf = vulk_begin_transient_commands(vk);
  vkCmdCopyBuffer(cmd_buf, src, dst, n_copies, copies);
  vulk_end_transient_commands(vk, cmd_buf);
}

internal
void vulk_destroy_image(VkDevice device, Vulk_Image image)
{
  if (image.view != VK_NULL_HANDLE)
    vkDestroyImageView(device, image.view, NULL);
  vkDestroyImage(device, image.image, NULL);
  vkFreeMemory(device, image.memory, NULL);
}

internal
void vulk_cleanup_swapchain(Gfx_Vulkan *vk)
{
  vulk_destroy_image(vk->device, vk->depth_image);
  for (u32 i = 0; i < vk->swapchain.images_count; ++i)
    vkDestroyFramebuffer(vk->device, vk->swapchain.framebuffers[i], NULL);
  for (u8 i = 0; i < vk->swapchain.images_count; ++i) {
    vkDestroyImageView(vk->device, vk->swapchain.image_views[i], NULL);
  }
  vkDestroySwapchainKHR(vk->device, vk->swapchain.handle, NULL);
}

internal
Vulk_Image vulk_create_image(VkPhysicalDevice phys_device, VkDevice device, u32 width, u32 height,
                             VkFormat format, VkImageTiling tiling, 
                             VkImageUsageFlags usage, VkMemoryPropertyFlags properties, b8 is_cube)
{
  VkImageCreateInfo image_info = {};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = width;
  image_info.extent.height = height;
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.format = format;
  image_info.tiling = tiling;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage = usage;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  if (is_cube) {
    image_info.arrayLayers = 6;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  } else {
    image_info.arrayLayers = 1;
  }

  Vulk_Image tex_image = {};
  tex_image.format = format;
  VkResult res = vkCreateImage(device, &image_info, NULL, &tex_image.image);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create tex_image");
    os_abort();
  }

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(device, tex_image.image, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = vulk_find_memory_type(phys_device, mem_reqs.memoryTypeBits, properties);

  res = vkAllocateMemory(device, &alloc_info, NULL, &tex_image.memory);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to allocate tex_image memory");
    os_abort();
  }

  vkBindImageMemory(device, tex_image.image, tex_image.memory, 0);

  return tex_image;
}

internal
void vulk_create_image_view(VkDevice device, VkImageAspectFlags aspects, Vulk_Image *image)
{
  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image->image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view_info.format = image->format;
  view_info.subresourceRange.aspectMask = aspects;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 1;

  VkResult res = vkCreateImageView(device, &view_info, NULL, &image->view);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create image view");
    os_abort();
  }
}

internal
void vulk_create_image_cube_view(VkDevice device, VkImageAspectFlags aspects, Vulk_Image *image)
{
  VkImageViewCreateInfo view_info = {};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image->image;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
  view_info.format = image->format;
  view_info.subresourceRange.aspectMask = aspects;
  view_info.subresourceRange.baseMipLevel = 0;
  view_info.subresourceRange.levelCount = 1;
  view_info.subresourceRange.baseArrayLayer = 0;
  view_info.subresourceRange.layerCount = 6;

  VkResult res = vkCreateImageView(device, &view_info, NULL, &image->view);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create image view");
    os_abort();
  }
}

internal
void vulk_transition_image_layout(Gfx_Vulkan *vk, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout, b8 is_cube)
{
  VkCommandBuffer cmd_buf = vulk_begin_transient_commands(vk);

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = is_cube ? 6 : 1;

  VkPipelineStageFlags src_stage = 0, dst_stage = 0;
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    FATAL_TAG("Vulkan", "unsupported layout transition!");
    os_abort();
  }

  vkCmdPipelineBarrier(cmd_buf, 
                       src_stage, dst_stage,
                       0,
                       0, NULL,
                       0, NULL,
                       1, &barrier);

  vulk_end_transient_commands(vk, cmd_buf);
}

internal
void vulk_copy_buffer_to_image(Gfx_Vulkan *vk, Vulk_Buffer buffer, VkImage image, u32 width, u32 height)
{
  VkCommandBuffer cmd_buf = vulk_begin_transient_commands(vk);

  VkBufferImageCopy region = {};
  region.bufferOffset = buffer.offset;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = (VkExtent3D) {
      width,
      height,
      1
  };

  vkCmdCopyBufferToImage(cmd_buf, buffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  vulk_end_transient_commands(vk, cmd_buf);
}

internal
void vulk_copy_buffer_to_cube_image(Gfx_Vulkan *vk, Vulk_Buffer buffer, VkImage image, u32 width, u32 height)
{
  VkCommandBuffer cmd_buf = vulk_begin_transient_commands(vk);

  VkBufferImageCopy regions[6] = {};
  for (u32 face = 0; face < 6; ++face) {
    regions[face].bufferOffset = buffer.offset;
    regions[face].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    regions[face].imageSubresource.layerCount = 1;
    regions[face].imageSubresource.baseArrayLayer = face;
    regions[face].imageExtent = (VkExtent3D) {
        width,
        height,
        1
    };
    u64 offset = 4 * width * height * face;
    regions[face].bufferOffset = offset;
  }

  vkCmdCopyBufferToImage(cmd_buf, buffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, countof(regions), regions);

  vulk_end_transient_commands(vk, cmd_buf);
}

internal
Vulk_Image vulk_create_tex_image(Gfx_Vulkan *vk, Image image)
{
  VkPhysicalDevice phys_device = vk->phys_device;
  VkDevice device = vk->device;
  VkDeviceSize img_size = image.width * image.height * image.channels;
  Vulk_Buffer staging = vulk_alloc_buffer(&vk->buf_alloc, img_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                                          Vulk_Buf_Mapped);
  memcpy(staging.mapped, image.pixels, (u64)img_size);
  vulk_unmap_buffer(&staging);

  // NOTE: technically we should verify that the image format is available. As long as we're using R8G8B8A8_UNORM we're probably good though.
  // TODO: probably the format should be specified from the outside.
  Vulk_Image tex_image = vulk_create_image(phys_device, device, image.width, image.height,
                                           VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                           Vulk_Buf_Unmapped);
  vulk_transition_image_layout(vk, tex_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, false);
  vulk_copy_buffer_to_image(vk, staging, tex_image.image, image.width, image.height);
  vulk_transition_image_layout(vk, tex_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

  vulk_free_buffer(&vk->buf_alloc, &staging);
  
  return tex_image;
}

internal
Vulk_Image vulk_create_tex_cube_image(Gfx_Vulkan *vk, Cube_Image image)
{
  VkPhysicalDevice phys_device = vk->phys_device;
  VkDevice device = vk->device;
  VkDeviceSize img_size = image.width * image.height * image.channels;
  Vulk_Buffer staging = vulk_alloc_buffer(&vk->buf_alloc, img_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, true);
  for (u32 face = 0; face < 6; ++face)
    memcpy((u8 *)staging.mapped + img_size * face, image.face[face].pixels, (u64)img_size);
  vulk_unmap_buffer(&staging);

  // NOTE: technically we should verify that the image format is available. 
  // TODO: probably the format should be specified from the outside.
  Vulk_Image tex_image = vulk_create_image(phys_device, device, image.width, image.height,
                                           VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true);
  vulk_transition_image_layout(vk, tex_image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, true);
  vulk_copy_buffer_to_cube_image(vk, staging, tex_image.image, image.width, image.height);
  vulk_transition_image_layout(vk, tex_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, true);

  vulk_free_buffer(&vk->buf_alloc, &staging);
  
  return tex_image;
}

internal
VkSampler vulk_create_tex_sampler(VkPhysicalDevice phys_device, VkDevice device, b8 anisotropy)
{
  VkPhysicalDeviceProperties properties = {};
  vkGetPhysicalDeviceProperties(phys_device, &properties);

  VkSamplerCreateInfo sampler_info = {};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampler_info.anisotropyEnable = anisotropy ? VK_TRUE : VK_FALSE;
  sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  VkSampler sampler;
  VkResult res = vkCreateSampler(device, &sampler_info, NULL, &sampler);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "failed to create texture sampler");
    os_abort();
  }

  return sampler;
}

internal
Vulk_Image vulk_create_depth_resources(VkPhysicalDevice phys_device, VkDevice device, VkExtent2D swap_extent)
{
  VkFormat depth_format = vulk_select_depth_format(phys_device);
  Vulk_Image depth_image = vulk_create_image(phys_device, device, swap_extent.width, swap_extent.height, depth_format,
                    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false);
  vulk_create_image_view(device, VK_IMAGE_ASPECT_DEPTH_BIT, &depth_image);

  return depth_image;
}

internal
Vulk_Buffer vulk_create_vertex_buffer(Gfx_Vulkan *vk, const Vertex *vertices, u64 n_vertices)
{
  u64 buf_size = sizeof(Vertex) * n_vertices;
  Vulk_Buffer staging = vulk_alloc_buffer(&vk->buf_alloc, buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                                          Vulk_Buf_Mapped);
  memcpy(staging.mapped, vertices, buf_size);
  vulk_unmap_buffer(&staging);

  Vulk_Buffer vbuf = vulk_alloc_buffer(&vk->buf_alloc, buf_size, 
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Vulk_Buf_Unmapped);
  VkBufferCopy copy = {};
  copy.size = staging.size;
  copy.srcOffset = staging.offset;
  copy.dstOffset = vbuf.offset;
  vulk_copy_buffer(vk, staging.buffer, vbuf.buffer, 1, &copy);

  vulk_free_buffer(&vk->buf_alloc, &staging);
  return vbuf;
}

internal
void vulk_init_text_vertices(Gfx_Vulkan *vk, Vulk_Text *text, const Vertex *vertices, u32 n_vertices)
{
  if (n_vertices)
    text->vbuffer = vulk_create_vertex_buffer(vk, vertices, n_vertices);
  text->n_vertices = n_vertices;
}

internal
void vulk_deinit_text_vertices(Gfx_Vulkan *vk, Vulk_Text *text)
{
  vulk_free_buffer(&vk->buf_alloc, &text->vbuffer);
}

// =================================================================
//                      PUBLIC API
// =================================================================

usz gfx_vulk_sizeof()
{
  return sizeof(Gfx_Vulkan);
}

// not to be confused with vulk_create_image (internal API)
Vulk_Image_Id vulk_add_image(Gfx_Vulkan *vk, Image image)
{
  assert(vk->images.n_images < countof(vk->images.images));
  Vulk_Image vimage = vulk_create_tex_image(vk, image);
  vulk_create_image_view(vk->device, VK_IMAGE_ASPECT_COLOR_BIT, &vimage);
  vk->images.images[vk->images.n_images] = vimage;
  vk->images.samplers[vk->images.n_images] = vulk_create_tex_sampler(vk->phys_device, vk->device, /* anisotropy = */ false);
  return (Vulk_Image_Id) { ++vk->images.n_images };
}

// This function is split from vulk_init() because it can give the caller an opportunity to tune the level of quality based on the selected device.
VkPhysicalDeviceProperties vulk_pre_init(Gfx_Vulkan *vk, Vulk_Window *window, Vulk_Init_Config *cfg)
{
  Temp scratch = scratch_begin(0, 0);
  
  vk->arena = arena_alloc();
  vk->srgb_enabled = cfg->use_srgb;
  vk->instance = vulk_create_instance(scratch.arena, &vk->vlayers_enabled);  
  if (vk->vlayers_enabled)
    vulk_init_debug_callback(vk->instance, &vk->debug_messenger);
  vk->surface = vulkwin_create_surface(vk->instance, window);
  Queue_Family_Idx queue_family_idx = {};
  Swapchain_Support swapchain_support = {};
  VkPhysicalDeviceProperties phys_props;
  vk->phys_device = vulk_select_physical_device(scratch.arena, vk->instance, vk->surface, cfg->prefer_integrated_gpu, 
                                                &queue_family_idx, &swapchain_support, &vk->min_storage_buf_offset_align, &phys_props);
  vk->device = vulk_create_logical_device(vk->phys_device, queue_family_idx, &vk->gfx_queue, &vk->present_queue, &vk->compute_queue);
  vk->buf_alloc = vulk_buf_alloc_init(vk->arena, vk->phys_device, vk->device);
  vk->swapchain = vulk_create_swapchain(window, vk->device, vk->surface, &queue_family_idx, &swapchain_support, VK_NULL_HANDLE, vk->srgb_enabled);
  vulk_create_swapchain_image_views(vk->device, &vk->swapchain);

  scratch_end(scratch);
  return phys_props;
}

void vulk_init(Gfx_Vulkan *vk, Vulk_Init_Config *cfg, Vulk_Init_Result *result)
{
  Temp scratch = scratch_begin(0, 0);

  Queue_Family_Idx queue_family_idx = vulk_find_queue_families(scratch.arena, vk->phys_device, vk->surface);

  vk->renderpass = vulk_create_renderpass(vk->phys_device, vk->device, &vk->swapchain);

  // Pipeline layouts / pipelines
  if (cfg->n_pipeline_cfgs) {
    vk->pipelines = arena_push_array(Vulk_Pipeline, vk->arena, cfg->n_pipeline_cfgs);
    vk->n_pipelines = cfg->n_pipeline_cfgs;
  }
  
  for (u32 i = 0; i < cfg->n_pipeline_cfgs; ++i) {
    Vulk_Pipeline_Cfg *pcfg = &cfg->pipeline_cfgs[i];
    vk->pipelines[i].desc_set_layout = pcfg->create_desc_set(vk->device);
    vk->pipelines[i].layout = vulk_create_pipeline_layout(vk->device, vk->pipelines[i].desc_set_layout, pcfg->push_const_ranges, pcfg->n_push_const_ranges);
    if (!pcfg->is_compute) {
      vk->pipelines[i].pipeline = vulk_create_graphics_pipeline(scratch.arena, vk->device, vk->renderpass, vk->pipelines[i].layout, cfg->shaders_dir,
                                                                pcfg->shader, pcfg->topology, pcfg->flags, pcfg->vert_bindings, pcfg->n_vert_bindings,
                                                                pcfg->vert_attrs, pcfg->n_vert_attrs);
    } else {
      vk->pipelines[i].pipeline = vulk_create_compute_pipeline(scratch.arena, vk->device, vk->pipelines[i].layout, cfg->shaders_dir, pcfg->shader);
    }
  }

  vk->depth_image = vulk_create_depth_resources(vk->phys_device, vk->device, vk->swapchain.extent);
  vulk_create_framebuffers(vk->device, vk->renderpass, &vk->swapchain, vk->depth_image.view);
  vk->cmd_pool = vulk_create_cmd_pool(vk->device, queue_family_idx, false);
  vk->transient_cmd_pool = vulk_create_cmd_pool(vk->device, queue_family_idx, true);

  if (cfg->n_obj_types)
    vk->instance_buf = arena_push_array(Vulk_Instances, vk->arena, cfg->n_obj_types);
  
  for (u32 i = 0; i < cfg->n_obj_types; ++i) {
    vk->instance_buf[i].buffer = vulk_create_instance_buffer(vk, VULK_INSTANCE_BUF_SIZE);
    vk->instance_buf[i].instance_size = cfg->obj_instance_sizes[i];
    result->instance_capacity[i] = vk->instance_buf[i].buffer.size / cfg->obj_instance_sizes[i];
  }

  // Font
  vk->font_image = vulk_create_tex_image(vk, cfg->font_image);
  vulk_create_image_view(vk->device, VK_IMAGE_ASPECT_COLOR_BIT, &vk->font_image);
  vk->font_image_sampler = vulk_create_tex_sampler(vk->phys_device, vk->device, /* anisotropy = */ false);
   
  // Descriptor sets
  vk->desc_pool = vulk_create_descriptor_pool(vk->device);

  for (u32 i = 0; i < vk->n_pipelines; ++i) {
    vulk_create_descriptor_sets(vk->device, vk->desc_pool, vk->pipelines[i].desc_set_layout, vk->pipelines[i].desc_sets);
    if (cfg->pipeline_cfgs[i].configure_desc_sets)
      cfg->pipeline_cfgs[i].configure_desc_sets(vk);
  }

  vulk_alloc_cmd_buffers(vk->device, vk->cmd_pool, vk->cmd_buf, VULK_MAX_FRAMES_IN_FLIGHT);
  vulk_alloc_cmd_buffers(vk->device, vk->cmd_pool, vk->compute_cmd_buf, VULK_MAX_FRAMES_IN_FLIGHT);
  for (u32 i = 0; i < VULK_MAX_FRAMES_IN_FLIGHT; ++i) {
    vk->semph_image_available[i] = vulk_create_semaphore(vk->device);
    vk->semph_render_finished[i] = vulk_create_semaphore(vk->device);
    vk->semph_compute_finished[i] = vulk_create_semaphore(vk->device);
    vk->fence_frame_in_flight[i] = vulk_create_fence(vk->device, true);
    vk->fence_compute_in_flight[i] = vulk_create_fence(vk->device, true);
  }

  scratch_end(scratch);
}

void vulk_deinit(Gfx_Vulkan *vk)
{
  vulk_cleanup_swapchain(vk);

  vkDestroySampler(vk->device, vk->font_image_sampler, NULL);
  vulk_destroy_image(vk->device, vk->font_image);
  for (u32 i = 0; i < vk->images.n_images; ++i) {
    vkDestroySampler(vk->device, vk->images.samplers[i], NULL);
    vulk_destroy_image(vk->device, vk->images.images[i]);
  }
  vkDestroyDescriptorPool(vk->device, vk->desc_pool, NULL);
  for (u32 i = 0; i < VULK_MAX_FRAMES_IN_FLIGHT; ++i) {
    vkDestroySemaphore(vk->device, vk->semph_image_available[i], NULL);
    vkDestroySemaphore(vk->device, vk->semph_compute_finished[i], NULL);
    vkDestroySemaphore(vk->device, vk->semph_render_finished[i], NULL);
    vkDestroyFence(vk->device, vk->fence_compute_in_flight[i], NULL);
    vkDestroyFence(vk->device, vk->fence_frame_in_flight[i], NULL);
  }
  vkDestroyCommandPool(vk->device, vk->transient_cmd_pool, NULL);
  vkDestroyCommandPool(vk->device, vk->cmd_pool, NULL);
  for (u32 i = 0; i < vk->n_pipelines; ++i) {
    vkDestroyPipeline(vk->device, vk->pipelines[i].pipeline, NULL);
    vkDestroyPipelineLayout(vk->device, vk->pipelines[i].layout, NULL);
    vkDestroyDescriptorSetLayout(vk->device, vk->pipelines[i].desc_set_layout, NULL);
  }
  vkDestroyRenderPass(vk->device, vk->renderpass, NULL);
  vkDestroySurfaceKHR(vk->instance, vk->surface, NULL);
  vulk_buf_alloc_deinit(&vk->buf_alloc);
  vkDestroyDevice(vk->device, NULL);
  if (vk->debug_messenger)
    vulk_deinit_debug_callback(vk->instance, vk->debug_messenger);
  vkDestroyInstance(vk->instance, NULL);

  arena_release(vk->arena); 
}

void vulk_wait_idle(Gfx_Vulkan *vk)
{
  vkDeviceWaitIdle(vk->device);
}

void vulk_recreate_swapchain(Gfx_Vulkan *vk, Vulk_Window *window)
{
  DEBUG_TAG("Vulkan", "Recreating swapchain");

  vulkwin_block_until_visible(window);
  vulk_wait_idle(vk);

  Temp scratch = scratch_begin(0, 0);

  Queue_Family_Idx queue_family_idx = vulk_find_queue_families(scratch.arena, vk->phys_device, vk->surface);
  Swapchain_Support swapchain_support = vulk_query_swapchain_support(scratch.arena, vk->phys_device, vk->surface);
  // NOTE: passing the old swapchain to create the new one
  Vulk_Swapchain new_swapchain = vulk_create_swapchain(window, vk->device, vk->surface,
                                                       &queue_family_idx, &swapchain_support, vk->swapchain.handle, vk->srgb_enabled);
  // cleanup the old swapchain before replacing it
  vulk_cleanup_swapchain(vk);
  vk->swapchain = new_swapchain;
  vulk_create_swapchain_image_views(vk->device, &vk->swapchain);
  vk->depth_image = vulk_create_depth_resources(vk->phys_device, vk->device, vk->swapchain.extent);
  vulk_create_framebuffers(vk->device, vk->renderpass, &vk->swapchain, vk->depth_image.view);

  scratch_end(scratch);
}

// `vertices` must contain all contiguous text vertices.
void vulk_add_texts(Gfx_Vulkan *vk, const Vertex *vertices, u32 n_texts, u32 *n_vertices, u32 *instance_id, b8 is_screenspace,
                    Vulk_Text **out_texts)
{
  Vulk_Text **head, **tail;
  if (is_screenspace) {
    head = &vk->texts.ss_head;
    tail = &vk->texts.ss_tail;
  } else {
    head = &vk->texts.ws_head;
    tail = &vk->texts.ws_tail;
  }
  Temp scratch = scratch_begin(0, 0);
  VkDeviceSize *vbuffer_sizes = arena_push_array_nozero(VkDeviceSize, scratch.arena, n_texts);
  u32 vbuffer_sizes_idx = 0;

  u64 staging_buf_size = 0;
  for (u32 i = 0; i < n_texts; ++i) {
    if (vk->texts.free) {
      out_texts[i] = vk->texts.free;
      vk->texts.free = vk->texts.free->next;
      zero_struct(out_texts[i]);
    } else {
      out_texts[i] = arena_push(Vulk_Text, vk->arena);
    }
    out_texts[i]->screenspace = is_screenspace;
    out_texts[i]->instance_id = instance_id[i];
    out_texts[i]->n_vertices = n_vertices[i];
    push_to_dll(*head, *tail, out_texts[i]);

    u64 buf_size = n_vertices[i] * sizeof(Vertex);
    vbuffer_sizes[vbuffer_sizes_idx++] = buf_size;
    staging_buf_size += buf_size;
  }

  Vulk_Buffer *vbuffers = arena_push_array_nozero(Vulk_Buffer, scratch.arena, n_texts);
  Vulk_Buffer tot_vbuf = vulk_alloc_contiguous_buffers(&vk->buf_alloc, vbuffer_sizes_idx, vbuffer_sizes, staging_buf_size,
                                                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, Vulk_Buf_Unmapped,
                                                       vbuffers);
  u32 vbuffer_idx = 0;
  for (u32 i = 0; i < n_texts; ++i) {
    if (n_vertices[i] > 0)
      out_texts[i]->vbuffer = vbuffers[vbuffer_idx++];
  }

  // Since `vertices` is contiguous, we can issue a single copy command.
  Vulk_Buffer staging = vulk_alloc_buffer(&vk->buf_alloc, staging_buf_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                                          Vulk_Buf_Mapped);
  memcpy(staging.mapped, vertices, staging_buf_size);
  vulk_unmap_buffer(&staging);

  VkBufferCopy copy = {};
  copy.size = staging.size;
  copy.srcOffset = staging.offset;
  copy.dstOffset = tot_vbuf.offset;
  vulk_copy_buffer(vk, staging.buffer, tot_vbuf.buffer, 1, &copy);

  vulk_free_buffer(&vk->buf_alloc, &staging);

  scratch_end(scratch);
}

Vulk_Text *vulk_add_text(Gfx_Vulkan *vk, const Vertex *vertices, u32 n_vertices, u32 instance_id, b8 is_screenspace)
{
  Vulk_Text *text;
  if (vk->texts.free) {
    text = vk->texts.free;
    vk->texts.free = vk->texts.free->next;
    zero_struct(text);
  } else {
    text = arena_push(Vulk_Text, vk->arena);
  }

  text->screenspace = is_screenspace;
  text->instance_id = instance_id;
  if (is_screenspace) {
    push_to_dll(vk->texts.ss_head, vk->texts.ss_tail, text);
  } else {
    push_to_dll(vk->texts.ws_head, vk->texts.ws_tail, text);
  }

  // @Speed: we could delay the actual copy to the gpu until vulk_do_frame (or close to it)
  // and try to coalesce as many copies as possible to avoid the overhead of recording and
  // submitting lots of cmd buffers. This would especially help during graph loading.
  // In other words, we need a vulk_add_texts API (and similarly a vulk_remove_texts,
  // vulk_add_quads etc).
  vulk_init_text_vertices(vk, text, vertices, n_vertices);

  return text;
}

void vulk_remove_text(Gfx_Vulkan *vk, Vulk_Text *text)
{
  vulk_deinit_text_vertices(vk, text);
  if (text->screenspace) {
    pop_from_dll_add_to_free(vk->texts.ss_head, vk->texts.ss_tail, text, vk->texts.free);
  } else {
    pop_from_dll_add_to_free(vk->texts.ws_head, vk->texts.ws_tail, text, vk->texts.free);
  }
}

void vulk_set_text_instance_id(Vulk_Text *text, u32 instance_id)
{
  text->instance_id = instance_id;
}

void vulk_set_text_vertices(Gfx_Vulkan *vk, Vulk_Text *text, const Vertex *vertices, u32 n_vertices)
{
  vulk_deinit_text_vertices(vk, text);
  vulk_init_text_vertices(vk, text, vertices, n_vertices);
}

void vulk_update_instance_buffer(Gfx_Vulkan *vk, Vulk_Obj_Type obj_type, void *data, u64 data_size, u64 offset)
{
  Vulk_Buffer *buf = &vk->instance_buf[obj_type].buffer;
  assert(data_size <= buf->size - offset);
  memcpy((u8 *)buf->mapped + offset, data, data_size);
}

VkDevice vulk_get_device(Gfx_Vulkan *vk)
{
  return vk->device;
}

Vulk_Buffer *vulk_get_instance_buffer(Gfx_Vulkan *vk, Vulk_Obj_Type obj_type)
{
  return &vk->instance_buf[obj_type].buffer;
}

Vulk_Pipeline *vulk_get_pipeline(Gfx_Vulkan *vk, u32 pipeline_idx)
{
  assert(pipeline_idx < vk->n_pipelines);
  return &vk->pipelines[pipeline_idx];
}

b8 vulk_begin_cmd_buf(VkCommandBuffer cmd_buf)
{
  VkCommandBufferBeginInfo begin_info = {};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  
  VkResult res = vkBeginCommandBuffer(cmd_buf, &begin_info);
  if (res != VK_SUCCESS) {
    ERR_TAG("Vulkan", "Failed to begin cmd buffer");
    return false;
  }
  return true;
}

b8 vulk_end_cmd_buf(VkCommandBuffer cmd_buf)
{
  VkResult res = vkEndCommandBuffer(cmd_buf);
  if (res != VK_SUCCESS) {
    ERR_TAG("Vulkan", "Failed to end cmd buffer");
    return false;
  }
  return true;
}
