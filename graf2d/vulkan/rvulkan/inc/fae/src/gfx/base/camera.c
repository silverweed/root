#define WHEEL_ZOOM_FACTOR 0.15f
#define WHEEL_FOV_FACTOR 0.05f

typedef struct {
  V3 pos;
  f32 zoom;
  f32 speed;
  // angles are all in radians
  f32 pitch;
  f32 yaw;

  f32 fov; // in radians

  b8 is_ortho;
} Camera;

typedef struct {
  f32 pixels_per_meter;
  b8 is_ortho;
  b8 enable_dragging;
} Camera_Move_Params;

internal
void camera_move(Camera *camera, const M44 *cam_xform, User_Input *input, f32 dt, Camera_Move_Params params)
{
  u8 *ks = input->key_state;
  if (params.is_ortho) {
    if (input->mouse_locked) {
      // If mouse is locked, do panning (ortho mode)
      V2 velocity = {};
      if (ks[Key_Left] & KEY_STATE_IS_DOWN || ks[Key_A] & KEY_STATE_IS_DOWN)
        velocity.x -= 1.f;
      if (ks[Key_Right] & KEY_STATE_IS_DOWN || ks[Key_D] & KEY_STATE_IS_DOWN)
        velocity.x += 1.f;
      if (ks[Key_Up] & KEY_STATE_IS_DOWN || ks[Key_W] & KEY_STATE_IS_DOWN)
        velocity.y -= 1.f;
      if (ks[Key_Down] & KEY_STATE_IS_DOWN || ks[Key_S] & KEY_STATE_IS_DOWN)
        velocity.y += 1.f;

      V3 velocity_ws = trans_mul_v3(cam_xform, v3_xy0(velocity));
      velocity_ws = v3_normalized(velocity_ws);
      velocity_ws = v3_muls(velocity_ws, camera->speed);

      camera->pos =
          v3_add(camera->pos, v3_muls(velocity_ws, dt * camera->zoom));
    } else if (params.enable_dragging && (input->mouse_btn_state[MouseBtn_Left] & MOUSE_BTN_STATE_IS_DOWN)) {
      // If camera is ortho and mouse is not locked, do click to drag
      V2 mdelta = v2(input->mouse_delta.x, -input->mouse_delta.y);
      mdelta = v2_muls(mdelta, camera->zoom / params.pixels_per_meter);
      camera->pos = v3_sub(camera->pos, v3_x0z(mdelta));
    }
    camera->zoom *= 1 - WHEEL_ZOOM_FACTOR * input->mouse_wheel_delta;
  } else {
    // perspective camera
    if (input->mouse_locked) {
      // If mouse is locked, do FPS movement (persp mode)
      // NOTE: velocity_hor is (in camera space): { x = right, y = fwd }
      V2 velocity_hor = {};
      f32 velocity_vert_ws = 0;

      if (ks[Key_Left] & KEY_STATE_IS_DOWN || ks[Key_A] & KEY_STATE_IS_DOWN)
        velocity_hor.x -= 1.f;
      if (ks[Key_Right] & KEY_STATE_IS_DOWN || ks[Key_D] & KEY_STATE_IS_DOWN)
        velocity_hor.x += 1.f;
      if (ks[Key_Up] & KEY_STATE_IS_DOWN || ks[Key_W] & KEY_STATE_IS_DOWN)
        velocity_hor.y += 1.f;
      if (ks[Key_Down] & KEY_STATE_IS_DOWN || ks[Key_S] & KEY_STATE_IS_DOWN)
        velocity_hor.y -= 1.f;
      if (ks[Key_Q] & KEY_STATE_IS_DOWN)
        velocity_vert_ws -= 1.f;
      if (ks[Key_E] & KEY_STATE_IS_DOWN)
        velocity_vert_ws += 1.f;

      V3 velocity_hor_ws =
          trans_mul_v3(cam_xform, v3_x0z(velocity_hor));
      V3 velocity = v3_normalized(
          v3_add(velocity_hor_ws, (V3){0.f, 0.f, velocity_vert_ws}));
      velocity = v3_muls(velocity, camera->speed);

      camera->pos = v3_add(camera->pos, v3_muls(velocity, dt / camera->zoom));

      f32 delta_yaw = input->mouse_delta.x * 0.002f;
      f32 delta_pitch = input->mouse_delta.y * 0.002f;
      camera->yaw -= delta_yaw;
      camera->pitch -= delta_pitch;
      camera->pitch =
          Clamp(camera->pitch, -K_HALF_PI - 0.01f, K_HALF_PI + 0.01f);

      camera->speed *= 1 + WHEEL_ZOOM_FACTOR * input->mouse_wheel_delta;
    } else {
      if (params.enable_dragging && (input->mouse_btn_state[MouseBtn_Left] & MOUSE_BTN_STATE_IS_DOWN)) {
        // If mouse is not locked, do click to drag
        V2 mdelta = v2(input->mouse_delta.x, -input->mouse_delta.y);
        mdelta = v2_muls(mdelta, camera->zoom / params.pixels_per_meter);
        V3 delta =
            transform_dir3(cam_xform, v3(mdelta.x, -mdelta.y, 0));
        camera->pos = v3_sub(camera->pos, delta);
      }
      camera->fov =
          Clamp(camera->fov - input->mouse_wheel_delta * WHEEL_FOV_FACTOR,
                0.10f, K_PI - 0.1f);
    }
  }

  VERY_VERBOSE_TAG("Camera",
            "%.1f %.1f %.1f (pitch: %.1f yaw: %.1f, zoom: %.2fx, fov: %.2f, "
            "speed: %.2f)",
            camera->pos.x, camera->pos.y, camera->pos.z, rad2deg(camera->pitch),
            rad2deg(camera->yaw), 1.f / camera->zoom, rad2deg(camera->fov),
            camera->speed);
}

internal
void camera_calc_view_and_inverse(Camera *camera, M44 *view, M44 *inv_view)
{
  // NOTE: we use a right-handed world coordinate system where:
  // X is right
  // Y is forward
  // Z is up
  //
  // Conversely, the camera coordinate system is (still right-handed):
  // X is right
  // Y is down
  // Z is forward
  //
  // So we need to change between the two coordinate systems by multiplying
  // by the matrix
  //
  // |1  0  0  0|
  // |0  0  1  0|
  // |0 -1  0  0|
  // |0  0  0  1|
  //
  // f32 s = camera->zoom; // XXX; zooming through the scale is weird and likely wrong
  f32 s = 1.f;
  f32 sp = sinf(camera->pitch);
  f32 cp = cosf(camera->pitch);
  f32 sy = sinf(camera->yaw);
  f32 cy = cosf(camera->yaw);
  V3 t = camera->pos;
  // Equivalent to:
  // result = change_coord_space; (aka the matrix described in the comment above)
  // result = make_uniform_scale(camera.zoom) * result; 
  // result = Transform4D { make_rotation_z(camera.yaw) * make_rotation_x(camera.pitch) } * result;
  // result = make_translation(camera.position) * result;
  *inv_view = m44(s*cy, -s*sy*sp, -s*sy*cp, t.x,
                  s*sy,  s*sp*cy,  s*cp*cy, t.y,
                  0.f,  -s*cp,     s*sp,    t.z,
                  0.f,   0.f,      0.f,     1.f);

  *view = transform_inverse(inv_view);
}

internal
V3 camera_right(Camera *camera)
{
  // @Speed: cache this?
  M44 view, inv_view;
  camera_calc_view_and_inverse(camera, &view, &inv_view);
  return v3_normalized(v3_from_v4(inv_view.col[0]));  
}

internal
V3 camera_fwd(Camera *camera)
{
  // @Speed: cache this?
  M44 view, inv_view;
  camera_calc_view_and_inverse(camera, &view, &inv_view);
  return v3_normalized(v3_from_v4(inv_view.col[2]));  
}

internal
V3 camera_up(Camera *camera)
{
  // @Speed: cache this?
  M44 view, inv_view;
  camera_calc_view_and_inverse(camera, &view, &inv_view);
  return v3_normalized(v3_neg(v3_from_v4(inv_view.col[1])));  
}

