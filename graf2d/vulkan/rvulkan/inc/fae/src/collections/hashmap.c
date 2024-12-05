// A hashmap that doesn't own its own memory and therefore never frees it.
// When an element is removed, it is placed in a free list and its slot is reused if needed.
// Usage:
// 
// Hash_Map hmap = hashmap_init(KeyType, ValueType, arena, n_buckets, key_hash_fn, key_eq_fn);
//
// or, if the type is plain:
//
// Hash_Map hmap = hashmap_init_default(KeyType, ValueType, arena, n_buckets);
//
// 'plain' here means that its value can be compared with memcmp and hashed blindly with
// reasonable results. An example of non-plain type is a String8 (one expects two strings
// to compare equal if they point to equal char arrays, not if the pointers themselves are
// equal).
// In general one must ensure that a == b  =>  key_hash_fn(a) == key_hash_fn(b) (for a reasonable
// definition of `==`).
#define HASHMAP_MAX_KEY_SIZE 16
#define HASHMAP_MAX_VAL_SIZE 512

typedef struct Hash_Node {
  struct Hash_Node *next;
  // (key|value)
  u8 data[];
} Hash_Node;

typedef struct {
  struct Hash_Node *head, *tail;
  // if the bucket contains invalid data `free` points to `&bucket->next`.
  // Notice that this doesn't imply that the bucket is empty, as its `next` may still point
  // to a valid chain of nodes. This can happen if the data in the inline node gets removed.
  struct Hash_Node *free;
} Hash_Bucket;

typedef Hash_Type (*Hash_Fn)(void *, u64);
typedef b8 (*Key_Eq_Fn)(void *, void *, u64);

internal
Hash_Type hash_default(void *key, u64 size)
{
  return hash_fnv1a(key, size);
}

internal
b8 hash_key_eq_default(void *a, void *b, u64 size)
{
  return memcmp(a, b, size) == 0;  
}

typedef struct {
  Arena *arena;
  Hash_Bucket *buckets;
  u32 n_buckets;
  u64 n_elems;
  usz key_size;
  usz val_size;
  usz key_align;
  usz val_align;
  Hash_Fn hash_fn;
  Key_Eq_Fn key_eq_fn;
} Hash_Map;

internal
u64 hashmap_get_node_size(Hash_Map *map)
{
  u64 sz = sizeof(Hash_Node);
  // calculate flexible array size
  u64 key_off_aligned = align_pow2(offsetof(Hash_Node, data), map->key_align);
  u64 key_pad = key_off_aligned - offsetof(Hash_Node, data);
  u64 val_base = key_off_aligned + map->key_size;
  u64 val_off_aligned = align_pow2(val_base, map->val_align);
  u64 val_pad = val_off_aligned - val_base;
  sz += key_pad + map->key_size + val_pad + map->val_size;
  sz = align_pow2(sz, sizeof(Hash_Node *));
  return sz;
}

internal
void *hashmap_get_node_key(Hash_Map *map, Hash_Node *node)
{
  u64 key_off_aligned = align_pow2(offsetof(Hash_Node, data), map->key_align);
  return (u8 *)node + key_off_aligned;
}

internal
void *hashmap_get_node_val(Hash_Map *map, Hash_Node *node)
{
  u64 key_off_aligned = align_pow2(offsetof(Hash_Node, data), map->key_align);
  u64 val_base = key_off_aligned + map->key_size;
  u64 val_off_aligned = align_pow2(val_base, map->val_align);
  return (u8 *)node + val_off_aligned;
}

internal
Hash_Bucket *hashmap_get_bucket(Hash_Map *map, u64 bucket_idx)
{
  return &map->buckets[bucket_idx];
}

internal
b8 hashmap_bucket_is_empty(Hash_Bucket *bucket)
{
  return bucket->head == NULL;
}

internal
Hash_Node *hashmap_alloc_node(Hash_Map *map)
{
  u64 node_size = hashmap_get_node_size(map);
  Hash_Node *node = (Hash_Node *)arena_push_array(u8, map->arena, node_size);
  return node;
}

internal
Hash_Map hashmap_init_(Arena *arena, u32 n_buckets, Hash_Fn hash_fn, Key_Eq_Fn key_eq_fn,
                       usz key_size, usz key_align, usz val_size, usz val_align)
{
  assert_always(key_size <= HASHMAP_MAX_KEY_SIZE);
  assert_always(val_size <= HASHMAP_MAX_VAL_SIZE);

  Hash_Map map = {};
  map.arena = arena;
  map.n_buckets = n_buckets;
  map.key_size = key_size;
  map.val_size = val_size;
  map.key_align = key_align;
  map.val_align = val_align;
  map.hash_fn = hash_fn;
  map.key_eq_fn = key_eq_fn;
  map.buckets = n_buckets ? arena_push_array(Hash_Bucket, arena, n_buckets) : NULL;
  return map;
}

internal
Hash_Type hashmap_hash(Hash_Map *map, void *key)
{
  Hash_Type h = map->hash_fn(key, map->key_size);
  return h;
}

// finds the node containing (hash, key) and optionally returns its containing bucket.
internal
Hash_Node *hashmap_find_node(Hash_Map *map, Hash_Type hash, void *key, Hash_Bucket **bucket, Hash_Node **prev)
{
  if (UNLIKELY(!map->n_buckets))
    return NULL;
  
  u64 idx = hash % map->n_buckets;
  Hash_Bucket *start = hashmap_get_bucket(map, idx);
  if (bucket)
    *bucket = start;
  if (hashmap_bucket_is_empty(start))
    return NULL;
  
  Hash_Node *prv = NULL;
  for (Hash_Node *node = start->head; node; node = node->next) {
    void *nkey = hashmap_get_node_key(map, node);
    if (map->key_eq_fn(key, nkey, map->key_size)) {
      if (prev)
        *prev = prv;
      return node;
    }
    prv = node;
  }
  return NULL;
}

internal
Hash_Node *hashmap_add_(Hash_Map *map, Hash_Type hash, void *key, void *val)
{
  Hash_Bucket *bucket;
  Hash_Node *existing = hashmap_find_node(map, hash, key, &bucket, NULL);
  if (existing) {
    // replace node value with `val`
    void *dst = hashmap_get_node_val(map, existing);
    memcpy(dst, val, map->val_size);
    return existing;
  }
  
  assert(bucket);
  Hash_Node *node;
  // check if we can recycle a free node
  if (bucket->free) {
    node = bucket->free;
    bucket->free = bucket->free->next;
    node->next = NULL;
  } else {
    // no node is available for reuse, allocate a new one
    node = hashmap_alloc_node(map);
  }

  push_to_sll(bucket->head, bucket->tail, node);

  void *key_dst = hashmap_get_node_key(map, node);
  memcpy(key_dst, key, map->key_size);

  void *val_dst = hashmap_get_node_val(map, node);
  memcpy(val_dst, val, map->val_size);

  ++map->n_elems;

  return node;
}

internal
void *hashmap_find_(Hash_Map *map, Hash_Type hash, void *key)
{
  Hash_Node *node = hashmap_find_node(map, hash, key, NULL, NULL);
  if (node) {
    return hashmap_get_node_val(map, node);
  }
  return NULL;
}

internal
void hashmap_remove_(Hash_Map *map, Hash_Type hash, void *key)
{
  Hash_Bucket *bucket;
  Hash_Node *prev = NULL;
  Hash_Node *node = hashmap_find_node(map, hash, key, &bucket, &prev);
  if (node) {
    // pop node from the alive list and push into the free list
    if (prev)
      prev->next = node->next;
    if (node == bucket->tail)
      bucket->tail = prev;
    if (node == bucket->head)
      bucket->head = node->next;
    node->next = bucket->free;
    bucket->free = node;
    assert(map->n_elems > 0);
    --map->n_elems;
  }
}

internal
u64 hashmap_count(Hash_Map *map)
{
  return map->n_elems;
}

typedef struct {
  Hash_Map *map;
  u32 cur_bucket;
  Hash_Node *cur_node;
} Hash_Map_Iter;

internal
Hash_Map_Iter hashmap_iter(Hash_Map *map)
{
  Hash_Map_Iter iter = {};
  iter.map = map;
  return iter;
}

internal
b8 hashmap_next(Hash_Map_Iter *iter, void *key, void *val)
{
  // have we finished the iteration?
  if (iter->cur_bucket == iter->map->n_buckets)
    return false;

  do {
    Hash_Bucket *cur_bucket = hashmap_get_bucket(iter->map, iter->cur_bucket);
    assert(cur_bucket);
    // are we at the end of the current bucket?
    if (iter->cur_node == cur_bucket->tail) {
      ++iter->cur_bucket;
      if (iter->cur_bucket < iter->map->n_buckets) {
        cur_bucket = hashmap_get_bucket(iter->map, iter->cur_bucket);
        iter->cur_node = cur_bucket->head;
      } else {
        // reached the end of the map
        return false;
      }
    } else if (!iter->cur_node) {
      // are we at the very beginning of the iteration?
      iter->cur_node = cur_bucket->head;
    } else {
      iter->cur_node = iter->cur_node->next;
      // since previous cur_node was not the tail, next should be valid.
      assert(iter->cur_node);
    }
  } while (!iter->cur_node);

  void *k = hashmap_get_node_key(iter->map, iter->cur_node);
  void *v = hashmap_get_node_val(iter->map, iter->cur_node);
  memcpy(key, k, iter->map->key_size);
  memcpy(val, v, iter->map->val_size);
  return true;
}

#define hashmap_print(K, V, map, fmt, loglv) \
do { \
  fae_log((loglv), "{\n"); \
  u64 n = 0; \
  for (u32 i = 0; i < (map)->n_buckets; ++i) { \
    Hash_Bucket *bucket = hashmap_get_bucket((map), i); \
\
    fae_log((loglv), "  bucket %u:\n", i); \
    for (Hash_Node *node = bucket->head; node; node = node->next) { \
      K *key = (K *)hashmap_get_node_key((map), node); \
      V *val = (V *)hashmap_get_node_val((map), node); \
      assert(key); \
      assert(val); \
      fae_log((loglv), "    " fmt "\n", *key, *val); \
      ++n; \
    } \
    for (Hash_Node *free = bucket->free; free; free = free->next) { \
      K *key = (K *)hashmap_get_node_key((map), free); \
      V *val = (V *)hashmap_get_node_val((map), free); \
      assert(key); \
      assert(val); \
      fae_log((loglv), "    " fmt " (FREED)\n", *key, *val); \
    } \
  } \
  fae_log((loglv), "}\n"); \
  assert(n == (map)->n_elems); \
} while (0)

#define hashmap_init(K, V, a, nb, hsh, eq) hashmap_init_((a), (nb), (hsh), (eq), sizeof(K), alignof(K), sizeof(V), alignof(V))
// NOTE: don't use this for types that contain pointers, such as String8.
// The hash fn should always return the same value for equal keys.
#define hashmap_init_default(K, V, a, nb) hashmap_init_((a), (nb), hash_default, hash_key_eq_default, sizeof(K), alignof(K), sizeof(V), alignof(V))
#define hashmap_add(hmap, k, v) hashmap_add_((hmap), hashmap_hash((hmap), (k)), (k), (v))
#define hashmap_find(V, hmap, k) (V *)hashmap_find_((hmap), hashmap_hash((hmap), (k)), (k))
#define hashmap_remove(hmap, k) hashmap_remove_((hmap), hashmap_hash((hmap), (k)), (k))

internal
void hashmap_clone(Hash_Map *dst, Hash_Map *src)
{
  Hash_Map_Iter iter = hashmap_iter(src);
  u8 key[HASHMAP_MAX_KEY_SIZE];
  u8 val[HASHMAP_MAX_VAL_SIZE];
  while (hashmap_next(&iter, &key, &val)) {
    hashmap_add(dst, &key, &val);
  }
}

#ifdef FAE_TESTING
void test_hashmap_simple()
{
  Arena *a = arena_alloc();

  Hash_Map map = hashmap_init(String8, u64, a, 100, hash_str8, hash_str8_eq);
  String8 key = str8("my key");
  u64 val = 99;
  hashmap_add(&map, &key, &val);

  u64 *found = hashmap_find(u64, &map, &key);
  assert(found);
  assert_eq(*found, val, "%lu");

  arena_release(a);
}

void test_hashmap_remove()
{
  Arena *a = arena_alloc();

  Hash_Map map = hashmap_init(String8, u64, a, 100, hash_str8, hash_str8_eq);
  String8 key = str8("my key");
  u64 val = 99;
  hashmap_add(&map, &key, &val);

  u64 *found = hashmap_find(u64, &map, &key);
  assert(found);
  assert_eq(*found, val, "%lu");

  hashmap_remove(&map, &key);
  
  found = hashmap_find(u64, &map, &key);
  assert(!found);

  arena_release(a);
}

void test_hashmap_node_reuse()
{
  Arena *a = arena_alloc();

  Hash_Map map = hashmap_init(String8, u64, a, 100, hash_str8, hash_str8_eq);
  String8 key = str8("my key");
  u64 val = 99;
  Hash_Node *node = hashmap_add(&map, &key, &val);
  hashmap_remove(&map, &key);
  Hash_Node *node2 = hashmap_add(&map, &key, &val); // should have reused the same node

  assert_eq(node, node2, "%p");

  // we should have 0 free nodes
  Hash_Bucket *bucket;
  hashmap_find_node(&map, hash_str8(&key, sizeof(String8)), &key, &bucket, NULL);
  assert_eq(bucket->free, NULL, "%p");

  arena_release(a);
}

void test_hashmap_node_replace()
{
  Arena *a = arena_alloc();

  Hash_Map map = hashmap_init(String8, u64, a, 100, hash_str8, hash_str8_eq);
  String8 key = str8("my key");
  u64 val = 99;
  hashmap_add(&map, &key, &val);
  u64 val2 = 42;
  hashmap_add(&map, &key, &val2);

  u64 *v = hashmap_find(u64, &map, &key);
  assert_eq(*v, val2, "%lu");

  arena_release(a);
}

void test_hashmap_many()
{
  Thread_Ctx tctx;
  tctx_init(&tctx);
  Arena *a = arena_alloc();
  Temp scratch = scratch_begin(&a, 1);

  Hash_Map map = hashmap_init(String8, i32, a, 20, hash_str8, hash_str8_eq);

  for (u64 i = 0; i < 10000; ++i) {
    String8 key = push_str8f(scratch.arena, "key_%lu", i);
    i32 val = (i32)i;
    hashmap_add(&map, &key, &val);
  }

  String8 k = str8("key_420");
  i32 *found = hashmap_find(i32, &map, &k);
  assert(found);
  assert_eq(*found, 420, "%d");

  k = str8("key_0");
  found = hashmap_find(i32, &map, &k);
  assert(found);
  assert_eq(*found, 0, "%d");

  hashmap_remove(&map, &k);
  found = hashmap_find(i32, &map, &k);
  assert(!found);

  scratch_end(scratch);
  arena_release(a);
  tctx_release();
}

void test_hashmap_iter()
{
  Thread_Ctx tctx;
  tctx_init(&tctx);
  Arena *a = arena_alloc();
  Temp scratch = scratch_begin(&a, 1);

  Hash_Map map = hashmap_init_default(i32, String8, a, 20);

  for (u64 i = 0; i < 100; ++i) {
    i32 key = (i32)i;
    String8 val = push_str8f(scratch.arena, "key_%lu", i);
    hashmap_add(&map, &key, &val);
  }

  i32 x = 44;
  hashmap_remove(&map, &x);
  x = 99;
  hashmap_remove(&map, &x);
  x = 21;
  hashmap_remove(&map, &x);

  u64 iterations = 0;
  Hash_Map_Iter iter = hashmap_iter(&map);
  i32 key;
  String8 value;
  while (hashmap_next(&iter, &key, &value)) {
    ++iterations;
    String8 sub = str8_copy_substr(a, value, 4, -1);
    i32 k = atoi(cstr(sub));
    assert_eq(k, key, "%d");
  }

  assert_eq(iterations, 97, "%" PRIu64);
  
  scratch_end(scratch);
  arena_release(a);
  tctx_release();
}

void test_hashmap_clone()
{
  Thread_Ctx tctx;
  tctx_init(&tctx);
  Arena *a = arena_alloc();
  Temp scratch = scratch_begin(&a, 1);

  Hash_Map map = hashmap_init_default(i32, f32, a, 20);

  for (u64 i = 0; i < 100; ++i) {
    i32 key = (i32)i;
    f32 val = (f32)(i * 2);
    hashmap_add(&map, &key, &val);
  }

  i32 x = 44;
  hashmap_remove(&map, &x);
  x = 90;
  hashmap_remove(&map, &x);
  x = 11;
  hashmap_remove(&map, &x);

  Hash_Map cloned = hashmap_init_default(i32, f32, a, 40);
  hashmap_clone(&cloned, &map);

  i32 k = 20;
  f32 *found = hashmap_find(f32, &map, &k);
  assert(found);
  assert_eq(*found, 40.f, "%f");
  found = hashmap_find(f32, &cloned, &k);
  assert(found);
  assert_eq(*found, 40.f, "%f");

  k = 0;
  found = hashmap_find(f32, &map, &k);
  assert(found);
  assert_eq(*found, 0.f, "%f");
  found = hashmap_find(f32, &cloned, &k);
  assert(found);
  assert_eq(*found, 0.f, "%f");

  k = 90;
  found = hashmap_find(f32, &map, &k);
  assert(!found);
  found = hashmap_find(f32, &cloned, &k);
  assert(!found);
  
  scratch_end(scratch);
  arena_release(a);
  tctx_release();
}
#endif
