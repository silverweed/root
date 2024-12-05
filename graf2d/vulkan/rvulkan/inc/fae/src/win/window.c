typedef struct {
  // Real width and height of the window
  i32 width;
  i32 height;
  b8 size_just_changed;

  f32 desired_aspect_ratio;
} Window_Data;
