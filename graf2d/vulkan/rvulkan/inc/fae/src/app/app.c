typedef struct {
  f32 *base;
  u16 count;
  u16 max;
  u16 start;
} Delta_Time_Accum;

internal
void accum_dt(Delta_Time_Accum *accum, f32 dt)
{
  if (accum->count < accum->max) {
    assert(accum->start == 0);
    accum->base[accum->count++] = dt;
  } else {
    accum->base[accum->start++] = dt;
    if (accum->start == accum->max)
      accum->start = 0;
  }
}

internal
f32 calc_avg_dt(const Delta_Time_Accum *accum)
{
  f32 res = 0;
  for (u16 idx = 0; idx < accum->count; ++idx)
    res += accum->base[idx];
  if (accum->count) res /= accum->count;
  return res;
}

typedef struct {
  Window_Data win_data;
  User_Input user_input;
  b8 should_quit;
  u64 target_frame_microseconds;

  Gfx gfx;
  Editor editor;

  b8 vsync;

  Gfx_Instance_Id fps_text;
  //f32 time; // DEBUG
  Gfx_Instance_Id hand; // DEBUG
} App_State;

internal u32 glfw_get_screen_refresh_rate();

internal
void app_init(App_State *app)
{
  u32 screen_freq = glfw_get_screen_refresh_rate();
  if (!screen_freq)
    screen_freq = 60;
  u64 target_ms = 1000000 / screen_freq;
  app->target_frame_microseconds = target_ms;
  app->vsync = true;
}

internal
void app_init_fps_counter(App_State *app)
{
  Cpu_Instance_Data inst_data = cpu_inst_data_default();
  // V2 vp_size = app->gfx.config.viewport_size;
  // inst_data.pos = v3(1.0, 0, 0.0); // TODO
  Char_Size fps_char_size = 2;
  app->fps_text = gfx_add_text(&app->gfx, str8("?? FPS"), fps_char_size, inst_data, Gfx_Screen_Space);

  // TEMP DEBUG
  // inst_data.scale = v3(1.f, 1.f, 0.2);
  // app->hand = gfx_add_quad(&app->gfx, inst_data);
}

internal
void app_handle_inputs(App_State *app)
{
  b8 handled = editor_handle_inputs(&app->editor, &app->user_input);

  if (handled)
    return;

  if (app->user_input.key_state[Key_V] & KEY_STATE_JUST_PRESSED) {
    app->vsync = !app->vsync;
  }
}

internal
void app_update_and_draw(App_State *app, GLFWwindow *window, f32 dt)
{
#if 0
  // TEMP DEBUG: "arm" positioning
  V2 mpos = v2_from_v2i(app->user_input.mouse_pos);
  V3 mwpos = unproject_screen_pos(mpos, &app->gfx.inv_view_proj, &app->gfx.proj, app->gfx.viewport_px, 1.f);
  Cpu_Instance_Data *hdata = gfx_get_instance_data(&app->gfx.instances, app->hand);
  // Camera *camera = &app->editor.camera;
  V3 base_pos = v3(0.25, 0.5, 1.0);
  V3 mvpos = transform_pos3(&app->gfx.view, mwpos);
  (void)mvpos;
  M33 lookat = m33_look_from_to_viewspace(base_pos, mvpos);
  // M33 lookat = m33_look_at_viewspace(mvpos);
  // Quat br = quat_from_axis_angle(v3(0, 1, 0), K_HALF_PI);
  // br = quat_mul(quat_from_axis_angle(v3(0, 1, 0), K_HALF_PI), br);
  // M33 baserot = quat_to_rot_matrix(br);
  // lookat = m33_mul(&lookat, &baserot);
  // lookat = baserot;
  // lookat = m33_identity();
  // (void)lookat;
  M33 iv = m33_from_m44(&app->gfx.inv_view);
  hdata->pos = transform_pos3(&app->gfx.inv_view, base_pos);
  M33 wlookat = m33_mul(&iv, &lookat);
  hdata->rot = quat_from_rot_matrix(&wlookat);
#endif

  // NOTE: the editor updates view and projection here
  editor_update_and_draw(&app->editor, &app->user_input, dt);
 
  gfx_draw(&app->gfx, window, dt);
}

internal 
void reset_user_input(User_Input *input)
{
  for (u32 i = 0; i < Key_COUNT; ++i)
    input->key_state[i] &= ~(KEY_STATE_JUST_PRESSED|KEY_STATE_JUST_RELEASED);

  for (u32 i = 0; i < MouseBtn_COUNT; ++i)
    input->mouse_btn_state[i] &= ~(MOUSE_BTN_STATE_JUST_PRESSED|MOUSE_BTN_STATE_JUST_RELEASED);

  zero_struct(&input->mouse_delta);
}

internal 
void run_main_loop(Arena *arena, GLFWwindow *window, App_State *app)
{
  f32 delta_time; // in seconds
  u64 last_saved_time_ns = os_clock_time_ns();

  Delta_Time_Accum dt_accum = {};
  dt_accum.max = 100;
  dt_accum.base = arena_push_array(f32, arena, dt_accum.max);

  f32 seconds_since_last_fps_update = 0;
  
  while (!app->should_quit) {
    // update frame time
    u64 frame_start_ns = os_clock_time_ns();
    u64 time_since_prev_frame_ns = frame_start_ns - last_saved_time_ns;
    delta_time = time_since_prev_frame_ns * 1e-9f;
    assert(delta_time >= 0);
    last_saved_time_ns = frame_start_ns;

    accum_dt(&dt_accum, delta_time);

    log_new_frame();

    reset_user_input(&app->user_input);

    glfwPollEvents();

    // update window size
    {
      Window_Data *wdata = &app->win_data;

      i32 prev_width = wdata->width;
      i32 prev_height = wdata->height;

      glfwGetWindowSize(window, &wdata->width, &wdata->height);

      wdata->size_just_changed = prev_width != wdata->width || prev_height != wdata->height;
    }

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwWindowShouldClose(window)) {
      app->should_quit = true;
      break;
    }

    b32 focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
    if (focused) {
      glfw_update_keyboard(window, &app->user_input);
      glfw_update_mouse(window, &app->user_input, &app->win_data);
    }

    app_handle_inputs(app);
    if (app->win_data.size_just_changed) {
      gfx_resized(&app->gfx, window);
    }
    app_update_and_draw(app, window, delta_time);

    // limit framerate
    if (app->vsync) {
      u64 frame_end_ns = os_clock_time_ns();
      u64 elapsed_ns = frame_end_ns - frame_start_ns;
      i64 spare_ms = (i64)(app->target_frame_microseconds / 1e3 - elapsed_ns / 1e6);
      if (spare_ms > 1) {
        os_sleep_ms((u32)spare_ms - 1);
      }
    }

    // report fps
    seconds_since_last_fps_update += delta_time;
    if (seconds_since_last_fps_update > 1.f) {
      seconds_since_last_fps_update -= 1.f;
      f32 avg_dt = calc_avg_dt(&dt_accum);
      i32 fps = (i32)(1.f / avg_dt);
      // update fps text
      Temp scratch = scratch_begin(&arena, 1);
      gfx_set_text_string(&app->gfx, app->fps_text, push_str8f(scratch.arena, "%d FPS", fps));
      scratch_end(scratch);
    }
  }

  gfx_end_main_loop(&app->gfx);
}
