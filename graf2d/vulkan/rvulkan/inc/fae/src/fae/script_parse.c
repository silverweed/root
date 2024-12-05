typedef enum {
  FaeType_INVALID,
  FAE_LANG_TYPES(FaeType),
  FaeType_Void,
  FaeType_Fn, // @Incomplete: should include signature
  FaeType_COUNT
} Fae_Type;

internal
const char *const g_Fae_Type_str[] = {
  "(invalid)",
  FAE_LANG_TYPE_STRS,
  "void",
  "fn",
  "(count)"
};

internal
String8 fae_type_str(Fae_Type type)
{
  if (type >= FaeType_LANG_FIRST && type < FaeType_COUNT) 
    return str8(g_Fae_Type_str[type]);
  return str8("(invalid)");
}

internal
Fae_Type fae_type_from_name(String8 name)
{
  for (u64 i = FaeType_LANG_FIRST; i < FaeType_COUNT; ++i)
    if (str8_eqc(name, g_Fae_Type_str[i]))
      return (Fae_Type)i;

  return FaeType_INVALID;
}

typedef struct {
  Fae_Type type;
  union {
    u64 value_u64;
    i64 value_i64;
    f64 value_f64;
  };
} Fae_Ast_Constant;

typedef struct {
  String8 fn_name;
  struct Fae_Ast_Expr *args;
  u32 n_args;  
  Fae_Type ret_type; // duplicated from Fae_Ast_Decl for conveniency
} Fae_Ast_Function_Call;

typedef struct {
  struct Fae_Ast_Decl *left;
  struct Fae_Ast_Expr *right;
} Fae_Ast_Assignment;

typedef struct {
  Fae_Type to;
  struct Fae_Ast_Expr *expr;
} Fae_Ast_Type_Cast;

typedef struct {
  struct Fae_Ast_Expr *cond;
  struct Fae_Ast_Stmt *then;
  struct Fae_Ast_Stmt *else_;
} Fae_Ast_If;

typedef enum {
  BinOp_Add,
  BinOp_Sub,
  BinOp_Mul,
  BinOp_Div,
  BinOp_Eq,
  BinOp_Ne,
  BinOp_Le,
  BinOp_Lt,
  BinOp_Ge,
  BinOp_Gt,
  BinOp_FnCall,
  BinOp_ArraySubscript,
  BinOp_Cast,
  BinOp_COUNT
} Fae_Bin_Op_Type;

typedef struct {
  struct Fae_Ast_Expr *lhs;
  struct Fae_Ast_Expr *rhs;
  Fae_Bin_Op_Type op_type;
} Fae_Ast_Bin_Op;

typedef enum {
  FaeExpr_INVALID,
  FaeExpr_Constant,
  FaeExpr_Function_Call,
  FaeExpr_Variable,
  FaeExpr_Assignment,
  FaeExpr_Type_Cast,
  FaeExpr_Bin_Op,
  FaeExpr_COUNT
} Fae_Ast_Expr_Type;

typedef struct Fae_Ast_Expr {
  Fae_Ast_Expr_Type type;
  union {
    Fae_Ast_Constant constant;   
    struct Fae_Ast_Decl *variable;
    Fae_Ast_Function_Call fn_call;
    Fae_Ast_Type_Cast cast;
    Fae_Ast_Bin_Op bin_op;
  };
  u16 start_col;
  u16 end_col;
} Fae_Ast_Expr;

typedef enum {
  FaeStmt_Decl,
  FaeStmt_Assignment,
  FaeStmt_Block,
  FaeStmt_Return,
  FaeStmt_If
} Fae_Ast_Stmt_Type;

typedef struct {
  struct Fae_Ast_Stmt *stmts;
  u64 n_stmts;
} Fae_Ast_Block;

typedef struct Fae_Ast_Stmt {
  Fae_Ast_Stmt_Type type;
  union {
    struct Fae_Ast_Decl *decl;
    Fae_Ast_Assignment assign;
    Fae_Ast_Block block;
    Fae_Ast_Expr *returned;
    Fae_Ast_If if_;
  };
} Fae_Ast_Stmt;

enum {
  FaeDeclFlag_NONE = 0x0,
  FaeDeclFlag_In   = 0x1,
  FaeDeclFlag_Out  = 0x2
};

typedef struct {
  Fae_Type ret_type;
  struct Fae_Ast_Decl *args;
  u32 n_args;
} Fae_Function_Signature;

typedef struct Fae_Ast_Decl {
  struct Fae_Ast_Decl *next;

  struct Fae_Scope *scope;

  String8 name;
  Fae_Type type;
  Fae_Ast_Expr *assign;
  u32 declared_at_line;
  // these are indices relative to the beginning of the file
  u64 decl_start;
  u64 decl_end;
  u64 flags;

  // These are non-null for functions only
  Fae_Function_Signature *signature;
  Fae_Ast_Block body;
} Fae_Ast_Decl;

typedef struct Fae_Scope {
  struct Fae_Scope *next;
  struct Fae_Scope *parent;
  
  Fae_Ast_Decl *vars_head, *vars_tail;
  Fae_Ast_Decl *funcs_head, *funcs_tail;
  // { name => Fae_Ast_Decl* }
  Hash_Map decl_map;
} Fae_Scope;

typedef struct {
  // persistent linked list with all the scopes in the program.
  // scopes_head is the outermost (global) scope
  Fae_Scope *scopes_head, *scopes_tail; 
  Fae_Ast_Block root_block;
} Fae_Script;

// Lexing
enum {
  FaeLexTok_Keyword_Var = Lex_FIRST,
  FaeLexTok_Keyword_In,    
  FaeLexTok_Keyword_Out,   
  FaeLexTok_Keyword_Fn,    
  FaeLexTok_Keyword_Cast,  
  FaeLexTok_Keyword_If,    
  FaeLexTok_Keyword_Else,  
  FaeLexTok_Keyword_Return,
  FaeLexTok_Colon,         
  FaeLexTok_SemiColon,     
  FaeLexTok_BraceOpen,     
  FaeLexTok_BraceClose,    
  FaeLexTok_ParenOpen,     
  FaeLexTok_ParenClose,    
  FaeLexTok_SquareOpen,    
  FaeLexTok_SquareClose,   
  FaeLexTok_Period,        
  FaeLexTok_Comma,         
  FaeLexTok_Assign,        
  FaeLexTok_Plus,          
  FaeLexTok_Minus,         
  FaeLexTok_Asterisk,      
  FaeLexTok_Slash,         
  FaeLexTok_Less,          
  FaeLexTok_LessEqual,     
  FaeLexTok_Greater,       
  FaeLexTok_GreaterEqual,  
  FaeLexTok_NotEqual,      
  FaeLexTok_Equal,         
};

internal
Lex_Token_Mapping *get_fae_script_lex_token_mappings(Arena *arena, u32 *len)
{
  const Lex_Token_Mapping lex_mapping[] = {
    (Lex_Token_Mapping){ str8("var"),  FaeLexTok_Keyword_Var },
    (Lex_Token_Mapping){ str8("in"),   FaeLexTok_Keyword_In },
    (Lex_Token_Mapping){ str8("out"),  FaeLexTok_Keyword_Out },
    (Lex_Token_Mapping){ str8("fn"),   FaeLexTok_Keyword_Fn },
    (Lex_Token_Mapping){ str8("cast"), FaeLexTok_Keyword_Cast },
    (Lex_Token_Mapping){ str8("if"),   FaeLexTok_Keyword_If },
    (Lex_Token_Mapping){ str8("else"), FaeLexTok_Keyword_Else },
    (Lex_Token_Mapping){ str8("ret"),  FaeLexTok_Keyword_Return },
    (Lex_Token_Mapping){ str8(":"),    FaeLexTok_Colon },
    (Lex_Token_Mapping){ str8(";"),    FaeLexTok_SemiColon },
    (Lex_Token_Mapping){ str8("{"),    FaeLexTok_BraceOpen },
    (Lex_Token_Mapping){ str8("}"),    FaeLexTok_BraceClose },
    (Lex_Token_Mapping){ str8("("),    FaeLexTok_ParenOpen },
    (Lex_Token_Mapping){ str8(")"),    FaeLexTok_ParenClose },
    (Lex_Token_Mapping){ str8("["),    FaeLexTok_SquareOpen },
    (Lex_Token_Mapping){ str8("]"),    FaeLexTok_SquareClose },
    (Lex_Token_Mapping){ str8("."),    FaeLexTok_Period },
    (Lex_Token_Mapping){ str8(","),    FaeLexTok_Comma },
    (Lex_Token_Mapping){ str8("="),    FaeLexTok_Assign },
    (Lex_Token_Mapping){ str8("+"),    FaeLexTok_Plus },
    (Lex_Token_Mapping){ str8("-"),    FaeLexTok_Minus },
    (Lex_Token_Mapping){ str8("*"),    FaeLexTok_Asterisk },
    (Lex_Token_Mapping){ str8("/"),    FaeLexTok_Slash },
    (Lex_Token_Mapping){ str8("<"),    FaeLexTok_Less },
    (Lex_Token_Mapping){ str8("<="),   FaeLexTok_LessEqual },
    (Lex_Token_Mapping){ str8(">"),    FaeLexTok_Greater },
    (Lex_Token_Mapping){ str8(">="),   FaeLexTok_GreaterEqual },
    (Lex_Token_Mapping){ str8("!="),   FaeLexTok_NotEqual },
    (Lex_Token_Mapping){ str8("=="),   FaeLexTok_Equal }
  };
  Lex_Token_Mapping *returned = arena_push_array_nozero(Lex_Token_Mapping, arena, countof(lex_mapping));
  memcpy(returned, lex_mapping, sizeof(lex_mapping));
  *len = countof(lex_mapping);
  return returned;
}

internal
Fae_Bin_Op_Type fae_lex_tok_to_bin_op(Lex_Token_Type type)
{
  switch (type) {
  case FaeLexTok_Plus:         return BinOp_Add;
  case FaeLexTok_Minus:        return BinOp_Sub;
  case FaeLexTok_Asterisk:     return BinOp_Mul;
  case FaeLexTok_Slash:        return BinOp_Div;
  case FaeLexTok_Equal:        return BinOp_Eq;
  case FaeLexTok_NotEqual:     return BinOp_Ne;
  case FaeLexTok_LessEqual:    return BinOp_Le;
  case FaeLexTok_GreaterEqual: return BinOp_Ge;
  case FaeLexTok_Less:         return BinOp_Lt;
  case FaeLexTok_Greater:      return BinOp_Gt;
  case FaeLexTok_ParenOpen:    return BinOp_FnCall;
  case FaeLexTok_SquareOpen:   return BinOp_ArraySubscript;
  default:                     return BinOp_COUNT;
  }
}

typedef i16 Fae_Precedence;

internal const Fae_Precedence FAE_LOWEST_PRECEDENCE = -1000;

internal
Fae_Precedence fae_get_bin_op_precedence(Fae_Bin_Op_Type type)
{
  switch (type) {
  case BinOp_FnCall:
  case BinOp_ArraySubscript:
    return 60;

  case BinOp_Cast:
    return 50;

  case BinOp_Add: 
  case BinOp_Sub: 
  case BinOp_Mul: 
  case BinOp_Div:
    return 40;

  case BinOp_Le:
  case BinOp_Ge:
  case BinOp_Lt:
  case BinOp_Gt:
    return 10;

  case BinOp_Eq:
  case BinOp_Ne: 
    return 5;

  default:
    assert(false);
    return 1;
  }
}

typedef struct {
  Parser inner; // must be the first member!

  Arena *script_arena;
  Fae_Script *script;
  // refers to a scope inside script->scopes_head/tail LL
  Fae_Scope *cur_scope;
} Fae_Parser;

// Creates a new scope and pushes it on top of the scope stack.
internal
void fae_push_scope(Fae_Parser *parser)
{
  Fae_Scope *scope = arena_push(Fae_Scope, parser->script_arena);
  push_to_sll(parser->script->scopes_head, parser->script->scopes_tail, scope);
  scope->decl_map = hashmap_init(String8, Fae_Ast_Decl*, parser->script_arena, 32, hash_str8, hash_str8_eq);
  scope->parent = parser->cur_scope;
  parser->cur_scope = scope;
}

// Pops the top scope off the stack. Note that that scope will remain valid, it simply won't be
// accessible from parser->cur_scope anymore.
internal
void fae_pop_scope(Fae_Parser *parser)
{
  assert(parser->cur_scope);
  parser->cur_scope = parser->cur_scope->parent;
}

internal
void fae_register_var_to_scope(Fae_Scope *scope, Fae_Ast_Decl *decl)
{
  assert(!decl->signature);

  // add to decl map
  hashmap_add(&scope->decl_map, &decl->name, &decl);
  // add to scope var list
  push_to_sll(scope->vars_head, scope->vars_tail, decl);
}

internal
void fae_register_func_to_scope(Fae_Scope *scope, Fae_Ast_Decl *decl)
{
  assert(decl->signature);

  // add to decl map
  hashmap_add(&scope->decl_map, &decl->name, &decl);
  // add to scope var list
  push_to_sll(scope->funcs_head, scope->funcs_tail, decl);
}

internal
Fae_Ast_Decl *fae_find_scoped_decl(Fae_Scope *innermost_scope, String8 decl_name)
{
  for (Fae_Scope *scope = innermost_scope; scope; scope = scope->parent) {
    Fae_Ast_Decl **decl = hashmap_find(Fae_Ast_Decl*, &scope->decl_map, &decl_name);
    if (decl)
      return *decl;
  }
  return NULL; 
}

internal
Fae_Type fae_get_expr_return_type(Fae_Ast_Expr *expr)
{
  switch (expr->type) {
  case FaeExpr_Constant:      return expr->constant.type;
  case FaeExpr_Variable:      return expr->variable->type;
  case FaeExpr_Function_Call: return expr->fn_call.ret_type; 
  case FaeExpr_Type_Cast:     return expr->cast.to;
  case FaeExpr_Bin_Op: {
    if (expr->bin_op.op_type >= BinOp_Eq && expr->bin_op.op_type <= BinOp_Gt)
      return FaeType_Bool;
    return fae_get_expr_return_type(expr->bin_op.lhs);
  }
  default: assert(false);
  }
  return FaeType_INVALID;
}

internal
b8 fae_typecheck(Fae_Ast_Expr *expr, Fae_Type type)
{
  Fae_Type expr_type = fae_get_expr_return_type(expr);
  if (expr_type == type)
    return true;

  return false;
}

internal String8 fae_pretty_print_stmt(Arena *arena, Fae_Ast_Stmt *stmt, u32 indent);
internal String8 fae_pretty_print_expr(Arena *arena, Fae_Ast_Expr *expr);
internal String8 fae_pretty_print_block(Arena *arena, Fae_Ast_Block *block, u32 indent);

internal
String8 fae_pretty_print_decl(Arena *arena, Fae_Ast_Decl *decl)
{
  String8 pre = str8("");
  if (decl->flags & FaeDeclFlag_In)
    pre = str8("in ");
  else if (decl->flags & FaeDeclFlag_Out)
    pre = str8("out ");

  String8 res;
  if (decl->signature) {
    String8_Node *n = NULL;
    for (u32 i = 0; i < decl->signature->n_args; ++i) {
      Fae_Ast_Decl *arg = &decl->signature->args[i];
      n = push_str8_node(arena, n, "%s: %s", cstr(arg->name), cstr(fae_type_str(arg->type)));
    }
    String8 args = str8_node_join(arena, n, ", ");
    res = push_str8f(arena, "%sfn %s(%s): %s\n{\n%s\n}", cstr(pre), cstr(decl->name), cstr(args), cstr(fae_type_str(decl->signature->ret_type)),
                     cstr(fae_pretty_print_block(arena, &decl->body, 2)));
  } else {
    String8 assign = str8("");
    if (decl->assign) {
      assign = push_str8f(arena, " = %s", cstr(fae_pretty_print_expr(arena, decl->assign)));
    }
    res = push_str8f(arena, "%svar %s: %s%s", cstr(pre), cstr(decl->name), cstr(fae_type_str(decl->type)), cstr(assign));
  }

  return res;
}

internal
void fae_emit_parse_note_fn_decl(Fae_Parser *parser, Fae_Ast_Decl *fn_decl)
{
  u64 word_start = fn_decl->decl_start;
  u64 word_end = fn_decl->decl_end;
  parse_err_at((Parser *)parser, str8("Note: function was declared as:"), word_start, word_end);
}

internal
b8 fae_type_can_be_cast(Fae_Type from, Fae_Type to)
{
  // @Incomplete
  (void)from;
  (void)to;
  return true;
}

internal
const char *fae_pretty_print_bin_op(Fae_Bin_Op_Type type)
{
  switch (type) {
  case BinOp_Add: return "+";
  case BinOp_Sub: return "-";
  case BinOp_Mul: return "*";
  case BinOp_Div: return "/";
  case BinOp_Eq:  return "==";
  case BinOp_Ne:  return "!=";
  case BinOp_Le:  return "<=";
  case BinOp_Lt:  return "<";
  case BinOp_Ge:  return ">=";
  case BinOp_Gt:  return ">";
  default: return "(unknown)";
  }
}

internal
String8 fae_pretty_print_expr(Arena *arena, Fae_Ast_Expr *expr)
{
  String8 res = str8("(unknown)");

  switch (expr->type) {
  case FaeExpr_Constant: {
    switch (expr->constant.type) {
    case FaeType_U64:
      res = push_str8f(arena, "%" PRIu64, expr->constant.value_u64);
      break;
    case FaeType_I64:
      res = push_str8f(arena, "%" PRIi64, expr->constant.value_i64);
      break;
    case FaeType_F64:
      res = push_str8f(arena, "%f", expr->constant.value_f64);
      break;
    default:
      res = str8("(unknown constant)");
    }
  } break;
  case FaeExpr_Function_Call: {
    String8_Node *sn = NULL;
    for (u32 i = 0; i < expr->fn_call.n_args; ++i)
      sn = push_str8_node(arena, sn, "%s", cstr(fae_pretty_print_expr(arena, &expr->fn_call.args[i])));
    res = push_str8f(arena, "%s(%s)", cstr(expr->fn_call.fn_name), cstr(str8_node_join(arena, sn, ", ")));
  } break;
  case FaeExpr_Variable: {
    res = expr->variable->name;
  } break;
  case FaeExpr_Type_Cast: {
    res = push_str8f(arena, "cast(%s) (%s)", cstr(fae_type_str(expr->cast.to)),
                     cstr(fae_pretty_print_expr(arena, expr->cast.expr)));
  } break;
  case FaeExpr_Bin_Op: {
    String8 left = fae_pretty_print_expr(arena, expr->bin_op.lhs);
    String8 right= fae_pretty_print_expr(arena, expr->bin_op.rhs);
    res = push_str8f(arena, "(%s %s %s)", cstr(left), fae_pretty_print_bin_op(expr->bin_op.op_type), cstr(right));
  } break;
  default:;
  }

  return res;
}

internal
String8 fae_pretty_print_block(Arena *arena, Fae_Ast_Block *block, u32 indent)
{
  String8_Node *n = NULL;
  String8 indent_str = str8_from_char(arena, ' ', indent);
  for (u64 i = 0; i < block->n_stmts; ++i) {
    n = push_str8_node(arena, n, "%s%s", cstr(indent_str), cstr(fae_pretty_print_stmt(arena, &block->stmts[i], indent)));
  }

  String8 res = str8_node_join(arena, n, "\n");

  return res;
}

internal
String8 fae_pretty_print_stmt(Arena *arena, Fae_Ast_Stmt *stmt, u32 indent)
{
  String8 res = str8("(unknown)");
  String8 indent_str = str8_from_char(arena, ' ', indent);

  switch (stmt->type) {
  case FaeStmt_Decl: {
    res = fae_pretty_print_decl(arena, stmt->decl);
  } break;

  case FaeStmt_Assignment: {
    res = push_str8f(arena, "%s = %s", cstr(stmt->assign.left->name),
                     cstr(fae_pretty_print_expr(arena, stmt->assign.right)));
  } break;

  case FaeStmt_Block: {
    res = push_str8f(arena, "{\n%s\n%s}", cstr(fae_pretty_print_block(arena, &stmt->block, indent + 2)), cstr(indent_str));
  } break;

  case FaeStmt_Return: {
    res = push_str8f(arena, "ret %s", cstr(fae_pretty_print_expr(arena, stmt->returned)));
  } break;

  case FaeStmt_If: {
    if (stmt->if_.else_) {
      res = push_str8f(arena, "if %s\n%s\n%selse\n%s", 
                       cstr(fae_pretty_print_expr(arena, stmt->if_.cond)),
                       cstr(fae_pretty_print_stmt(arena, stmt->if_.then, indent + 2)),
                       cstr(indent_str),
                       cstr(fae_pretty_print_stmt(arena, stmt->if_.else_, indent + 2)));
    } else {
      res = push_str8f(arena, "if %s\n%s", 
                       cstr(fae_pretty_print_expr(arena, stmt->if_.cond)),
                       cstr(fae_pretty_print_stmt(arena, stmt->if_.then, indent + 2)));
    }
  } break;
  }

  return push_str8f(arena, "%s%s", cstr(indent_str), cstr(res));
}

internal Fae_Ast_Expr *fae_parse_expression(Fae_Parser *parser, Fae_Precedence min_prec);

internal
b8 fae_parse_expr_list(Fae_Parser *parser, Fae_Ast_Expr **exprs, u32 *n_exprs)
{
  // parses:
  // (expr1, expr2, ...)
  // into a contiguous array of expressions.
  if (!lex_expect(parser, FaeLexTok_ParenOpen))
    return false;

  Lexer *lx = get_lexer(parser);

  // create a linked list of exprs in the scratch arena, then copy them to the
  // array once we know their number.

  struct Expr_Node {
    struct Expr_Node *next;
    Fae_Ast_Expr *expr;
  } *head = NULL, *tail = NULL;

  u32 n = 0;
  b8 failed = false;
  Lex_Token nxt = lex_peek(parser);
  while (nxt.type != FaeLexTok_ParenClose) {
    struct Expr_Node *expr_node = arena_push(struct Expr_Node, lx->arena);
    expr_node->expr = fae_parse_expression(parser, FAE_LOWEST_PRECEDENCE); 
    if (!expr_node->expr) {
      failed = true;
      break;
    }
    push_to_sll(head, tail, expr_node);
    ++n;

    nxt = lex_peek(parser);
    if (nxt.type == FaeLexTok_Comma)
      lex_eat(lx);
    else if (nxt.type != FaeLexTok_ParenClose) {
      parse_err(parser, push_str8f(lx->arena, "unexpected token '%s' found while parsing argument list (expected ',')",
                                   lex_tok_ty_to_human_friendly(lx, nxt.type)));
      failed = true;
      break;
    }
  }

  if (!failed) {
    lex_eat(lx); // eat ')'

    *n_exprs = n;
    if (n)
      *exprs = arena_push_array_nozero(Fae_Ast_Expr, parser->script_arena, n);
    u32 i = 0;
    // NOTE: can use memcpy because all the persistent stuff (such as strings)
    // has already been allocated in the persistent arena, and we're just copying
    // pointers and POD data here.
    for (struct Expr_Node *node = head; node; node = node->next)
      memcpy(&(*exprs)[i++], node->expr, sizeof(Fae_Ast_Expr));
  }
  return !failed;
}

typedef enum {
  FaeParseVar_None          = 0x0,
  FaeParseVar_Formal_Param  = 0x1,
  FaeParseVar_Dont_Register = 0x2
} Fae_Parse_Var_Decl_Flags;

internal b8 fae_parse_var_decl(Fae_Parser *parser, Fae_Parse_Var_Decl_Flags flags, Fae_Ast_Decl *out);

internal
b8 fae_parse_decl_list(Fae_Parser *parser, Fae_Ast_Decl **decls, u32 *n_decls)
{
  // @Cleanup: This is 99% @Copypasted from fae_parse_expr_list, consider refactoring
  
  // parses:
  // (ident: type, ident2: type2, ...)
  // into a contiguous array of decls.
  // The initial '(' is assumed to already have been lexed.
  Lexer *lx = get_lexer(parser);

  // create a linked list of exprs in the scratch arena, then copy them to the
  // array once we know their number.

  struct Decl_Node {
    struct Decl_Node *next;
    Fae_Ast_Decl decl;
  } *head = NULL, *tail = NULL;

  u32 n = 0;
  b8 failed = false;
  Lex_Token nxt = lex_peek(parser);
  while (nxt.type != FaeLexTok_ParenClose) {
    struct Decl_Node *decl = arena_push(struct Decl_Node, lx->arena);
    // NOTE: don't register vars while parsing them, as we don't want to register pointers to the temporary
    // arena. We will manually register them later when copying them to the persistent arena.
    if (!fae_parse_var_decl(parser, FaeParseVar_Formal_Param|FaeParseVar_Dont_Register, &decl->decl)) {
      failed = true;
      break;
    }
    push_to_sll(head, tail, decl);
    ++n;

    nxt = lex_peek(parser);
    if (nxt.type == FaeLexTok_Comma)
      lex_eat(lx);
    else if (nxt.type != FaeLexTok_ParenClose) {
      parse_err(parser, push_str8f(lx->arena, "unexpected token '%s' found while parsing argument list (expected ',')",
                                   lex_tok_ty_to_human_friendly(lx, nxt.type)));
      failed = true;
      break;
    }
  }

  if (!failed) {
    lex_eat(lx); // eat ')'

    *n_decls = n;
    if (n)
      *decls = arena_push_array_nozero(Fae_Ast_Decl, parser->script_arena, n);
    u32 i = 0;
    // NOTE: the node names have already been allocated in the persistent arena
    for (struct Decl_Node *node = head; node; node = node->next) {
      Fae_Ast_Decl *decl = &(*decls)[i++];
      memcpy(decl, &node->decl, sizeof(Fae_Ast_Decl));
      fae_register_var_to_scope(parser->cur_scope, decl);
    }
  }
  return !failed;
}

internal
b8 fae_parse_assignment_rhs(Fae_Parser *parser, Fae_Ast_Decl *left, Fae_Ast_Assignment *out)
{
  // parses "= expr"
  if (!lex_expect(parser, FaeLexTok_Assign))
    return false;

  Fae_Ast_Expr *expr = fae_parse_expression(parser, FAE_LOWEST_PRECEDENCE);
  if (!expr)
    return false;

  out->left = left;
  out->right = expr;

  // TODO: do some automatic casting (e.g. integer constants to real)
  if (!fae_typecheck(expr, left->type)) {
    parse_err(parser, push_str8f(get_lexer(parser)->arena, "expression has wrong type %s (expected: %s)", 
                                 cstr(fae_type_str(fae_get_expr_return_type(expr))), cstr(fae_type_str(left->type))));
    parser_pop_err_ctx(parser);
    return false;
  }
   
  return true;
}

internal
Fae_Ast_Expr *fae_parse_bin_op_rhs(Fae_Parser *parser, Fae_Precedence expr_prec, Fae_Bin_Op_Type op_type, Fae_Ast_Expr *lhs)
{
  // parses the right-hand side of a bin-op. Assumes the bin-op itself hasn't been parsed yet.

  Lexer *lx = get_lexer(parser);
  while (1) {
    Fae_Precedence op_prec = fae_get_bin_op_precedence(op_type);
    // If this operator has lower precedence than what we're currently parsing, we're done
    if (op_prec < expr_prec)
      return lhs;

    Lex_Token tok = lex_next(lx); // eat bin op
    (void)tok;
    assert(fae_lex_tok_to_bin_op(tok.type) == op_type);
    Fae_Ast_Expr *rhs = fae_parse_expression(parser, FAE_LOWEST_PRECEDENCE);
    if (!rhs)
      return NULL;

    // check if we should parse more
    Lex_Token nxt = lex_peek(parser);
    Fae_Bin_Op_Type nxt_op_type = fae_lex_tok_to_bin_op(nxt.type);
    if (nxt_op_type < BinOp_COUNT) {
      Fae_Precedence nxt_op_prec = fae_get_bin_op_precedence(nxt_op_type);
      if (nxt_op_prec > op_prec) {
        // follows a bin op with higher precedence than this: parse it
        if (!fae_parse_bin_op_rhs(parser, op_prec + 1, nxt_op_type, rhs))
          return NULL;
      }
    }

    Fae_Ast_Expr *new_lhs = arena_push(Fae_Ast_Expr, parser->script_arena);
    new_lhs->type = FaeExpr_Bin_Op;
    new_lhs->bin_op.op_type = op_type;
    new_lhs->bin_op.lhs = lhs;
    new_lhs->bin_op.rhs = rhs;
    lhs = new_lhs;
  }

  assert(false);
  return NULL;
}

internal
b8 fae_parse_fn_call(Fae_Parser *parser, String8 fn_name, Fae_Ast_Expr *out)
{
  Lexer *lx = get_lexer(parser);
  
  // lookup function
  Fae_Ast_Decl *fn_decl = fae_find_scoped_decl(parser->cur_scope, fn_name);
  if (!fn_decl) {
    parse_err(parser, push_str8f(lx->arena, "calling unknown function: '%s'", cstr(fn_name)));
    return false;
  }
  if (!fn_decl->signature) {
    parse_err(parser, push_str8f(lx->arena, "trying to call '%s', which is not a function", cstr(fn_name)));
    return false;
  }
  out->type = FaeExpr_Function_Call;
  out->fn_call.fn_name = str8_copy(parser->script_arena, fn_name);
  out->fn_call.ret_type = fn_decl->signature->ret_type;

  parser_push_err_ctx(parser, push_str8f(lx->arena, "parsing function call '%s(...)'", cstr(fn_name)));

  // parse arguments
  if (!fae_parse_expr_list(parser, &out->fn_call.args, &out->fn_call.n_args)) {
    parser_pop_err_ctx(parser);
    return false;
  } 

  // verify the number of args matches the signature
  if (out->fn_call.n_args != fn_decl->signature->n_args) {
    parse_err(parser, push_str8f(lx->arena, "calling '%s' with %u arguments, but %u were expected.", 
                                 cstr(fn_name), out->fn_call.n_args, fn_decl->signature->n_args));
    fae_emit_parse_note_fn_decl(parser, fn_decl);
    parser_pop_err_ctx(parser);        
    return false;
  }

  // typecheck the arguments
  for (u32 i = 0; i < out->fn_call.n_args; ++i) {
    Fae_Ast_Decl *exp_param = &fn_decl->signature->args[i];
    Fae_Type expr_ret_type = fae_get_expr_return_type(&out->fn_call.args[i]);
    if (!fae_typecheck(&out->fn_call.args[i], exp_param->type)) {
      parse_err_at(parser, push_str8f(lx->arena, "param '%s' has type '%s' instead of expected '%s'",
                                      cstr(exp_param->name), cstr(fae_type_str(expr_ret_type)),
                                      cstr(fae_type_str(exp_param->type))), 
                   out->fn_call.args[i].start_col, out->fn_call.args[i].end_col);
      fae_emit_parse_note_fn_decl(parser, fn_decl);
      parser_pop_err_ctx(parser);        
      return false;
    }
  }

  parser_pop_err_ctx(parser);        
  return true;
}

internal
b8 fae_parse_assignment(Fae_Parser *parser, Fae_Ast_Stmt *out)
{
  Lexer *lx = get_lexer(parser);
  Fae_Ast_Expr *lhs = fae_parse_expression(parser, FAE_LOWEST_PRECEDENCE);
  if (!lhs)
    return false;

  // @Incomplete (e.g. assign to array subscript)
  if (lhs->type != FaeExpr_Variable) {
    parse_err(parser, push_str8f(lx->arena, "expression '%s' cannot be assigned to.", cstr(fae_pretty_print_expr(lx->arena, lhs))));
    return false;
  }

  parser_push_err_ctx(parser, push_str8f(lx->arena, "parsing assignment to '%s'", cstr(lhs->variable->name)));

  if (lhs->variable->signature) {
    parse_err(parser, str8("a function cannot be assigned to."));
    parser_pop_err_ctx(parser);
    return false;
  }

  if (!fae_parse_assignment_rhs(parser, lhs->variable, &out->assign)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  out->type = FaeStmt_Assignment;
  parser_pop_err_ctx(parser);
  return true;
}

internal
b8 fae_parse_literal(Lex_Token tok, i32 sign, Fae_Ast_Expr *out)
{
  out->type = FaeExpr_Constant;

  switch (tok.type) {
  case Lex_Integer: {
    // FIXME: properly deduce / impose type of expr based on context
    if (sign >= 0) {
      out->constant.type = FaeType_U64;
      out->constant.value_u64 = (u64)tok.integer; // XXX: sussy cast, remove later (properly support I64)
    } else {
      out->constant.type = FaeType_I64;
      out->constant.value_i64 = sign * tok.integer;
    }
  } break;

  case Lex_Real: {
    out->type = FaeExpr_Constant;
    out->constant.type = FaeType_F64;
    out->constant.value_f64 = sign * tok.real;
  } break;

  default:
    assert(false);
  }

  return true;
}

internal
b8 fae_parse_cast(Fae_Parser *parser, Fae_Ast_Expr *out)
{
  if (!lex_expect(parser, FaeLexTok_ParenOpen))
    return false;
  
  Lexer *lx = get_lexer(parser);
  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, str8("expected type name after 'cast' keyword"));
    return false;
  }
  Fae_Type cast_to = fae_type_from_name(tok.string);
  if (cast_to == FaeType_INVALID) {
    parse_err(parser, push_str8f(lx->arena, "trying to cast to invalid type '%s'", cstr(tok.string)));
    return false;
  }

  if (!lex_expect(parser, FaeLexTok_ParenClose))
    return false;

  Fae_Precedence min_prec = fae_get_bin_op_precedence(BinOp_Cast);
  Fae_Ast_Expr *subexpr = fae_parse_expression(parser, min_prec);
  if (!subexpr)
    return false;

  Fae_Type subexpr_type = fae_get_expr_return_type(subexpr);
  if (!fae_type_can_be_cast(subexpr_type, cast_to)) {
    parse_err(parser, push_str8f(lx->arena, "cannot cast expression (%s) from type %s to type %s",
                                 cstr(fae_pretty_print_expr(lx->arena, subexpr)), 
                                 cstr(fae_type_str(subexpr_type)), cstr(fae_type_str(cast_to))));
    return false;
  }

  out->type = FaeExpr_Type_Cast;
  out->cast.to = cast_to;
  out->cast.expr = subexpr;

  return true;
}

typedef enum {
  FaeParseStmt_None           = 0x0,
  FaeParseStmt_Is_Top_Level   = 0x1,
  FaeParseStmt_Return_Allowed = 0x2
} Fae_Parse_Stmt_Flags;

internal b8 fae_parse_stmt(Fae_Parser *parser, Lex_Token tok, Fae_Parse_Stmt_Flags flags, Fae_Ast_Stmt *out);

internal
b8 fae_parse_if(Fae_Parser *parser, Fae_Ast_Stmt *out)
{
  Fae_Ast_Expr *cond = fae_parse_expression(parser, FAE_LOWEST_PRECEDENCE);
  if (!cond)
    return false;

  Lex_Token nxt = lex_peek(parser);
  Fae_Ast_Stmt *then = arena_push(Fae_Ast_Stmt, parser->script_arena);
  if (!fae_parse_stmt(parser, nxt, FaeParseStmt_Return_Allowed, then))
    return false;

  Fae_Ast_Stmt *else_ = NULL;
  nxt = lex_peek(parser);
  if (nxt.type == FaeLexTok_Keyword_Else) {
    lex_eat(get_lexer(parser));
    nxt = lex_peek(parser);
    else_ = arena_push(Fae_Ast_Stmt, parser->script_arena);
    if (!fae_parse_stmt(parser, nxt, FaeParseStmt_Return_Allowed, else_))
      return false;
  }

  out->type = FaeStmt_If;
  out->if_.cond = cond;
  out->if_.then = then;
  out->if_.else_ = else_;

  return true;
}

internal
Fae_Ast_Expr *fae_parse_decl(Fae_Parser *parser, Lex_Token tok)
{
  Lexer *lx = get_lexer(parser);
  Fae_Ast_Decl *decl = fae_find_scoped_decl(parser->cur_scope, tok.string);
  if (!decl) {
    Lex_Token nxttok = lex_peek(parser);
    if (nxttok.type == FaeLexTok_ParenOpen)
      parse_err(parser, push_str8f(lx->arena, "unknown function: '%s'", cstr(tok.string)));
    else
      parse_err(parser, push_str8f(lx->arena, "unknown variable: '%s'", cstr(tok.string)));
    return NULL;
  }
  Fae_Ast_Expr *expr = arena_push(Fae_Ast_Expr, parser->script_arena);
  expr->type = FaeExpr_Variable;
  expr->variable = decl;
  return expr;
}

internal
Fae_Ast_Expr *fae_parse_leaf(Fae_Parser *parser)
{
  Lex_Token next = lex_next(parser);
  i32 sign = 1;
  switch (next.type) {
  case Lex_EOF:
  case Lex_ERROR:
  case FaeLexTok_Assign:
    return NULL;

  case FaeLexTok_Minus:
    sign = -1;
    next = lex_next(parser);
    // fallthrough
  case Lex_Integer:
  case Lex_Real: {
    Fae_Ast_Expr *expr = arena_push(Fae_Ast_Expr, parser->script_arena);
    if (fae_parse_literal(next, sign, expr)) return expr;
    return NULL;
  } break;

  case Lex_Ident: {
    return fae_parse_decl(parser, next);
  } break;

  case FaeLexTok_ParenOpen: {
    Fae_Ast_Expr *expr = fae_parse_expression(parser, FAE_LOWEST_PRECEDENCE);
    if (!expr)
      return NULL;
    lex_expect(parser, FaeLexTok_ParenClose);
    return expr;
  } break;

  case FaeLexTok_Keyword_Cast: {
    Fae_Ast_Expr *expr = arena_push(Fae_Ast_Expr, parser->script_arena);
    if (!fae_parse_cast(parser, expr))
      return NULL;
    return expr;
  } break;

  default:
    parse_err(parser, push_str8f(get_lexer(parser)->arena, "Expected leaf expression, found '%s'",
                                 lex_tok_to_human_friendly(get_lexer(parser), next)));
    return NULL;
  }
}

internal
Fae_Ast_Expr *fae_parse_increasing_precedence(Fae_Parser *parser, Fae_Ast_Expr *left, Fae_Precedence min_prec)
{
  assert(left);
  
  Lexer *lx = get_lexer(parser);
  Lex_Token next = lex_peek(parser);

  Fae_Bin_Op_Type op_type = fae_lex_tok_to_bin_op(next.type);
  if (op_type == BinOp_COUNT)
    return left; // not a bin op

  Fae_Precedence next_prec = fae_get_bin_op_precedence(op_type);

  if (next_prec <= min_prec) {
    return left;
  } else {
    if (op_type == BinOp_ArraySubscript) {
      // TODO
      lex_eat(lx);
      parse_err(parser, str8("arrays are not supported yet."));
      return NULL;
      // return parse_array_subscript(left);
    } else if (op_type == BinOp_FnCall) {
      String8 fn_name;
      if (left->type == FaeExpr_Variable && left->variable->signature) {
        fn_name = left->variable->name;
      } else {
        // TODO: support other expressions as fn call left hand side?
        parse_err(parser, push_str8f(lx->arena, "Expression cannot be called: '%s'", cstr(
                                     fae_pretty_print_expr(lx->arena, left))));
        return NULL;
      }
      Fae_Ast_Expr *right = arena_push(Fae_Ast_Expr, parser->script_arena);
      if (!fae_parse_fn_call(parser, fn_name, right))
        return NULL;
      return right;
    } else {
      lex_eat(lx);
      Fae_Ast_Expr *right = fae_parse_expression(parser, next_prec);
      if (!right)
        return NULL;
      Fae_Ast_Expr *expr = arena_push(Fae_Ast_Expr, parser->script_arena);
      expr->type = FaeExpr_Bin_Op;
      expr->bin_op.lhs = left;
      expr->bin_op.rhs = right;
      expr->bin_op.op_type = op_type; 
      return expr;
    }
  }
}

internal
Fae_Ast_Expr *fae_parse_expression(Fae_Parser *parser, Fae_Precedence min_prec)
{
  Fae_Ast_Expr *left = fae_parse_leaf(parser);
  if (!left)
    return NULL;

  while (left) {
    Fae_Ast_Expr *node = fae_parse_increasing_precedence(parser, left, min_prec);
    if (node == left)
      break;

    left = node;
  }

  return left;
}

internal
Fae_Type fae_get_func_return_type(Fae_Parser *parser, Fae_Ast_Function_Call *fn_call)
{
  Lexer *lx = get_lexer(parser);
  Fae_Ast_Decl *fn_decl = fae_find_scoped_decl(parser->cur_scope, fn_call->fn_name);
  if (!fn_decl) {
    parse_err(parser, push_str8f(lx->arena, "unknown function '%s'", cstr(fn_call->fn_name)));
    return FaeType_INVALID;
  }
  return fn_decl->signature->ret_type;
}

// Parses either a variable declaration or a function parameter declaration
internal
b8 fae_parse_var_decl(Fae_Parser *parser, Fae_Parse_Var_Decl_Flags flags, Fae_Ast_Decl *out)
{
  // var name: type [= expr]
  // ('var' is assumed to already have been parsed by the caller)
  Lexer *lx = get_lexer(parser);
  assert(lx->cur >= 3);
  out->decl_start = lx->cur - 3;

  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, push_str8f(lx->arena, "expected variable name, got '%s'", lex_tok_to_human_friendly(lx, tok)));
    return false;
  }
  String8 name = tok.string;

  // check duplicate decls
  // NOTE: we currently don't allow shadowing variables, even with fn formal params
  {
    Fae_Ast_Decl *existing = fae_find_scoped_decl(parser->cur_scope, name);
    if (existing) {
      if (existing->scope == parser->cur_scope) {
        parse_err(parser, push_str8f(lx->arena, "redeclaration of var '%s' (already declared at line %u)",
                                     cstr(name), existing->declared_at_line));
      } else {
        parse_err(parser, push_str8f(lx->arena, "declaration of variable '%s' shadows a variable with the same name declared at line %u",
                                     cstr(name), existing->declared_at_line));
      }
      u64 word_start = existing->decl_start;
      u64 word_end = existing->decl_end;
      parse_err_at((Parser *)parser, str8("here is the original declaration:"), word_start, word_end);
      return false;
    }
  }

  if (!lex_expect(parser, FaeLexTok_Colon))
    return false;

  tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, push_str8f(lx->arena, "expected type after var name '%s', got '%s'", cstr(name),
                                 lex_tok_to_human_friendly(lx, tok)));
    return false;
  }
  Fae_Type type = fae_type_from_name(tok.string);
  if (type == FaeType_INVALID) {
    parse_err(parser, push_str8f(lx->arena, "invalid type '%s' for var '%s'", cstr(tok.string), cstr(name)));
    return false;
  }

  out->name = str8_copy(parser->script_arena, name);
  out->type = type;
  out->declared_at_line = lx->cur_line;
  out->decl_end = lx->cur;

  // parse optional var assignment
  Lex_Token nxt = lex_peek(parser);
  if (nxt.type == FaeLexTok_Assign) {
    // NOTE: currently we don't allow for default values for function parameters
    if (!(flags & FaeParseVar_Formal_Param)) {
      parser_push_err_ctx(parser, push_str8f(lx->arena, "parsing var declaration '%s : %s'",
                                             cstr(out->name), cstr(fae_type_str(out->type))));

      // allocate the assignment in temporary memory, but assign->right will be allocatementd
      // (correctly) in the permanent arena.
      Fae_Ast_Assignment *assign = arena_push(Fae_Ast_Assignment, lx->arena);
      if (!fae_parse_assignment_rhs(parser, out, assign))
        return false;
      out->assign = assign->right;
      parser_pop_err_ctx(parser);
    } else {
      parse_err(parser, push_str8f(lx->arena, "assigning to '%s' is not allowed in this context.", cstr(out->name)));
      parser_pop_err_ctx(parser);
      return false;
    }
  }

  if (!(flags & FaeParseVar_Dont_Register)) {
    fae_register_var_to_scope(parser->cur_scope, out);
  }

  return true;
}

internal
b8 fae_parse_stmt_list(Fae_Parser *parser, Lex_Token tok, Fae_Parse_Stmt_Flags flags, Fae_Ast_Block *out)
{
  // parses a list of statements, typically included in a block (might be an implicit block
  // in the case of top-level statements). Does not parse the block delimiters.
  
  struct Stmt_Node {
    struct Stmt_Node *next;
    Fae_Ast_Stmt stmt;
  } *head = NULL, *tail = NULL;
  
  Lexer *lx = get_lexer(parser);
  u64 num_stmts = 0;
  while (tok.type != Lex_EOF && tok.type != FaeLexTok_BraceClose) {
    struct Stmt_Node *node = arena_push(struct Stmt_Node, lx->arena);
    if (!fae_parse_stmt(parser, tok, flags, &node->stmt))
      return false;
    push_to_sll(head, tail, node);
    ++num_stmts;
    tok = lex_peek(parser);
  }

  out->stmts = arena_push_array_nozero(Fae_Ast_Stmt, parser->script_arena, num_stmts);
  out->n_stmts = num_stmts;
  u64 idx = 0;
  for (struct Stmt_Node *n = head; n; n = n->next)
    memcpy(&out->stmts[idx++], &n->stmt, sizeof(Fae_Ast_Stmt));

  return true;
}

internal
b8 fae_parse_fn_decl(Fae_Parser *parser, Fae_Ast_Decl *out)
{
  // fn Fn_Name(arg_list)[: ret_type] block
  // ('fn' is assumed to already have been parsed by the caller)
  Lexer *lx = get_lexer(parser);
  out->decl_start = lx->cur;
  Lex_Token tok = lex_next(parser);
  if (tok.type != Lex_Ident) {
    parse_err(parser, str8("expected function name"));
    return false;
  }
  String8 fn_name = tok.string;

  parser_push_err_ctx(parser, push_str8f(lx->arena, "parsing function declaration '%s'", cstr(fn_name)));

  if (!lex_expect(parser, FaeLexTok_ParenOpen)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  // push function scope, so the arg declarations will be pushed to that scope
  fae_push_scope(parser);
  
  Fae_Function_Signature *signature = arena_push(Fae_Function_Signature, parser->script_arena);
  if (!fae_parse_decl_list(parser, &signature->args, &signature->n_args)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  // parse return type
  Lex_Token nxt = lex_peek(parser);
  if (nxt.type == FaeLexTok_Colon) {
    lex_eat(lx); // eat ':'
    tok = lex_next(parser);
    if (tok.type != Lex_Ident) {
      parse_err(parser, push_str8f(lx->arena, "expected return type for function '%s'", cstr(fn_name)));
      parser_pop_err_ctx(parser);
      return false;
    }
    Fae_Type ret_type = fae_type_from_name(tok.string);
    if (ret_type == FaeType_INVALID) {
      parse_err(parser, push_str8f(lx->arena, "invalid return type '%s' for function '%s'", cstr(tok.string), cstr(fn_name)));
      parser_pop_err_ctx(parser);
      return false;
    }
    signature->ret_type = ret_type;
  } else {
    // deduce void
    signature->ret_type = FaeType_Void;
  }

  out->decl_end = lx->cur;

  // parse body
  if (!lex_expect(parser, FaeLexTok_BraceOpen)) {
    parser_pop_err_ctx(parser);
    return false;
  }
  tok = lex_next(parser);
  if (!fae_parse_stmt_list(parser, tok, FaeParseStmt_Return_Allowed, &out->body)) {
    parser_pop_err_ctx(parser);
    return false;
  }
  if (!lex_expect(parser, FaeLexTok_BraceClose)) {
    parser_pop_err_ctx(parser);
    return false;
  }

  fae_pop_scope(parser);

  out->name = str8_copy(parser->script_arena, fn_name);
  out->type = FaeType_Fn;
  out->declared_at_line = lx->cur_line;
  out->signature = signature;

  fae_register_func_to_scope(parser->cur_scope, out);

  parser_pop_err_ctx(parser);
  return true;
}

internal
b8 fae_parse_stmt(Fae_Parser *parser, Lex_Token tok, Fae_Parse_Stmt_Flags flags, Fae_Ast_Stmt *out)
{
  // - in var decl
  // - out var decl
  // - local var decl
  // - fn decl
  // - '{' statement list '}'
  // - assignment
  // - ret expr
  // - if/else
  Lexer *lx = get_lexer(parser);
  switch (tok.type) {
  case FaeLexTok_Keyword_In: {
    if (!(flags & FaeParseStmt_Is_Top_Level)) {
      parse_err(parser, str8("declaring input variables is only allowed at top level."));
      return false;
    }
    lex_eat(lx);
    if (!lex_expect(parser, FaeLexTok_Keyword_Var))
      return false;

    Fae_Ast_Decl *decl = arena_push(Fae_Ast_Decl, parser->script_arena);
    if (!fae_parse_var_decl(parser, FaeParseVar_None, decl))
      return false;
    decl->flags |= FaeDeclFlag_In;
    out->type = FaeStmt_Decl;
    out->decl = decl;
  } break;

  case FaeLexTok_Keyword_Out: {
    if (!(flags & FaeParseStmt_Is_Top_Level)) {
      parse_err(parser, str8("declaring output variables is only allowed at top level."));
      return false;
    }
    lex_eat(lx);
    if (!lex_expect(parser, FaeLexTok_Keyword_Var))
      return false;

    Fae_Ast_Decl *decl = arena_push(Fae_Ast_Decl, parser->script_arena);
    if (!fae_parse_var_decl(parser, FaeParseVar_None, decl))
      return false;
    decl->flags |= FaeDeclFlag_Out;
    out->type = FaeStmt_Decl;
    out->decl = decl;
  } break;

  case FaeLexTok_Keyword_Var: {
    // "local" variable (not in or out)
    lex_eat(lx);
    Fae_Ast_Decl *decl = arena_push(Fae_Ast_Decl, parser->script_arena);
    if (!fae_parse_var_decl(parser, FaeParseVar_None, decl))
      return false;
    out->type = FaeStmt_Decl;
    out->decl = decl;
  } break;

  case FaeLexTok_Keyword_Fn: {
    lex_eat(lx);
    Fae_Ast_Decl *decl = arena_push(Fae_Ast_Decl, parser->script_arena);
    if (!fae_parse_fn_decl(parser, decl))
      return false;
    out->type = FaeStmt_Decl;
    out->decl = decl;
  } break;

  case FaeLexTok_BraceOpen: {
    // statement list (aka block)
    lex_eat(lx);
    tok = lex_peek(parser);
    fae_push_scope(parser);
    if (!fae_parse_stmt_list(parser, tok, flags & ~FaeParseStmt_Is_Top_Level, &out->block))
      return false;

    if (!lex_expect(parser, FaeLexTok_BraceClose))
      return false;

    fae_pop_scope(parser);
    out->type = FaeStmt_Block;
  } break;

  case FaeLexTok_Keyword_Return: {
    if (!(flags & FaeParseStmt_Return_Allowed)) {
      parse_err(parser, str8("returning is not allowed here"));
      return false;
    }
    lex_eat(lx);
    Fae_Ast_Expr *expr = fae_parse_expression(parser, FAE_LOWEST_PRECEDENCE);
    if (!expr) {
      parse_err(parser, str8("expected an expression after 'ret'"));
      return false;
    }
    out->returned = expr;
    out->type = FaeStmt_Return;
  } break;

  case FaeLexTok_Keyword_If: {
    parser_push_err_ctx(parser, str8("parsing `if` statement"));
    lex_eat(lx);
    if (!fae_parse_if(parser, out)) {
      parser_pop_err_ctx(parser);
      return false;
    }
    parser_pop_err_ctx(parser);
  } break;

  default: {
    // assignment
    if (!fae_parse_assignment(parser, out))
      return false;
  } break;
  }

  // FIXME: somewhere here we have a memory corruption where parser->inner.err_ctx_free gets overwritten!
  DEBUG("Parsed %s", cstr(fae_pretty_print_stmt(lx->arena, out, 0)));
  return true;
}

internal
void fae_describe_script(Fae_Script *script, Arena **conflicts, u32 n_conflicts)
{
  Temp scratch = scratch_begin(conflicts, n_conflicts);

  DEBUG_TAG("Script", "Vars:");
  u32 scope_idx = 0;
  for (Fae_Scope *scope = script->scopes_head; scope; scope = scope->next) {
    for (Fae_Ast_Decl *decl = scope->vars_head; decl; decl = decl->next) {
      String8 pre = str8("");
      if (decl->flags & FaeDeclFlag_In)
        pre = str8("in ");
      else if (decl->flags & FaeDeclFlag_Out)
        pre = str8("out ");
      String8 assign = str8("");
      if (decl->assign) {
        assign = push_str8f(scratch.arena, " = %s", cstr(fae_pretty_print_expr(scratch.arena, decl->assign)));
      }
      DEBUG_TAG("Script", "  %svar %s: %s%s (scope %u)", cstr(pre), cstr(decl->name),
                cstr(fae_type_str(decl->type)), cstr(assign), scope_idx);
    }
    ++scope_idx;
  }

  DEBUG_TAG("Script", "Functions:");
  scope_idx = 0;
  for (Fae_Scope *scope = script->scopes_head; scope; scope = scope->next) {
    for (Fae_Ast_Decl *decl = scope->funcs_head; decl; decl = decl->next) {
      String8 pretty_fn = fae_pretty_print_decl(scratch.arena, decl);
      DEBUG_TAG("Script", "  %s (scope %u)", cstr(pretty_fn), scope_idx);
    }
    ++scope_idx;

    DEBUG_TAG("Script", "Statements:");
    DEBUG_TAG("Script", "%s", cstr(fae_pretty_print_block(scratch.arena, &script->root_block, 0)));
  }

  scratch_end(scratch);
}

internal
b8 fae_parse_script(Arena *arena, String8 src, Fae_Script *script)
{
  Temp scratch = scratch_begin(&arena, 1);

  u32 n_mappings;
  const Lex_Token_Mapping *mappings = get_fae_script_lex_token_mappings(scratch.arena, &n_mappings);
  Lexer lexer = lex_init(mappings, n_mappings, str8("#"));
  lex_start(&lexer, scratch.arena, src.str, src.size);

  Fae_Parser parser = {};
  parser_init((Parser *)&parser, &lexer, "Script.Parser");
  parser.script = script;  
  parser.script_arena = arena;

  // push global scope
  fae_push_scope(&parser);

  b8 failed = false;
  while (1) {
    Lex_Token tok = lex_peek(&parser);
    if (tok.type == Lex_EOF)
      break;

    if (!fae_parse_stmt_list(&parser, tok, FaeParseStmt_Is_Top_Level, &script->root_block)) {
      ERR_TAG("Script.Parser", "Failed to parse script.");
      failed = true;
      break;
    }
  }

  // If this trips, we have unbalanced scopes
  assert(parser.cur_scope == script->scopes_head);

  fae_describe_script(script, &lexer.arena, 1);
  
  DEBUG_TAG("Script.Parser", "temp arena mem usage: %s", cstr(to_pretty_size(lexer.arena, arena_pos(lexer.arena))));
  lex_end(&lexer);
  scratch_end(scratch);
  return !failed;
}

internal
b8 fae_parse_script_from_file(Arena *arena, String8 fname, Fae_Script *script)
{
  Temp scratch = scratch_begin(&arena, 1);

  String8 script_raw = file_read_to_string(scratch.arena, fname);
  b8 success = fae_parse_script(arena, script_raw, script);

  scratch_end(scratch);
  return success;
}

