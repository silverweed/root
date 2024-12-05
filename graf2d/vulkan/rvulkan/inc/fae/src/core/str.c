String8 str8_(u8 *str, usz size)
{
  String8 s;
  s.str = str;
  s.size = size;
  return s;
}

b8 str8_eq(String8 a, String8 b)
{
  if (a.size != b.size)
    return false;
  return memcmp(a.str, b.str, a.size) == 0;
}

b8 str8_eqc(String8 a, const char *b)
{
  return strncmp(cstr(a), b, a.size) == 0;
}

String8 push_str8fv(Arena *arena, const char *fmt, va_list args)
{
  va_list args2;
  va_copy(args2, args);
  u32 needed_bytes = vsnprintf(0, 0, fmt, args) + 1;
  String8 result = {};
  result.str = (u8 *)arena_push_aligned(arena, needed_bytes, 1);
  result.size = vsnprintf((char*)result.str, needed_bytes, fmt, args2);
  result.str[result.size] = 0;
  va_end(args2);
  return result;
}

String8 push_str8f(Arena *arena, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  String8 result = push_str8fv(arena, fmt, args);
  va_end(args);
  return result;
}

// NOTE: the returned string has the same lifetime as `str`!
String8 str8_tmp_from_c(const char *str)
{
  String8 s;
  s.str = (u8 *)str;
  s.size = strlen(str);
  return s;
}

String8 str8_from_buf(Arena *arena, const u8 *buf, usz size)
{
  String8 s;
  s.str = arena_push_array_nozero(u8, arena, size + 1);
  s.size = size;
  s.str[size] = 0;
  memcpy(s.str, buf, size);
  return s;
}

String8 str8_copy(Arena *arena, String8 s)
{
  String8 res = str8_from_buf(arena, s.str, s.size);
  return res;
}

String8 str8_from_char(Arena *arena, u8 ch, usz size)
{
  String8 s;
  s.str = arena_push_array_nozero(u8, arena, size + 1);
  s.size = size;
  s.str[size] = 0;
  memset(s.str, ch, size);
  return s;
}

String8 str8_concat(Arena *arena, String8 a, String8 b)
{
  String8 res;
  res.size = a.size + b.size;
  res.str = arena_push_array_nozero(u8, arena, res.size + 1);
  memcpy(res.str, a.str, a.size);
  memcpy(res.str + a.size, b.str, b.size);
  res.str[res.size] = 0;
  return res;
}

String8_Node *push_str8_node(Arena *arena, String8_Node *prev, const char *fmt, ...)
{
  String8_Node *snode = arena_push(String8_Node, arena);

  va_list args;
  va_start(args, fmt);
  snode->str = push_str8fv(arena, fmt, args);
  va_end(args);

  if (prev) {
    prev->next = snode;
    snode->head = prev->head;
  } else {
    snode->head = snode;
  }
  ++snode->head->count;

  return snode;
}

// Converts a chain of String8_Node to a single string by concatenating all `next` nodes.
// If `snode` is null, returns an empty string.
String8 str8_node_join(Arena *arena, String8_Node *snode, const char *sep)
{
  if (!snode) {
    return str8("");
  }
  assert(snode->head);

  u64 sep_len = strlen(sep);
  u64 tot_sz = 0;
  for (String8_Node *node = snode->head; node; node = node->next) {
    tot_sz += node->str.size + sep_len;
  }
  if (tot_sz)
    tot_sz -= sep_len;

  String8 res;
  res.str = arena_push_array_nozero(u8, arena, tot_sz + 1);
  res.size = 0;
  res.str[tot_sz] = 0;

  for (String8_Node *node = snode->head; node; node = node->next) {
    memcpy(res.str + res.size, node->str.str, node->str.size);
    res.size += node->str.size;
    if (sep_len && node->next) {
      memcpy(res.str + res.size, sep, sep_len);
      res.size += sep_len;
    }
  }

  return res;
}

// Passing `to_exclusive < 0 ` means "until the end of the string"
String8 str8_copy_substr(Arena *arena, String8 str, u64 from, i64 to_exclusive)
{
  String8 sub = {};
  if ((u64)to_exclusive <= from || from >= str.size)
    return sub;
  
  sub.size = to_exclusive >= 0 ? to_exclusive - from : str.size - from;
  if (sub.size) {
    sub.str = arena_push_array_nozero(u8, arena, sub.size + 1);
    memcpy(sub.str, str.str + from, sub.size);
    sub.str[sub.size] = 0;
  }
  return sub;
}

// Splits `str` into multiple strings based on `splitter`.
// Corner cases:
// - the empty string will result in a chain of 1 empty String8_Node
// - str8_split(":::", ':') will result in 3 (not 4!) empty String8_Node
// This function will never return null.
String8_Node *str8_split(Arena *arena, String8 str, u8 splitter)
{
  // if str is empty, return a valid empty String8_Node 
  if (!str.size) {
    return push_str8_node(arena, NULL, "");
  }
  
  String8_Node *node = NULL;
  u64 cur_idx = 0;
  while (cur_idx < str.size) {
    u64 node_size = 0;
    for (u64 i = cur_idx; i < str.size && str.str[i] != splitter; ++i)
      ++node_size;
    assert(cur_idx + node_size <= str.size);
    if (node_size) {
      String8 s = str8_copy_substr(arena, str, cur_idx, cur_idx + node_size);
      String8_Node *newnode = arena_push(String8_Node, arena);
      newnode->str = s;
      if (node) {
        node->next = newnode;
        newnode->head = node->head;
      } else {
        newnode->head = newnode;
      }
      node = newnode;
      ++node->head->count;
    } else {
      node = push_str8_node(arena, node, "");
    }
    cur_idx += node_size + 1;
  }

  assert(node);
  return node->head;
}

//// functions for hashmap
internal
Hash_Type hash_str8(void *s, u64 size)
{
  (void)size;
  assert(size == sizeof(String8));
  String8 *str = (String8*)s;
  Hash_Type h = hash_fnv1a(str->str, str->size);
  return h;
}

internal
b8 hash_str8_eq(void *a, void *b, u64 size)
{
  (void)size;
  assert(size == sizeof(String8));
  String8 *sa = (String8 *)a;
  String8 *sb = (String8 *)b;
  return str8_eq(*sa, *sb);
}

#ifdef FAE_TESTING
void test_str_join()
{
  Arena *arena = arena_alloc();
  
  String8_Node *n = push_str8_node(arena, NULL, "First");
  n = push_str8_node(arena, n, "Second");
  n = push_str8_node(arena, n, "Third");
  n = push_str8_node(arena, n, "Fourth");
  n = push_str8_node(arena, n, "Last");

  String8 joined = str8_node_join(arena, n->head, ";");
  assert(str8_eq(joined, str8("First;Second;Third;Fourth;Last")));

  arena_release(arena);
}

void test_str_copy_substr()
{
  Arena *arena = arena_alloc();

  String8 s = str8("hello sailor");
  String8 ss = str8_copy_substr(arena, s, 6, -1);
  assert(str8_eq(ss, str8("sailor")));

  ss = str8_copy_substr(arena, s, 0, -1);
  assert(str8_eq(ss, s));

  ss = str8_copy_substr(arena, s, 0, 5);
  assert(str8_eq(ss, str8("hello")));
  
  arena_release(arena);
}

void test_str_split()
{
  Arena *arena = arena_alloc();

  String8 s = str8("foo,bar, baz ,");
  String8_Node *n = str8_split(arena, s, ',');
  assert(str8_eq(n->str, str8("foo")));
  assert(str8_eq(n->next->str, str8("bar")));
  assert(str8_eq(n->next->next->str, str8(" baz ")));
  assert(!n->next->next->next);

  s = str8("");
  n = str8_split(arena, s, ',');
  assert(n->str.size == 0);

  s = str8(",");
  n = str8_split(arena, s, ',');
  assert(n->str.size == 0);
  assert(!n->next);

  s = str8("not even a single comma!");
  n = str8_split(arena, s, ',');
  assert(str8_eq(n->str, s));

  n = str8_split(arena, str8(":::"), ':');
  assert(n->str.size == 0);
  assert(n->next->str.size == 0);
  assert(n->next->next->str.size == 0);
  assert(!n->next->next->next);

  arena_release(arena);
}
#endif
