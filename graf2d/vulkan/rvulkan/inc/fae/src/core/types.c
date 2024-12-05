
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

