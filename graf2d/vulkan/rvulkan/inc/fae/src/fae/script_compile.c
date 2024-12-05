#define VM_INSTRS_PER_CHUNK (4096 / sizeof(Vm_Instr))

typedef struct Vm_Instr_Chunk {
  struct Vm_Instr_Chunk *next;
  Vm_Instr instrs[VM_INSTRS_PER_CHUNK];
  u32 count;
} Vm_Instr_Chunk;

typedef struct {
  Fae_Ast_Decl *var;
  Vm_Addr addr;
} LValue;

typedef struct LValue_Node {
  struct LValue_Node *parent;
  LValue *lvalue; // refers to a value in lvalue_map
} LValue_Node;

typedef struct {
  // arena that should have the same lifetime as the compiler (externally managed)
  Arena *arena;
  Vm_Instr_Chunk *chunks_head, *chunks_tail;
  u64 n_instrs;

  // stack of lvalues being assigned
  LValue_Node *cur_lvalue;
  LValue_Node *cur_lvalue_free;

  Vm_Reg_Id latest_tmp_reg;
  Vm_Addr next_addr_available;

  // map { Fae_Ast_Decl* => LValue }
  Hash_Map lvalue_map;
  b8 reg_used[Reg_COUNT - 1];
} Fae_Compiler;

internal
u64 fae_type_size(Fae_Type type)
{
  switch (type) {
  case FaeType_U64:
  case FaeType_I64:
  case FaeType_F64:
    return 8;
  case FaeType_Void:
  case FaeType_Fn: // @Temporary
    return 0;
  default:
    assert(false);
  }
  return 0;
}

internal
void faec_push_instr(Fae_Compiler *comp, Vm_Instr instr)
{
  Temp s = scratch_begin(&comp->arena, 1);
  DEBUG_TAG("Compiler", "pushing instr: %s", cstr(vm_pretty_print_instr(s.arena, instr)));
  scratch_end(s);

  if (comp->chunks_tail->count == countof(comp->chunks_tail->instrs)) {
    // push new chunk
    Vm_Instr_Chunk *newchk = arena_push(Vm_Instr_Chunk, comp->arena);
    push_to_sll(comp->chunks_head, comp->chunks_tail, newchk);
  }
  comp->chunks_tail->instrs[comp->chunks_tail->count++] = instr;
  ++comp->n_instrs;
}

internal
Vm_Reg_Id faec_alloc_reg(Fae_Compiler *comp)
{
  for (u32 i = 0; i < countof(comp->reg_used); ++i) {
    if (!comp->reg_used[i]) {
      comp->reg_used[i] = true;
      return (Vm_Reg_Id)(i + 1);
    }
  }
  return Reg_INVALID;
}

internal
void faec_free_reg(Fae_Compiler *comp, Vm_Reg_Id reg)
{
  assert(comp->reg_used[reg - 1]);
  comp->reg_used[reg - 1] = false;
}

internal
Vm_Reg_Id faec_alloc_tmp_reg(Fae_Compiler *comp)
{
  Vm_Reg_Id tmp = faec_alloc_reg(comp);
  comp->latest_tmp_reg = tmp;
  DEBUG_TAG("Compiler", "alloc %s as tmp reg", vm_reg_id_name(tmp));
  return tmp;
}

internal
Vm_Reg_Id faec_get_tmp_reg(Fae_Compiler *comp)
{
  assert(comp->latest_tmp_reg != Reg_INVALID);
  return comp->latest_tmp_reg;
}

internal
void faec_free_tmp_reg(Fae_Compiler *comp)
{
  faec_free_reg(comp, faec_get_tmp_reg(comp));
  comp->latest_tmp_reg = Reg_INVALID;
}

internal
LValue *faec_find_lvalue(Fae_Compiler *comp, Fae_Ast_Decl *decl)
{
  // @Temporary and dumb routine that basically maps variables to memory locations.
  // Right now we simply assign a new memory address to any new variable declaration we
  // see and never forget them.
  // I guess we should at the very least remove mappings when we exit a scope?
  
  LValue *lval = hashmap_find(LValue, &comp->lvalue_map, &decl);
  if (!lval) {
    LValue lvalue = {};
    lvalue.var = decl;
    lvalue.addr = comp->next_addr_available;
    DEBUG_TAG("Compiler", "&%s = %" PRIu64, cstr(decl->name), lvalue.addr);
    comp->next_addr_available += fae_type_size(decl->type);
    Hash_Node *hnode = hashmap_add(&comp->lvalue_map, &decl, &lvalue);
    return (LValue *)hashmap_get_node_val(&comp->lvalue_map, hnode);
  } else {
    return lval;
  }
}

internal
void faec_pop_lvalue(Fae_Compiler *comp)
{
  assert(comp->cur_lvalue);
  INFO_TAG("Compiler", "popping lvalue %s", cstr(comp->cur_lvalue->lvalue->var->name));
  LValue_Node *parent_node = comp->cur_lvalue->parent;
  comp->cur_lvalue->parent = comp->cur_lvalue_free;
  comp->cur_lvalue_free = comp->cur_lvalue;
  comp->cur_lvalue = parent_node;
}

internal
void faec_push_lvalue(Fae_Compiler *comp, LValue *lvalue)
{
  INFO_TAG("Compiler", "pushing lvalue %s", cstr(lvalue->var->name));
  LValue_Node *node;
  if (comp->cur_lvalue_free) {
    node = comp->cur_lvalue_free;
    comp->cur_lvalue_free = comp->cur_lvalue_free->parent;
  } else {
    Arena *arena = comp->arena;
    node = arena_push(LValue_Node, arena);
  }
  node->lvalue = lvalue;
  // stack-like chain: new node points to the last added
  node->parent = comp->cur_lvalue;
  comp->cur_lvalue = node;
}

internal
void faec_compile_expr(Fae_Compiler *comp, Fae_Ast_Expr *expr)
{
  Temp scratch = scratch_begin(&comp->arena, 1);

  INFO_TAG("Compiler", "compiling expr %s", cstr(fae_pretty_print_expr(scratch.arena, expr)));
  switch (expr->type) {
  case FaeExpr_Constant: {
    if (!comp->cur_lvalue) {
      WARN_TAG("Compiler", "expression has no effect: %s", cstr(fae_pretty_print_expr(scratch.arena, expr)));
    } else {
      Vm_Reg_Id reg = faec_alloc_tmp_reg(comp);
      switch (expr->constant.type) {
      case FaeType_I64:
        faec_push_instr(comp, (Vm_Instr){ .type = Op_MovLit64, .lit_i64 = expr->constant.value_i64, .reg1 = reg });
        break;
      case FaeType_U64:
        faec_push_instr(comp, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = expr->constant.value_u64, .reg1 = reg });
        break;
      case FaeType_F64:
        faec_push_instr(comp, (Vm_Instr){ .type = Op_MovLit64, .lit_f64 = expr->constant.value_f64, .reg1 = reg });
        break;
      default:
        assert(false);
      }
    }
  } break;

  case FaeExpr_Variable: {
    if (!comp->cur_lvalue) {
      WARN_TAG("Compiler", "expression has no effect: %s", cstr(fae_pretty_print_expr(scratch.arena, expr)));
    } else {
      LValue *lval = faec_find_lvalue(comp, expr->variable);
      Vm_Reg_Id tmp = faec_alloc_tmp_reg(comp);
      faec_push_instr(comp, (Vm_Instr){ .type = Op_MovCur, .lit_u64 = lval->addr });
      faec_push_instr(comp, (Vm_Instr){ .type = Op_Pop64, .reg1 = tmp });
    }
  } break;

  default: {
    // TODO
    faec_push_instr(comp, (Vm_Instr){ .type = Op_Noop });
  } break;
  }
  
  scratch_end(scratch);
}

internal
void faec_compile_stmt(Fae_Compiler *comp, Fae_Ast_Stmt *stmt)
{
  switch (stmt->type) {
  case FaeStmt_Assignment: {
    LValue *lvalue = faec_find_lvalue(comp, stmt->assign.left);
    faec_push_lvalue(comp, lvalue);
    // pushes value to tmp reg
    faec_compile_expr(comp, stmt->assign.right);

    Vm_Reg_Id tmp = faec_get_tmp_reg(comp);
    faec_push_instr(comp, (Vm_Instr){ .type = Op_MovCur, .lit_u64 = lvalue->addr });
    faec_push_instr(comp, (Vm_Instr){ .type = Op_Push64, .reg1 = tmp });

    faec_free_tmp_reg(comp);
    faec_pop_lvalue(comp);

    if (comp->cur_lvalue) {
      // save our own value to the tmp register so that our parent lvalue can grab it
      tmp = faec_alloc_tmp_reg(comp);
      faec_push_instr(comp, (Vm_Instr){ .type = Op_Pop64, .reg1 = tmp });
    }
  } break; 

  default: {
    // TODO
  } break;
  }
}

internal
Fae_Compiler faec_init(Arena *arena)
{
  Fae_Compiler comp = {};
  comp.arena = arena;
  comp.lvalue_map = hashmap_init_default(Fae_Ast_Decl*, LValue, arena, 256);
  comp.next_addr_available = 1000;

  Vm_Instr_Chunk *chunk = arena_push(Vm_Instr_Chunk, arena);
  push_to_sll(comp.chunks_head, comp.chunks_tail, chunk);

  return comp;
}

internal
// u64 fae_compile_script(Arena *arena, Fae_Script *script, Vm_Instr **instrs)
Fae_Compiler fae_compile_script(Arena *arena, Fae_Script *script, Vm_Instr **instrs)
{
  // Temp scratch = scratch_begin(&arena, 1);
  
  Fae_Compiler compiler = faec_init(/*scratch.*/arena);
  for (u64 i = 0; i < script->root_block.n_stmts; ++i) {
    faec_compile_stmt(&compiler, &script->root_block.stmts[i]);
  }

  faec_push_instr(&compiler, (Vm_Instr){ .type = Op_Die });

  if (compiler.n_instrs) {
    *instrs = arena_push_array_nozero(Vm_Instr, arena, compiler.n_instrs);
    u64 pos = 0;
    for (Vm_Instr_Chunk *ch = compiler.chunks_head; ch; ch = ch->next) {
      memcpy(*instrs + pos, ch->instrs, ch->count * sizeof(Vm_Instr));
      pos += ch->count;
    }
  }

  // scratch_end(scratch);
  // return compiler.n_instrs;
  return compiler;
}
