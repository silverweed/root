#pragma once

typedef struct {
  void *pixels;
  i32 width, height;
  i32 channels; 
} Image;

typedef struct {
  // @Cleanup: don't repeat width/height/channels 7 times.
  Image face[6];
  i32 width, height;
  i32 channels; 
} Cube_Image;

FAE_API Image image_load_from_file(String8 file);
FAE_API Image image_load(const void *img, u64 img_size);
FAE_API void image_unload(Image image);

FAE_API Cube_Image image_load_cube_from_file(String8 dir);
FAE_API Cube_Image image_load_cube(String8 images[6]);
FAE_API void image_cube_unload(Cube_Image image);

