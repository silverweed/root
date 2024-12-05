#define T(test_fn) \
  fprintf(stderr, "Running test " #test_fn " ...\n"); \
  test_fn()

int main()
{
  T(test_file_basename);
  T(test_file_dirname);

  T(test_str_join);
  T(test_str_split);
  T(test_str_copy_substr);

  T(test_vm);
  T(test_vm_loop);

  T(test_hashmap_simple);
  T(test_hashmap_remove);
  T(test_hashmap_node_reuse);
  T(test_hashmap_node_replace);
  T(test_hashmap_many);
  T(test_hashmap_iter);
  T(test_hashmap_clone);

  T(test_insert_dll_1);
  T(test_insert_dll_2);
  T(test_relink_dll_1);
  T(test_relink_dll_2);
  T(test_relink_dll_3);
  T(test_relink_dll_4);
  T(test_relink_dll_5);
  T(test_relink_dll_6);
}
