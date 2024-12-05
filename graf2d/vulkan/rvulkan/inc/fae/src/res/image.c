Image image_load(const void *img, u64 img_size)
{  
  u64 start_ns = os_clock_time_ns();
  
  Image image;
  // @ForeignAlloc: consider redefining STBI_MALLOC/REALLOC/FREE
  // NOTE: we don't really need the alpha channel, but it's more likely that the Vulkan implementation supports
  // a 4-channel image than a 3-channel one.
  image.pixels = stbi_load_from_memory((const stbi_uc *)img, img_size, &image.width, &image.height, &image.channels, STBI_rgb_alpha);
  // Since we requested STBI_rgb_alpha, the channels *will* be 4, but image.channels now contains the number of channels
  // that the image would have had, so we need to overwrite it (see stb_image.h:154 onwards).
  image.channels = 4;
  if (!image.pixels) {
    FATAL_TAG("Font", "Failed to load image");
    os_abort();
  }

  u64 end_ns = os_clock_time_ns();
  INFO_TAG("Font", "Loading image took %.2f ms", (end_ns - start_ns) * 1e-6f);

  return image;
}

// @Speed: load a single image instead of one per face
Cube_Image image_load_cube(String8 images[6])
{  
  Cube_Image cube;
  for (u32 i = 0; i < 6; ++i) {
    Image image = image_load(images[i].str, images[i].size);
    cube.face[i] = image;
  }
  cube.width = cube.face[0].width;
  cube.height = cube.face[0].height;
  cube.channels = cube.face[0].channels;
  return cube;
}

void image_unload(Image image)
{
  stbi_image_free((stbi_uc *)image.pixels);
}

void image_cube_unload(Cube_Image image)
{
  for (u32 i = 0; i < 6; ++i)
    image_unload(image.face[i]);
}

Image image_load_from_file(String8 file)
{
  Temp scratch = scratch_begin(0, 0);
  
  String8 file_content = file_read_to_string(scratch.arena, file);
  Image image = image_load(file_content.str, file_content.size);

  scratch_end(scratch);

  return image;
}

// XXX: improve this API.
// Currently assumes that there are 6 files names px.png, py.png, pz.png, nx.png, ... in `dir`.
Cube_Image image_load_cube_from_file(String8 dir)
{
  Temp scratch = scratch_begin(0, 0);
  
  String8 file_contents[6];
  file_contents[0] = file_read_to_string(scratch.arena, push_str8f(scratch.arena, "%s/px.png", cstr(dir)));
  file_contents[1] = file_read_to_string(scratch.arena, push_str8f(scratch.arena, "%s/nx.png", cstr(dir)));
  file_contents[2] = file_read_to_string(scratch.arena, push_str8f(scratch.arena, "%s/py.png", cstr(dir)));
  file_contents[3] = file_read_to_string(scratch.arena, push_str8f(scratch.arena, "%s/ny.png", cstr(dir)));
  file_contents[4] = file_read_to_string(scratch.arena, push_str8f(scratch.arena, "%s/pz.png", cstr(dir)));
  file_contents[5] = file_read_to_string(scratch.arena, push_str8f(scratch.arena, "%s/nz.png", cstr(dir)));
  Cube_Image image = image_load_cube(file_contents);

  scratch_end(scratch);

  return image;
}

