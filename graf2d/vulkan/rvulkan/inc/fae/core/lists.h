#define push_to_sll(lhead, ltail, new) \
  if ((ltail)) { \
    assert((lhead)); \
    (ltail)->next = (new); \
  } else { \
    assert(!(ltail)); \
    (lhead) = (new); \
  } \
  (ltail) = (new)

#define push_to_dll_ex(lhead, ltail, new, next, prev) \
  if ((ltail)) { \
    assert((lhead)); \
    (ltail)->next = (new); \
    (new)->prev = (ltail); \
  } else { \
    assert(!(ltail)); \
    (lhead) = (new); \
  } \
  (ltail) = (new)

#define push_to_dll(lhead, ltail, new) \
  push_to_dll_ex(lhead, ltail, new, next, prev)

#define pop_from_dll_ex(lhead, ltail, popped, next, prev) do { \
  if ((popped)->prev) { \
    (popped)->prev->next = (popped)->next; \
  } else { \
    (lhead) = (popped)->next; \
  } \
  if ((popped)->next) { \
    (popped)->next->prev = (popped)->prev; \
  } else { \
    (ltail) = (popped)->prev; \
  } \
} while (0)

#define pop_from_dll(lhead, ltail, popped) \
  pop_from_dll_ex(lhead, ltail, popped, next, prev)

#define pop_from_dll_add_to_free(lhead, ltail, popped, free_head) \
  pop_from_dll(lhead, ltail, popped); \
  (popped)->next = (free_head); \
  (free_head) = (popped)

#define insert_into_dll_ex(T, head, tail, node, node_prev, next, prev) \
  T *nprev = (node_prev); \
  assert((node) != nprev); \
  if (nprev) { \
    (node)->next = nprev->next; \
    if (nprev->next) { \
      nprev->next->prev = (node); \
    } else { \
      (tail) = (node); \
    } \
    nprev->next = (node); \
  } else { \
    (node)->next = (head); \
    (head)->prev = (node); \
    (head) = (node); \
  } \
  (node)->prev = nprev
  
#define insert_into_dll(T, lhead, ltail, node, node_prev) \
  insert_into_dll_ex(T, lhead, ltail, node, node_prev, next, prev)

#define print_sll_ex(T, list_name, loglv, hd, conflicts, n_conflicts, next) do { \
  Temp scr = scratch_begin((conflicts), (n_conflicts)); \
  String8_Node *sn = NULL; \
  for (T *n = (hd); n; n = n->next) \
    sn = push_str8_node(scr.arena, sn, "%p", n); \
  String8 s = sn ? str8_node_join(scr.arena, sn->head, " -> ") : str8("(empty)"); \
  fae_log((loglv), "Generic", "%s: %s", (list_name), cstr(s)); \
  scratch_end(scr); \
} while (0)

#define print_sll(T, list_name, loglv, hd, conflicts, n_conflicts) \
  print_sll_ex(T, list_name, loglv, hd, conflicts, n_conflicts, next)

#define relink_dll_node_ex(T, head, tail, node, new_prev, next, prev) \
  pop_from_dll_ex(head, tail, node, next, prev); \
  insert_into_dll_ex(T, head, tail, node, new_prev, next, prev)

#define relink_dll_node(T, head, tail, node, new_prev) \
  relink_dll_node_ex(T, head, tail, node, new_prev, next, prev)


#ifdef FAE_TESTING
void test_insert_dll_1()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  struct A n6;

  insert_into_dll(struct A, head, tail, &n6, &n3);
  assert(head == &n1);
  assert(n1.next == &n2);
  assert(n2.next == &n3);
  assert(n3.next == &n6);
  assert(n6.next == &n4);
  assert(n4.next == &n5);
  assert(n5.next == NULL);
  assert(tail == &n5);
  assert(n5.prev == &n4);
  assert(n4.prev == &n6);
  assert(n6.prev == &n3);
  assert(n3.prev == &n2);
  assert(n2.prev == &n1);
  assert(n1.prev == NULL);
}

void test_insert_dll_2()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  struct A n6, *null = NULL;

  insert_into_dll(struct A, head, tail, &n6, null);
  assert(head == &n6);
  assert(n6.next == &n1);
  assert(n1.next == &n2);
  assert(n2.next == &n3);
  assert(n3.next == &n4);
  assert(n4.next == &n5);
  assert(n5.next == NULL);
  assert(tail == &n5);
  assert(n5.prev == &n4);
  assert(n4.prev == &n3);
  assert(n3.prev == &n2);
  assert(n2.prev == &n1);
  assert(n1.prev == &n6);
  assert(n6.prev == NULL);
}

void test_relink_dll_1()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  relink_dll_node(struct A, head, tail, &n2, &n4);
  assert(head == &n1);
  assert(n1.next == &n3);
  assert(n3.next == &n4);
  assert(n4.next == &n2);
  assert(n2.next == &n5);
  assert(n5.next == NULL);
  assert(tail == &n5);
  assert(n5.prev == &n2);
  assert(n2.prev == &n4);
  assert(n4.prev == &n3);
  assert(n3.prev == &n1);
  assert(n1.prev == NULL);
}

void test_relink_dll_2()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  relink_dll_node(struct A, head, tail, &n1, &n2);
  assert(head == &n2);
  assert(n2.next == &n1);
  assert(n1.next == &n3);
  assert(n3.next == &n4);
  assert(n4.next == &n5);
  assert(n5.next == NULL);
  assert(tail == &n5);
  assert(n5.prev == &n4);
  assert(n4.prev == &n3);
  assert(n3.prev == &n1);
  assert(n1.prev == &n2);
  assert(n2.prev == NULL);
}

void test_relink_dll_3()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  relink_dll_node(struct A, head, tail, &n1, &n5);
  assert(head == &n2);
  assert(n2.next == &n3);
  assert(n3.next == &n4);
  assert(n4.next == &n5);
  assert(n5.next == &n1);
  assert(n1.next == NULL);
  assert(tail == &n1);
  assert(n1.prev == &n5);
  assert(n5.prev == &n4);
  assert(n4.prev == &n3);
  assert(n3.prev == &n2);
  assert(n2.prev == NULL);
}

void test_relink_dll_4()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  struct A *null = NULL;

  relink_dll_node(struct A, head, tail, &n3, null);
  assert(head == &n3);
  assert(n3.next == &n1);
  assert(n1.next == &n2);
  assert(n2.next == &n4);
  assert(n4.next == &n5);
  assert(n5.next == NULL);
  assert(tail == &n5);
  assert(n5.prev == &n4);
  assert(n4.prev == &n2);
  assert(n2.prev == &n1);
  assert(n1.prev == &n3);
  assert(n3.prev == NULL);
}

void test_relink_dll_5()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  relink_dll_node(struct A, head, tail, &n3, &n2);
  assert(head == &n1);
  assert(n1.next == &n2);
  assert(n2.next == &n3);
  assert(n3.next == &n4);
  assert(n4.next == &n5);
  assert(n5.next == NULL);
  assert(tail == &n5);
  assert(n5.prev == &n4);
  assert(n4.prev == &n3);
  assert(n3.prev == &n2);
  assert(n2.prev == &n1);
  assert(n1.prev == NULL);
}

void test_relink_dll_6()
{
  struct A {
    struct A *next, *prev;
    int x;
  } n1, n2, n3, n4, n5;
  struct A *head = &n1, *tail = &n5;
  n1.next = &n2; n2.prev = &n1;
  n2.next = &n3; n3.prev = &n2;
  n3.next = &n4; n4.prev = &n3;
  n4.next = &n5; n5.prev = &n4;

  struct A *null = NULL;

  relink_dll_node(struct A, head, tail, &n5, null);
  assert(head == &n5);
  assert(n5.next == &n1);
  assert(n1.next == &n2);
  assert(n2.next == &n3);
  assert(n3.next == &n4);
  assert(n4.next == NULL);
  assert(tail == &n4);
  assert(n4.prev == &n3);
  assert(n3.prev == &n2);
  assert(n2.prev == &n1);
  assert(n1.prev == &n5);
  assert(n5.prev == NULL);
}
#endif
