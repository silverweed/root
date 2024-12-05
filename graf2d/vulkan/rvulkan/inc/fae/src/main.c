// Stdlibs
// ---------------------------
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Third-party includes
// ---------------------------
#ifdef FAE_SANITIZE_ARENA
#include <sanitizer/asan_interface.h>
#endif

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include "stb_image.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Internal includes
// ---------------------------
#include "core/types.h"
#include "core/mem.h"
#include "core/str.h"

#include "math/math.h"

#include "res/image.h"
#include "res/font.h"

#include "gfx/backend/gfx_vulk.h"

// Sources
// ---------------------------
#include "core/misc.c"
#include "core/log.c"

#ifdef _WIN32
#include "platform/platform_win32.c"
#else
#include "platform/platform_posix.c"
#endif

#include "core/lists.c"
#include "core/mem.c"
#include "core/str.c"
#include "core/files.c"

#include "collections/hashmap.c"

#include "math/math.c"

#include "res/image.c"
#include "res/font.c"

#include "win/input.c"
#include "win/window.c"

#include "gfx/base/meshes.c"
#include "gfx/base/text.c"
#include "gfx/base/camera.c"

#include "gfx/backend/gfx_vulk_buf.c"
#include "gfx/backend/gfx_vulk.c"
#include "gfx/backend/gfx_vulk_glfw.c"
#include "gfx/backend/gfx_vulk_init.c"
#include "gfx/backend/gfx_vulk_draw.c"

#include "gfx/gfx.c"
#include "gfx/win_glfw.c"

#include "graph/parse_common.c"
#include "fae/lang_types.c" // NOTE: this will be moved down when we have data-driven graph pin types
#include "graph/graph_build.c"
#include "graph/graph.c"

#include "editor/editor.c"

#include "fae/script_parse.c"
#include "fae/vm.c"
#include "fae/script_compile.c"

#include "app/app.c"
#include "app/argparse.c"

#ifndef FAE_TESTING
#define FAE_MAIN main
#else
#define FAE_MAIN main_unused
#include "tests.c"
#endif

#define DEFAULT_WIN_WIDTH 800
#define DEFAULT_WIN_HEIGHT 600

typedef struct {
  String8 shaders;
  String8 fonts;
  String8 textures;
} Resources_Dirs;

internal
Resources_Dirs get_resources_dirs(Arena *arena)
{
  Temp scratch = scratch_begin(&arena, 1);

  Resources_Dirs dirs = {};
  String8 exe_path = str8_tmp_from_c(os_get_exe_path());
  String8 exe_dir = file_dirname(scratch.arena, exe_path);
  // @Robustness: maybe this can be improved
  dirs.shaders = push_str8f(arena, "%s/shaders", cstr(exe_dir));
  dirs.fonts = push_str8f(arena, "%s/../assets/fonts", cstr(exe_dir));
  dirs.textures = push_str8f(arena, "%s/../assets/textures", cstr(exe_dir));

  scratch_end(scratch);
  return dirs;
}

internal
Gfx_Config make_gfx_cfg(Cmdline_Args *args, Resources_Dirs *res_dirs)
{
  Gfx_Config gfx_cfg = {};
  f32 win_width  = args->win_size.x ? (f32)args->win_size.x : DEFAULT_WIN_WIDTH;
  f32 win_height = args->win_size.y ? (f32)args->win_size.y : DEFAULT_WIN_HEIGHT;
  gfx_cfg.viewport_size_meters = (V2) { win_width / (f32)PIXELS_PER_METER, win_height / (f32)PIXELS_PER_METER };
  gfx_cfg.pixels_per_meter = PIXELS_PER_METER;
  gfx_cfg.srgb = !args->no_srgb;
  gfx_cfg.prefer_integrated_gpu = args->prefer_cpu;
  gfx_cfg.shaders_dir = res_dirs->shaders;
  gfx_cfg.initial_instances_capacity[GfxObj_Quad] = 5e5;
  gfx_cfg.initial_instances_capacity[GfxObj_Text] = 5e5;
  gfx_cfg.initial_instances_capacity[GfxObj_Screenspace_Quad] = 100;
  gfx_cfg.initial_instances_capacity[GfxObj_Spline] = 5e5;
  gfx_cfg.fonts_dir = res_dirs->fonts;
  gfx_cfg.textures_dir = res_dirs->textures;
  gfx_cfg.font_name = str8("Hack-Regular");
  gfx_cfg.show_spline_control_points = args->show_spline_cpoints;
  gfx_cfg.wireframe = args->wireframe;
  return gfx_cfg;
}

internal
void add_origin_gizmo(Gfx *gfx)
{
  Cpu_Instance_Data inst_data = cpu_inst_data_default();
  inst_data.scale = v3(0.1f, 0.1f, 0.1f);
  inst_data.color_a = inst_data.color_b = v4(0, 1, 0, 1);
  gfx_add_quad(gfx, inst_data);
  inst_data.rot = quat_from_euler(K_HALF_PI, 0, 0);
  // inst_data.rot = quat_from_axis_angle(v3(1, 0, 0), K_HALF_PI);
  inst_data.color_a = inst_data.color_b = v4(0, 0, 1, 1);
  gfx_add_quad(gfx, inst_data);
  inst_data.rot = quat_from_euler(0, 0, K_HALF_PI);
  // inst_data.rot = quat_from_axis_angle(v3(0, 0, 1), K_HALF_PI);
  inst_data.color_a = inst_data.color_b = v4(1, 0, 0, 1);
  gfx_add_quad(gfx, inst_data);

  inst_data = cpu_inst_data_default();
  inst_data.pos.x -= 0.05;
  inst_data.pos.y -= 0.001;
  Char_Size origin_char_size = 3;
  gfx_add_text(gfx, str8("Origin"), origin_char_size, inst_data, Gfx_World_Space);
}

int FAE_MAIN(int argc, char **argv)
{
  Thread_Ctx tctx;
  tctx_init(&tctx);

  // Main persistent arena for the program
  Arena *arena = arena_alloc();
  if (!arena) {
    FATAL("Failed to allocate program memory.");
    tctx_release();
    return 1;
  }
  
  Cmdline_Args args = parse_args(argc, argv);
  if (args.show_help_and_exit) {
    print_help(argv[0]);
    arena_release(arena);
    tctx_release();
    return 0;
  }
  log_set_lv(args.verbosity);

  if (!args.no_gfx) {
    i32 win_width  = args.win_size.x ? args.win_size.x : DEFAULT_WIN_WIDTH;
    i32 win_height = args.win_size.y ? args.win_size.y : DEFAULT_WIN_HEIGHT;
    GLFWwindow *window = glfw_init(win_width, win_height);
    App_State *app = arena_push(App_State, arena);
    app_init(app);
    Resources_Dirs res_dirs = get_resources_dirs(arena);
    Gfx_Config gfx_cfg = make_gfx_cfg(&args, &res_dirs);
    gfx_init(&app->gfx, arena, window, gfx_cfg);
    editor_init(&app->editor, &app->gfx);
    app_init_fps_counter(app);
    add_origin_gizmo(&app->gfx);

    {
      Temp s = scratch_begin(&arena, 1);
      String8 filename = args.graph.size ? args.graph : str8("examples/sum.faed");
      Script_Graph *graph = parse_script_graph_from_file(s.arena, filename);
      editor_set_graph(&app->editor, graph);
      scratch_end(s);
    }

    // Gfx_Instance_Metadata meta;
    // gfx_push_instance_get_meta(&app->gfx.instances,cpu_inst_data_default(), GfxObj_Text, &meta);

    // u32 n_texts = 1;
    // Text *texts = arena_push_array_nozero(Text, arena, n_texts);
    // Text_Create_Data *text_data = arena_push_array_nozero(Text_Create_Data, arena, n_texts);
    // for (u32 i = 0; i < n_texts; ++i) {
    //    text_data[i] = (Text_Create_Data) {
    //     .string = str8("asdsad"),
    //     .char_size = 20
    //   };
    // }
    // Vertex *vertices = texts_create(arena, &app->gfx.font, n_texts, text_data, texts);
    // u32 *vk_inst_ids = arena_push_array_nozero(u32, arena, n_texts);
    // u32 *n_vertices = arena_push_array_nozero(u32, arena, n_texts);
    // for (u32 i = 0; i < n_texts; ++i) {
    //   vk_inst_ids[i] = meta.index_in_data;
    //   n_vertices[i] = texts[i].n_vertices;
    // }
    // b8 screenspace = false;
    // Vulk_Text **vtexts = arena_push_array(Vulk_Text*, arena, n_texts);
    // vulk_add_texts(app->gfx.vk, vertices, n_texts, n_vertices, vk_inst_ids, screenspace, vtexts);
    // for (u32 i = 0; i < n_texts; ++i) {
    //   vulk_add_text(app->gfx.vk, vertices, texts[i].n_vertices, vk_inst_ids[i], screenspace);
    //   vertices += texts[i].n_vertices;
    // }

    // for (u32 i = 0; i < 10000; ++i) {
    //   V3 cp[4] = {
    //     v3((i % 10),       1.5 * ((i % 100) / 10),       1.5 * (i / 100)),
    //     v3((i % 10) + 0.4, 1.5 * ((i % 100) / 10),       1.5 * (i / 100)),
    //     v3((i % 10) + 0.4, 1.5 * ((i % 100) / 10) + 0.5, 1.5 * (i / 100) + 0.4),
    //     v3((i % 10) + 0.4, 1.5 * ((i % 100) / 10) + 0.5, 1.5 * (i / 100) + 0.8),
    //   };
    //   gfx_add_spline(&app->gfx, cp, cpu_inst_data_default());
    // }

    run_main_loop(arena, window, app);
    
    editor_deinit(&app->editor);
    gfx_deinit(&app->gfx);
    glfw_deinit(window);
  } else {
    // TEST
    Fae_Script script = {};
    b8 success = fae_parse_script_from_file(arena, str8("examples/sum.fae"), &script);
    // b8 success = fae_parse_script_from_file(arena, str8("examples/test2.fae"), &script);
    if (!success)
      return 0;

    Vm_Instr *instrs = NULL;
    Fae_Compiler comp = fae_compile_script(arena, &script, &instrs);
    INFO("Script compiled to %lu vm instructions.", comp.n_instrs);

    Fae_Vm vm = vm_create(16 * 1024);
    for (u64 i = 0; i < comp.n_instrs; ++i) {
      INFO(" %s", cstr(vm_pretty_print_instr(arena, instrs[i])));
      vm_push_instr(&vm, instrs[i]);
    }
    while (vm_step(&vm)) ;

    INFO("Value of all variables at the end of the script:");
    Hash_Map_Iter iter = hashmap_iter(&comp.lvalue_map);
    Fae_Ast_Decl *key;
    LValue value;
    while (hashmap_next(&iter, &key, &value)) {
      u64 var_val;
      memcpy(&var_val, vm.memory + value.addr, sizeof(var_val));
      INFO("  %s: %lu", cstr(key->name), var_val);
    }

    vm_destroy(&vm);
    // parse_script_graph_from_file(arena, str8("examples/sum.faed"));
  }

  arena_release(arena);
  tctx_release();
  
  return 0;
}
