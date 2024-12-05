// These are all the types that are both valid graph pin types and script types.
#define FAE_LANG_TYPES(Prefix) \
  Prefix##_U64, \
  Prefix##_LANG_FIRST = Prefix##_U64, \
  Prefix##_I64, \
  Prefix##_F64, \
  Prefix##_Bool, \
  Prefix##_LANG_LAST = Prefix##_Bool

#define FAE_LANG_TYPE_STRS \
  "u64", \
  "i64", \
  "f64", \
  "bool"
