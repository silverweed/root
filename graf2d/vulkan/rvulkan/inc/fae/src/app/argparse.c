typedef struct {
  b8 show_help_and_exit;
  Log_Level verbosity;
  String8 graph;
  b8 no_gfx;
  b8 no_srgb;
  b8 prefer_cpu;
  b8 show_spline_cpoints;
  b8 wireframe;
  V2i win_size;
} Cmdline_Args;

internal
void print_help(const char *argv0)
{
  fprintf(stderr,
    "Usage: %s [opts]"   
    "\nOptions:"
    "\n\t--graph <filename>   graph to load"
    "\n\t--no-gfx             disable graphics"
    "\n\t-v[vv][0-9]          verbosity up"
    "\n\t--no-srgb            disable SRGB"
    "\n\t--prefer-cpu         try to use the integrated graphics rather than discrete"
    "\n\t--spline-cpoints     show debug markers on spline control points"
    "\n\t--wireframe          enable wireframe mode"
    "\n\t--winsize <width> <height>   specify window size"
    "\n"
  , cstr(file_basename(str8_tmp_from_c(argv0))));
}

typedef struct {
  u64 num;
  b8 error;
} Conv_Res;

internal
Conv_Res str_to_u64(String8 s)
{
  Conv_Res res = {};
  if (s.size > 2 && s.str[0] == '0' && s.str[1] == 'x') {
    // hexadecimal
    for (u64 i = 2; i < s.size; ++i) {
      u8 c = s.str[i];
      if (c >= '0' && c <= '9') {
        res.num *= 16;
        res.num += c - '0';
      } else if (c >= 'A' && c <= 'F') {
        res.num *= 16;
        res.num += 10 + c - 'A';
      } else if (c >= 'a' && c <= 'f') {
        res.num *= 16;
        res.num += 10 + c - 'a';
      } else {
        res.error = true;
        break;
      }
    }
  } else {
    // decimal
    for (u64 i = 0; i < s.size; ++i) {
      u8 c = s.str[i];
      if (c >= '0' && c <= '9') {
        res.num *= 10;
        res.num += c - '0';
      } else {
        res.error = true;
        break;
      }
    }
  }
  return res;
}

internal
void parse_int_arg(i32 *cur_arg_idx, i32 argc, char **argv, u64 *out)
{
  const char *arg = argv[*cur_arg_idx];
  if (*cur_arg_idx < argc - 1) {
    String8 nxt_arg = str8_tmp_from_c(argv[++*cur_arg_idx]);
    Conv_Res res = str_to_u64(nxt_arg);
    if (res.error)
      fprintf(stderr, "Invalid integer after %s flag.\n", arg);
    else
      *out = res.num;
  } else {
    fprintf(stderr, "Argument required after %s flag.\n", arg);
  }
}

internal
Cmdline_Args parse_args(i32 argc, char **argv)
{
  Cmdline_Args args = {};
  args.verbosity = Log_Info;

  for (i32 i = 1; i < argc; ++i) {
    String8 arg = str8_tmp_from_c(argv[i]);
    if (arg.str[0] == '-') {
      if (arg.str[1] == 'v') {
        if (arg.size > 2) {
          if (arg.str[2] == 'v') {
            args.verbosity = Log_Verbose;
          } else {
            String8 nxtarg = arg;
            nxtarg.str += 2, nxtarg.size -= 2;
            Conv_Res res = str_to_u64(nxtarg);
            if (res.error) {
              fprintf(stderr, "Invalid integer after '-v' flag.\n");
              args.show_help_and_exit = true;
            } else
              args.verbosity = res.num;
          }
        } else {
          args.verbosity = Log_Debug;
        }
      } else if (str8_eq(arg, str8("--no-gfx"))) {
        args.no_gfx = true;
      } else if (str8_eq(arg, str8("--no-srgb"))) {
        args.no_srgb = true;
      } else if (str8_eq(arg, str8("--prefer-cpu"))) {
        args.prefer_cpu = true;
      } else if (str8_eq(arg, str8("--spline-cpoints"))) {
        args.show_spline_cpoints = true;
      } else if (str8_eq(arg, str8("--wireframe"))) {
        args.wireframe = true;        
      } else if (str8_eq(arg, str8("--graph"))) {
        if (i < argc - 1) {
          ++i;
          args.graph = str8_tmp_from_c(argv[i]);
        } else {
          fprintf(stderr, "Missing filename after --graph flag.\n");
          args.show_help_and_exit = true;
        }
      } else if (str8_eq(arg, str8("--winsize"))) {
        if (i < argc - 2) {
          ++i;
          Conv_Res resx = str_to_u64(str8_tmp_from_c(argv[i]));
          ++i;
          Conv_Res resy = str_to_u64(str8_tmp_from_c(argv[i]));
          if (resx.error || resy.error) {
              fprintf(stderr, "Invalid integer(s) after '--winsize' flag.\n");
              args.show_help_and_exit = true;
          } else {
            args.win_size.x = (i32)Min(resx.num, INT_MAX);
            args.win_size.y = (i32)Min(resy.num, INT_MAX);
          }
        } else {
          fprintf(stderr, "Missing argument(s) after --winsize flag.\n");
          args.show_help_and_exit = true;
        }
      } else {
        args.show_help_and_exit = true;
      }
    } 
  }
  return args;
}

