typedef struct {
  Graph_Node *head, *tail;
  u64 count;
  // Nodes that are removed from the graph end up here. They are reused, but their
  // id will change every time they are.
  Graph_Node *free;
  // maps { node_id => Graph_Node* }
  Hash_Map id_map;
  Graph_Node_Id next_id;
} Graph_Nodes;

typedef struct {
  Graph_Link_Node *head, *tail;
  u64 count;
  Graph_Link_Node *free;
} Graph_Links;

typedef struct {
  Arena *arena;
  String8 name;
  
  // indexed by node proto id
  Graph_Node_Proto *node_protos;
  u64 n_node_protos;

  Graph_Nodes nodes;
  Graph_Links links;

  Graph_Pin *entrypoints;
  u32 n_entrypoints;

  Graph_Pin *exitpoints;
  u32 n_exitpoints;
} Script_Graph;

internal
Graph_Node_Id graph_next_id(Graph_Nodes *nodes)
{
  ++nodes->next_id.id;
  Graph_Node_Id id = nodes->next_id;
  return id;
}

internal
Graph_Node *graph_create_node(Script_Graph *graph)
{
  Graph_Nodes *nodes = &graph->nodes;
  Graph_Node *node;
  if (nodes->free) {
    node = nodes->free;
    nodes->free = node->next;
    zero_struct(node);
  } else {
    node = arena_push(Graph_Node, graph->arena);
  }
  node->id = graph_next_id(&graph->nodes);

  push_to_dll(nodes->head, nodes->tail, node);
  hashmap_add(&nodes->id_map, &node->id, &node);
  ++nodes->count;

  return node;
}

internal
void graph_remove_node(Script_Graph *graph, Graph_Node *node)
{
  Graph_Nodes *nodes = &graph->nodes;
  pop_from_dll_add_to_free(nodes->head, nodes->tail, node, nodes->free);
  hashmap_remove(&nodes->id_map, &node->id);
  --nodes->count;
}

internal
Graph_Node *graph_get_node(Script_Graph *graph, Graph_Node_Id id)
{
  Graph_Node **n = hashmap_find(Graph_Node*, &graph->nodes.id_map, &id);
  return n ? *n : NULL;
}

internal
Graph_Node_Proto *graph_get_node_proto(Script_Graph *graph, Graph_Node_Proto_Id id)
{
  Graph_Node_Proto *proto = &graph->node_protos[id];
  return proto;
}

internal
Graph_Link_Node *graph_create_link(Script_Graph *graph)
{
  Graph_Links *links = &graph->links;
  Graph_Link_Node *link;
  if (links->free) {
    link = links->free;
    links->free = link->next;
    zero_struct(link);
  } else {
    link = arena_push(Graph_Link_Node, graph->arena);
  }

  push_to_dll(links->head, links->tail, link);
  ++links->count;

  return link;
}

internal
void graph_remove_link(Script_Graph *graph, Graph_Link_Node *link)
{
  Graph_Links *links = &graph->links;
  pop_from_dll_add_to_free(links->head, links->tail, link, links->free);
  --links->count;
}

internal
void graph_clone_pin(Arena *arena, Graph_Pin in, Graph_Pin *out)
{
  out->name = str8_copy(arena, in.name);
  out->type = in.type;
}

internal
Graph_Pin finalize_graph_pin(Arena *arena, Graph_Pin_Node *node)
{
  Graph_Pin pin;
  pin.name = str8_copy(arena, node->pin.name);
  pin.type = node->pin.type;
  return pin;
}

internal
void graph_clone_node_proto(Arena *arena, Graph_Node_Proto *proto, Graph_Node_Proto *out)
{
  out->name = str8_copy(arena, proto->name);
  out->n_inputs = proto->n_inputs;
  out->n_outputs = proto->n_outputs;
  if (proto->n_inputs)
    out->inputs = arena_push_array_nozero(Graph_Pin, arena, proto->n_inputs);
  if (proto->n_outputs)
    out->outputs = arena_push_array_nozero(Graph_Pin, arena, proto->n_outputs);
  for (u32 i = 0; i < proto->n_inputs; ++i)
    graph_clone_pin(arena, proto->inputs[i], &out->inputs[i]);
  for (u32 i = 0; i < proto->n_outputs; ++i)
    graph_clone_pin(arena, proto->outputs[i], &out->outputs[i]);
}

internal
void graph_init(Script_Graph *graph, Arena *arena)
{
  graph->arena = arena;
  graph->nodes.id_map = hashmap_init_default(Graph_Node_Id, Graph_Node*, arena, 1024);
}

internal
Graph_Link_Node *graph_clone_link(Script_Graph *cloned_graph, Hash_Map *node_id_remap, Graph_Link_Node *link)
{
  Graph_Link_Node *cloned_link = graph_create_link(cloned_graph);
  cloned_link->link.from.pin = link->link.from.pin;
  cloned_link->link.to.pin = link->link.to.pin;
  // remap node id
  Graph_Node_Id old_from = link->link.from.node;
  Graph_Node_Id old_to = link->link.to.node;
  if (graph_node_id_eq(old_from, GRAPH_NODE_ID_ENTRY) || graph_node_id_eq(old_from, GRAPH_NODE_ID_EXIT)) {
    cloned_link->link.from.node = old_from;
  } else {
    Graph_Node_Id *new_from_id = hashmap_find(Graph_Node_Id, node_id_remap, &old_from);
    assert(new_from_id);
    cloned_link->link.from.node = *new_from_id;
  }
  if (graph_node_id_eq(old_to, GRAPH_NODE_ID_ENTRY) || graph_node_id_eq(old_to, GRAPH_NODE_ID_EXIT)) {
    cloned_link->link.to.node = old_to;
  } else {
    Graph_Node_Id *new_to_id = hashmap_find(Graph_Node_Id, node_id_remap, &old_to);
    assert(new_to_id);
    cloned_link->link.to.node = *new_to_id;
  }
  return cloned_link;
}

internal
void graph_describe(Script_Graph *graph, Log_Level lv)
{
  if (g_loglv < lv)
    return;
  
  Temp scratch = scratch_begin(0, 0);
  
  fae_log(lv, "Graph", "Entrypoints:");
  for (u32 i = 0; i < graph->n_entrypoints; ++i) {
    Graph_Pin pin = graph->entrypoints[i];
    fae_log(lv, "Graph", "   %s : %s", cstr(pin.name), g_Graph_Pin_Type_str[pin.type]);
  }

  fae_log(lv, "Graph", "Exitpoints:");
  for (u32 i = 0; i < graph->n_exitpoints; ++i) {
    Graph_Pin pin = graph->exitpoints[i];
    fae_log(lv, "Graph", "   %s : %s", cstr(pin.name), g_Graph_Pin_Type_str[pin.type]);
  }

  fae_log(lv, "Graph", "Graph with arena %p (pos: 0x%" PRIX64 ")", graph->arena, arena_pos(graph->arena));
  fae_log(lv, "Graph", "Node protos:");
  for (u64 i = 0; i < graph->n_node_protos; ++i) {
    fae_log(lv, "Graph", "   %" PRIu64 ": %s", i, cstr(graph->node_protos[i].name));
  }

  fae_log(lv, "Graph", "Nodes:");
  for (Graph_Node *node = graph->nodes.head; node; node = node->next) {
    fae_log(lv, "Graph", "   %" PRIu64 ": %s : %" PRIu64 " @%s", node->id, cstr(node->name), node->proto,
             cstr(graph_node_coord_print(scratch.arena, &node->coord)));
  }

  fae_log(lv, "Graph", "Links:");
  for (Graph_Link_Node *node = graph->links.head; node; node = node->next) {
    Graph_Link l = node->link;
    fae_log(lv, "Graph", "   %d;%d -> %d;%d", l.from.node, l.from.pin, l.to.node, l.to.pin);
  }

  scratch_end(scratch);
}

internal
Graph_Node_Proto graph_invalid_node_proto()
{
  Graph_Node_Proto proto = {};
  proto.name = str8("__INVALID__");
  return proto;
}

internal
void finalize_script_graph(Script_Graph_Builder *builder, Script_Graph *out)
{
  Arena *arena = out->arena;
  
  // copy node protos from the linked list to an array
  out->n_node_protos = builder->n_node_protos + 1;
  out->node_protos = arena_push_array_nozero(Graph_Node_Proto, arena, out->n_node_protos);
  // push invalid proto at position 0
  out->node_protos[0] = graph_invalid_node_proto();
  for (Graph_Node_Proto_Node *node = builder->node_protos_head; node; node = node->next) {
    assert(node->id < out->n_node_protos);
    graph_clone_node_proto(arena, &node->proto, &out->node_protos[node->id]);
  }

  Temp scratch = scratch_begin(&arena, 1);
  Hash_Map node_id_remap = hashmap_init_default(Graph_Node_Id, Graph_Node_Id, scratch.arena, builder->n_nodes);
  {
    // XXX: doing potentially unnecessary work here: we're copying a chain of nodes
    // from one arena to the other. Maybe we can just allocate the nodes directly
    // in the final arena during graph building?
    for (Graph_Node *node = builder->nodes_head; node; node = node->next) {
      Graph_Node *new_node = graph_create_node(out);
      new_node->name = str8_copy(arena, node->name);
      new_node->proto = node->proto;
      memcpy(&new_node->coord, &node->coord, sizeof(new_node->coord));
      hashmap_add(&node_id_remap, &node->id, &new_node->id);
    }
  }

  {
    // XXX: see comment above
    for (Graph_Link_Node *link = builder->links_head; link; link = link->next) {
      graph_clone_link(out, &node_id_remap, link);
    }
  }

  out->n_entrypoints = builder->n_entrypoints;
  if (out->n_entrypoints) {
    out->entrypoints = arena_push_array_nozero(Graph_Pin, arena, out->n_entrypoints);
    u32 i = 0;
    for (Graph_Pin_Node *pin = builder->entrypoints_head; pin; pin = pin->next) {
      graph_clone_pin(arena, pin->pin, &out->entrypoints[i]);
      ++i;
    }
  }

  out->n_exitpoints = builder->n_exitpoints;
  if (out->n_exitpoints) {
    out->exitpoints = arena_push_array_nozero(Graph_Pin, arena, out->n_exitpoints);
    u32 i = 0;
    for (Graph_Pin_Node *pin = builder->exitpoints_head; pin; pin = pin->next) {
      graph_clone_pin(arena, pin->pin, &out->exitpoints[i]);
      ++i;
    }
  }

  scratch_end(scratch);
}

internal
Script_Graph *graph_clone(Arena *arena, Script_Graph *graph)
{
  assert(graph);
  
  VERBOSE_TAG("Graph", "Orig:");
  graph_describe(graph, Log_Verbose);

  Script_Graph *cloned = arena_push(Script_Graph, arena);
  graph_init(cloned, arena);
  cloned->name = str8_copy(arena, graph->name);
  
  if (graph->n_node_protos)
    cloned->node_protos = arena_push_array_nozero(Graph_Node_Proto, arena, graph->n_node_protos);
  cloned->n_node_protos = graph->n_node_protos;
  for (u32 i = 0; i < graph->n_node_protos; ++i)
    graph_clone_node_proto(arena, &graph->node_protos[i], &cloned->node_protos[i]);

  Temp scratch = scratch_begin(&arena, 1);
  Hash_Map node_id_remap = hashmap_init_default(Graph_Node_Id, Graph_Node_Id, scratch.arena, graph->nodes.count);

  for (Graph_Node *node = graph->nodes.head; node; node = node->next) {
    Graph_Node *cloned_node = graph_create_node(cloned);
    cloned_node->name = str8_copy(arena, node->name);
    cloned_node->proto = node->proto;
    memcpy(&cloned_node->coord, &node->coord, sizeof(cloned_node->coord));
    hashmap_add(&node_id_remap, &node->id, &cloned_node->id);
  }

  for (Graph_Link_Node *link = graph->links.head; link; link = link->next) {
    graph_clone_link(cloned, &node_id_remap, link);
  }

  cloned->n_entrypoints = graph->n_entrypoints;
  if (cloned->n_entrypoints) {
    cloned->entrypoints = arena_push_array_nozero(Graph_Pin, arena, cloned->n_entrypoints);
    for (u32 i = 0; i < cloned->n_entrypoints; ++i)
      graph_clone_pin(arena, graph->entrypoints[i], &cloned->entrypoints[i]);
  }

  cloned->n_exitpoints = graph->n_exitpoints;
  if (cloned->n_exitpoints) {
    cloned->exitpoints = arena_push_array_nozero(Graph_Pin, arena, cloned->n_exitpoints);
    for (u32 i = 0; i < cloned->n_exitpoints; ++i)
      graph_clone_pin(arena, graph->exitpoints[i], &cloned->exitpoints[i]);
  }

  VERBOSE_TAG("Graph", "Cloned:");
  graph_describe(cloned, Log_Verbose);

  scratch_end(scratch);
  return cloned;
}

internal
Script_Graph *parse_script_graph(Arena *arena, String8 graph_raw)
{
  Temp scratch = scratch_begin(&arena, 1);

  Script_Graph_Builder graph_bld = {};
  b8 parsed = parse_script_graph_internal(scratch.arena, graph_raw, &graph_bld);

  Script_Graph *graph = arena_push(Script_Graph, arena);
  graph_init(graph, arena);
  if (!parsed) {
    ERR_TAG("Graph", "Graph failed to parse");
    // If the graph fails to parse, simply initialize it with an empty builder.
    // Since the graph builder was using the scratch arena, we can simply forget about all its pointers.
    zero_struct(&graph_bld);
  }
  finalize_script_graph(&graph_bld, graph);

  scratch_end(scratch);
  return graph;
}

internal
Script_Graph *parse_script_graph_from_file(Arena *arena, String8 graph_fname)
{
  Temp scratch = scratch_begin(&arena, 1);

  String8 graph_raw = file_read_to_string(scratch.arena, graph_fname);
  Script_Graph *graph = parse_script_graph(arena, graph_raw);
  if (graph)
    graph->name = str8_copy(arena, graph_fname);

  scratch_end(scratch);
  return graph;
}
