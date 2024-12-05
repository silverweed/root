#pragma once

typedef struct {
  u8* str;  // 0-terminated
  usz size; // does not include trailing zero
} String8;

#define cstr(s) (const char *)((s).str)
#define str8(s) (String8){ (u8 *)(s), sizeof(s) - 1 }

typedef struct String8_Node {
  struct String8_Node *head, *next;
  String8 str;
  u32 count; // number of nodes in the chain. Only valid for `head`.
} String8_Node;

FAE_API String8 str8_(u8 *str, usz size);

FAE_API b8 str8_eq(String8 a, String8 b);
FAE_API b8 str8_eqc(String8 a, const char *b);

FAE_API String8 push_str8fv(Arena *arena, const char *fmt, va_list args);
FAE_API String8 push_str8f(Arena *arena, const char *fmt, ...);

// NOTE: the returned string has the same lifetime as `str`!
FAE_API String8 str8_tmp_from_c(const char *str);
FAE_API String8 str8_from_buf(Arena *arena, const u8 *buf, usz size);
FAE_API String8 str8_from_char(Arena *arena, u8 ch, usz size);

FAE_API String8 str8_copy(Arena *arena, String8 s);
FAE_API String8 str8_concat(Arena *arena, String8 a, String8 b);

FAE_API String8_Node *push_str8_node(Arena *arena, String8_Node *prev, const char *fmt, ...);

// Converts a chain of String8_Node to a single string by concatenating all `next` nodes.
// If `snode` is null, returns an empty string.
FAE_API String8 str8_node_join(Arena *arena, String8_Node *snode, const char *sep);

// Passing `to_exclusive < 0 ` means "until the end of the string"
FAE_API String8 str8_copy_substr(Arena *arena, String8 str, u64 from, i64 to_exclusive);

// Splits `str` into multiple strings based on `splitter`.
// Corner cases:
// - the empty string will result in a chain of 1 empty String8_Node
// - str8_split(":::", ':') will result in 3 (not 4!) empty String8_Node
// This function will never return null.
FAE_API String8_Node *str8_split(Arena *arena, String8 str, u8 splitter);
