const char **vulkwin_get_required_instance_extensions(u32 *ext_count)
{
  return glfwGetRequiredInstanceExtensions(ext_count);
}

VkSurfaceKHR vulkwin_create_surface(VkInstance instance, Vulk_Window *window)
{
  VkSurfaceKHR surface;
  VkResult res = glfwCreateWindowSurface(instance, (GLFWwindow *)window, NULL, &surface);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create window surface");
    os_abort();
  }

  return surface;
}

VkExtent2D vulkwin_select_swap_extent(Vulk_Window *window, VkSurfaceCapabilitiesKHR *caps)
{
  if (caps->currentExtent.width != UINT32_MAX) {
    return caps->currentExtent;
  }
  i32 w, h;
  glfwGetFramebufferSize((GLFWwindow *)window, &w, &h);

  VkExtent2D actual_extent = { (u32)w, (u32)h };
  actual_extent.width = Clamp(actual_extent.width, caps->minImageExtent.width, caps->maxImageExtent.width);
  actual_extent.height = Clamp(actual_extent.height, caps->minImageExtent.height, caps->maxImageExtent.height);

  return actual_extent;
}

void vulkwin_block_until_visible(Vulk_Window *win)
{
  GLFWwindow *window = (GLFWwindow *)win;
  if (window) {
    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }
  }
} 
