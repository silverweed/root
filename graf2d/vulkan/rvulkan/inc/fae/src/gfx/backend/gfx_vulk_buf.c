#define VULK_MAX_BACKING_BUFFERS 100
#define VULK_BACKING_BUFFER_MIN_SIZE (100 * 1024 * 1024)

typedef struct {
  VkPhysicalDevice phys_device;
  VkDevice device;

  Arena *arena;
  Vulk_Backing_Buffer buffers[VULK_MAX_BACKING_BUFFERS];
  u16 n_backing_buffers;

  // somewhat confusing, but this is the list of freed Vulk_Buf_Node that are available
  // for reuse for the free_list of the various backing buffers.
  Vulk_Buf_Node *free_list_free;

  // How many Vulk_Buffers are currently allocated
  u32 n_buffers_allocated;
  u32 n_free_nodes_allocated;
} Vulk_Buf_Alloc;

internal
Vulk_Buf_Alloc vulk_buf_alloc_init(Arena *arena, VkPhysicalDevice phys_device, VkDevice device)
{
  Vulk_Buf_Alloc res = {};
  res.phys_device = phys_device;
  res.device = device;
  res.arena = arena;
  return res;
}

internal
void vulk_buf_alloc_deinit(Vulk_Buf_Alloc *alloc)
{
  for (u16 i = 0; i < alloc->n_backing_buffers; ++i) {
    vkDestroyBuffer(alloc->device, alloc->buffers[i].buffer, NULL);
    vkFreeMemory(alloc->device, alloc->buffers[i].memory, NULL);
  }
}

internal
u32 vulk_find_memory_type(VkPhysicalDevice phys_device, u32 type_filter, VkMemoryPropertyFlags props)
{
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(phys_device, &mem_props);

  for (u32 i = 0; i < mem_props.memoryTypeCount; ++i) {
    if (type_filter & (1 << i) && (mem_props.memoryTypes[i].propertyFlags & props) == props)
      return i;
  }

  FATAL_TAG("Vulkan", "Failed to find suitable memory type");
  os_abort();

  return 0;
}

internal
Vulk_Buf_Node *vulk_alloc_buf_free_node(Vulk_Buf_Alloc *alloc, u64 start, u64 size)
{
  Vulk_Buf_Node *node;
  if (alloc->free_list_free) {
    node = alloc->free_list_free;
    alloc->free_list_free = alloc->free_list_free->next_by_size;
    zero_struct(node);
  } else {
    node = arena_push(Vulk_Buf_Node, alloc->arena);
  }
  node->rng.start = start;
  node->rng.size = size;

  ++alloc->n_free_nodes_allocated;

  return node;
}

internal
void vulk_dealloc_buf_free_node(Vulk_Buf_Alloc *alloc, Vulk_Buf_Node *free, Vulk_Backing_Buffer *backing)
{
  pop_from_dll_ex(backing->free_list_head_by_size, backing->free_list_tail_by_size, free, next_by_size, prev_by_size);
  pop_from_dll_ex(backing->free_list_head_by_addr, backing->free_list_tail_by_addr, free, next_by_addr, prev_by_addr);
  free->next_by_size = alloc->free_list_free;
  alloc->free_list_free = free;
  --alloc->n_free_nodes_allocated;
}

// creates an actual Vulkan buffer and allocates memory for it.
internal
Vulk_Backing_Buffer vulk_create_buffer(Vulk_Buf_Alloc *alloc, VkDeviceSize size,
                                       VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
{
  assert_always(size > 0);

  Vulk_Backing_Buffer buffer = {};
  buffer.size = size;
  
  VkBufferCreateInfo buf_info = {};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.size = size;
  buf_info.usage = usage;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkResult res = vkCreateBuffer(alloc->device, &buf_info, NULL, &buffer.buffer);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to create buffer");
    os_abort();
  }

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(alloc->device, buffer.buffer, &mem_reqs);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_reqs.size;
  alloc_info.memoryTypeIndex = vulk_find_memory_type(alloc->phys_device, mem_reqs.memoryTypeBits, props);

  res = vkAllocateMemory(alloc->device, &alloc_info, NULL, &buffer.memory);
  if (res != VK_SUCCESS) {
    FATAL_TAG("Vulkan", "Failed to allocate buffer memory");
    os_abort();
  }

  vkBindBufferMemory(alloc->device, buffer.buffer, buffer.memory, 0);

  // if this buffer can in principle be mapped, map it to allow sub-buffers to be "mapped" individually
  // (vkMapMemory cannot map multiple suballocations, so we must map everything)
  if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    VkResult res = vkMapMemory(alloc->device, buffer.memory, 0, VK_WHOLE_SIZE, 0, &buffer.mapped);
    if (res != VK_SUCCESS) {
      FATAL_TAG("Vulkan", "Failed to map buffer memory to host");
      os_abort();
    }
  }

  buffer.usage = usage;
  buffer.props = props;
  buffer.free_list_head_by_size = vulk_alloc_buf_free_node(alloc, 0, size);
  buffer.free_list_tail_by_size = buffer.free_list_head_by_addr = buffer.free_list_tail_by_addr =
    buffer.free_list_head_by_size;

  return buffer;
}

internal
void vulk_destroy_buffer(VkDevice device, Vulk_Backing_Buffer *buffer)
{
  vkDestroyBuffer(device, buffer->buffer, NULL);
  vkFreeMemory(device, buffer->memory, NULL);
  buffer->size = 0;
}

internal
void vulk_buf_check_free_lists(Vulk_Backing_Buffer *backing)
{
#ifndef NDEBUG
  i64 prev_start = -1;
  for (Vulk_Buf_Node *node = backing->free_list_head_by_addr; node; node = node->next_by_addr) {
    i64 start = (i64)node->rng.start;
    if (start <= prev_start) {
      FATAL_TAG("Vulkan.Buf", "free list is not properly sorted by addr!");
      Temp scr = scratch_begin(0, 0);
      String8_Node *sn = NULL;
      for (Vulk_Buf_Node *n = backing->free_list_head_by_size; n; n = n->next_by_size)
        sn = push_str8_node(scr.arena, sn, "0x%" PRIX64, n->rng.start);
      String8 s = sn ? str8_node_join(scr.arena, sn->head, " -> ") : str8("(empty)");
      FATAL_TAG("Vulkan.Buf", "list: %s", cstr(s));
      scratch_end(scr);
      os_abort();
    }
    prev_start = start;
  }

  u64 prev_size = 0;
  for (Vulk_Buf_Node *node = backing->free_list_head_by_size; node; node = node->next_by_size) {
    if (node->rng.size < prev_size) {
      FATAL_TAG("Vulkan.Buf", "free list is not properly sorted by size!");
      Temp scr = scratch_begin(0, 0);
      String8_Node *sn = NULL;
      for (Vulk_Buf_Node *n = backing->free_list_head_by_size; n; n = n->next_by_size)
        sn = push_str8_node(scr.arena, sn, "%lu", n->rng.size);
      String8 s = sn ? str8_node_join(scr.arena, sn->head, " -> ") : str8("(empty)");
      FATAL_TAG("Vulkan.Buf", "list: %s", cstr(s));
      scratch_end(scr);
      os_abort();
    }
    prev_size = node->rng.size;
  }
#else
  (void)backing;
#endif
}

internal
Vulk_Buffer vulk_alloc_buffer(Vulk_Buf_Alloc *alloc, VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags props, b8 host_mapped)
{
  // Find if we already allocated a buffer with these properties
  u16 found = countof(alloc->buffers);
  for (u16 i = 0; i < alloc->n_backing_buffers; ++i) {
    if (alloc->buffers[i].usage == usage && alloc->buffers[i].props == props) {
      // check that this buffer has the capacity to hold the requested size
      if (alloc->buffers[i].free_list_tail_by_size && alloc->buffers[i].free_list_tail_by_size->rng.size >= size) {
        found = i;
        break;
      }
    }
  }

  if (found == countof(alloc->buffers)) {
    // not found: allocate new
    if (alloc->n_backing_buffers == VULK_MAX_BACKING_BUFFERS) {
      FATAL_TAG("Vulkan.Buf", "Attempting to allocate too many buffers.");
      os_abort();
    }
    u64 backing_size = Max(size, VULK_BACKING_BUFFER_MIN_SIZE);
    found = alloc->n_backing_buffers++;
    alloc->buffers[found] = vulk_create_buffer(alloc, backing_size, usage, props);
    DEBUG_TAG("Vulkan.Buf", "Allocating new backing buffer with size %" PRIu64, backing_size);
  }

  /// suballocate
  assert(found < alloc->n_backing_buffers);
  Vulk_Backing_Buffer *backing_buf = &alloc->buffers[found];

  Vulk_Buffer buf = {};
  // find first free slot
  for (Vulk_Buf_Node *free = backing_buf->free_list_head_by_size; free; free = free->next_by_size) {
    if (free->rng.size >= size) {
      buf.backing = backing_buf;
      buf.buffer = backing_buf->buffer;
      buf.memory = backing_buf->memory;
      buf.offset = free->rng.start;
      buf.size = size;
      if (host_mapped) {
        assert(backing_buf->mapped);
        buf.mapped = (u8 *)backing_buf->mapped + buf.offset;
      }
      
      // TODO: alignment
      VERY_VERBOSE_TAG("Vulkan.Buf", "allocating [%lu, %lu] from buf %u", free->rng.start, free->rng.start + buf.size, found);
      free->rng.start += buf.size;
      free->rng.size -= buf.size;
      if (free->rng.size == 0) {
        vulk_dealloc_buf_free_node(alloc, free, backing_buf);
      } else if (free != backing_buf->free_list_head_by_size) {
        // since the size of this node changed, it might need to be reordered (by sliding it to the left in the list)
        Vulk_Buf_Node *new_prev = free->prev_by_size;
        assert(new_prev);
        while (new_prev->rng.size > free->rng.size) {
          new_prev = new_prev->prev_by_size;
          if (!new_prev)
            break;
        }
        relink_dll_node_ex(Vulk_Buf_Node, backing_buf->free_list_head_by_size, backing_buf->free_list_tail_by_size, 
                           free, new_prev, next_by_size, prev_by_size);
      }
      
      print_sll_ex(Vulk_Buf_Node, "free list", Log_Very_Verbose, backing_buf->free_list_head_by_size, 0, 0, next_by_size);
      print_sll_ex(Vulk_Buf_Node, "free list free", Log_Very_Verbose, alloc->free_list_free, 0, 0, next_by_size);
      break;
    }
  }

  if (!buf.backing) {
    // TODO: allocate a new backing buffer?
    FATAL_TAG("Vulkan", "Failed to allocate new buffer.");
    os_abort();
  }

  ++alloc->n_buffers_allocated;
  VERBOSE_TAG("Vulkan.Buf", "Allocated buf %d: 0x%lX - 0x%lX. Tot allocated: %u (backing: %u)", 
              found, buf.offset, buf.offset + buf.size, alloc->n_buffers_allocated, alloc->n_backing_buffers);
  vulk_buf_check_free_lists(backing_buf);
  return buf;
}

internal
void vulk_free_buffer(Vulk_Buf_Alloc *alloc, Vulk_Buffer *buffer)
{
  buffer->mapped = NULL;

  Vulk_Backing_Buffer *backing = buffer->backing;
  u32 backing_idx = (backing - alloc->buffers);
  u64 buffer_end = buffer->offset + buffer->size;

  // Insert the new free range in the free list.
  // We check if we can coalesce the freed range with another; otherwise, we insert a new node and fragment the memory
  Vulk_Buf_Node *first_bigger = NULL;
  Vulk_Buf_Node *last_visited_by_addr = NULL;
  b8 coalesced = false;
  for (Vulk_Buf_Node *free = backing->free_list_head_by_addr; free; free = free->next_by_addr) {
    last_visited_by_addr = free;
    u64 free_end = free->rng.start + free->rng.size;
    if (buffer_end == free->rng.start) {
      // The range we're freeing can be prepended to this free node.
      free->rng.start = buffer->offset;
      free->rng.size += buffer->size;
      VERBOSE_TAG("Vulkan.Buf", "freeing [0x%" PRIX64", 0x%" PRIX64"] from buf %u (coalescing with 0x%" PRIX64 ", 0x%" PRIX64")", 
                  buffer->offset, buffer_end, backing_idx, free->rng.start, free_end);
      coalesced = true;
    } else if (buffer->offset == free_end) {
      // The range we're freeing can be appended to this free node.
      free->rng.size += buffer->size;
      VERBOSE_TAG("Vulkan.Buf", "freeing [0x%" PRIX64", 0x%" PRIX64"] from buf %u (coalescing with 0x%" PRIX64 ", 0x%" PRIX64")", 
                  buffer->offset, buffer_end, backing_idx, free->rng.start, free_end);

      // Check if we perfectly fit between two nodes and, if so, merge them
      if (free->next_by_addr && buffer_end == free->next_by_addr->rng.start) {
        free->rng.size += free->next_by_addr->rng.size;
        vulk_dealloc_buf_free_node(alloc, free->next_by_addr, backing);
        VERBOSE_TAG("Vulkan.Buf", "merging two consecutive free nodes");
      }
      coalesced = true;
    }

    if (coalesced) {
      // the node we extended may need to be reordered
      Vulk_Buf_Node *cur = free;
      while (cur->next_by_size && cur->next_by_size->rng.size < free->rng.size) {
        cur = cur->next_by_size;
      }
      if (cur != free) {
        relink_dll_node_ex(Vulk_Buf_Node, backing->free_list_head_by_size, backing->free_list_tail_by_size, free, cur,
                           next_by_size, prev_by_size);
      }
      break;
    }

    // Keep track of the smallest buffer we see that's bigger than the one we're freeing.
    // We can use that as the starting point to find the slot we can fit the buffer in if we fail to coalesce it
    if (free->rng.size > buffer->size && (!first_bigger || first_bigger->rng.size > free->rng.size))
      first_bigger = free;

    if (free->rng.start > buffer_end) {
      // no change to coalesce this, give up
      break;
    }
  }

  if (!coalesced) {
    VERBOSE_TAG("Vulkan.Buf", "freeing [0x%lX, 0x%lX] from buf %u (fragmenting memory)", 
                buffer->offset, buffer_end, backing_idx);

    // This range wasn't adjacent to any other free range: add it to the list in the right order
    Vulk_Buf_Node *newfree = vulk_alloc_buf_free_node(alloc, buffer->offset, buffer->size);

    // push to by-size list
    if (first_bigger) {
      // traverse the by-size list until we find the right slot for the new node
      while (first_bigger->prev_by_size && first_bigger->prev_by_size->rng.size > buffer->size)
        first_bigger = first_bigger->prev_by_size;

      insert_into_dll_ex(Vulk_Buf_Node, backing->free_list_head_by_size, backing->free_list_tail_by_size, 
                         newfree, first_bigger->prev_by_size, next_by_size, prev_by_size);
    } else {
      // either the free list was empty or this buffer was bigger than any other
      push_to_dll_ex(backing->free_list_head_by_size, backing->free_list_tail_by_size, 
                     newfree, next_by_size, prev_by_size);
    }

    // push to by-addr list
    if (last_visited_by_addr) {
      if (last_visited_by_addr->rng.start < buffer->offset) {
        insert_into_dll_ex(Vulk_Buf_Node, backing->free_list_head_by_addr, backing->free_list_tail_by_addr, 
                           newfree, last_visited_by_addr, next_by_addr, prev_by_addr);
      } else {
        insert_into_dll_ex(Vulk_Buf_Node, backing->free_list_head_by_addr, backing->free_list_tail_by_addr,
                           newfree, last_visited_by_addr->prev_by_addr, next_by_addr, prev_by_addr);
      }
    } else {
      push_to_dll_ex(backing->free_list_head_by_addr, backing->free_list_tail_by_addr,
                     newfree, next_by_addr, prev_by_addr);      
    }
  }

  --alloc->n_buffers_allocated;
  VERBOSE_TAG("Vulkan.Buf", "Freed buffer. Tot allocated: %u (backing: %u); holes: %u", 
              alloc->n_buffers_allocated, alloc->n_backing_buffers, alloc->n_free_nodes_allocated);
  vulk_buf_check_free_lists(backing);
}

internal
void vulk_map_buffer(Vulk_Buffer *buf, u64 off, u64 size)
{
  (void)size;
  assert(!buf->mapped);
  assert(size <= buf->size);
  assert(buf->backing->mapped);
  buf->mapped = (u8 *)buf->backing->mapped + buf->offset + off;
}

internal
void vulk_unmap_buffer(Vulk_Buffer *buf)
{
  assert(buf->mapped);
  buf->mapped = NULL;
}

// Allocates a single continuous range of size equal to the sum of `sizes` and splits it into multiple sub-buffers.
// `tot_size` must be equal to the sum of `sizes`.
// Returns the big contiguous buffer.
internal
Vulk_Buffer vulk_alloc_contiguous_buffers(Vulk_Buf_Alloc *alloc, u32 n_backing_buffers, VkDeviceSize *sizes, VkDeviceSize tot_size,
                                   VkBufferUsageFlags usage, VkMemoryPropertyFlags props, Vulk_Buf_Map_Status host_mapped,
                                   Vulk_Buffer *buffers)
{
  Vulk_Buffer tot_buf = vulk_alloc_buffer(alloc, tot_size, usage, props, host_mapped);
  VkDeviceSize cur_off = 0;
  for (u32 i = 0; i < n_backing_buffers; ++i) {
    assert(sizes[i] > 0);
    memcpy(&buffers[i], &tot_buf, sizeof(tot_buf));
    buffers[i].offset += cur_off;
    buffers[i].size = sizes[i];
    cur_off += sizes[i];
  }
  return tot_buf;
}
