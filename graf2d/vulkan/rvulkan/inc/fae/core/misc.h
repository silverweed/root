#define internal static

#ifndef __cplusplus
#define thread_local _Thread_local
#define true 1
#define false 0
#define alignof(T) __alignof(T)
#endif

#define KiB(b) (b * 1024)
#define MiB(b) (KiB(b) * 1024)
#define GiB(b) (MiB(b) * 1024)

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x) 
#endif

#define align_pow2(x, b) (((x) + (b) - 1) & ( ~((b) - 1)))

internal
String8 to_pretty_size(Arena *arena, u64 bytes)
{
  if (bytes >= GiB(1)) return push_str8f(arena, "%.1f GiB", (f32)bytes / GiB(1));
  if (bytes >= MiB(1)) return push_str8f(arena, "%.1f MiB", (f32)bytes / MiB(1));
  if (bytes >= KiB(1)) return push_str8f(arena, "%.1f KiB", (f32)bytes / KiB(1));
  return push_str8f(arena, "%" PRIu64 " B", bytes);
}

#define assert_always(cond) do { \
  if (UNLIKELY(!(cond))) { \
    FATAL_TAG("Assert", "%s:%d: Condition failed: " #cond, __FILE__, __LINE__); \
    os_trap(); \
  } \
} while (0)

#ifndef NDEBUG
#define assert_eq(a, b, fmt) do { \
  if (UNLIKELY((a) != (b))) { \
    FATAL_TAG("Assert", "%s:%d: expected equality of values: " fmt " != " fmt, __FILE__, __LINE__, (a), (b)); \
    os_abort(); \
  } \
} while (0)
#else
#define assert_eq(a, b, fmt) \
  (void)(a); \
  (void)(b)
#endif

#define GEN_ENUM(Enum) Enum
#define GEN_ENUM_STR(Enum) #Enum

typedef u32 Hash_Type;

internal
Hash_Type hash_fnv1a(const u8 *buf, u64 len) {
  u32 fnv_prime32 = 16777619;
  Hash_Type result = 2166136261;

  for (usz i = 0; i < len; ++i) {
    result ^= (u32)buf[i];
    result *= fnv_prime32;
  }

  return result;
}

