typedef struct {
  i64 id;
} Graph_Node_Id;

#define GRAPH_NODE_ID_INVALID (Graph_Node_Id) { 0 }
#define GRAPH_NODE_ID_ENTRY   (Graph_Node_Id) { -1 }
#define GRAPH_NODE_ID_EXIT    (Graph_Node_Id) { -2 }

internal
b8 graph_node_id_eq(Graph_Node_Id a, Graph_Node_Id b)
{
  return a.id == b.id;
}

internal
b8 graph_node_id_is_node(Graph_Node_Id id)
{
  return id.id > 0;
}

typedef u64 Graph_Node_Proto_Id;

// TODO: the pin types should probably become data-driven in the future.
typedef enum {
  PinType_INVALID,
  PinType_Exec, // must be the first valid entry
  FAE_LANG_TYPES(PinType_),
  PinType_COUNT
} Graph_Pin_Type;

internal const char *const g_Graph_Pin_Type_str[PinType_COUNT] = {
  "(invalid)",
  "exec",
  FAE_LANG_TYPE_STRS
};

internal
Graph_Pin_Type pin_type_from_name(String8 name)
{
  for (u64 i = PinType_Exec; i < PinType_COUNT; ++i)
    if (str8_eqc(name, g_Graph_Pin_Type_str[i]))
      return (Graph_Pin_Type)i;

  return PinType_INVALID;
}

internal
V4 graph_get_pin_color(Graph_Pin_Type type)
{
  static const u32 graph_pin_type_colors[PinType_COUNT] = {
    0xff00ff, // invalid
    0x0080ff, // exec
    0x00804d, // u64
    0x00ab0a, // i64
    0x66f73d, // f64
    0xd41026, // bool
  };
  assert(type < countof(graph_pin_type_colors));
  u32 col = graph_pin_type_colors[type];
  V4 res = v4_from_v3(v3_from_hex(col), 1);
  return res;
}

// @Volatile: if this is changed, remember to change finalize_graph_pin!
typedef struct {
  String8 name;
  Graph_Pin_Type type;
} Graph_Pin;

typedef struct Graph_Pin_Node {
  struct Graph_Pin_Node *next;
  Graph_Pin pin;
} Graph_Pin_Node;

// If this is changed, remember to change clone_graph_node_proto and
// finalize_graph_node_proto
typedef struct {
  String8 name;
  Graph_Pin *inputs;
  Graph_Pin *outputs;
  u16 n_inputs;
  u16 n_outputs;
} Graph_Node_Proto;

typedef struct Graph_Node_Proto_Node {
  struct Graph_Node_Proto_Node *next;

  Graph_Node_Proto_Id id;
  Graph_Node_Proto proto;
} Graph_Node_Proto_Node;

typedef u16 Graph_Pin_Id;

typedef struct {
  // NOTE: this node id may be GRAPH_NODE_ID_ENTRY or GRAPH_NODE_ID_EXIT,
  // in which case `pin` refers to the `pin`-th entrypoint or exitpoint.
  Graph_Node_Id node;
  Graph_Pin_Id pin;
} Pin_Ref;

typedef struct {
  Pin_Ref from;
  Pin_Ref to;
} Graph_Link;

typedef struct Graph_Link_Node {
  struct Graph_Link_Node *next;
  // not used during building, only at runtime
  struct Graph_Link_Node *prev;
  Graph_Link link;
} Graph_Link_Node;

typedef struct {
  V3 pos;
  Quat rot;
  // `size` is a scale that only affects the bg quad.
  V2 size;
  // `scale` affects the whole node (bg, text, ...). It is always uniform.
  f32 scale;
} Graph_Node_Coord;

typedef struct Graph_Node {
  struct Graph_Node *next;
  // not used during building, only at runtime
  struct Graph_Node *prev;

  Graph_Node_Proto_Id proto;
  Graph_Node_Id id;
  String8 name;
  Graph_Node_Coord coord;
} Graph_Node;

#define DEFINE_GRAPH_DECL(T) \
  typedef struct { \
    T *data; \
    u32 declared_at_line; \
  } T##_Decl

DEFINE_GRAPH_DECL(Graph_Node_Proto_Node);

// structure used during graph parsing, ultimately gets finalized into a Script_Graph
typedef struct {
  Graph_Pin_Node *entrypoints_head, *entrypoints_tail; 
  u64 n_entrypoints;

  Graph_Pin_Node *exitpoints_head, *exitpoints_tail; 
  u64 n_exitpoints;

  Graph_Node_Proto_Node *node_protos_head, *node_protos_tail;
  u64 n_node_protos;
  // { node_proto_name => node_proto* }
  Hash_Map node_protos_map;

  Graph_Node *nodes_head, *nodes_tail;
  u64 n_nodes;
  // { node_id => node* }
  Hash_Map nodes_map;
  
  Graph_Link_Node *links_head, *links_tail;
  u64 n_links;
} Script_Graph_Builder;

/// Lexing
enum {
  GraphLexTok_Keyword_In = Lex_FIRST,
  GraphLexTok_Keyword_Out,
  GraphLexTok_Keyword_Node,
  GraphLexTok_Keyword_Link,
  GraphLexTok_Keyword_Id,
  GraphLexTok_Keyword_Coord,
  GraphLexTok_Minus,
  GraphLexTok_Colon,
  GraphLexTok_SemiColon,
  GraphLexTok_BraceOpen,
  GraphLexTok_BraceClose,
  GraphLexTok_Period,
  GraphLexTok_Comma
};

internal
Lex_Token_Mapping *get_graph_lex_token_mappings(Arena *arena, u32 *len)
{
  Lex_Token_Mapping graph_lex_mapping[] = {
    (Lex_Token_Mapping){ str8("in"),    GraphLexTok_Keyword_In },
    (Lex_Token_Mapping){ str8("out"),   GraphLexTok_Keyword_Out },
    (Lex_Token_Mapping){ str8("node"),  GraphLexTok_Keyword_Node },
    (Lex_Token_Mapping){ str8("link"),  GraphLexTok_Keyword_Link },
    (Lex_Token_Mapping){ str8("id"),    GraphLexTok_Keyword_Id },
    (Lex_Token_Mapping){ str8("coord"), GraphLexTok_Keyword_Coord },
    (Lex_Token_Mapping){ str8("-"),     GraphLexTok_Minus },
    (Lex_Token_Mapping){ str8(":"),     GraphLexTok_Colon },
    (Lex_Token_Mapping){ str8(";"),     GraphLexTok_SemiColon },
    (Lex_Token_Mapping){ str8("{"),     GraphLexTok_BraceOpen },
    (Lex_Token_Mapping){ str8("}"),     GraphLexTok_BraceClose },
    (Lex_Token_Mapping){ str8("."),     GraphLexTok_Period },
    (Lex_Token_Mapping){ str8(","),     GraphLexTok_Comma }
  };
  Lex_Token_Mapping *returned = arena_push_array_nozero(Lex_Token_Mapping, arena, countof(graph_lex_mapping));
  memcpy(returned, graph_lex_mapping, sizeof(graph_lex_mapping));
  *len = countof(graph_lex_mapping);
  return returned;
}

/// Parsing
typedef struct {
  Parser inner; // must be the first member!
  
  // the graph being built
  Script_Graph_Builder *graph;
  // arena used for the graph allocation
  Arena *graph_arena;
} Graph_Parser;

internal
Graph_Parser graph_parser_init(Lexer *lexer, Arena *graph_arena, Script_Graph_Builder *graph)
{
  Graph_Parser parser = {};
  parser_init((Parser *)&parser, lexer, "Graph.Parser");
  parser.graph_arena = graph_arena;
  parser.graph = graph;
  return parser;
}

internal
Graph_Pin_Node *parse_pin_decl(Graph_Parser *parser)
{
  // <in|out> name : type
  Lexer *lx = get_lexer(parser);

  Graph_Pin_Node *pin = arena_push(Graph_Pin_Node, parser->graph_arena);

  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, str8("expected pin name after `in` or `out` keyword"));
    return NULL;
  }
  pin->pin.name = str8_copy(parser->graph_arena, tok.string);

  if (!lex_expect(parser, GraphLexTok_Colon))
    return NULL;
  
  tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, push_str8f(lx->arena, "expected type name after `%s:`", cstr(pin->pin.name)));
    return NULL;
  }
  String8 pin_type_name = tok.string;
  pin->pin.type = pin_type_from_name(pin_type_name);
  if (pin->pin.type == PinType_INVALID) {
    parse_err(parser, push_str8f(lx->arena, "invalid pin type: `%s`", cstr(pin_type_name)));
    return NULL;
  }

  return pin;
}

internal
b8 parse_pin_decl_list(Graph_Parser *parser, Graph_Node_Proto *pin_owner)
{
  Lexer *lx = get_lexer(parser);
  Graph_Pin_Node *in_pins_head = NULL, *in_pins_tail = NULL, *out_pins_head = NULL, *out_pins_tail = NULL;

  Lex_Token nxt = lex_peek(parser);
  while (nxt.type == GraphLexTok_Keyword_In || nxt.type == GraphLexTok_Keyword_Out) {
    lex_eat(lx);
    Graph_Pin_Node *pin = parse_pin_decl(parser);
    if (!pin)
      return false;

    if (nxt.type == GraphLexTok_Keyword_In) {
      push_to_sll(in_pins_head, in_pins_tail, pin);
      ++pin_owner->n_inputs;
    } else if (nxt.type == GraphLexTok_Keyword_Out) {
      push_to_sll(out_pins_head, out_pins_tail, pin);
      ++pin_owner->n_outputs;
    }

    nxt = lex_peek(parser);
  }

  if (pin_owner->n_inputs)
    pin_owner->inputs = arena_push_array_nozero(Graph_Pin, parser->graph_arena, pin_owner->n_inputs);
  if (pin_owner->n_outputs)
    pin_owner->outputs = arena_push_array_nozero(Graph_Pin, parser->graph_arena, pin_owner->n_outputs);
  u32 i = 0;
  for (Graph_Pin_Node *pin = in_pins_head; pin; pin = pin->next) {
    pin_owner->inputs[i] = pin->pin;
    ++i;
  }
  i = 0;
  for (Graph_Pin_Node *pin = out_pins_head; pin; pin = pin->next) {
    pin_owner->outputs[i] = pin->pin;
    ++i;
  }
  
  return true;
}

internal
b8 parse_node_proto(Graph_Parser *parser)
{
  parser_push_err_ctx(parser, str8("parsing node prototype"));
  
  Lexer *lx = get_lexer(parser);
  u32 line_decl_start = lx->cur_line;
  // parse node name
  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, str8("expected node name after `node` keyword"));
    parser_pop_err_ctx(parser);
    return false;
  }
  String8 node_name = tok.string;

  parser_pop_err_ctx(parser);
  parser_push_err_ctx(parser, push_str8f(lx->arena, "parsing node prototype '%s'", cstr(node_name)));

  Script_Graph_Builder *graph = parser->graph;

  // verify node name is unique
  Graph_Node_Proto_Node_Decl *existing = hashmap_find(Graph_Node_Proto_Node_Decl, &graph->node_protos_map, &node_name);
  if (existing) {
    parse_err(parser, push_str8f(lx->arena, "cannot declare multiple node prototypes named '%s' (first seen at line %u)", 
                                 cstr(node_name), existing->declared_at_line));
    parser_pop_err_ctx(parser);
    return false;
  }

  Graph_Node_Proto_Node *proto = arena_push(Graph_Node_Proto_Node, parser->graph_arena);
  proto->proto.name = str8_copy(parser->graph_arena, node_name);
  proto->id = hashmap_count(&graph->node_protos_map) + 1; // id 0 is reserved, so we start at 1
  push_to_sll(graph->node_protos_head, graph->node_protos_tail, proto);
  ++graph->n_node_protos;
  Graph_Node_Proto_Node_Decl decl = { proto, line_decl_start };
  hashmap_add(&graph->node_protos_map, &node_name, &decl);
  
  b8 ok = lex_expect(parser, GraphLexTok_BraceOpen) &&
          parse_pin_decl_list(parser, &parser->graph->node_protos_tail->proto) &&
          lex_expect(parser, GraphLexTok_BraceClose);

  parser_pop_err_ctx(parser);
  return ok;
}

// NOTE: this method allocates `name` on the parser graph arena!
internal
b8 parse_typed_ident(Graph_Parser *parser, String8 *name, Graph_Pin_Type *type)
{
  // name : type

  Lexer *lx = get_lexer(parser);

  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, str8("expected identifier"));
    return false;
  }
  *name = str8_copy(parser->graph_arena, tok.string);

  if (!lex_expect(parser, GraphLexTok_Colon))
    return false;

  tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, push_str8f(lx->arena, "expected type name for '%s'", cstr(*name)));
    return false;
  }
  *type = pin_type_from_name(tok.string);
  if (type == PinType_INVALID) {
    parse_err(parser, push_str8f(lx->arena, "invalid type '%s' for '%s'", tok.string, cstr(*name)));
    return false;
  }

  return true;
}

internal
b8 parse_entry_or_exitpoint(Graph_Parser *parser, b8 is_entrypoint)
{
  Lexer *lx = get_lexer(parser);
  Script_Graph_Builder *graph = parser->graph;

  parser_push_err_ctx(parser, push_str8f(lx->arena, "parsing %s", is_entrypoint ? "entrypoint" : "exitpoint"));

  String8 xpoint_name;
  Graph_Pin_Type xpoint_type;
  if (parse_typed_ident(parser, &xpoint_name, &xpoint_type)) {
    Graph_Pin_Node *xpoint = arena_push(Graph_Pin_Node, parser->graph_arena);
    if (is_entrypoint) {
      push_to_sll(graph->entrypoints_head, graph->entrypoints_tail, xpoint);
      ++graph->n_entrypoints;
    } else {
      push_to_sll(graph->exitpoints_head, graph->exitpoints_tail, xpoint);
      ++graph->n_exitpoints;
    }
    xpoint->pin.name = xpoint_name;
    xpoint->pin.type = xpoint_type;
  }

  parser_pop_err_ctx(parser);
  return true;
}

internal
b8 parse_pin_ref(Graph_Parser *parser, Pin_Ref *out_ref)
{
  // A pin ref is `node_id;pin_id`.
  Lexer *lx = get_lexer(parser);

  Lex_Token node = lex_next(parser);
  if (node.type != Lex_Integer) {
    parse_err(parser, str8("expected node id while parsing pin ref in link"));
    return false;
  }
  Graph_Node **referenced_node = hashmap_find(Graph_Node*, &parser->graph->nodes_map, &node.integer);
  if (!referenced_node) {
    parse_err(parser, push_str8f(lx->arena, "link refers to unknown node id %" PRIu64, node.integer));
    return false;
  }

  if (!lex_expect(parser, GraphLexTok_SemiColon))
    return false;

  Lex_Token pin = lex_next(parser);
  if (pin.type != Lex_Integer) {
    parse_err(parser, str8("expected pin id while parsing pin ref in link"));
    return false;
  }
  if (pin.integer > UINT16_MAX) {
    parse_err(parser, push_str8f(lx->arena, "pin id %d is out of range"));
    return false;
  }

  out_ref->node = (Graph_Node_Id) { node.integer };
  if (graph_node_id_eq(out_ref->node, GRAPH_NODE_ID_INVALID)) {
    parse_err(parser, str8("cannot use GRAPH_NODE_ID_INVALID (0) in a pin ref declaration"));
    return false;
  }
  out_ref->pin = (Graph_Pin_Id)pin.integer;

  return true;
}

internal
b8 parse_link_entry_or_exitpoint(Graph_Parser *parser, b8 is_entrypoint, Pin_Ref *ref)
{
  Lexer *lx = get_lexer(parser);
  
  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, is_entrypoint
              ? str8("expected entrypoint name")
              : str8("expected exitpoint name"));
    return false;
  }
  String8 name = tok.string;

  // find the entry/exitpoint by name
  Graph_Pin_Node *node = is_entrypoint ? parser->graph->entrypoints_head : parser->graph->exitpoints_head;
  u64 pin_id = 0;
  for (; node; node = node->next) {
    if (str8_eq(node->pin.name, name)) {
      ref->node = is_entrypoint ? GRAPH_NODE_ID_ENTRY : GRAPH_NODE_ID_EXIT;
      ref->pin = pin_id;
      return true;
    }
    ++pin_id;
  }

  parse_err(parser, push_str8f(lx->arena, "unknown %s '%s'", is_entrypoint ? "entrypoint" : "exitpoint", cstr(name)));
  return false;
}

internal
b8 parse_link(Graph_Parser *parser)
{
  // (pin_ref or entrypoint) : (pin_ref or exitpoint)

  Script_Graph_Builder *graph = parser->graph;
  Lexer *lx = get_lexer(parser);
  
  Graph_Link_Node *link = arena_push(Graph_Link_Node, parser->graph_arena);
  push_to_sll(graph->links_head, graph->links_tail, link);
  ++graph->n_links;

  // Parse `from`
  Lex_Token tok = lex_peek(parser);
  if (tok.type == Lex_Integer) {
    if (!parse_pin_ref(parser, &link->link.from))
      return false;
  } else if (tok.type == Lex_Ident) {
    if (!parse_link_entry_or_exitpoint(parser, true, &link->link.from))
      return false;
  } else {
    parse_err(parser, push_str8f(lx->arena, "expected `node_id;pin_id` or an entrypoint, got '%s'", lex_tok_to_human_friendly(lx, tok)));
    return false;
  }

  if (!lex_expect(parser, GraphLexTok_Colon))
    return false;

  // Parse `to`
  tok = lex_peek(parser);
  if (tok.type == Lex_Integer) {
    if (!parse_pin_ref(parser, &link->link.to))
      return false;
  } else if (tok.type == Lex_Ident) {
    if (!parse_link_entry_or_exitpoint(parser, false, &link->link.to))
      return false;
  } else {
    parse_err(parser, push_str8f(lx->arena, "expected `node_id;pin_id` or an exitpoint, got '%s'", 
                                 lex_tok_ty_to_human_friendly(lx, tok.type)));
    return false;
  }

  // TODO: maybe make sure link is unique?
  return true;
}

internal
b8 parse_link_list(Graph_Parser *parser)
{
  Lexer *lx = get_lexer(parser);
  Lex_Token nxt = lex_peek(parser);
  while (!lex_is_mono_token(lx, nxt)) {
    if (!parse_link(parser))
      return false;
    nxt = lex_peek(parser);
  }
  return true;
}

internal
b8 parse_link_block(Graph_Parser *parser)
{
  parser_push_err_ctx(parser, str8("parsing link block"));

  b8 ok = lex_expect(parser, GraphLexTok_BraceOpen) &&
          parse_link_list(parser) &&
          lex_expect(parser, GraphLexTok_BraceClose);

  parser_pop_err_ctx(parser);
  return ok;
}

internal
b8 parse_graph_node_coord(Graph_Parser *parser, Graph_Node_Coord *coord)
{
  parser_push_err_ctx(parser, str8("parsing node coordinates"));
  
  Lexer *lx = get_lexer(parser);
  Lex_Token tok;

#define READ_COORD(val) do { \
    f32 sign = 1; \
    tok = lex_next(parser); \
    if (tok.type == GraphLexTok_Minus) { \
      sign = -1; \
      tok = lex_next(parser); \
    } \
    if (tok.type == Lex_Integer) { \
      val = sign * (f32)tok.integer; \
    } else if (tok.type == Lex_Real) { \
      val = sign * tok.real; \
    } else { \
      parse_err(parser, push_str8f(lx->arena, "expected number, found '%s'", lex_tok_to_human_friendly(lx, tok))); \
      parser_pop_err_ctx(parser); \
      return false; \
    } \
  } while (0)

#define EXPECT(type) do { \
    if (!lex_expect(parser, type)) { \
      parser_pop_err_ctx(parser); \
      return false; \
    } \
  } while (0)

  READ_COORD(coord->pos.x);   EXPECT(GraphLexTok_Comma);
  READ_COORD(coord->pos.y);   EXPECT(GraphLexTok_Comma);
  READ_COORD(coord->pos.z);   EXPECT(GraphLexTok_SemiColon);
  READ_COORD(coord->rot.x);   EXPECT(GraphLexTok_Comma);
  READ_COORD(coord->rot.y);   EXPECT(GraphLexTok_Comma);
  READ_COORD(coord->rot.z);   EXPECT(GraphLexTok_Comma);
  READ_COORD(coord->rot.w);   EXPECT(GraphLexTok_SemiColon);
  READ_COORD(coord->size.x);  EXPECT(GraphLexTok_Comma);
  READ_COORD(coord->size.y);  EXPECT(GraphLexTok_SemiColon);
  READ_COORD(coord->scale);

#undef EXPECT
#undef READ_COORD

  parser_pop_err_ctx(parser);
  return true;
}

internal
b8 parse_node(Graph_Parser *parser, String8 node_name)
{
  parser_push_err_ctx(parser, str8("parsing node"));

  // node_name : Node_Proto {
  //   id: <id>
  // }
  Lexer *lx = get_lexer(parser);
  Script_Graph_Builder *graph = parser->graph;
  
  Graph_Node *node = arena_push(Graph_Node, parser->graph_arena);
  node->name = str8_copy(parser->graph_arena, node_name);
  push_to_dll(graph->nodes_head, graph->nodes_tail, node);
  ++graph->n_nodes;

  parser_pop_err_ctx(parser);
  parser_push_err_ctx(parser, push_str8f(lx->arena, "parsing node '%s'", cstr(node->name)));

  if (!lex_expect(parser, GraphLexTok_Colon)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, push_str8f(lx->arena, "expected node prototype name after '%s :'", cstr(node->name)));
    parser_pop_err_ctx(parser);
    return false;
  }
  // check if we know this type, error otherwise
  // TODO: allow late-declared types
  Graph_Node_Proto_Node **proto_node = hashmap_find(Graph_Node_Proto_Node*, &graph->node_protos_map, &tok.string);
  if (!proto_node) {
    parse_err(parser, push_str8f(lx->arena, "unknown node prototype: '%s'", cstr(tok.string)));
    parser_pop_err_ctx(parser);
    return false;
  }
  node->proto = (*proto_node)->id;

  if (!(lex_expect(parser, GraphLexTok_BraceOpen) &&
        lex_expect(parser, GraphLexTok_Keyword_Id) &&
        lex_expect(parser, GraphLexTok_Colon)))
  {
    parser_pop_err_ctx(parser);
    return false;
  }

  tok = lex_next(parser);
  if (tok.type != Lex_Integer) {
    parse_err(parser, push_str8f(lx->arena, "id of node '%s' is not a number", cstr(node_name)));
    parser_pop_err_ctx(parser);
    return false;
  }
  // check node id is unique
  Graph_Node **existing_node = hashmap_find(Graph_Node*, &graph->nodes_map, &tok.integer);
  if (existing_node) {
    parse_err(parser, push_str8f(lx->arena, "duplicate node id: %" PRIu64, tok.integer));
    parser_pop_err_ctx(parser);
    return false;
  }
  node->id = (Graph_Node_Id) { tok.integer };
  if (graph_node_id_eq(node->id, GRAPH_NODE_ID_INVALID)) {
    parse_err(parser, push_str8f(lx->arena, "cannot use GRAPH_NODE_ID_INVALID (0) as the id of '%s'", cstr(node_name)));
    return false;
  }
  hashmap_add(&graph->nodes_map, &node->id, &node);

  // parse coord (optional)
  tok = lex_peek(parser);
  if (tok.type == GraphLexTok_BraceClose) {
    lex_eat(lx);
    // assign default coords
    node->coord.scale = 1;
    node->coord.size = v2(1.5, 1);
    parser_pop_err_ctx(parser);
    return true;
  }

  if (!lex_expect(parser, GraphLexTok_Keyword_Coord)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  if (!lex_expect(parser, GraphLexTok_Colon)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  if (!parse_graph_node_coord(parser, &node->coord)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  if (!lex_expect(parser, GraphLexTok_BraceClose)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  parser_pop_err_ctx(parser);
  return true;
}

internal
String8 graph_node_coord_print(Arena *arena, Graph_Node_Coord *c)
{
  return push_str8f(arena, "%.2f,%.2f,%.2f;%.2f,%.2f,%.2f; %.2f",
                    c->pos.x, c->pos.y, c->pos.z, c->rot.x, c->rot.y, c->rot.z,
                    c->size.x, c->size.y, c->scale);
}

internal
void graph_builder_describe(Script_Graph_Builder *graph, Arena **conflicts, u32 n_conflicts)
{
  Temp scratch = scratch_begin(conflicts, n_conflicts);
  
  DEBUG_TAG("Graph", "Entrypoints:");
  for (Graph_Pin_Node *node = graph->entrypoints_head; node; node = node->next) {
    DEBUG_TAG("Graph", "   %s : %s", cstr(node->pin.name), g_Graph_Pin_Type_str[node->pin.type]);
  }

  DEBUG_TAG("Graph", "Exitpoints:");
  for (Graph_Pin_Node *node = graph->exitpoints_head; node; node = node->next) {
    DEBUG_TAG("Graph", "   %s : %s", cstr(node->pin.name), g_Graph_Pin_Type_str[node->pin.type]);
  }

  DEBUG_TAG("Graph", "Node protos:");
  u64 id = 0;
  for (Graph_Node_Proto_Node *node = graph->node_protos_head; node; node = node->next) {
    DEBUG_TAG("Graph", "   %" PRIu64 ": %s", id++, cstr(node->proto.name));
  }

  DEBUG_TAG("Graph", "Nodes:");
  for (Graph_Node *node = graph->nodes_head; node; node = node->next) {
    DEBUG_TAG("Graph", "   %" PRIu64 ": %s : %" PRIu64 " @%s", node->id,
              cstr(node->name), node->proto, cstr(graph_node_coord_print(scratch.arena, &node->coord)));
  }

  DEBUG_TAG("Graph", "Links:");
  for (Graph_Link_Node *node = graph->links_head; node; node = node->next) {
    Graph_Link l = node->link;
    DEBUG_TAG("Graph", "   %d;%d -> %d;%d", l.from.node, l.from.pin, l.to.node, l.to.pin);
  }

  scratch_end(scratch);
}

internal
b8 parse_script_graph_internal(Arena *arena, String8 graph_raw, Script_Graph_Builder *builder)
{
  Temp scratch = scratch_begin(&arena, 1);

  u64 start_ns = os_clock_time_ns();
  
  builder->node_protos_map = hashmap_init(String8, Graph_Node_Proto_Node_Decl, arena, 256, hash_str8, hash_str8_eq);
  builder->nodes_map = hashmap_init_default(Graph_Node_Id, Graph_Node*, arena, 2048);

  u32 n_mappings;
  Lex_Token_Mapping *mappings = get_graph_lex_token_mappings(scratch.arena, &n_mappings);
  Lexer lexer = lex_init(mappings, n_mappings, str8("#"));
  lex_start(&lexer, scratch.arena, graph_raw.str, graph_raw.size);
  Graph_Parser parser = graph_parser_init(&lexer, arena, builder);
  
  b8 failed = false;
  Lex_Token tok = lex_next(&parser);
  while (tok.type != Lex_ERROR && tok.type != Lex_EOF) {
    // top level production rules:
    // - node decl
    // - node proto decl
    // - link block
    // - entrypoint
    // - exitpoint
    if (tok.type == GraphLexTok_Keyword_Node) {
      failed = !parse_node_proto(&parser);
    } else if (tok.type == GraphLexTok_Keyword_Link) {
      failed = !parse_link_block(&parser);
    } else if (tok.type == Lex_Ident) {
      failed = !parse_node(&parser, tok.string);
    } else if (tok.type == GraphLexTok_Keyword_In) {
      failed = !parse_entry_or_exitpoint(&parser, true);
    } else if (tok.type == GraphLexTok_Keyword_Out) {
      failed = !parse_entry_or_exitpoint(&parser, false);
    }

    if (failed)
      break;

    tok = lex_next(&parser);
  }

  u64 end_ns = os_clock_time_ns();
  
  INFO_TAG("Graph.Parse", "Took %.2f ms to parse builder with %" PRIu64 " node prototypes, %" PRIu64 " nodes, %" PRIu64 " links.", 
            (end_ns - start_ns) * 1e-6, builder->n_node_protos, builder->n_nodes, builder->n_links);
  graph_builder_describe(builder, &lexer.arena, 1);

  lex_end(&lexer);
  scratch_end(scratch);
  return !failed;
}
