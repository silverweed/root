typedef enum {
  Key_Up,
  Key_Down,
  Key_Left,
  Key_Right,
  Key_A,
  Key_C,
  Key_D,
  Key_E,
  Key_H,
  Key_O,
  Key_Q,
  Key_R,
  Key_S,
  Key_V,
  Key_W,
  Key_LCtrl,
  Key_LAlt,
  Key_LShift,
  Key_RCtrl,
  Key_RAlt,
  Key_RShift,
  Key_Tab,

  Key_COUNT
} Input_Key;

typedef enum {
  KEY_STATE_IS_DOWN = 0x1,
  KEY_STATE_JUST_PRESSED = 0x2,
  KEY_STATE_JUST_RELEASED = 0x4,
} Key_State;

typedef enum {
  MouseBtn_Left,
  MouseBtn_Right,

  MouseBtn_COUNT
} Mouse_Button;

typedef enum {
  MOUSE_BTN_STATE_IS_DOWN = 0x1,
  MOUSE_BTN_STATE_JUST_PRESSED = 0x2,
  MOUSE_BTN_STATE_JUST_RELEASED = 0x4,
} Mouse_Button_State;

typedef struct {
  u8 key_state[Key_COUNT];
  b8 ctrl;
  b8 alt;
  b8 shift;

  u16 mouse_btn_state[MouseBtn_COUNT];
  V2i mouse_delta;
  V2i mouse_pos;
  f32 mouse_wheel_delta;
  b8 mouse_locked;
} User_Input;
