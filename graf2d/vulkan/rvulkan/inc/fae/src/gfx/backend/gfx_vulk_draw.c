enum Pipeline_Id {
  Pipeline_Quad,
  Pipeline_Text,
  Pipeline_COUNT
};

struct Push_Const_Gfx {
  alignas(16) M44 view_proj;
};

typedef struct {
  // used for quads and texts
  Push_Const_Gfx gfx;
} Vulk_Draw_Push_Constants;

typedef struct {
  Vulk_Draw_Push_Constants pc;
  u64 n_quads;
  V3 clear_color;
  b8 proj_is_ortho;
} Vulk_Draw_Data;

internal
void vulk_draw_quads(Gfx_Vulkan *vk, Vulk_Draw_Data *draw_data, VkCommandBuffer cmd_buf)
{
  Vulk_Pipeline *pipeline = &vk->pipelines[Pipeline_Quad];
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout,
                          0, 1, &pipeline->desc_sets[vk->cur_frame], 0, NULL);
  vkCmdPushConstants(cmd_buf, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(draw_data->pc.gfx), &draw_data->pc.gfx);
  vkCmdDraw(cmd_buf, 4, draw_data->n_quads, 0, 0);
}

internal
void vulk_draw_texts(Gfx_Vulkan *vk, Vulk_Draw_Data *draw_data, VkCommandBuffer cmd_buf)
{
  Vulk_Pipeline *pipeline = &vk->pipelines[Pipeline_Text];
  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
  vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout,
                          0, 1, &pipeline->desc_sets[vk->cur_frame], 0, NULL);

  // World-space texts
  vkCmdPushConstants(cmd_buf, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(draw_data->pc.gfx), &draw_data->pc.gfx);
  for (Vulk_Text *text = vk->texts.ws_head; text; text = text->next) {
    if (!text->n_vertices)
      continue;
    
    VkDeviceSize offsets[1] = { text->vbuffer.offset };
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &text->vbuffer.buffer, offsets);
    vkCmdDraw(cmd_buf, text->n_vertices, 1, 0, text->instance_id);
  }

  // Screen-space texts
  // vkCmdPushConstants(cmd_buf, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(draw_data->pc.ss_text), &draw_data->pc.ss_text);
  // for (Vulk_Text *text = vk->texts.ss_head; text; text = text->next) {
  //   if (!text->n_vertices)
  //     continue;

  //   // TODO: culling
  //   vkCmdBindVertexBuffers(cmd_buf, 0, 1, &text->vbuffer.buffer, &text->vbuffer.offset);
  //   vkCmdDraw(cmd_buf, text->n_vertices, 1, 0, text->instance_id);
  // }
}

internal
void vulk_draw(Gfx_Vulkan *vk, u32 img_idx, Vulk_Draw_Data *draw_data)
{
  V3 cc = draw_data->clear_color;
  VkClearValue clear_values[2] = {};
  clear_values[0].color = (VkClearColorValue){{ cc.x, cc.y, cc.z, 1.0f }};
  clear_values[1].depthStencil = (VkClearDepthStencilValue){ 0.f, 0 };

  VkRenderPassBeginInfo renderpass_info = {};
  renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderpass_info.renderPass = vk->renderpass;
  renderpass_info.framebuffer = vk->swapchain.framebuffers[img_idx];
  renderpass_info.renderArea.extent = vk->swapchain.extent;
  renderpass_info.clearValueCount = countof(clear_values);
  renderpass_info.pClearValues = clear_values;

  VkViewport viewport = {};
  viewport.width = (f32)vk->swapchain.extent.width;
  viewport.height = (f32)vk->swapchain.extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.extent = vk->swapchain.extent;

  VkCommandBuffer cmd_buf = vk->cmd_buf[vk->cur_frame];
  vkResetCommandBuffer(cmd_buf, 0);
  if (!vulk_begin_cmd_buf(cmd_buf))
    os_abort();
  
  vkCmdBeginRenderPass(cmd_buf, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
  vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

  vulk_draw_quads(vk, draw_data, cmd_buf);
  vulk_draw_texts(vk, draw_data, cmd_buf);

  vkCmdEndRenderPass(cmd_buf);

  if (!vulk_end_cmd_buf(cmd_buf))
    os_abort();
}

internal
void vulk_do_frame(Gfx_Vulkan *vk, GLFWwindow *window, Vulk_Draw_Data *draw_data)
{
  // wait for previous frame to finish
  vkWaitForFences(vk->device, 1, &vk->fence_frame_in_flight[vk->cur_frame], VK_TRUE, UINT64_MAX);

  // acquire image from the swapchain
  u32 img_idx;
  VkResult res = vkAcquireNextImageKHR(vk->device, vk->swapchain.handle, UINT64_MAX, vk->semph_image_available[vk->cur_frame], VK_NULL_HANDLE, &img_idx);
  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    vulk_recreate_swapchain(vk, window);
    return;
  }
  if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
    FATAL_TAG("Vulkan", "Failed to acquire swapchain image");
    os_abort();
  }
  
  vkResetFences(vk->device, 1, &vk->fence_frame_in_flight[vk->cur_frame]);
  vulk_draw(vk, img_idx, draw_data);

  // submit the command buffer
  VkSemaphore wait_semaphores[] = { vk->semph_image_available[vk->cur_frame] };
  VkSemaphore signal_semaphores[] = { vk->semph_render_finished[vk->cur_frame] };
  VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  assert(countof(wait_stages) == countof(wait_semaphores));
  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = countof(wait_semaphores);
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &vk->cmd_buf[vk->cur_frame];
  submit_info.signalSemaphoreCount = countof(signal_semaphores);
  submit_info.pSignalSemaphores = signal_semaphores;

  res = vkQueueSubmit(vk->gfx_queue, 1, &submit_info, vk->fence_frame_in_flight[vk->cur_frame]);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to submit the draw command buffer");
    os_abort();
  }

  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = countof(signal_semaphores);
  present_info.pWaitSemaphores = signal_semaphores;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &vk->swapchain.handle;
  present_info.pImageIndices = &img_idx;

  res = vkQueuePresentKHR(vk->present_queue, &present_info);
  if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || vk->swapchain_stale) {
    vulk_recreate_swapchain(vk, window);
    vk->swapchain_stale = false;
  } else if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to present swapchain image");
    os_abort();
  }

  vk->cur_frame = (vk->cur_frame + 1) % VULK_MAX_FRAMES_IN_FLIGHT;
}
