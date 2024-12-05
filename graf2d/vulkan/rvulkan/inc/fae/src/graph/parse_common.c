//
// "Generic" lexer implementation + some parser utilities.
//
// The lexer should be configured by passing an array of token mappings (defining
// the keywords and similar):
// ```
// Lexer lx = lx_init(mappings, n_mappings);
// lex_start(&lx, arena, src, src_size); // binds the lexer source
// ```
// 
// Since the parsing process is much more specific, a parser should probably define
// its own structure, using the generic Parser as a member, to allow it to
// use the generic parsing functions.
//

#define LEX_MAX_KEYWORDS 256
#define LEX_MONO_TOKEN_TABLE_FIRST 33

typedef u32 Lex_Token_Type;

#define Lex_ERROR   0
#define Lex_EOF     1
#define Lex_Ident   2
#define Lex_Integer 3
#define Lex_Real    4
// first token type available to the user
#define Lex_FIRST 1000

typedef struct {
  Lex_Token_Type type;
  union {
    i64 integer;
    f64 real;
    String8 string;
  };
} Lex_Token;

typedef struct {
  String8 ident;
  Lex_Token_Type type;
} Lex_Token_Mapping;

typedef struct {
  // Arena used for all allocations that are not supposed to outlive the parsing process.
  // NOTE: this arena should never be popped in the middle of parsing, as it contains data
  // that needs to remain around for potentially all parsing, such as the err_ctx strings!
  Arena *arena;
  const u8 *src;
  u64 src_size;
  u64 cur;
  u32 cur_line;
  u64 cur_line_start;
  u64 latest_word_start;

  // position we jump to when calling lex_eat(). This gets set by lex_peek_internal().
  u64 next;
  // this is set by lex_peek_internal(). When lex_eat() is called, `latest_word_start` becomes this.
  u64 next_word_start;

  // Mapping { ASCII character => Lex_Token_Type }
  // It starts from ASCII 33  and goes up to ASCII 126.
  Lex_Token_Type mono_token_table[93];
  // This array contains all keywords and then all mono token mappings.
  // The reason why we also keep the mono token mappings, even if they are
  // in the mono_char_table, is that we need it to perform the reverse
  // mapping { Lex_Token_Type => String }, which would be less performant otherwise.
  // The size of this array is n_keywords + n_mono_tokens.
  // The keywords are sorted by string length.
  Lex_Token_Mapping keyword_table[LEX_MAX_KEYWORDS];
  u32 n_keywords;
  u32 n_mono_tokens;

  String8 line_comment_start;

  u32 arena_start_flags;
  u64 arena_start_pos;
} Lexer;

internal
Lexer lex_init(const Lex_Token_Mapping *mappings, u32 n_mappings, String8 line_comment_start)
{
  Lexer state = {};

  state.line_comment_start = line_comment_start;

  // fill keywords and mono tokens table.
  // we do this in two passes so we ensure that all keywords come first in the array.
  assert(n_mappings < LEX_MAX_KEYWORDS);
  for (u32 i = 0; i < n_mappings; ++i) {
    Lex_Token_Mapping m = mappings[i];
    if (m.ident.size > 1) {
      // keyword
      state.keyword_table[state.n_keywords++] = m;
    }
  }
  // sort keywords by string length
  i32 n = (i32)state.n_keywords;
  do {
    b8 swapped = false;
    for (i32 j = 1; j < n; ++j) {
      if (state.keyword_table[j - 1].ident.size < state.keyword_table[j].ident.size) {
        Lex_Token_Mapping tmp = state.keyword_table[j - 1];
        state.keyword_table[j - 1] = state.keyword_table[j];
        state.keyword_table[j] = tmp;
        swapped = true;
      }
    }
    if (!swapped)
      break;
    --n;
  } while (1);
  
  for (u32 i = 0; i < n_mappings; ++i) {
    Lex_Token_Mapping m = mappings[i];
    if (m.ident.size == 1) {
      // mono token: ensure it's ASCII
      u8 ch = m.ident.str[0];
      if (ch < 33 || ch > 126) {
        FATAL_TAG("Lexer", "Invalid mono token mapping: %c is outside the valid ASCII range [33, 126]", ch);
        os_abort();
      }
      state.mono_token_table[ch - LEX_MONO_TOKEN_TABLE_FIRST] = m.type;
      state.keyword_table[state.n_keywords + state.n_mono_tokens] = m;
      ++state.n_mono_tokens;
    }
  }

  return state;
}

// The arena will be used for all lexer allocations. It typically should have a lifetime
// similar to the lexing+parsing process.
internal
void lex_start(Lexer *lx, Arena *arena, const u8 *src, u64 src_size)
{
  lx->arena = arena;
  lx->arena_start_flags = arena->flags;
  lx->arena_start_pos = arena_pos(arena);
  lx->arena->flags |= ArenaFlag_Disallow_Pop;
  lx->src = src;
  lx->src_size = src_size;
  lx->cur_line = 1;
}

internal
void lex_end(Lexer *lx)
{
  lx->arena->flags = lx->arena_start_flags;
  arena_pop_to(lx->arena, lx->arena_start_pos);
}

internal
b8 lex_is_whitespace(u8 ch)
{
  // TODO: consider other weird cases
  return ch == ' ' || ch == '\t';
}

internal
b8 lex_is_newline(u8 ch)
{
  return ch == '\n' || ch == '\r';  
}

internal
b8 lex_is_mono_token(Lexer *lx, Lex_Token tok)
{
  if (tok.type < Lex_FIRST || tok.string.size != 1)
    return false;
  
  assert(tok.string.str);
  u8 ch = tok.string.str[0];
  return lx->mono_token_table[ch - LEX_MONO_TOKEN_TABLE_FIRST] == tok.type;
}

// If `ch` is a mono token, return its token, otherwise return a token of type Lex_ERROR.
internal
Lex_Token lex_check_mono_token(Lexer *lx, u8 ch)
{
  Lex_Token_Type type;
  u8 idx = ch - LEX_MONO_TOKEN_TABLE_FIRST;
  if (idx >= countof(lx->mono_token_table))
    type = Lex_ERROR;
  else
    type = lx->mono_token_table[idx];

  // For convenience, save the mono token char in the token itself
  String8 str;
  str.size = 1;
  str.str = arena_push_array_nozero(u8, lx->arena, 2);
  str.str[0] = ch;
  str.str[1] = 0;
  
  return (Lex_Token) { .type = type, .string = str };
}

internal
b8 lex_is_word_terminator(Lexer *lx, u64 pos)
{
  u8 ch = lx->src[pos];
  return lex_is_whitespace(ch) || lex_is_newline(ch) || lex_check_mono_token(lx, ch).type != Lex_ERROR;
}

internal
void lex_find_cur_word_and_line_end(Lexer *lx, u64 *word_end, u64 *line_end)
{
  // try to find the end of the current word and line
  u64 cur = lx->cur;
  while (cur < lx->src_size && !lex_is_word_terminator(lx, cur))
    ++cur;
  *word_end = cur;
  while (cur < lx->src_size && !lex_is_newline(lx->src[cur]))
    ++cur;
  *line_end = cur;
}

// Creates a string with the form:
// ```
// error at line `lineno`: `msg`
//
//    some line of code
//         ^^^^
// ```
// `line_start`, `line_end`, `underline_start` and `underline_end` are relative to the start of `data`
internal
String8 make_line_diagnostic(Arena *arena, String8 msg, u64 lineno, u64 line_start, u64 line_end,
                             u64 underline_start, u64 underline_end, const u8 *data, u16 indent)
{
  Temp scratch = scratch_begin(&arena, 1);
  
  u64 line_len = line_end - line_start;
  String8 line = str8_from_buf(arena, data + line_start, line_len);
  String8 indent_str = str8_from_char(arena, ' ', indent);
  String8 res = push_str8f(arena, "error at line %u: %s\n\n%s%s\n", lineno, cstr(msg), cstr(indent_str), cstr(line));
  // handle tabs. XXX: assuming tab width of 4
  for (u64 i = line_start; i < underline_start; ++i) {
    if (data[i] == '\t') {
      underline_start += 3;
      underline_end += 3;
    }
  }
  String8 underline = str8_from_char(arena, ' ', indent + underline_end - line_start);
  assert(underline_start >= line_start);
  assert(underline_end >= underline_start);
  u64 squiggle_len = underline_end - underline_start;
  memset(underline.str + indent + underline_start - line_start, '^', squiggle_len);
  res = str8_concat(arena, res, underline);

  scratch_end(scratch);
  return res;
}

internal
u32 lex_get_cur_column(Lexer *lx)
{
  assert(lx->cur_line_start <= lx->cur);
  return lx->cur - lx->cur_line_start + 1;
}

internal
void lex_start_newline(Lexer *lx, u64 cur)
{ 
  lx->cur = lx->next = cur;
  lx->cur_line_start = cur;
  lx->latest_word_start = lx->next_word_start = cur;
  ++lx->cur_line;
}

internal
Lex_Token lex_tok_simple(Lex_Token_Type type)
{
  Lex_Token t = {};
  t.type = type;
  return t;
}

internal
Lex_Token lex_err(String8 msg)
{
  Lex_Token tok;
  tok.type = Lex_ERROR;
  tok.string = msg;
  return tok;
}

internal
b8 lex_is_number_start(u8 ch)
{
  return ch == '.' || (ch >= '0' && ch <= '9');
}

internal
b8 lex_may_be_part_of_number(u8 ch)
{
  return lex_is_number_start(ch) || ch == 'e' || ch == 'E';
}

// lexes a number, assuming `inout_pos` points to a valid number start.
// Returns Lex_ERROR if it fails to lex.
// If it lexes successfully, `inout_pos` is set to the new
// lexer position (but this function doesn't directly change the lexer's cursor).
internal
Lex_Token lex_check_number(Lexer *lx, u64 *inout_pos)
{
  u64 cur = *inout_pos;
  assert(cur < lx->src_size);
  
  u8 ch = lx->src[cur];
  enum {
    Int_Part,
    Mantissa,
    Exponent
  } section = Int_Part;
  i64 int_accum = 0;
  u64 mantissa_accum = 0;
  u8 mantissa_len = 0;
  u16 exponent_accum = 0;
  do {
    if (ch == '.') {
      if (section > Int_Part)
        return lex_err(str8("found multiple decimal separators in the same number."));
      if (cur == lx->src_size)
        return lex_err(str8("decimal separator must be followed by at least a digit."));
      section = Mantissa;
    } else if (ch == 'e' || ch == 'E') {
      if (section == Exponent)
        return lex_err(str8("found multiple exponent separators in the same number."));
      if (cur == lx->src_size)
        return lex_err(str8("exponent separator must be followed by at least a digit."));
      section = Exponent;
    } else if (ch >= '0' && ch <= '9') {
      if (section <= Int_Part) {
        section = Int_Part;
        int_accum *= 10;
        int_accum += (ch - '0');
      } else if (section == Mantissa) {
        if (mantissa_len < UINT8_MAX) {
          mantissa_accum *= 10;
          mantissa_accum += (ch - '0');
          ++mantissa_len;
        }
      } else if (section == Exponent) {
        exponent_accum *= 10;
        exponent_accum += (ch - '0');
      } else {
        assert(false);
      }
    } else {
      return lex_err(push_str8f(lx->arena, "found invalid character '%c' while parsing number.", ch));
    }
    ++cur;
    if (cur == lx->src_size)
      break;
    ch = lx->src[cur];
  } while (lex_may_be_part_of_number(ch));

  Lex_Token tok;
  tok.type = (section == Int_Part) ? Lex_Integer : Lex_Real;
  if (tok.type == Lex_Integer) {
    assert(mantissa_accum == 0 && exponent_accum == 0);
    tok.integer = int_accum;
  } else {
    f64 mantissa = (f64)mantissa_accum;
    for (u8 i = 0; i < mantissa_len; ++i)
      mantissa *= 0.1;
    tok.real = (f64)int_accum + mantissa;
    tok.real *= powf(10, exponent_accum);
  }

  *inout_pos = cur;

  return tok;
}

// assumes lx->src[lx->cur] is not a newline character!
internal
u64 lex_get_pos_of_next_line(Lexer *lx, u64 start)
{
  u64 cur = start;
  do { 
    ++cur;
  } while (cur < lx->src_size && !lex_is_newline(lx->src[cur]));
  cur += 1 + (lx->src[cur] == '\r');
  return cur;
}

internal
b8 lex_is_comment_start(Lexer *lx, u64 pos)
{
  for (u64 i = 0; i < lx->line_comment_start.size && (pos + i < lx->src_size); ++i)
    if (lx->src[pos + i] != lx->line_comment_start.str[i])
      return false;
  return true;
}

// If the characters following `pos` correspond to a keyword (without looking ahead to other characters)
// return the index of that keyword.
// Note that, since `keyword_table` is sorted by string length, we will always return the longest match.
internal
i64 lex_check_potential_keyword(Lexer *lx, u64 pos)
{
  for (u32 i = 0; i < lx->n_keywords; ++i) {
    String8 keyword = lx->keyword_table[i].ident;
    b8 viable = true;
    for (u64 l = 0; l < keyword.size; ++l) {
      if (pos + l == lx->src_size || keyword.str[l] != lx->src[pos + l]) {
        viable = false;
        break;
      }
    }
    if (viable)
      return i;
  }
  return -1;
}

// Finds the next valid token starting at lx->cur and returns it.
// This function is idempotent, but it can modify lx->cur as an optimization. More specifically,
// it remembers all the whitespace and newlines skipped.
internal
Lex_Token lex_peek_internal(Lexer *lx)
{
  assert(lx->next >= lx->cur);
  assert(lx->cur <= lx->src_size);
  if (lx->cur == lx->src_size)
    return lex_tok_simple(Lex_EOF);

  u64 cur = lx->cur;
  
  do {
    u8 ch = lx->src[cur];
    ++cur;

    if (lex_is_whitespace(ch)) {
      lx->cur = lx->next = cur;
      continue;
    }

    if (lex_is_newline(ch)) {
      // if ch == '\r', assume next character is \n. If not, the input is malformed.
      assert((ch != '\r') || (cur < lx->src_size && lx->src[cur] == '\n'));
      cur += (ch == '\r');
      lex_start_newline(lx, cur);
      continue;
    }

    // comment to EOL
    if (lex_is_comment_start(lx, cur - 1)) {
      cur = lex_get_pos_of_next_line(lx, cur);
      lex_start_newline(lx, cur);
      continue;
    }

    lx->next_word_start = cur - 1;

    // try number. Do this before mono tokens to properly treat '.'
    if (lex_is_number_start(ch)) {
      --cur;
      Lex_Token number_tok = lex_check_number(lx, &cur);
      // don't error out if we found '.', maybe it was not meant to be a number 
      if (number_tok.type != Lex_ERROR || ch != '.') {
        lx->next = cur;
        return number_tok;
      }
      ++cur;
    }

    // lex identifier (or keyword)

    // NOTE: check all keywords first because they might be ambiguous with mono tokens
    // (e.g. we need to try '==' before '=')
    i64 candidate_idx = lex_check_potential_keyword(lx, cur - 1);
    if (candidate_idx >= 0) {
      String8 candidate = lx->keyword_table[candidate_idx].ident;
      if (cur - 1 + candidate.size < lx->src_size && lex_is_word_terminator(lx, cur - 1 + candidate.size)) {
        cur += candidate.size - 1;
        lx->next = cur;
        return lex_tok_simple(lx->keyword_table[candidate_idx].type);
      }
    } 

    // Do mono tokens after keywords to avoid mistakes such as parsing '==' as two '='
    Lex_Token mono = lex_check_mono_token(lx, ch);
    if (mono.type != Lex_ERROR) {
      lx->next = cur;
      return mono;
    }

    do {
      if (lex_is_word_terminator(lx, cur))
        break;
      ++cur;
    } while (cur < lx->src_size);
    u64 ident_size = cur - lx->next_word_start;
    String8 ident = str8_from_buf(lx->arena, lx->src + lx->next_word_start, ident_size);

    // for (u32 kw = 0; kw < lx->n_keywords; ++kw)
    //   if (str8_eq(ident, lx->keyword_table[kw].ident))
    //     return lex_tok_simple(lx->keyword_table[kw].type);
 
    lx->next = cur;
    return (Lex_Token){ Lex_Ident, .string = ident };

  } while (cur < lx->src_size);

  assert(cur == lx->src_size);
  lx->next = cur;
  return lex_tok_simple(Lex_EOF);
}

internal
void lex_eat(Lexer *lx)
{
  lx->cur = lx->next;
  assert(lx->next_word_start >= lx->latest_word_start);
  lx->latest_word_start = lx->next_word_start;
}

internal
const char *lex_tok_ty_to_human_friendly(Lexer *lx, Lex_Token_Type type)
{
  for (u32 i = 0; i < lx->n_keywords + lx->n_mono_tokens; ++i)
    if (type == lx->keyword_table[i].type)
      return cstr(lx->keyword_table[i].ident);
  
  switch (type) {
  case Lex_Ident: return "identifier";
  case Lex_Integer: return "integer";
  case Lex_Real: return "real";
  case Lex_ERROR: return "(ERROR)";
  case Lex_EOF: return "EOF";  
  default: assert(false);
  }
  return "";
}

internal
const char *lex_tok_to_human_friendly(Lexer *lx, Lex_Token tok)
{
  static char buf[256];
  if (tok.type == Lex_Integer) {
    snprintf(buf, countof(buf), "%" PRIi64, tok.integer);
    return buf;
  }
  if (tok.type == Lex_Real) {
    snprintf(buf, countof(buf), "%f", tok.real);
    return buf;
  }
  if (tok.type == Lex_Ident) {
    snprintf(buf, countof(buf), "%s", cstr(tok.string));
    return buf;
  }
  return lex_tok_ty_to_human_friendly(lx, tok.type);
}

/// Parsing

typedef struct Err_Ctx {
  struct Err_Ctx *next;
  String8 name;  
  u32 line;
} Err_Ctx;

typedef struct {
  Lexer *lexer;
  b8 err_happened;

  // used in error reporting
  Err_Ctx *err_ctx;
  Err_Ctx *err_ctx_free;
  const char *log_prelude;
} Parser;

internal
void parser_init(Parser *parser, Lexer *lexer, const char *log_prelude)
{
  parser->lexer = lexer;
  parser->log_prelude = log_prelude;
}

internal
void parser_push_err_ctx_(Parser *parser, String8 name)
{
  Err_Ctx *ctx;
  if (parser->err_ctx_free) {
    ctx = parser->err_ctx_free;
    parser->err_ctx_free = parser->err_ctx_free->next;
    assert(!parser->err_ctx_free || (u64)parser->err_ctx_free->next < 0x7fffffffffff);
  } else {
    Arena *arena = parser->lexer->arena;
    ctx = arena_push(Err_Ctx, arena);
  }
  ctx->name = name;
  ctx->line = parser->lexer->cur_line;
  // stack-like chain: new node points to the last added
  ctx->next = parser->err_ctx;
  parser->err_ctx = ctx;
}

internal
void parser_pop_err_ctx_(Parser *parser)
{
  assert(parser->err_ctx);
  Err_Ctx *next_ctx = parser->err_ctx->next;
  parser->err_ctx->next = parser->err_ctx_free;
  parser->err_ctx_free = parser->err_ctx;
  parser->err_ctx = next_ctx;
}

internal
void parse_err_internal(Parser *parser, String8 msg, u64 word_start, u64 word_end, u64 line_start, u64 line_end, u16 indent)
{
  Lexer *lx = parser->lexer;
  Arena *arena = lx->arena;
  u64 lineno = lx->cur_line;
  String8 err = make_line_diagnostic(arena, msg, lineno, line_start, line_end, word_start, word_end, lx->src, indent);
  ERR_TAG(parser->log_prelude, "%s", cstr(err));

  indent = 0;
  for (Err_Ctx *ctx = parser->err_ctx; ctx; ctx = ctx->next) {
    String8 indent_str = indent == 0 ? str8("") : str8_from_char(arena, ' ', indent);
    ERR_TAG(parser->log_prelude, "%s...%s @ line %u", cstr(indent_str), cstr(ctx->name), ctx->line);
    indent += 2;
  }

  parser->err_happened = true;
}

internal
void parse_err_(Parser *parser, String8 msg)
{
  Lexer *lx = parser->lexer;
  u64 word_end, line_end;
  lex_find_cur_word_and_line_end(lx, &word_end, &line_end);

  u64 word_start = lx->latest_word_start;
  u64 line_start = lx->cur_line_start;
  parse_err_internal(parser, msg, word_start, word_end, line_start, line_end, 4);
}

// Given a position `pos`, returns the indices of the line start and end containing that pos.
// If `pos` is itself a newline, it will be considered as `line_end`, and `line_start` will be
// the beginning of the previous line (unless `line_end == 0`, in which case it's the first line.)
internal
void lex_find_line_boundaries(Lexer *lx, u64 pos, u64 *line_start, u64 *line_end)
{
  // start one character before `pos` if possible
  u64 ps = pos - (pos > 0);
  while (ps > 0 && !lex_is_newline(lx->src[ps]))
    --ps;
  *line_start = ps;

  // if pos == line_start, we need to look for the next newline, so start one char ahead.
  u64 pe = pos + (ps == pos);
  while (pe < lx->src_size - 1 && !lex_is_newline(lx->src[pe]))
    ++pe;
  *line_end = pe;
}

internal
void parse_err_at_(Parser *parser, String8 msg, u16 start_col, u16 end_col)
{
  Lexer *lx = parser->lexer;
  u64 line_start, line_end;
  lex_find_line_boundaries(lx, start_col, &line_start, &line_end);
  // line_start += 1;

  parse_err_internal(parser, msg, start_col, end_col, line_start, line_end, 0);
}

// These could in principle be lexer procedures, but it's handy to have the parser err ctx.
internal
Lex_Token lex_peek_(Parser *parser)
{
  Lexer *lx = parser->lexer;
  Lex_Token tok = lex_peek_internal(lx);
  assert(lx->next >= lx->cur);
  if (tok.type == Lex_ERROR) {
    parse_err_(parser, tok.string);
    return tok;
  }
  VERY_VERBOSE_TAG("Parse.Lex", "Lexed '%s' cur: %lu, next: %lu, cur word start: %lu, next word start: %lu", 
                   lex_tok_to_human_friendly(lx, tok),  lx->cur, lx->next, lx->latest_word_start, lx->next_word_start);
  return tok;
}

internal
Lex_Token lex_next_(Parser *parser)
{
  Lex_Token tok = lex_peek_(parser);
  lex_eat(parser->lexer);
  return tok;
}

internal
b8 lex_expect_(Parser *parser, Lex_Token_Type expected)
{
  Lexer *lx = parser->lexer;
  Lex_Token tok = lex_next_(parser);
  if (tok.type != expected) {
    parse_err_(parser, push_str8f(lx->arena, "expected '%s', got '%s'", 
               lex_tok_ty_to_human_friendly(lx, expected), lex_tok_to_human_friendly(lx, tok)));
    return false;
  }
  return true;
}

// begin glue code --
// WARNING! to properly use this "generic" glue code your parser must have
// a Parser as its first member!
internal
Lexer *get_lexer(void *parser)
{
  return ((Parser *)parser)->lexer;
}

internal
void parser_push_err_ctx(void *parser, String8 ctx)
{
  parser_push_err_ctx_((Parser *)parser, ctx);
}

internal
void parser_pop_err_ctx(void *parser)
{
  parser_pop_err_ctx_((Parser *)parser);
}

internal
void parse_err(void *parser, String8 err)
{
  parse_err_((Parser *)parser, err);
}

internal
void parse_err_at(void *parser, String8 err, u16 start_col, u16 end_col)
{
  parse_err_at_((Parser *)parser, err, start_col, end_col);
}

internal
b8 lex_expect(void *parser, Lex_Token_Type type)
{
  return lex_expect_((Parser *)parser, type);
}

internal
Lex_Token lex_peek(void *parser)
{
  return lex_peek_((Parser *)parser);
}

internal
Lex_Token lex_next(void *parser)
{
  return lex_next_((Parser *)parser);
}
// -- end glue code
