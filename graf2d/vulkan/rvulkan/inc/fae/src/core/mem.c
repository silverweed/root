internal
b8 is_aligned(intptr_t val, usz align)
{
  return (val & (align - 1)) == 0;
}

internal
intptr_t align_offset(intptr_t addr, usz align)
{
  return (~addr + 1) & (align - 1);
}

#ifdef FAE_SANITIZE_ARENA
// disable LeakSanitizer report when exiting the app
const char *__asan_default_options() { return "detect_leaks=0"; }
#define asan_poison_memory_region(mem, cmt) __asan_poison_memory_region(mem, cmt)
#define asan_unpoison_memory_region(mem, cmt) __asan_unpoison_memory_region(mem, cmt)
#else
#define asan_poison_memory_region(mem, cmt)
#define asan_unpoison_memory_region(mem, cmt)
#endif

// init_res: initial reserved size
// init_cmt: initial committed size
internal 
Arena *arena_alloc_sized(u64 init_res, u64 init_cmt)
{
  assert(ARENA_HEADER_SIZE < init_cmt && init_cmt <= init_res);

  u64 page_size = os_page_size();
  u64 res = align_pow2(init_res + ARENA_HEADER_SIZE, page_size);
  u64 cmt = align_pow2(init_cmt + ARENA_HEADER_SIZE, page_size);

  // reserve memory
  DEBUG_TAG("Mem", "reserving %" PRIu64 " kiB of memory", res >> 10);
  void *mem = os_reserve(res);
  if (!os_commit(mem, cmt)) {
    os_release(mem, res);
  }

  Arena *arena = (Arena *)mem;
  if (arena) {
    asan_poison_memory_region(mem, cmt);
    asan_unpoison_memory_region(mem, ARENA_HEADER_SIZE);

    arena->cur = arena;
    arena->pos = ARENA_HEADER_SIZE;
    arena->cmt = cmt;
    arena->res = res;
    arena->mem_used = arena->mem_peak_used = ARENA_HEADER_SIZE;
  }

  return arena;
}

Arena *arena_alloc()
{
  return arena_alloc_sized(ARENA_RESERVE_SIZE, ARENA_COMMIT_SIZE);
}

void arena_release(Arena *arena)
{
  for (Arena *node = arena->cur, *prev = 0; node; node = prev) {
    prev = node->prev;
    os_release(node, node->res);
  }
}

void *arena_push_aligned(Arena *arena, u64 size, u64 align)
{
  assert(size > 0);
  assert(align > 0 && (align & (align - 1)) == 0);

  Arena *cur = arena->cur;
  u64 pos_mem = align_pow2(cur->pos, align);
  u64 pos_new = pos_mem + size;

  if (cur->res < pos_new) {
    u64 res_size = cur->res;
    u64 cmt_size = cur->cmt;
    if (size > res_size) {
      res_size = size + ARENA_HEADER_SIZE;
      cmt_size = size + ARENA_HEADER_SIZE;
    }
    if (arena->mem_used + cmt_size > ARENA_MAX_COMMIT) {
      FATAL_TAG("Arena", "Tried to allocate too much memory! (more than %u GB)",
                ARENA_MAX_COMMIT >> 30);
      os_abort();
    }
    Arena *new_block = arena_alloc_sized(res_size, cmt_size);
    new_block->base_pos = cur->base_pos + cur->res;
    new_block->prev = arena->cur;
    arena->cur = new_block;

    cur = new_block;
    pos_mem = align_pow2(cur->pos, align);
    pos_new = pos_mem + size;
  }

  if (cur->cmt < pos_new) {
    u64 cmt_new_aligned = align_pow2(pos_new, cur->cmt);
    u64 cmt_new_clamped = Min(cmt_new_aligned, cur->res);
    u64 cmt_new_size = cmt_new_clamped - cur->cmt;
    DEBUG_TAG("Mem", "Committing %" PRIu64 " kiB of memory", cmt_new_size >> 10);
    os_commit((u8*)cur + cur->cmt, cmt_new_size);
    cur->cmt = cmt_new_clamped;
  }

  void *mem = 0;
  if (cur->cmt >= pos_new) {
    mem = (u8*)cur + pos_mem;
    u64 added_size = pos_new - cur->pos;
    assert(added_size >= size);
    arena->mem_used += added_size;
    arena->mem_peak_used = Max(arena->mem_peak_used, arena->mem_used);
    cur->pos = pos_new;
    asan_unpoison_memory_region(mem, size);
  }

  if (UNLIKELY(!mem)) {
    FATAL("Failed to grow arena.\n");
    os_abort();
  }

  return mem;
}

u64 arena_pos(Arena *arena)
{
  Arena *cur = arena->cur;
  u64 pos = cur->base_pos + cur->pos;
  return pos;
}

void arena_pop_to(Arena *arena, u64 big_pos_unclamped)
{
  assert(!(arena->flags & ArenaFlag_Disallow_Pop));

  u64 big_pos = Max(ARENA_HEADER_SIZE, big_pos_unclamped);
  
  // unroll the chain
  Arena *current = arena->cur;
  for (Arena *prev = 0; current->base_pos >= big_pos; current = prev) {
    prev = current->prev;
    os_release(current, current->res);
  }
  assert(current);
  arena->cur = current;
  
  // compute arena-relative position
  u64 new_pos = big_pos - current->base_pos;
  assert(new_pos <= current->pos);
  
  // poison popped memory block
  asan_poison_memory_region((u8*)current + new_pos, (current->pos - new_pos));

  arena->mem_used -= (current->pos - new_pos);
  
  // update position
  current->pos = new_pos;
}

Temp temp_begin(Arena *arena)
{
  u64 pos = arena_pos(arena);
  Temp temp = {arena, pos};
  return temp;
}

void temp_end(Temp temp)
{
  arena_pop_to(temp.arena, temp.pos);
}

thread_local Thread_Ctx *thread_local_ctx;

void tctx_init(Thread_Ctx *tctx)
{
  zero_struct(tctx);
  Arena **arena_ptr = tctx->arenas;
  for (u64 i = 0; i < countof(tctx->arenas); ++i, ++arena_ptr) {
    *arena_ptr = arena_alloc();
  }
  thread_local_ctx = tctx;
}

void tctx_release()
{
  for (u64 i = 0; i < countof(thread_local_ctx->arenas); ++i)
    arena_release(thread_local_ctx->arenas[i]);
}

Arena *tctx_get_scratch(Arena **conflicts, u64 count)
{
  assert(!count == !conflicts);
  
  Thread_Ctx *tctx = thread_local_ctx;
  Arena *res = NULL;
  Arena **arena_ptr = tctx->arenas;
  for (u64 i = 0; i < countof(tctx->arenas); ++i, ++arena_ptr) {
    Arena **conflict_ptr = conflicts;
    b8 has_conflict = false;
    for (u64 j = 0; j < count; ++j, ++conflict_ptr) {
      if (*arena_ptr == *conflict_ptr) {
        has_conflict = true;
        break;
      }
    }
    if (!has_conflict) {
      res = *arena_ptr;
      break;
    }
  }

  return res;
}

