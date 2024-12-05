#pragma once

// Mostly taken from the raddebugger -- thanks, Ryan.
#define ARENA_HEADER_SIZE 128
#define ARENA_COMMIT_SIZE (64 * 1024)
#define ARENA_RESERVE_SIZE (64 * 1024 * 1024)
#define ARENA_MAX_COMMIT (12ll << 30)

enum {
  ArenaFlag_None         = 0x0,
  ArenaFlag_Disallow_Pop = 0x1,
};

typedef struct Arena {
  struct Arena *prev;
  struct Arena *cur;
  u64 base_pos;
  u64 pos;
  u64 cmt;
  u64 res;
  u64 align;
  u64 mem_used;
  u64 mem_peak_used;
  u32 flags;
} Arena;

typedef struct {
  Arena *arena;
  u64 pos;
} Temp;

typedef struct {
  Arena *arenas[2];
} Thread_Ctx;

// =========================== PUBLIC API ===================================

// Thread context
FAE_API void tctx_init(Thread_Ctx *tctx);
FAE_API void tctx_release();
FAE_API Arena *tctx_get_scratch(Arena **conflicts, u64 count);

// Arena
FAE_API Arena *arena_alloc();
FAE_API void arena_release(Arena *arena);
FAE_API u64 arena_pos(Arena *arena);
FAE_API void *arena_push_aligned(Arena *arena, u64 size, u64 align);
FAE_API void arena_pop_to(Arena *arena, u64 big_pos_unclamped);

FAE_API Temp temp_begin(Arena *arena);
FAE_API void temp_end(Temp temp);

#define zero_struct(x) memset((x), 0, sizeof(*(x)))
#define zero_mem(ptr, size) memset((ptr), 0, (size))

#define arena_push_array_nozero(T, a, cnt) (T *)arena_push_aligned((a), sizeof(T) * (cnt), Fae_Max(8, alignof(T)))
#define arena_push_array(T, a, cnt) (T *)zero_mem(arena_push_array_nozero(T, a, cnt), sizeof(T) * (cnt))
#define arena_push_nozero(T, a) (T *)arena_push_array_nozero(T, a, 1)
#define arena_push(T, a) (T *)zero_mem(arena_push_nozero(T, a), sizeof(T))

#define scratch_begin(conflicts, count) temp_begin(tctx_get_scratch((conflicts), (count)))
#define scratch_end(scratch)            temp_end(scratch)
