#ifndef ROOT_LLR_TYPES
#define ROOT_LLR_TYPES

namespace ROOT::LLR {

template <typename T>
struct V3 final {
  T x, y, z;

  V3() = default;
  V3(const V3 &) = default;
  V3(V3 &&) = default;
  V3 &operator=(const V3 &) = default;
  V3 &operator=(V3 &&) = default;

  V3(T x, T y, T z) : x(x), y(y), z(z) {}

  template <typename U>
  explicit V3(const V3<U> &v) : x(v.x), y(v.y), z(v.z) {}
};

using V3f = V3<float>;
using V3d = V3<double>;

struct RVertex {
  V3f fPos;

  RVertex() = default;
  
  template <typename T>
  RVertex(const V3<T> &pos) : fPos(pos) {}
};

}

#endif
