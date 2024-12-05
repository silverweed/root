internal
f32 g_mouse_wheel_delta;

internal 
void glfw_error_callback(i32 error, const char *description)
{
  ERR_TAG("GLFW", "error %d: %s\n", error, description);
}

internal
void glfw_scroll_callback(GLFWwindow *win, f64 xoff, f64 yoff)
{
  (void)win;
  (void)xoff;
  
  g_mouse_wheel_delta = (f32)yoff;
}

internal 
GLFWwindow *glfw_init(i32 desired_win_width, i32 desired_win_height)
{
  glfwSetErrorCallback(glfw_error_callback);
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  // glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
  // glfwWindowHint(GLFW_DEPTH_BITS, 32);

  GLFWwindow *window = glfwCreateWindow(
      desired_win_width, desired_win_height,
      "Fae editor",
      NULL, NULL
  );

  glfwSetScrollCallback(window, glfw_scroll_callback);

  return window;
}

internal
void glfw_deinit(GLFWwindow *window)
{
  glfwDestroyWindow(window);
  glfwTerminate();
}

internal
void glfw_monitor_key(GLFWwindow *window, u8 *key_state, i32 glfw_key, Input_Key key)
{
  u8 *state = &key_state[key];
  if (glfwGetKey(window, glfw_key) == GLFW_PRESS) {
    if (!(*state & KEY_STATE_IS_DOWN))
      *state |= KEY_STATE_JUST_PRESSED;
    *state |= KEY_STATE_IS_DOWN;
  } else if (glfwGetKey(window, glfw_key) == GLFW_RELEASE) {
    if (*state & KEY_STATE_IS_DOWN) 
      *state |= KEY_STATE_JUST_RELEASED;
    *state &= ~KEY_STATE_IS_DOWN;
  }
}

internal
void glfw_monitor_mouse_btn(GLFWwindow *window, u16 *mouse_btn_state, i32 glfw_btn, Mouse_Button btn)
{
  u16 *state = &mouse_btn_state[btn];
  if (glfwGetMouseButton(window, glfw_btn) == GLFW_PRESS) {
    if (!(*state & MOUSE_BTN_STATE_IS_DOWN)) 
      *state |= MOUSE_BTN_STATE_JUST_PRESSED;
    *state |= MOUSE_BTN_STATE_IS_DOWN;
  } else if (glfwGetMouseButton(window, glfw_btn) == GLFW_RELEASE) {
    if (*state & MOUSE_BTN_STATE_IS_DOWN) 
      *state |= MOUSE_BTN_STATE_JUST_RELEASED;
    *state &= ~MOUSE_BTN_STATE_IS_DOWN;
  }
}

internal
void glfw_update_keyboard(GLFWwindow *window, User_Input *input)
{
  u8 *key_state = input->key_state;
  glfw_monitor_key(window, key_state, GLFW_KEY_A, Key_A);
  glfw_monitor_key(window, key_state, GLFW_KEY_C, Key_C);
  glfw_monitor_key(window, key_state, GLFW_KEY_D, Key_D);
  glfw_monitor_key(window, key_state, GLFW_KEY_E, Key_E);
  glfw_monitor_key(window, key_state, GLFW_KEY_H, Key_H);
  glfw_monitor_key(window, key_state, GLFW_KEY_O, Key_O);
  glfw_monitor_key(window, key_state, GLFW_KEY_Q, Key_Q);
  glfw_monitor_key(window, key_state, GLFW_KEY_R, Key_R);
  glfw_monitor_key(window, key_state, GLFW_KEY_S, Key_S);
  glfw_monitor_key(window, key_state, GLFW_KEY_V, Key_V);
  glfw_monitor_key(window, key_state, GLFW_KEY_W, Key_W);
  glfw_monitor_key(window, key_state, GLFW_KEY_UP, Key_Up);
  glfw_monitor_key(window, key_state, GLFW_KEY_DOWN, Key_Down);
  glfw_monitor_key(window, key_state, GLFW_KEY_LEFT, Key_Left);
  glfw_monitor_key(window, key_state, GLFW_KEY_RIGHT, Key_Right);
  glfw_monitor_key(window, key_state, GLFW_KEY_LEFT_ALT, Key_LAlt);
  glfw_monitor_key(window, key_state, GLFW_KEY_LEFT_CONTROL, Key_LCtrl);
  glfw_monitor_key(window, key_state, GLFW_KEY_LEFT_SHIFT, Key_LShift);
  glfw_monitor_key(window, key_state, GLFW_KEY_RIGHT_ALT, Key_RAlt);
  glfw_monitor_key(window, key_state, GLFW_KEY_RIGHT_CONTROL, Key_RCtrl);
  glfw_monitor_key(window, key_state, GLFW_KEY_RIGHT_SHIFT, Key_RShift);
  glfw_monitor_key(window, key_state, GLFW_KEY_TAB, Key_Tab);

  input->ctrl = key_state[Key_LCtrl] & KEY_STATE_IS_DOWN || key_state[Key_RCtrl] & KEY_STATE_IS_DOWN;
  input->alt = key_state[Key_LAlt] & KEY_STATE_IS_DOWN || key_state[Key_RAlt] & KEY_STATE_IS_DOWN;
  input->shift = key_state[Key_LShift] & KEY_STATE_IS_DOWN || key_state[Key_RShift] & KEY_STATE_IS_DOWN;
}

internal
b8 glfw_update_mouse_lock_state(GLFWwindow *window, User_Input *input)
{
  if (input->key_state[Key_Tab] & KEY_STATE_JUST_PRESSED) {
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      input->mouse_locked = false;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      input->mouse_locked = true;
    }

    DEBUG_TAG("Window", "Mouse lock state changed");
    return true;
  }
  return false;
}

internal
void glfw_update_mouse(GLFWwindow *window, User_Input *input, Window_Data *win_data)
{
  f64 mposx, mposy;
  glfwGetCursorPos(window, &mposx, &mposy);

  b8 changed = glfw_update_mouse_lock_state(window, input);

  i32 win_halfwidth = win_data->width / 2;
  i32 win_halfheight = win_data->height / 2;
  if (!changed && glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
    input->mouse_delta.x = (i32)mposx - win_halfwidth;
    input->mouse_delta.y = (i32)mposy - win_halfheight;
    glfwSetCursorPos(window, win_halfwidth, win_halfheight);
  } else {
    input->mouse_delta.x = (i32)mposx - input->mouse_pos.x;
    input->mouse_delta.y = (i32)mposy - input->mouse_pos.y;
    if (changed) glfwSetCursorPos(window, win_halfwidth, win_halfheight);
  }

  input->mouse_pos.x = (i32)mposx;
  input->mouse_pos.y = (i32)mposy;

  input->mouse_wheel_delta = g_mouse_wheel_delta;
  g_mouse_wheel_delta = 0;

  u16 *mouse_btn_state = input->mouse_btn_state;
  glfw_monitor_mouse_btn(window, mouse_btn_state, GLFW_MOUSE_BUTTON_LEFT, MouseBtn_Left);
  glfw_monitor_mouse_btn(window, mouse_btn_state, GLFW_MOUSE_BUTTON_RIGHT, MouseBtn_Right);
}

internal
u32 glfw_get_screen_refresh_rate()
{
  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *video_mode = glfwGetVideoMode(monitor);
  return video_mode ? video_mode->refreshRate : 0;
}


