#define PIN_SCALE v3(0.15, 0.1, 0.1)

typedef enum {
  EdPin_INVALID,
  EdPin_In,
  EdPin_Out
} Ed_Pin_InOut;

typedef struct Ed_Pin {
  struct Ed_Pin *next, *prev;
  struct Ed_Node *owner;
  
  Gfx_Instance_Id pin_gfx_id;
  Gfx_Instance_Id text_gfx_id;
  // pin offset relative to the node's center
  V3 offset;
  // text offset relative to the pin's offset
  V2 text_offset;
  Ed_Pin_InOut inout;
  Graph_Pin_Type type;
  Graph_Pin_Id pin_id;
} Ed_Pin;

// NOTE: this struct has a somewhat complex linking logic going on, as it's used
// simultaneously in 3 linked lists:
// 1. the list of currently allocated Ed_Links
// 2. the list of links belonging to `from`
// 3. the list of links belonging to `to`
// List 1 is considered the "owning" list, while the others only have a specific view thereof.
// The reason why we do this is that it's convenient to have the Ed_Links centrally owned by the
// graph rather than by the nodes, as each link refers to 2 separate nodes (or a node and an entry/exitpoint),
// so it would not be very clear who should destroy a link if they belonged to the individual nodes.
typedef struct Ed_Link {
  // links to next/prev node in the allocated list
  struct Ed_Link *next, *prev;
  // link to next node in `from`'s list
  struct Ed_Link *from_next, *from_prev;
  // link to next node in `to`'s list
  struct Ed_Link *to_next, *to_prev;
  // the pins that this link is linking
  struct Ed_Pin *from, *to;
  Graph_Link_Node *link;
  Gfx_Instance_Id spline_id;
} Ed_Link;

typedef struct {
  Cpu_Instance_Data *bg;
  Cpu_Instance_Data *name_text;
  Cpu_Instance_Data *proto_text;
} Ed_Node_Inst_Data;

typedef struct Ed_Node {
  struct Ed_Node *next, *prev;

  // These are unowning pointers to Ed_Graph->links_head/tail
  Ed_Link *in_links_head, *in_links_tail;
  Ed_Link *out_links_head, *out_links_tail;

  // These are owning pointers that all share the same free list in Ed_Graph
  Ed_Pin *pins_head, *pins_tail;
  
  Gfx_Instance_Id bg_gfx_id;
  Gfx_Instance_Id name_text_gfx_id, proto_text_gfx_id;

  Graph_Node *graph_node;

  // these are relative to the node
  V3 name_text_offset;
  V3 proto_text_offset;
} Ed_Node;

// The graphical representation of a Script_Graph. Has spatial/visual information etc.
typedef struct {
  Ed_Node *nodes_head, *nodes_tail;
  Ed_Node *nodes_free;
  Ed_Node *selected_node;
  Ed_Pin *selected_pin;
  // { node_id => Ed_Node* }
  Hash_Map node_map;

  Ed_Link *links_head, *links_tail;
  Ed_Link *links_free;

  // list of entry/exitpoints
  Ed_Pin *epoints_head, *epoints_tail;

  Ed_Pin *pins_free;
} Ed_Graph;

typedef struct {
  // NOTE: the graph is owned by the editor
  Arena *graph_arena;
  Script_Graph *graph;

  // A copy of the graph as it was when editor_set_graph() was called.
  Arena *orig_graph_arena;
  Script_Graph *orig_graph;

  // Arena used to create the various editor objects (Ed_Node, Ed_Link, ...)
  Arena *ed_graph_arena;
  Ed_Graph ed_graph;

  Camera camera;
  Gfx *gfx;

  // Drag stuff
  b8 was_dragging_node;
  f32 node_dist_at_drag_start;
  V2 mouse_offset_from_center_at_drag_start;
  // The link that gets created when dragging out of a pin
  Ed_Link *pending_link;

  // Staged data for delaying the creation of texts during graph loading. 
  // As we load a graph, we collect all the data required to create its texts here and submit it 
  // all together to the gfx layer at once, rather than one at a time, to dramatically speed up their creation.
  Text_Create_Data *pending_texts;
  Cpu_Instance_Data *pending_text_idata;
  Gfx_Instance_Id **pending_text_ids;
  u32 pending_texts_capacity;
  u32 n_pending_texts;
} Editor;

#define editor_alloc_obj(Type, obj, arena, head, tail, free) do { \
  if ((free)) { \
    obj = (free); \
    (free) = obj->next; \
    zero_struct(obj); \
  } else { \
    obj = arena_push(Type, (arena)); \
  } \
  push_to_dll((head), (tail), obj); \
} while (0)

internal
void editor_graph_init(Ed_Graph *graph, Arena *arena)
{
  graph->node_map = hashmap_init_default(Graph_Node_Id, Ed_Node*, arena, 2048);
}

internal
void editor_init(Editor *editor, Gfx *gfx)
{
  editor->camera.zoom = 6.f;
  editor->camera.speed = 3.f;
  editor->camera.pos.y = -1.f;
  editor->camera.fov = K_HALF_PI;

  editor->gfx = gfx;

  editor->ed_graph_arena = arena_alloc();
  editor->graph_arena = arena_alloc();
  editor->orig_graph_arena = arena_alloc();

  editor_graph_init(&editor->ed_graph, editor->ed_graph_arena);
}

internal
void editor_deinit(Editor *editor)
{
  arena_release(editor->ed_graph_arena);
  arena_release(editor->graph_arena);
  arena_release(editor->orig_graph_arena);
}

internal
void editor_debug_print_ed_graph(Editor *editor, Log_Level lv)
{
  if (g_loglv < lv)
    return;
  
  Temp scratch = scratch_begin(0, 0);
  String8_Node *sn = NULL;
  for (Ed_Node *node = editor->ed_graph.nodes_head; node; node = node->next) {
    sn = push_str8_node(scratch.arena, sn, "%lu", node->graph_node->id);
  }
  String8 s = str8_node_join(scratch.arena, sn, ", ");
  fae_log(lv, "Editor", "Ed_Nodes: %s", cstr(s));
  scratch_end(scratch);
}

typedef struct {
  // This is null if the raycast didn't hit anything. In that case, all other values are undefined.
  union {
    Ed_Node *node;
    Ed_Pin *pin;
  };
  V3 intersection;
  V2 intersection_offset_from_center;
  f32 intersection_dist;
} Ed_Raycast_Result;

// `intersection_offset_from_center` is the vector planar to the node that points from the node center to
// the intersection point (only valid if the returned node is not null)
internal
Ed_Raycast_Result editor_raycast_node(Editor *editor, V3 origin, V3 direction)
{
  Ed_Raycast_Result res = {};
  res.intersection_dist = FLT_MAX;
  
  // @Speed: add some spatial acceleration structure
  for (Ed_Node *node = editor->ed_graph.nodes_head; node; node = node->next) {
    M33 node_ref_frame = quat_to_rot_matrix(node->graph_node->coord.rot);
    // XXX: why is normal negated? double-check ref_frame_from_euler_angles
    V3 node_normal = v3_neg(node_ref_frame.col[1]);
    V3 node_pos = node->graph_node->coord.pos;
    Plane node_plane = plane_from_normal_pos(node_normal, node_pos);
    f32 t = plane_raycast_from(origin, direction, node_plane);
    if (t < 0 || t > res.intersection_dist)
      continue;

    V3 intersection = v3_add(origin, v3_muls(direction, t));
    // vector pointing from center of the quad to the intersection
    V3 diff_inter_quad = v3_sub(intersection, node_pos);
    assert(v3_dot(diff_inter_quad, node_normal) < 0.001f);
    f32 dist_x = v3_dot(diff_inter_quad, node_ref_frame.col[0]);
    f32 dist_y = v3_dot(diff_inter_quad, node_ref_frame.col[2]);
    V2 node_size = node->graph_node->coord.size;
    // expand the node size to include also all pins
    node_size.x += PIN_SCALE.x;
    if (fabs(dist_x) <= node_size.x * 0.5 && fabs(dist_y) <= node_size.y * 0.5) {
      res.intersection = intersection;
      res.intersection_dist = t;
      res.node = node;
      res.intersection_offset_from_center = v2(dist_x, dist_y);
    }
  }
  return res;
}

internal
Ed_Raycast_Result editor_raycast_epoint(Editor *editor, V3 origin, V3 direction)
{
  Ed_Raycast_Result res = {};
  res.intersection_dist = FLT_MAX;
  
  // entry/exitpoints are always facing -Y
  V3 pin_normal = v3(0, -1, 0);
  // @Speed: add some spatial acceleration structure
  for (Ed_Pin *pin = editor->ed_graph.epoints_head; pin; pin = pin->next) {
    V3 pin_pos = pin->offset;
    Plane pin_plane = plane_from_normal_pos(pin_normal, pin_pos);
    f32 t = plane_raycast_from(origin, direction, pin_plane);
    if (t < 0 || t > res.intersection_dist)
      continue;

    V3 intersection = v3_add(origin, v3_muls(direction, t));
    // vector pointing from center of the quad to the intersection
    V3 diff_inter_quad = v3_sub(intersection, pin_pos);
    assert(v3_dot(diff_inter_quad, pin_normal) < 0.001f);
    f32 dist_x = diff_inter_quad.x;
    f32 dist_y = diff_inter_quad.z;
    if (fabs(dist_x) <= PIN_SCALE.x * 0.5 && fabs(dist_y) <= PIN_SCALE.z * 0.5) {
      res.intersection = intersection;
      res.intersection_dist = t;
      res.pin = pin;
      res.intersection_offset_from_center = v2(dist_x, dist_y);
    }
  }
  return res;
}

typedef struct {
  Ed_Node *node;
  Ed_Pin *pin;
} Ed_Select_Result;

// casts a ray in the camera direction and returns the closest node intersected.
internal
Ed_Select_Result editor_select_node_or_pin(Editor *editor, V2 mouse_pos)
{
  f32 unproj_depth = 0.f;
  V3 ray_origin = unproject_screen_pos(mouse_pos, &editor->gfx->inv_view_proj, &editor->gfx->proj, editor->gfx->viewport_px, unproj_depth);
  V3 ray_dir = editor->gfx->config.proj_mode == Proj_Ortho
               ? camera_fwd(&editor->camera)
               : v3_normalized(v3_sub(ray_origin, editor->camera.pos));

  // First try raycasting a node. If that fails, try with entry/exitpoints.
  Ed_Raycast_Result res = editor_raycast_node(editor, ray_origin, ray_dir);
  Ed_Select_Result sel = {};
  sel.node = res.node;
  if (res.node) {
    // FIXME
    editor->mouse_offset_from_center_at_drag_start = v2_neg(res.intersection_offset_from_center);
    // editor->node_dist_at_drag_start = res.intersection_dist;
    V3 c = v3_add(res.intersection, v3_x0z(res.intersection_offset_from_center));
    V3 c_view = transform_pos3(&editor->gfx->view, c);
    // editor->node_dist_at_drag_start = v3_len(v3_sub(c, editor->camera.pos));
    editor->node_dist_at_drag_start = c_view.z;

    // check if we selected a pin in this node
    for (Ed_Pin *pin = res.node->pins_head; pin; pin = pin->next) {
      // these are in node-local space
      Rect pin_rect = rect_center_halfsize(v2_xz(pin->offset), v2_muls(v2_xz(PIN_SCALE), 0.5));
      if (rect_contains(pin_rect, res.intersection_offset_from_center)) {
        sel.pin = pin;
        sel.node = NULL;
        break;
      }
    }
  } else {
    res = editor_raycast_epoint(editor, ray_origin, ray_dir);
    if (res.pin) {
      sel.pin = res.pin;
    }
  }
  return sel;
}

internal
Ed_Node_Inst_Data editor_get_node_instance_data(Editor *editor, Ed_Node *node)
{
  Ed_Node_Inst_Data data;
  data.bg = gfx_get_instance_data(editor->gfx, node->bg_gfx_id);
  data.name_text = gfx_get_instance_data(editor->gfx, node->name_text_gfx_id);
  data.proto_text = gfx_get_instance_data(editor->gfx, node->proto_text_gfx_id);
  return data;
}

internal
void editor_calc_link_control_points(Editor *editor, Ed_Link *link, V3 cpoints[4])
{
  Cpu_Instance_Data *from_idata = gfx_get_instance_data(editor->gfx, link->from->pin_gfx_id);
  Cpu_Instance_Data *to_idata = gfx_get_instance_data(editor->gfx, link->to->pin_gfx_id);
  cpoints[0] = from_idata->pos;
  cpoints[3] = to_idata->pos;
  M44 from_xform = transform_from_pos_rot_scale(from_idata->pos, from_idata->rot, v3s(1));
  M44 to_xform = transform_from_pos_rot_scale(to_idata->pos, to_idata->rot, v3s(1));
  V3 from_fwd = v3_from_v4(from_xform.col[0]);
  V3 diff = v3_normalized(v3_sub(to_idata->pos, from_idata->pos));
  f32 cos_a = v3_dot(from_fwd, diff);
  f32 t = 0.5 * (cos_a + 1);
  f32 dist = 0.4f + lerp(0.f, 2.f, 1.f - t);
  cpoints[1] = transform_pos3(&from_xform, v3(dist, 0, 0));
  cpoints[2] = transform_pos3(&to_xform, v3(-dist, 0, 0));
}

internal
void editor_calc_pending_link_control_points(Editor *editor, Ed_Link *link, V3 cpoints[4], V3 mpos)
{
  if (link->from) {
    Cpu_Instance_Data *fixed_end_idata = gfx_get_instance_data(editor->gfx, link->from->pin_gfx_id);
    cpoints[0] = fixed_end_idata->pos;
    M44 xform = transform_from_pos_rot_scale(fixed_end_idata->pos, fixed_end_idata->rot, v3s(1));
    cpoints[1] = transform_pos3(&xform, v3(0.4, 0, 0));
    cpoints[3] = mpos;
    cpoints[2] = v3_avg(cpoints[1], cpoints[3]); // TODO: improve
  } else {
    Cpu_Instance_Data *fixed_end_idata = gfx_get_instance_data(editor->gfx, link->to->pin_gfx_id);
    cpoints[0] = mpos;
    cpoints[3] = fixed_end_idata->pos;
    M44 xform = transform_from_pos_rot_scale(fixed_end_idata->pos, fixed_end_idata->rot, v3s(1));
    cpoints[2] = transform_pos3(&xform, v3(-0.4, 0, 0));
    cpoints[1] = v3_avg(cpoints[0], cpoints[2]); // TODO: improve
  }
}

internal
void editor_apply_node_transform(Editor *editor, Ed_Node *node)
{
  Graph_Node_Coord *xform = &node->graph_node->coord;

  Ed_Node_Inst_Data idata = editor_get_node_instance_data(editor, node);
  idata.bg->pos = xform->pos;
  idata.bg->rot = xform->rot;
  idata.bg->scale = v3_muls(v3(xform->size.x, 1, xform->size.y), xform->scale);

  // @Speed: maybe we should save directly the transform matrix in the nodes?
  M44 node_xform = transform_from_pos_rot_scale(xform->pos, xform->rot, v3(xform->scale, xform->scale, xform->scale));

  V3 name_text_wpos = transform_pos3(&node_xform, node->name_text_offset);
  idata.name_text->pos = name_text_wpos;
  idata.name_text->rot = xform->rot;
  idata.name_text->scale = v3(xform->scale, xform->scale, xform->scale);

  V3 proto_text_wpos = transform_pos3(&node_xform, node->proto_text_offset);
  idata.proto_text->pos = proto_text_wpos;
  idata.proto_text->rot = xform->rot;
  idata.proto_text->scale = v3(xform->scale, xform->scale, xform->scale);

  // Update pins
  for (Ed_Pin *pin = node->pins_head; pin; pin = pin->next) {
    Cpu_Instance_Data *pin_idata = gfx_get_instance_data(editor->gfx, pin->pin_gfx_id);
    pin_idata->rot = xform->rot;
    pin_idata->pos = transform_pos3(&node_xform, pin->offset);

    Cpu_Instance_Data *text_idata = gfx_get_instance_data(editor->gfx, pin->text_gfx_id);
    V3 text_off = v3_add(pin->offset, v3_x0z(pin->text_offset));
    text_idata->rot = xform->rot;
    text_idata->pos = transform_pos3(&node_xform, text_off);
  }

  // Update links
  for (Ed_Link *link = node->in_links_head; link; link = link->to_next) {
    V3 cpoints[4];
    editor_calc_link_control_points(editor, link, cpoints);
    gfx_update_spline(editor->gfx, link->spline_id, cpoints);
  }
  for (Ed_Link *link = node->out_links_head; link; link = link->from_next) {
    V3 cpoints[4];
    editor_calc_link_control_points(editor, link, cpoints);
    gfx_update_spline(editor->gfx, link->spline_id, cpoints);
  }
}

internal
Ed_Node *editor_node_from_id(Editor *editor, Graph_Node_Id node_id)
{
  if (!graph_node_id_is_node(node_id))
    return NULL;

  Ed_Node **enode = hashmap_find(Ed_Node*, &editor->ed_graph.node_map, &node_id);
  return enode ? *enode : NULL;
}

internal
Ed_Pin *editor_node_get_pin(Ed_Node *node, Graph_Pin_Id pin_id, Ed_Pin_InOut inout)
{
  (void)inout;
  Graph_Pin_Id n = 0;
  for (Ed_Pin *pin = node->pins_head; pin; pin = pin->next) {
    if (n == pin_id) {
      assert(pin->inout == inout);
      return pin;
    }
    ++n;
  }
  // pin_idx-th pin of inout `inout` didn't exist
  assert(false);
  return NULL;
}

internal
Ed_Pin *editor_get_epoint(Ed_Graph *ed_graph, Pin_Ref ref)
{
  b8 is_entry = graph_node_id_eq(ref.node, GRAPH_NODE_ID_ENTRY);
  assert(is_entry || graph_node_id_eq(ref.node, GRAPH_NODE_ID_EXIT));

  Ed_Pin_InOut pin_type = is_entry ? EdPin_Out : EdPin_In;
  u32 id = 0;
  for (Ed_Pin *pin = ed_graph->epoints_head; pin; pin = pin->next) {
    if (pin->inout == pin_type && id++ == ref.pin)
      return pin;
  }
  return NULL;
}

// Creates a new editor link without touching the graph (lnk is assumed to come from
// the graph).
internal
Ed_Link *editor_create_link_from_existing(Editor *editor, Graph_Link_Node *lnk)
{
  Ed_Link *link;
  editor_alloc_obj(Ed_Link, link, editor->ed_graph_arena, 
                   editor->ed_graph.links_head, editor->ed_graph.links_tail, 
                   editor->ed_graph.links_free);
  link->link = lnk;

  // Set pointers to the pins
  if (graph_node_id_is_node(lnk->link.from.node)) {
    Ed_Node *from_node = editor_node_from_id(editor, lnk->link.from.node);
    assert(from_node);
    link->from = editor_node_get_pin(from_node, lnk->link.from.pin, EdPin_Out);
    push_to_dll_ex(from_node->out_links_head, from_node->out_links_tail, link, from_next, from_prev);
  } else {
    link->from = editor_get_epoint(&editor->ed_graph, lnk->link.from);
  }
  if (graph_node_id_is_node(lnk->link.to.node)) {
    Ed_Node *to_node = editor_node_from_id(editor, lnk->link.to.node);
    assert(to_node);
    link->to = editor_node_get_pin(to_node, lnk->link.to.pin, EdPin_In);
    push_to_dll_ex(to_node->in_links_head, to_node->in_links_tail, link, to_next, to_prev);
  } else {
    link->to = editor_get_epoint(&editor->ed_graph, lnk->link.to);
  }

  // Create the spline
  V3 cpoints[4];
  editor_calc_link_control_points(editor, link, cpoints);
  Gfx_Instance_Id spline_id = gfx_add_spline(editor->gfx, cpoints, cpu_inst_data_default());
  link->spline_id = spline_id;

  return link;
}

// Creates a new link on the internal graph and a matching editor link
internal
Ed_Link *editor_create_new_link(Editor *editor, Ed_Pin *from, Ed_Pin *to)
{
  Graph_Link_Node *glink = graph_create_link(editor->graph);
  if (from->owner)
    glink->link.from.node = from->owner->graph_node->id;
  else
    glink->link.from.node = GRAPH_NODE_ID_ENTRY;
  glink->link.from.pin = from->pin_id;
  if (to->owner)
    glink->link.to.node = to->owner->graph_node->id;
  else
    glink->link.to.node = GRAPH_NODE_ID_EXIT;
  glink->link.to.pin = to->pin_id;

  Ed_Link *link = editor_create_link_from_existing(editor, glink);
  return link;
}

internal
Ed_Link *editor_create_pending_link(Editor *editor, Ed_Pin *fixed_end, V2 mouse_pos)
{
  Ed_Link *link;
  editor_alloc_obj(Ed_Link, link, editor->ed_graph_arena, 
                   editor->ed_graph.links_head, editor->ed_graph.links_tail, 
                   editor->ed_graph.links_free);

  if (fixed_end->inout == EdPin_Out)
    link->from = fixed_end;
  else
    link->to = fixed_end;

  f32 dist = 1.5f;
  V3 mwpos = unproject_screen_pos(mouse_pos, &editor->gfx->inv_view_proj, &editor->gfx->proj, editor->gfx->viewport_px, dist);

  // Create the spline
  V3 cpoints[4];
  editor_calc_pending_link_control_points(editor, link, cpoints, mwpos);
  Gfx_Instance_Id spline_id = gfx_add_spline(editor->gfx, cpoints, cpu_inst_data_default());
  link->spline_id = spline_id;

  return link;
}

internal
void editor_update_pending_link(Editor *editor, V2 mouse_pos)
{
  assert(editor->pending_link);
  
  f32 dist = 1.5f;
  V3 mwpos = unproject_screen_pos(mouse_pos, &editor->gfx->inv_view_proj, &editor->gfx->proj, editor->gfx->viewport_px, dist);
  V3 cpoints[4];
  editor_calc_pending_link_control_points(editor, editor->pending_link, cpoints, mwpos);
  gfx_update_spline(editor->gfx, editor->pending_link->spline_id, cpoints);
}

internal
void editor_destroy_link(Editor *editor, Ed_Link *link)
{
  gfx_remove_spline(editor->gfx, link->spline_id);

  if (link != editor->pending_link)
    graph_remove_link(editor->graph, link->link);
  
  if (link->from && link->from->owner) {
    pop_from_dll_ex(link->from->owner->out_links_head, link->from->owner->out_links_tail, link, from_next, from_prev);
  }
  if (link->to && link->to->owner) {
    pop_from_dll_ex(link->to->owner->in_links_head, link->to->owner->in_links_tail, link, to_next, to_prev);
  }
  Ed_Graph *graph = &editor->ed_graph;
  pop_from_dll_add_to_free(graph->links_head, graph->links_tail, link, graph->links_free);
}

internal
void editor_prepare_pending_texts(Arena *arena, Editor *editor, u32 n_texts)
{
  assert(!editor->n_pending_texts);
  assert(!editor->pending_texts_capacity);
  editor->pending_texts_capacity = n_texts;
  if (n_texts) {
    editor->pending_texts = arena_push_array_nozero(Text_Create_Data, arena, editor->pending_texts_capacity);
    editor->pending_text_idata = arena_push_array_nozero(Cpu_Instance_Data, arena, editor->pending_texts_capacity);
    editor->pending_text_ids = arena_push_array_nozero(Gfx_Instance_Id*, arena, editor->pending_texts_capacity);
  }
}

internal
void editor_commit_pending_texts(Editor *editor)
{
  if (editor->n_pending_texts > 0) {
    Temp scratch = scratch_begin(0, 0);

    gfx_add_texts(editor->gfx, editor->n_pending_texts, editor->pending_texts, editor->pending_text_idata,
                  Gfx_World_Space, editor->pending_text_ids);

    editor->n_pending_texts = 0;
    scratch_end(scratch);
  }

  editor->pending_texts_capacity = 0;
}

internal
void editor_push_pending_text(Editor *editor, String8 string, Char_Size char_size, Cpu_Instance_Data idata, Gfx_Instance_Id *pending_id)
{
  // XXX: maybe we should commit the pending texts if they get too many at once?

  assert(editor->n_pending_texts < editor->pending_texts_capacity);

  Text_Create_Data *pending = &editor->pending_texts[editor->n_pending_texts];
  pending->string = string;
  pending->char_size = char_size;
  editor->pending_text_idata[editor->n_pending_texts] = idata;
  editor->pending_text_ids[editor->n_pending_texts] = pending_id;
  ++editor->n_pending_texts;
}

internal
void editor_adjust_text_offsets(Editor *editor, Ed_Node *node)
{
  V2 name_text_size = gfx_get_text_size(editor->gfx, node->name_text_gfx_id);
  node->name_text_offset = v3(-name_text_size.x * 0.5, -0.01, node->graph_node->coord.size.y * 0.5 + 0.1);
  V2 proto_text_size = gfx_get_text_size(editor->gfx, node->proto_text_gfx_id);
  node->proto_text_offset = v3(-proto_text_size.x * 0.5, -0.01, node->graph_node->coord.size.y * 0.5 - 0.2);

  for (Ed_Pin *pin = node->pins_head; pin; pin = pin->next) {
    V2 text_size = gfx_get_text_size(editor->gfx, pin->text_gfx_id);
    if (pin->inout == EdPin_In) {
      pin->text_offset.x = 0.1;
    } else {
      pin->text_offset.x = -(text_size.x + 0.1);
    }
    pin->text_offset.y = -text_size.y * 0.5f;
  }
}

// pin_rel_idx = index of the pin relative to Ins or Outs
// pin_id = absolute pin index in the node
internal
Ed_Pin *editor_create_pin(Editor *editor, Ed_Node *owner, Graph_Pin from_pin, Ed_Pin_InOut inout, 
                          u16 pin_rel_idx, u16 n_pins, Graph_Pin_Id pin_id)
{  
  Ed_Pin *pin;
  if (owner) {
    editor_alloc_obj(Ed_Pin, pin, editor->ed_graph_arena, 
                     owner->pins_head, owner->pins_tail, 
                     editor->ed_graph.pins_free);
  } else {
    editor_alloc_obj(Ed_Pin, pin, editor->ed_graph_arena,
                     editor->ed_graph.epoints_head, editor->ed_graph.epoints_tail,
                     editor->ed_graph.pins_free);
  }
  pin->owner = owner;
  pin->inout = inout;
  pin->pin_id = pin_id;
  pin->type = from_pin.type;

  Cpu_Instance_Data pin_idata = {};
  pin_idata.scale = PIN_SCALE;
  pin_idata.color_a = pin_idata.color_b = graph_get_pin_color(pin->type);
  pin->pin_gfx_id = gfx_add_quad(editor->gfx, pin_idata);

  Char_Size pin_char_size = 14;
  Cpu_Instance_Data pin_text_idata = cpu_inst_data_default();
  pin_text_idata.color_a = owner ? v4(0, 0, 0, 1) : v4(1, 1, 1, 1);
  editor_push_pending_text(editor, from_pin.name, pin_char_size, pin_text_idata, &pin->text_gfx_id);

  // if this pin is owned by a node, adjust its offset relative to it.
  if (owner) {
    V2 node_size = owner->graph_node->coord.size;
    f32 y_delta = node_size.y / (n_pins + 1);
    pin->offset.z = node_size.y * 0.5 - (pin_rel_idx + 1) * y_delta;
    if (inout == EdPin_In) {
      pin->offset.x = -node_size.x * 0.5;
    } else {
      pin->offset.x = node_size.x * 0.5;
    }
    pin->offset.y = -0.01; // to avoid z fighting
  }

  return pin;
}

internal
void editor_destroy_pin(Editor *editor, Ed_Pin *pin)
{  
  gfx_remove_quad(editor->gfx, pin->pin_gfx_id);
  gfx_remove_text(editor->gfx, pin->text_gfx_id);
  pop_from_dll_add_to_free(pin->owner->pins_head, pin->owner->pins_tail, pin, editor->ed_graph.pins_free);
}

internal
Ed_Node *editor_create_node_from_existing(Editor *editor, Graph_Node *from_node)
{
  Temp s = scratch_begin(0, 0);

  Ed_Node *node;
  editor_alloc_obj(Ed_Node, node, editor->ed_graph_arena, 
                   editor->ed_graph.nodes_head, editor->ed_graph.nodes_tail, 
                   editor->ed_graph.nodes_free);
  node->graph_node = from_node;

  VERBOSE_TAG("Editor", "Creating node at %f,%f,%f", 
            from_node->coord.pos.x, from_node->coord.pos.y, from_node->coord.pos.z);

  // Adjust node height depending on pin number
  Graph_Node_Proto *proto = graph_get_node_proto(editor->graph, from_node->proto);
  f32 min_height_per_pin = PIN_SCALE.z + 0.02f;
  f32 min_height_needed = min_height_per_pin * Max(proto->n_inputs, proto->n_outputs);
  node->graph_node->coord.size.y = Max(node->graph_node->coord.size.y, min_height_needed);

  // Create bg
  Cpu_Instance_Data bg_data = cpu_inst_data_default();
  bg_data.color_a = v4(0.4, 0.9, 0.3, 1);
  node->bg_gfx_id = gfx_add_quad(editor->gfx, bg_data);

  // Create node name
  Cpu_Instance_Data text_data = bg_data;
  text_data.color_a = v4(1, 1, 1, 1);
  Char_Size node_name_char_size = 30;
  editor_push_pending_text(editor, from_node->name, node_name_char_size, text_data, &node->name_text_gfx_id);

  // Create node proto text
  text_data.color_a = v4(0, 0, 0, 1);
  Char_Size node_proto_char_size = 15;
  editor_push_pending_text(editor, proto->name, node_proto_char_size, text_data, &node->proto_text_gfx_id);

  /// Create node pins
  // @Robustness: this currently assumes that all `in` nodes come before `out` nodes (which currently should be consistent
  // with how we construct the node protos, but it's maybe not very robust)
  for (u32 i = 0; i < proto->n_inputs; ++i)
    editor_create_pin(editor, node, proto->inputs[i], EdPin_In, i, proto->n_inputs, i);

  for (u32 i = 0; i < proto->n_outputs; ++i)
    editor_create_pin(editor, node, proto->outputs[i], EdPin_Out, i, proto->n_outputs, proto->n_inputs + i);

  hashmap_add(&editor->ed_graph.node_map, &node->graph_node->id, &node);

  scratch_end(s);
  return node;
}

internal
Ed_Node *editor_create_new_node(Editor *editor, V3 position, Quat rotation, String8 node_name, Graph_Node_Proto_Id proto_id)
{
  Graph_Node *gnode = graph_create_node(editor->graph);
  gnode->name = node_name;
  gnode->coord.pos = position;
  gnode->coord.rot = rotation;
  gnode->coord.size = v2(1.5, 1);
  gnode->coord.scale = 1;
  gnode->proto = proto_id;

  if (gnode->proto >= editor->graph->n_node_protos) {
    // set to invalid proto
    gnode->proto = 0;
  }

  Temp scratch = scratch_begin(0, 0);

  Graph_Node_Proto *proto = graph_get_node_proto(editor->graph, gnode->proto);
  u32 n_tot_texts = proto->n_inputs + proto->n_outputs + 2; // + 2 for node name and proto
  editor_prepare_pending_texts(scratch.arena, editor, n_tot_texts);
  
  Ed_Node *node = editor_create_node_from_existing(editor, gnode);

  editor_commit_pending_texts(editor);
  editor_adjust_text_offsets(editor, node);
  editor_apply_node_transform(editor, node);

  scratch_end(scratch);
  return node;
}

internal
void editor_destroy_node_internal(Editor *editor, Ed_Node *node)
{
  Ed_Graph *graph = &editor->ed_graph;
  
  V3 pos = node->graph_node->coord.pos;
  DEBUG_TAG("Editor", "destroying node_id %lu, gfx_id bg: %lu, text: %lu at %.2f,%.2f,%.2f", 
           node->graph_node->id, node->bg_gfx_id, node->name_text_gfx_id, pos.x, pos.y, pos.z);

  // destroy all links
  for (Ed_Link *link = node->in_links_head; link; ) {
    Ed_Link *l = link;
    link = link->to_next;
    editor_destroy_link(editor, l);
  }
  for (Ed_Link *link = node->out_links_head; link; ) {
    Ed_Link *l = link;
    link = link->from_next;
    editor_destroy_link(editor, l);
  }

  // destroy all pins
  for (Ed_Pin *pin = node->pins_head; pin; ) {
    Ed_Pin *p = pin;
    pin = pin->next;
    editor_destroy_pin(editor, p);
  }

  // remove from gfx instances
  gfx_remove_quad(editor->gfx, node->bg_gfx_id);
  gfx_remove_text(editor->gfx, node->name_text_gfx_id);
  gfx_remove_text(editor->gfx, node->proto_text_gfx_id);

  hashmap_remove(&editor->ed_graph.node_map, &node->graph_node->id);

  // remove from script graph
  graph_remove_node(editor->graph, node->graph_node);
  pop_from_dll_add_to_free(graph->nodes_head, graph->nodes_tail, node, graph->nodes_free);

  gfx_debug_print_instance_ids(&editor->gfx->instances, Log_Verbose);
  editor_debug_print_ed_graph(editor, Log_Verbose);

  if (node == graph->selected_node)
    graph->selected_node = NULL;
}

internal
void editor_destroy_node(Editor *editor, Graph_Node_Id node_id)
{
  Ed_Node *node = editor_node_from_id(editor, node_id);
  if (node)
    editor_destroy_node_internal(editor, node);
  else
    WARN_TAG("Editor", "Tried to remove inexistent node %lu", node_id);
}

internal
Ed_Node *editor_create_node_at_mouse_pos(Editor *editor, V2 mouse_pos)
{
  f32 unproj_depth = 1.5f;
  V3 world_pos = unproject_screen_pos(mouse_pos, &editor->gfx->inv_view_proj, &editor->gfx->proj, editor->gfx->viewport_px, unproj_depth);
  Quat world_rot = quat_from_euler(editor->camera.pitch, 0.f, editor->camera.yaw);
  // #TODO
  Graph_Node_Proto_Id node_proto = 0;
  Ed_Node *node = editor_create_new_node(editor, world_pos, world_rot, str8("Test Node"), node_proto);
  return node;
}

internal
void editor_clear(Editor *editor)
{
  Ed_Graph *ed_graph = &editor->ed_graph;
  for (Ed_Node *node = ed_graph->nodes_head; node; ) {
    Ed_Node *n = node;
    node = node->next; // do this before destroying node!
    editor_destroy_node_internal(editor, n);
  }
  zero_struct(ed_graph);
  arena_pop_to(editor->ed_graph_arena, 0);
  editor_graph_init(&editor->ed_graph, editor->ed_graph_arena);
}

// Loads `graph` into the editor. This will clone the graph and not modify
// or refer to the original in any way.
internal
void editor_set_graph(Editor *editor, Script_Graph *graph)
{
  u64 start_ns = os_clock_time_ns();

  // Copy the new graph both to the orig graph arena and to the
  // mutable graph arena.
  if (graph != editor->orig_graph) {
    // Only clone the graph if this is not already the orig_graph (which we know is immutable)
    arena_pop_to(editor->orig_graph_arena, 0);
    editor->orig_graph = graph_clone(editor->orig_graph_arena, graph);
  }
  
  arena_pop_to(editor->graph_arena, 0);
  DEBUG_TAG("Editor", "Cloning graph...");
  editor->graph = graph_clone(editor->graph_arena, graph);

  Temp scratch = scratch_begin(0, 0);

  // Prepare enough scratch space to hold all texts; they will be submitted to the GPU altogether. 
  // Pushing each text to Vulkan separately incurs a MASSIVE performance penalty, so we don't do it.
  u32 n_tot_texts = 0; 
  for (Graph_Node *node = editor->graph->nodes.head; node; node = node->next) {
    n_tot_texts += 2; // for the node title and node proto name
    Graph_Node_Proto *proto = graph_get_node_proto(editor->graph, node->proto);
    n_tot_texts += proto->n_inputs + proto->n_outputs;
  }
  n_tot_texts += editor->graph->n_entrypoints;
  n_tot_texts += editor->graph->n_exitpoints;
  editor_prepare_pending_texts(scratch.arena, editor, n_tot_texts);

  f32 min_node_x = FLT_MAX;
  f32 max_node_x = FLT_MIN;
  
  // Load nodes
  u32 n_nodes = 0;
  for (Graph_Node *node = editor->graph->nodes.head; node; node = node->next) {
    editor_create_node_from_existing(editor, node);
    min_node_x = Min(min_node_x, node->coord.pos.x);
    max_node_x = Min(max_node_x, node->coord.pos.x);
    ++n_nodes;
    if (editor->graph->nodes.count > 1000 && (n_nodes % 1000 == 0))
      DEBUG_TAG("Editor", "loaded %u / %u nodes...", n_nodes, editor->graph->nodes.count);
  }

  // Load entry/exitpoints
  for (u32 i = 0; i < editor->graph->n_entrypoints; ++i) {
    editor_create_pin(editor, NULL, editor->graph->entrypoints[i], EdPin_Out, 0, 0, 0);
  }
  for (u32 i = 0; i < editor->graph->n_exitpoints; ++i) {
    editor_create_pin(editor, NULL, editor->graph->exitpoints[i], EdPin_In, 0, 0, 0);
  }

  editor_commit_pending_texts(editor);

  // Now that we know the texts sizes, adjust the pin offsets. This needs to be done
  // after committing the pending texts.
  for (Ed_Node *node = editor->ed_graph.nodes_head; node; node = node->next) {
    editor_adjust_text_offsets(editor, node);
    editor_apply_node_transform(editor, node);
  }

  // adjust entry/exitpoints positions
  f32 entrypoint_z_off = 0.f, exitpoint_z_off = 0.f;
  for (Ed_Pin *pin = editor->ed_graph.epoints_head; pin; pin = pin->next) {
    Cpu_Instance_Data *pin_idata = gfx_get_instance_data(editor->gfx, pin->pin_gfx_id);
    Cpu_Instance_Data *text_idata = gfx_get_instance_data(editor->gfx, pin->text_gfx_id);

    if (pin->inout == EdPin_Out) {
      pin_idata->pos = v3(min_node_x - 2.f, 0.f, 2.f + entrypoint_z_off);
      entrypoint_z_off -= 0.2f;
    } else {
      pin_idata->pos = v3(max_node_x + 2.f, 0.f, 2.f + exitpoint_z_off);
      exitpoint_z_off -= 0.2f;
    }
    // Save pos to offset to speed up raycasting
    pin->offset = pin_idata->pos;

    V2 text_size = gfx_get_text_size(editor->gfx, pin->text_gfx_id);
    V3 text_offset = {};
    if (pin->inout == EdPin_In) {
      text_offset.x = 0.1;
    } else {
      text_offset.x = -(text_size.x + 0.1);
    }
    text_offset.z = -text_size.y * 0.5f;
    text_idata->pos = v3_add(pin_idata->pos, text_offset);
  }

  // Load links. Do this after repositioning the epoints so the splines end up in the right place
  u32 n_links = 0;
  for (Graph_Link_Node *link = editor->graph->links.head; link; link = link->next) {
    editor_create_link_from_existing(editor, link);
    ++n_links;
    if (editor->graph->links.count > 1000 && (n_links % 1000 == 0))
        DEBUG_TAG("Editor", "loaded %u / %u links...", n_links, editor->graph->links.count);
  }

  u64 end_ns = os_clock_time_ns();
  INFO_TAG("Editor", "Took %.2f ms to load %u nodes and %u links from %s", (end_ns - start_ns) * 1e-6f, n_nodes, n_links, cstr(graph->name));

  gfx_debug_print_instance_ids(&editor->gfx->instances, Log_Verbose);
  editor_debug_print_ed_graph(editor, Log_Verbose);

  scratch_end(scratch);
}

internal
void editor_reset_graph_to_orig(Editor *editor)
{
  editor_clear(editor);
  editor_set_graph(editor, editor->orig_graph);

  editor_debug_print_ed_graph(editor, Log_Verbose);
}

// returns true if any input was handled
internal
b8 editor_handle_inputs(Editor *editor, User_Input *input)
{
  b8 handled = false;

  // DEBUG
  if (input->key_state[Key_R] & KEY_STATE_JUST_PRESSED) {
    editor_reset_graph_to_orig(editor);
  }

  // Toggle projection mode
  if (input->key_state[Key_O] & KEY_STATE_JUST_PRESSED) {
    editor->gfx->config.proj_mode = !editor->gfx->config.proj_mode;
    if (editor->gfx->config.proj_mode == Proj_Ortho) {
      // reset the camera orientation
      editor->camera.yaw = 0;
      editor->camera.pitch = 0;
      // make sure the camera is in front of the origin
      editor->camera.pos.y = -1.f;
    }
    handled = true;
  }

  if (editor->graph) {
    V2 mouse_pos = v2_from_v2i(input->mouse_pos);

    if (input->mouse_btn_state[MouseBtn_Left] & MOUSE_BTN_STATE_JUST_PRESSED) {
      if (input->ctrl) {
        /// Create new node
        editor_create_node_at_mouse_pos(editor, mouse_pos);
      } else {
        /// Try select node
        Ed_Select_Result sel = editor_select_node_or_pin(editor, mouse_pos);

        Ed_Node *prev_node = editor->ed_graph.selected_node;
        if (prev_node && prev_node != sel.node) {
          Ed_Node_Inst_Data idata = editor_get_node_instance_data(editor, prev_node);
          idata.bg->color_b = v4(1, 1, 1, 1);
          idata.name_text->color_a = v4(1, 1, 1, 1);
        }
        if (sel.node) {
          Ed_Node_Inst_Data idata = editor_get_node_instance_data(editor, sel.node);
          idata.bg->color_b = v4(0.4, 0.3, 0.9, 1);
          idata.name_text->color_a = v4(0.7, 0.6, 0.0, 1);
        }
        editor->ed_graph.selected_node = sel.node;

        Ed_Pin *prev_pin = editor->ed_graph.selected_pin;
        if (prev_pin && prev_pin != sel.pin) {
          Cpu_Instance_Data *prev_idata = gfx_get_instance_data(editor->gfx, prev_pin->pin_gfx_id);
          prev_idata->color_a = prev_idata->color_b = graph_get_pin_color(prev_pin->type);
        }
        if (sel.pin) {
          Cpu_Instance_Data *idata = gfx_get_instance_data(editor->gfx, sel.pin->pin_gfx_id);
          idata->color_a = v4(1, 1, 0, 1);
          idata->color_b = v4(1, 0.7, 0, 1);

          assert(!editor->pending_link);
          editor->pending_link = editor_create_pending_link(editor, sel.pin, mouse_pos);
        }
        editor->ed_graph.selected_pin = sel.pin;
      }
      handled = true;
    } else if (input->mouse_btn_state[MouseBtn_Left] & MOUSE_BTN_STATE_JUST_RELEASED) {
      if (editor->pending_link) {
        // See if we're releasing the link on a pin, in which case consolidate the link.
        // Otherwise, destroy the pending link. (TODO: show popup menu to create a new node)
        // FIXME: this is broken when connecting an entrypoint with an exitpoint!
        Ed_Pin *fixed_pin = editor->pending_link->from ? editor->pending_link->from : editor->pending_link->to;
        assert(fixed_pin);
        Ed_Select_Result sel = editor_select_node_or_pin(editor, mouse_pos);
        Ed_Link *new_link = NULL;
        if (sel.pin && sel.pin->inout != fixed_pin->inout) {
          if (editor->pending_link->from)
            new_link = editor_create_new_link(editor, editor->pending_link->from, sel.pin);
          else
            new_link = editor_create_new_link(editor, sel.pin, editor->pending_link->to);
        }

        // unlink the nodes from the pending link before destroying it, otherwise it will also
        // remove the newly formed link from the originating node
        editor->pending_link->from = editor->pending_link->to = NULL;
        editor_destroy_link(editor, editor->pending_link);
        editor->pending_link = NULL;

        // force to redraw the spline
        if (new_link) {
          V3 cpoints[4];
          editor_calc_link_control_points(editor, new_link, cpoints);
          gfx_update_spline(editor->gfx, new_link->spline_id, cpoints);
        }
      }
    }

    if (input->ctrl && input->mouse_btn_state[MouseBtn_Right] & MOUSE_BTN_STATE_JUST_PRESSED) {
      Ed_Node *node = editor_select_node_or_pin(editor, mouse_pos).node;
      if (node)
        editor_destroy_node(editor, node->graph_node->id);
    }

    if (!input->ctrl && input->mouse_btn_state[MouseBtn_Right] & MOUSE_BTN_STATE_JUST_PRESSED) {
      // TODO: show a menu with all the node protos to select one
    }
  }

  return handled;
}

internal
void editor_update_and_draw(Editor *editor, User_Input *input, f32 dt)
{
  Gfx *gfx = editor->gfx;
  Camera *camera = &editor->camera;

  b8 dragging = !input->ctrl && (input->mouse_btn_state[MouseBtn_Left] & MOUSE_BTN_STATE_IS_DOWN);
  b8 dragging_node = dragging && editor->ed_graph.selected_node;
  b8 dragging_pin = dragging && editor->ed_graph.selected_pin;

  Camera_Move_Params params = {
    .pixels_per_meter = gfx->config.pixels_per_meter,
    .is_ortho = gfx->config.proj_mode == Proj_Ortho,
    .enable_dragging = !dragging_node && !dragging_pin
  };
  camera_move(camera, &gfx->inv_view, input, dt, params);
  gfx_update_matrices(gfx, camera);

  V2 mpos = v2_from_v2i(input->mouse_pos);
  // Drag node
  if (dragging_node) {
    Ed_Node *node = editor->ed_graph.selected_node;
    // if (!editor->was_dragging_node) {
    //   editor->node_dist_at_drag_start = v3_distance(camera->pos, node->graph_node->coord.pos);
    // }
    // FIXME: node should not be recentered on the cursor
    // f32 dist = sqrtf(Square(editor->node_dist_at_drag_start) - Square(editor->mouse_offset_from_center_at_drag_start.x));
    Rect vp = gfx->viewport_px;
    V2 mouse_off_center = v2(mpos.x * 2 / vp.width - 1, mpos.y * 2 / vp.height - 1);
    f32 dist = editor->node_dist_at_drag_start;
    // FIXME: this is broken
    if (gfx->config.proj_mode == Proj_Persp) 
      dist /= sqrtf(1 + v2_len2(mouse_off_center) / Square(gfx->proj_distance));
    V3 pos = unproject_screen_pos(mpos, &gfx->inv_view_proj, &gfx->proj, gfx->viewport_px, dist);
    pos = v3_add(pos, v3_x0z(editor->mouse_offset_from_center_at_drag_start));
    // INFO("pos: %f,%f,%f, dist: %f", pos.x, pos.y, pos.z, editor->node_dist_at_drag_start);
    node->graph_node->coord.pos = pos;
    node->graph_node->coord.rot = quat_from_euler(camera->pitch, 0.f, camera->yaw);
    editor_apply_node_transform(editor, node);
  } else if (dragging_pin) {
    editor_update_pending_link(editor, mpos);    
  }
  editor->was_dragging_node = dragging_node;
}

