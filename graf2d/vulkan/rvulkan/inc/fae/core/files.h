u64 file_size(FILE *f);

// NOTE: the returned string has the same lifetime as fname.
String8 file_basename(String8 fname);
String8 file_dirname(Arena *arena, String8 fname);

String8 file_read_to_string(Arena *arena, String8 fname);
