u64 file_size(FILE *f)
{
  fseek(f, 0, SEEK_END);
  size_t res = ftell(f);
  fseek(f, 0, SEEK_SET);
  return res;
}

// NOTE: the returned string has the same lifetime as fname.
String8 file_basename(String8 fname)
{
  usz bname_size = 0;
  while (bname_size < fname.size) {
    u64 idx = fname.size - bname_size - 1;
    if (fname.str[idx] == '/' || fname.str[idx] == '\\')
      break;
    ++bname_size;
  }
  String8 bname = {
    fname.str + fname.size - bname_size,
    bname_size
  };
  return bname;
}

String8 file_dirname(Arena *arena, String8 fname)
{
  usz bname_size = 0;
  while (bname_size < fname.size) {
    u64 idx = fname.size - bname_size - 1;
    if (fname.str[idx] == '/' || fname.str[idx] == '\\')
      break;
    ++bname_size;
  }

  u64 dirname_size = fname.size - bname_size;
  if (dirname_size > 1)
    --dirname_size; // strip the trailing '/' unless the full dirname is '/'
  String8 dname = str8_from_buf(arena, fname.str, dirname_size);
  return dname;
}

String8 file_read_to_string(Arena *arena, String8 fname)
{
  String8 res = {};
  FILE *file = fopen(cstr(fname), "rb");
  if (!file) {
    ERR("Failed to open file '%s' for reading: %s (%d)", cstr(fname), strerror(errno), errno);
    return res;
  }
  usz fsize = file_size(file);
  res.str = arena_push_array_nozero(u8, arena, fsize + 1);
  res.size = fsize;
  res.str[fsize] = 0;

  usz nread = fread(res.str, 1, fsize, file);
  if (nread != fsize) {
    ERR("Read %lu bytes from %s instead of expected %lu", nread, cstr(fname), fsize);
  }

  return res;
}
